/*
 * -------------------------------------------------------------------
 * A10001986 Crosley CR-9 Jukebox
 * (C) 2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Jukebox
 * https://jukebox.out-a-ti.me
 *
 * Main controller
 *
 * -------------------------------------------------------------------
 * License: Modified MIT NON-AI
 * 
 * Permission is hereby granted, free of charge, to any person 
 * obtaining a copy of this software and associated documentation 
 * files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, 
 * merge, publish, distribute, sublicense, and/or sell copies of the 
 * Software, and to permit persons to whom the Software is furnished to 
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software.
 * 
 * Links inside the Software pointing to the original source must not 
 * be changed or removed.
 *
 * In addition, the following restrictions apply:
 * 
 * 1. The Software and any modifications made to it may not be used 
 * for the purpose of training or improving machine learning algorithms, 
 * including but not limited to artificial intelligence, natural 
 * language processing, or data mining. This condition applies to any 
 * derivatives, modifications, or updates based on the Software code. 
 * Any usage of the Software in an AI-training dataset is considered a 
 * breach of this License.
 *
 * 2. The Software may not be included in any dataset used for 
 * training or improving machine learning algorithms, including but 
 * not limited to artificial intelligence, natural language processing, 
 * or data mining.
 *
 * 3. Any person or organization found to be in violation of these 
 * restrictions will be subject to legal action and may be held liable 
 * for any damages resulting from such use.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "jb_global.h"

#include <Arduino.h>
#include <WiFi.h>
#include "input.h"

#include "jb_main.h"
#include "jb_settings.h"
#include "jb_audio.h"
#include "jb_wifi.h"
#include "jbdisplay.h"

                               // rotary encoders for volume
#define DUPPAV2_ADDR      0x01 // [user configured: A0 closed]
#define ADDA4991_ADDR     0x36 // [default]

                               // rotary encoders for jog dial 1
#define DUPPAV2J1_ADDR    0x02 // [user configured: A1 closed]
#define ADDA4991J1_ADDR   0x37 // [non-default: A0 closed]

                               // rotary encoders for jog dial 2
#define DUPPAV1MJ2_ADDR_U 0x20 // Default; Will be re-set to 0x04 by firmware
#define DUPPAV1MJ2_ADDR   0x04
#define DUPPAV2J2_ADDR    0x04 // [user configured: A2 closed]
#define ADDA4991J2_ADDR   0x38 // [non-default: A1 closed]

unsigned long powerupMillis = 0;

uint32_t csf = 0;

// whiteLED PWM properties
#define WLED_FREQ     5000
#define WLED_CHANNEL  0
#define WLED_RES      8
static PWMLED whiteLED(TOPLIGHT_PWM_PIN);
static uint32_t       whiteMaxBri = 255;
static unsigned long  whiteFade = 0;
static unsigned long  wfDelay = 0;
static int32_t        wfStep = 0;

// blueLED PWM properties
#define BLED_FREQ     5000
#define BLED_CHANNEL  1
#define BLED_RES      8
static PWMLED blueLED(BLUELIGHT_PWM_PIN);
static uint32_t       blueMaxBri = 255;
static unsigned long  blueFade = 0;
static unsigned long  bfDelay = 0;
static int32_t        bfStep = 0;

static JBLED jbLED = JBLED(0);

static PanelLEDs pLEDs = PanelLEDs();

static bool useFakePowerSwitch = true;

static unsigned long lastPwrPress = 0;

static bool isFPBKeyPressed = false;
static bool isFPBKeyHeld = false;
static bool isFPBKeyChange = false;

static const uint8_t rotEncVAddr[2*2] = {
    DUPPAV2_ADDR,  TC_RE_TYPE_DUPPAV2,
    ADDA4991_ADDR, TC_RE_TYPE_ADA4991
};
static TCRotEnc rotEncV(2, rotEncVAddr);

static const uint8_t rotEncJ1Addr[2*2] = {
    DUPPAV2J1_ADDR,  TC_RE_TYPE_DUPPAV2,
    ADDA4991J1_ADDR, TC_RE_TYPE_ADA4991
};
static TCRotEnc rotEncJ1(2, rotEncJ1Addr);

static const uint8_t rotEncJ2Addr[4*2] = {
    DUPPAV1MJ2_ADDR_U, TC_RE_TYPE_DUPPAV1M_U,
    DUPPAV1MJ2_ADDR, TC_RE_TYPE_DUPPAV1M,
    DUPPAV2J2_ADDR,  TC_RE_TYPE_DUPPAV2,
    ADDA4991J2_ADDR, TC_RE_TYPE_ADA4991
};
static TCRotEnc rotEncJ2(2, rotEncJ2Addr);

bool showUpdAvail = true;

int  opMode = 0;

static unsigned long triggerOpAnnounce = 0;

const char jbltrs[] = "ABCDEFGHJK";

// Panel LEDs
static bool panelLEDsLock = false;
static unsigned long panelLEDsLockNow = 0;
static bool triggerUpdatePLEDs = false;
static int  targetTrack = 0;
static int  MFSelectMode = 0;
static unsigned int targetFolder = 0;
static unsigned long lastMFsel = 0;
static int numnomusic = 0;

static int numStreams = 0;
static int maxStream = 0;
static int targetStream = 0;
int        curStream = 0;
int        streamStatus[NUM_STREAMS];

static bool   haveRem = true;
static bool   remHaveStatus = false;      // We received something on the backchannel
static bool   remHaveFullStatus = false;  // We have song number, first and last -> we can use the jogdials and panelLEDs
static bool   remHaveMusic = false;       // Status reported "no music" (which deducted from !off, and defaults to true)
static bool   haveGoto = false;
static struct mp_rem_stat rem_stat;
static int    remFirstTrack = 0, remLastTrack = 0;
static bool   remActiveInt = false, remShuffleInt = false;
static int    remTargetTrack = 0, remTargetLtr = 0, remTargetNum = 0;
static int    remVolPerc = 0;
static bool   remAllowShuffleSignal = false;
bool          remIncI = false;

bool networkTimeTravel = false;
bool networkTCDTT      = false;
bool networkReentry    = false;
bool networkAbort      = false;
bool networkAlarm      = false;
uint16_t networkLead   = ETTO_LEAD;
uint16_t networkP1     = 6600;

static bool tcdIsBusy  = false;

static bool useNM = false;
static bool tcdNM = false;
static bool NM_blue = true;
static bool ignoreTT = false;

bool doPrepareTT = false;
bool doWakeup = false;

// Time travel status flags etc.
static bool          TTSeqActive = false;
static unsigned long TTstart = 0;
static unsigned long P0duration = ETTO_LEAD;
static unsigned long P1_maxtimeout = 10000;

// Durations of tt phases for internal tt
#define P0_DUR          5000    // acceleration phase
#define P1_DUR_TCD      6600    // time tunnel phase (synced; overruled by TCD network commands)
#define P1_DUR          5000    // time tunnel phase (stand-alone)
#define P2_DUR          3000    // re-entry phase (unused)

static unsigned long volchgnow = 0;
static int           newVolume = 0;

static unsigned long strchgnow = 0;
static unsigned long opmchgnow = 0;

static bool          nmOld = false;

#define INPUTLEN_MAX 6
static char          inputBuffer[INPUTLEN_MAX + 2];
static char          inputBackup[INPUTLEN_MAX + 2];
static int           inputIndex = 0;
static bool          inputRecord = false;
static unsigned long lastKeyPressed = 0;

// BTTF network
#define BTTFN_VERSION              1
#define BTTFN_SUP_MC            0x80
#define BTTFN_SUP_ND            0x40
#define BTTF_PACKET_SIZE          48
#define BTTF_DEFAULT_LOCAL_PORT 1338
#define BTTFN_POLL_INT          1300
#define BTTFN_RESPONSE_TO        700
#define BTTFN_KA_OFFSET            0
#define BTTFN_KA_INTERVAL  ((60+BTTFN_KA_OFFSET)*1000)
#define BTTFN_DATA_TO          20000
#define BTTFN_TYPE_ANY     0    // Any, unknown or no device
#define BTTFN_TYPE_FLUX    1    // Flux Capacitor
#define BTTFN_TYPE_SID     2    // SID
#define BTTFN_TYPE_PCG     3    // Dash Gauges
#define BTTFN_TYPE_VSR     4    // VSR
#define BTTFN_TYPE_AUX     5    // Aux (user custom device) = Jukebox
#define BTTFN_TYPE_REMOTE  6    // Futaba remote control
#define BTTFN_NOT_PREPARE  1
#define BTTFN_NOT_TT       2
#define BTTFN_NOT_REENTRY  3
#define BTTFN_NOT_ABORT_TT 4
#define BTTFN_NOT_ALARM    5
#define BTTFN_NOT_REFILL   6
#define BTTFN_NOT_FLUX_CMD 7
#define BTTFN_NOT_SID_CMD  8
#define BTTFN_NOT_PCG_CMD  9
#define BTTFN_NOT_WAKEUP   10
#define BTTFN_NOT_AUX_CMD  11
#define BTTFN_NOT_VSR_CMD  12
#define BTTFN_NOT_SPD      15
#define BTTFN_NOT_INFO     16
#define BTTFN_NOT_DATA     128  // bit only, not value
#define BTTFN_REMCMD_KP_PING     4
#define BTTFN_REMCMD_KP_KEY      5
#define BTTFN_REMCMD_KP_BYE      6
#define BTTFN_REM_MAX_COMMAND  BTTFN_REMCMD_KP_BYE
#define BTTFN_REMCMD_KEEPALIVE 101
#define BTTFN_TCDI1_NOREM   0x0001
#define BTTFN_TCDI1_NOREMKP 0x0002
#define BTTFN_TCDI1_EXT     0x0004
#define BTTFN_TCDI1_OFF     0x0008
#define BTTFN_TCDI1_NM      0x0010
#define BTTFN_TCDI2_BUSY    0x0001
static const uint8_t BTTFUDPHD[4] = { 'B', 'T', 'T', 'F' };
static bool          useBTTFN = false;
static WiFiUDP       bttfUDP;
static UDP*          bUDP;
static WiFiUDP       bttfMcUDP;
static UDP*          jbMcUDP;
static byte          BTTFUDPBuf[BTTF_PACKET_SIZE];
static byte          BTTFUDPTBuf[BTTF_PACKET_SIZE];
static unsigned long BTTFNUpdateNow = 0;
static unsigned long bttfnJBPollInt = BTTFN_POLL_INT;
static unsigned long BTTFNTSRQAge = 0;
static unsigned long BTTFNLastCmdSent = 0;
static bool          BTTFNPacketDue = false;
static bool          BTTFNWiFiUp = false;
static uint8_t       BTTFNfailCount = 0;
static uint32_t      BTTFUDPID = 0;
static unsigned long lastBTTFNpacket = 0;
static unsigned long lastBTTFNKA = 0;
static unsigned long bttfnLastNotData = 0;
static bool          BTTFNBootTO = false;
static bool          haveTCDIP = false;
static IPAddress     bttfnTcdIP;
static uint32_t      bttfnTCDSeqCnt = 0;
static uint32_t      bttfnTCDDataSeqCnt = 0;
static uint32_t      bttfnSessionID = 0;
int                  bttfnHaveTCDSSID = 0;
char                 TCDSSID[8] = { 0 };
uint8_t              TCDpwMarker = 0;
static uint8_t       bttfnReqStatus = 0x50; // Request capabilities, status
static bool          TCDSupportsNOTData = false;
static bool          TCDSupportsSSID = false;
static bool          bttfnDataNotEnabled = false;
static uint32_t      tcdHostNameHash = 0;
static byte          BTTFMCBuf[BTTF_PACKET_SIZE];
static IPAddress     bttfnMcIP(224, 0, 0, 224);

static int      iCmdIdx = 0;
static int      oCmdIdx = 0;
static uint32_t commandQueue[16] = { 0 };

#ifdef ESP32
/*  "warning: taking address of packed member of 'struct <anonymous>' may 
 *  result in an unaligned pointer value"
 *  "GCC will issue this warning when accessing an unaligned member of 
 *  a packed struct due to the incurred penalty of unaligned memory 
 *  access. However, all ESP chips (on both Xtensa and RISC-V 
 *  architectures) allow for unaligned memory access and incur no extra 
 *  penalty."
 *  https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s3/migration-guides/release-5.x/5.0/gcc.html
 */
#define GET32(a,b)    *((uint32_t *)((a) + (b)))
#define SET32(a,b,c)  *((uint32_t *)((a) + (b))) = c
#else
#define GET32(a,b)          \
    (((a)[b])            |  \
    (((a)[(b)+1]) << 8)  |  \
    (((a)[(b)+2]) << 16) |  \
    (((a)[(b)+3]) << 24))   
#define SET32(a,b,c)                        \
    (a)[b]       = ((uint32_t)(c)) & 0xff;  \
    ((a)[(b)+1]) = ((uint32_t)(c)) >> 8;    \
    ((a)[(b)+2]) = ((uint32_t)(c)) >> 16;   \
    ((a)[(b)+3]) = ((uint32_t)(c)) >> 24; 
#endif


// Forward declarations ------

static void init_streams();

static void rem_request_status();

static void switch_opMode(int previousMode);

static void timeTravel(bool TCDtriggered, uint16_t P0Dur, uint16_t P1Dur = 0);

static void handleRemoteCommand();
static int  execute(bool injected);

static void play_nomusic();
static void play_startup();

static void illuminateMusicFolder(int t, int direct = 0);

static void bluelight(int onOff, int useFader = 0);
static void toplights(int onOff, int useFader = 0);
static void nightmode(bool onOff);

static void fpbKeyPressed();
static void fpbKeyLongPressStart();

static void jdKeyPressed();
static void jdKeyLongPressStart();
static void jd2KeyPressed();
static void jd2KeyLongPressStart();
static void jd2KeyLongPressStop();
static void jd2KeyELongPressStart();

static void jbButtonEventHandler(int idx, ButtonState bstate);

static void volWasChanged();
static void waitAudioDone();
static void re_vol_reset();

static void re_jd_reset();

static bool bttfn_connected();
static bool bttfn_send_command(uint8_t cmd, uint8_t p1, uint8_t p2);
static void bttfn_setup();
static void bttfn_loop_quick();

void main_boot()
{
    // Set up LED for signals
    jbLED.begin(SIGNALLIGHT_PIN);

    // Setup LED (top lights)
    // Driven either by low power LED through original LED output
    // or Keyes module through PWM (originally button input)
    pinMode(TOPLIGHT_PIN, OUTPUT);
    whiteLED.begin(WLED_CHANNEL, WLED_FREQ, WLED_RES);
    toplights(0);

    pinMode(BLUELIGHT_PIN, OUTPUT);
    blueLED.begin(BLED_CHANNEL, BLED_FREQ, BLED_RES);
    blueLED.setDC(0);

    // Init Panel LEDs
    pLEDs.begin();

    rem_stat.validFields = 0;
}

void main_setup()
{
    Serial.println("Jukebox version " JB_VERSION " " JB_VERSION_EXTRA);
    
    loadOpMode();
    loadCurStream();

    // Evaluate settings

    playJB = evalBool(settings.playJB);
    useFakePowerSwitch = true;
    
    useNM = evalBool(settings.useNM);
    NM_blue = evalBool(settings.NMB);
    ignoreTT = evalBool(settings.ignTT);

    remIncI = evalBool(settings.mqmpii);

    // Init RotEnc for volume (top-most)
    // Button is used as fake power button and for opmode change
    if(rotEncV.begin(RE_FUNC_SIMPLE, 19)) {
        csf |= CSF_URotEncVol;
        re_vol_reset();
        rotEncV.switch_setTiming(100, 2000);
        rotEncV.attachPress(fpbKeyPressed);
        rotEncV.attachLongPressStart(fpbKeyLongPressStart);
    } else {
        useFakePowerSwitch = false;
    }

    // Init RotEncs as jog dials 
    // Jog dial 1 (letter); switch is play/stop etc.
    if(rotEncJ1.begin(RE_FUNC_SIMPLE, 0)) {
        rotEncJ1.switch_setTiming(100, 2000);
        rotEncJ1.attachPress(jdKeyPressed);
        rotEncJ1.attachLongPressStart(jdKeyLongPressStart);
        // Jog dial 2 (number); switch is next/prev, MF sel., etc
        if(rotEncJ2.begin(RE_FUNC_SIMPLE, 0)) {
            csf |= CSF_URotEncJD;
            rotEncJ2.switch_setTiming(100, 2000, 5000);
            rotEncJ2.attachPress(jd2KeyPressed);
            rotEncJ2.attachLongPressStart(jd2KeyLongPressStart);
            rotEncJ2.attachLongPressStop(jd2KeyLongPressStop);
            rotEncJ2.attachELongPressStart(jd2KeyELongPressStart);
        } else {
            #ifdef JB_DBG_BOOT
            Serial.println("Jog dial 2 not found");
            #endif
        }
    } else {
        #ifdef JB_DBG_BOOT
        Serial.println("Jog dial 1 not found");
        #endif
    }

    // Invoke audio file installer if SD content qualifies
    if(check_allow_CPA()) {
        showWaitSequence();
        if(prepareCopyAudioFiles()) {
            play_file("/_installing.mp3", PA_ALLOWSD, 1.0f);
            waitAudioDone();
        }
        doCopyAudioFiles();
        // We never return here. The ESP is rebooted.
    }

    if(!haveAudioFiles) {
        jbLED.specialSignal(JBSEQ_NOAUDIO);
        while(!jbLED.specialDone()) {
            mydelay(100);
        }
    } else if(showUpdAvail && updateAvailable()) {
        jbLED.specialSignal(JBSEQ_UPDAVAIL);
        while(!jbLED.specialDone()) {
            mydelay(100);
        }
    }

    // Init music player
    switchMusicFolder(musFolderNum, true);
    targetTrack = get_song_idx_0();

    // Init streaming: Check for configured streams
    init_streams();

    // Init target for HA/MQTT
    haveRem = false;
    if(useMQTT &&
            *settings.mqmpt[MQ_PLAY] &&
            *settings.mqmpt[MQ_STOP] &&
            *settings.mqmpt[MQ_NEXT] &&
            *settings.mqmpt[MQ_PREV]) {
        haveRem = true;
        haveGoto = !!*settings.mqmpt[MQ_GOTO];
    } else {
        if(opMode == 2) opMode = 0;
    }
    remTargetTrack = -1;
    remTargetLtr = remTargetNum = -1;
    
    // Initialize BTTF network
    bttfn_setup();
    bttfn_loop();
    
    if(csf & CSF_MQTTPM) {
        if(csf & CSF_MQTTPWROFF) csf |= CSF_OFF;
    } else if(useFakePowerSwitch && evalBool(settings.waitForPower)) {
        csf |= CSF_OFF;
    }

    if(!(csf & CSF_OFF)) {

        switch_opMode(opMode);

        nightmode(!!(csf & CSF_NM));

        play_startup();
        // Leaves pLEDs on, but cleared

        triggerUpdatePLEDs = true;

        re_vol_reset();
        re_jd_reset();

    } else {
      
        jbLED.specialSignal(JBSEQ_MPMODE + opMode);

    }
    
    #ifdef JB_DBG_BOOT
    Serial.println("main_setup() done");
    #endif
}


void main_loop()
{
    unsigned long now = millis();

    // Fake-power button. We power-up on press, and power-off on hold.
    if(useFakePowerSwitch || (csf & (CSF_MQTTPM|CSF_RESTOREFP))) {

        csf &= ~CSF_RESTOREFP;

        if(useFakePowerSwitch) rotEncV.switch_scan();

        if(isFPBKeyPressed) {

            isFPBKeyPressed = false;

            if(csf & CSF_OFF) {
    
                // Power on: 
                csf &= ~CSF_OFF;

                // Delete rotenc changes during power off
                re_vol_reset();
                
                // Restore menu illumination to correct state
                nightmode(!!(csf & CSF_NM));
                
                play_startup();
                // Leaves pLEDs on, but cleared

                panelLEDsLock = false;
                triggerUpdatePLEDs = true;
                
                networkTimeTravel = false;

                newVolume = 0;

                mp_sendStatus();
                
                re_jd_reset();

                lastPwrPress = 0;
                triggerOpAnnounce = 0;

            } else if(!lastPwrPress || (millis() - lastPwrPress > 5000)) {

                jbLED.specialSignal(JBSEQ_MPMODE + opMode);
                triggerOpAnnounce = millisNonZero();

            } else {

                int o = opMode; 

                triggerOpAnnounce = 0;

                switch(opMode) {
                case 0:
                    if(numStreams) opMode = 1;
                    else if(haveRem) opMode = 2;
                    break;
                case 1:
                    if(haveRem) opMode = 2;
                    else opMode = 0;
                    break;
                case 2:
                    opMode = 0;
                    numnomusic = 0;
                    break;
                }

                if(opMode != o) switch_opMode(o);
              
            }

            lastPwrPress = millisNonZero();
                
        } else if(isFPBKeyHeld) {

            isFPBKeyHeld = false;

            if(!(csf & CSF_OFF)) {
        
                // Power off:
                csf |= CSF_OFF;

                csf &= ~CSF_TT;

                allOff();

                mp_stop(true, true);
                st_stop();
                stopAudio();

                flushDelayedSave();

                triggerUpdatePLEDs = false;
    
                doPrepareTT = false;
                doWakeup = false;

                mp_sendStatus();

                triggerOpAnnounce = 0;
                
            } else {
              
                // TODO: Power held while unit is fake-off: Carmode?
            
            }
        }
    }

    if(triggerOpAnnounce && (millis() - triggerOpAnnounce > 1000)) {
        triggerOpAnnounce = 0;
        switch(opMode) {
        case 0:
            if(!mpActive) play_file("/mpmode.mp3", PA_CHECKNM|PA_ALLOWSD);
            break;
        case 1:
            if(!st_active()) play_file("/strmode.mp3", PA_CHECKNM|PA_ALLOWSD);
            break;
        case 2:
            play_file("/remmode.mp3", PA_CHECKNM|PA_ALLOWSD);
            break;
        }
    }
    
    // Scan RotEnc Jog Dial buttons
    if(csf & CSF_URotEncJD) {
        rotEncJ1.switch_scan();
        rotEncJ2.switch_scan();
    }
    
    fade_loop();
    
    // Eval flags set in handle_tcd_notification
    /*
    if(doPrepareTT) {
        if(!(csf & (CSF_OFF|CSF_TT))) {
            prepareTT();
        }
        doPrepareTT = false;
    }
    if(doWakeup) {
        if(!(csf & (CSF_OFF|CSF_TT))) {
            wakeup();
        }
        doWakeup = false;
    }
    */

    // Execute commands from TCD/MQTT
    handleRemoteCommand();
    
    if(csf & CSF_OFF) {

        if(MFSelectMode) {
            if(csf & CSF_URotEncJD) {
                int oldNum = targetFolder, newNum, fc, fstep = 1, found = 0;
                newNum = oldNum + rotEncJ2.pollSimple(false);
                if(newNum != oldNum) {
                    lastMFsel = millisNonZero();
                    
                    if(newNum < oldNum) fstep = -1;

                    if(newNum < 0) newNum = NUM_MUSIC_FOLDERS - 1;
                    else if(newNum > NUM_MUSIC_FOLDERS - 1) newNum = 0;
                
                    for(int i = 0; i < NUM_MUSIC_FOLDERS; i++) {
                        fc = mfstatus[newNum];
                        if(fc == -1 || fc == 1) {
                            found = 1;
                            break;
                        }
                        newNum += fstep;
                        if(newNum < 0) newNum = NUM_MUSIC_FOLDERS - 1;
                        else if(newNum > NUM_MUSIC_FOLDERS - 1) newNum = 0;
                    }

                    if(found) {
                        if(newNum != oldNum) {
                              targetFolder = newNum;
                              illuminateMusicFolder(targetFolder);
                        }
                    } else {
                        // Not a single musicX folder found:
                        MFSelectMode = 0;
                        jbLED.specialSignal(JBSEQ_NOMUSIC);
                        pLEDs.onOff(0);
                        re_jd_reset();
                    }
                } else {
                    if(lastMFsel && (millis() - lastMFsel > 30000)) {
                        MFSelectMode = 0;
                        pLEDs.onOff(0);
                        re_jd_reset();
                        lastMFsel = 0;
                    }
                }
            }
        }
      
    } else {

        // Play volchg sound if nothing else is running ATM
        if(newVolume) {
            newVolume = 0;
            if(!mpActive && !st_active() && checkAudioDone()) {
                play_file("/volchg.mp3", PA_VOLCHG|PA_ALLOWSD|PA_DYNVOL, 1.0f); // not PA_CHECKNM!
            }
        }

        // Time-out for panel lock
        if(panelLEDsLock) {
            if(millis() - panelLEDsLockNow > 15*1000) {
                panelLEDsLock = false;
                triggerUpdatePLEDs = true;
            }
        }

        // Update panel LEDs to currently played: Re-set target to current
        if(triggerUpdatePLEDs && !panelLEDsLock) {
            triggerUpdatePLEDs = false;
            switch(opMode) {
            case 0:
                if(!(csf & CSF_NOMUSIC)) {
                    targetTrack = mp_get_currently_playing();
                    pLEDs.setLEDs(targetTrack / 10, targetTrack % 10);
                } else {
                    targetTrack = -1;
                    pLEDs.setLEDs(-1, -1);
                }
                break;
            case 1:
                if(numStreams) {
                    targetStream = curStream;
                    pLEDs.setLEDs(targetStream / 10, targetStream % 10);
                } else {
                    targetStream = -1;
                    pLEDs.setLEDs(-1, -1);
                }
                break;
            case 2:
                if(remHaveFullStatus && remHaveMusic) {
                    if(rem_stat.validFields & RMPS_CURLN) {
                        remTargetLtr = rem_stat.curLtr;
                        remTargetNum = rem_stat.curNum;
                        if(remTargetLtr < 10 && remTargetNum < 10) {
                            pLEDs.setLEDs(remTargetLtr, remTargetNum);
                        } else {
                            pLEDs.setLEDs(-1, -1);
                        }
                    } else {
                        remTargetTrack = rem_stat.curTrack;       // targetTrack = real number as known to the player
                        int rcs = remTargetTrack - remFirstTrack;
                        remTargetLtr = rcs / 10;                  // targetLtr/Num = our internal numbers 0-9
                        remTargetNum = rcs % 10;
                        if(remTargetLtr < 10 && remTargetNum < 10) {
                            pLEDs.setLEDs(remTargetLtr, remTargetNum);
                        } else {
                            pLEDs.setLEDs(-1, -1);
                        }
                    }
                } else {
                    remTargetTrack = -1;
                    remTargetLtr = remTargetNum = -1;
                    pLEDs.setLEDs(-1, -1);
                }
                break;
            }
        }

        // Handle jog dials: Modify target and set LEDs
        if(csf & CSF_URotEncJD) {
            switch(opMode) {
            case 0:
                if(!(csf & CSF_NOMUSIC)) {
                    int maxLtr = aud_state.maxMusic / 10;
                    int maxNum = aud_state.maxMusic % 10;
                    int oldLtr = targetTrack / 10;
                    int oldNum = targetTrack % 10;
                    int newLtr = oldLtr + rotEncJ1.pollSimple(false);
                    int newNum = oldNum + rotEncJ2.pollSimple(false);
                    if(newLtr < 0) newLtr = 0;
                    if(newNum < 0) newNum = 0;
                    if(newLtr > maxLtr) newLtr = maxLtr;
                    if(newLtr == maxLtr) {
                        if(newNum > maxNum) newNum = maxNum;
                    } else {
                        if(newNum > 9) newNum = 9;
                    }
                    if((newLtr != oldLtr) || (newNum != oldNum)) {
                        pLEDs.setLEDs(newLtr, newNum);
                        targetTrack = (newLtr * 10) + newNum;
                        panelLEDsLock = true;
                        panelLEDsLockNow = millis();
                        triggerUpdatePLEDs = false;
                    }
                }
                break;
            case 1:
                if(numStreams) {
                    int stepd = 1;
                    int maxLtr = maxStream / 10;
                    int maxNum = maxStream % 10;
                    int oldLtr = targetStream / 10;
                    int oldNum = targetStream % 10;
                    int newLtr = oldLtr + rotEncJ1.pollSimple(false);
                    if(newLtr < oldLtr) stepd = -1;
                    int newNum = oldNum + rotEncJ2.pollSimple(false);
                    if(newNum < oldNum) stepd = -1;
                    if(newLtr < 0) newLtr = 0;
                    if(newNum < 0) newNum = 0;
                    if(newLtr > maxLtr) newLtr = maxLtr;
                    if(newLtr == maxLtr) {
                        if(newNum > maxNum) newNum = maxNum;
                    } else {
                        if(newNum > 9) newNum = 9;
                    }
                    if((newLtr != oldLtr) || (newNum != oldNum)) {
                        targetStream = (newLtr * 10) + newNum;
                        while(streamStatus[targetStream] > 0) {
                            targetStream += stepd;
                            if(targetStream > maxStream) targetStream = 0;
                            else if(targetStream < 0) targetStream = maxStream;
                        }
                        pLEDs.setLEDs(targetStream / 10, targetStream % 10);
                        panelLEDsLock = true;
                        panelLEDsLockNow = millis();
                        triggerUpdatePLEDs = false;
                    }
                }
                break;
            case 2:
                if(remHaveFullStatus && remHaveMusic && haveGoto) {
                    int move1 = rotEncJ1.pollSimple(false);
                    int move2 = rotEncJ2.pollSimple(false);
                    if(move1 || move2) {
                        int maxLtr, maxNum;
                        int oldLtr = remTargetLtr;
                        int oldNum = remTargetNum;
                        if(rem_stat.validFields & RMPS_CURLN) {
                            if(rem_stat.validFields & RMPS_MAXLN) {
                                maxLtr = rem_stat.maxLtr;
                                maxNum = rem_stat.maxNum;
                            } else {
                                maxLtr = 9;
                                maxNum = 9;
                            }
                            if(oldLtr < 0 || oldNum < 0) {
                                oldLtr = rem_stat.curLtr;
                                oldNum = rem_stat.curNum;
                            }
                        } else {
                            int maxMus = remLastTrack - remFirstTrack;
                            maxLtr = maxMus / 10;
                            maxNum = maxMus % 10;
                            if(oldLtr < 0 || oldNum < 0) {
                                int rcs = rem_stat.curTrack - remFirstTrack;
                                oldLtr = rcs / 10;
                                oldNum = rcs % 10;
                            }
                        }
                        int newLtr = oldLtr + move1;
                        int newNum = oldNum + move2;
                        if(newLtr < 0) newLtr = 0;
                        if(newNum < 0) newNum = 0;
                        if(newLtr > maxLtr) newLtr = maxLtr;
                        if(newLtr == maxLtr) {
                            if(newNum > maxNum) newNum = maxNum;
                        } else {
                            if(newNum > 9) newNum = 9;
                        }
                        if((newLtr != oldLtr) || (newNum != oldNum)) {
                            if(newLtr < 10 && newNum < 10) {
                                pLEDs.setLEDs(newLtr, newNum);
                            } else {
                                pLEDs.setLEDs(-1, -1);
                            }
                            remTargetTrack = ((newLtr * 10) + newNum) + remFirstTrack;
                            remTargetLtr = newLtr;
                            remTargetNum = newNum;
                            panelLEDsLock = true;
                            panelLEDsLockNow = millis();
                            triggerUpdatePLEDs = false;
                        }
                    }
                }
                break;
            }
        }

        // TT evaluation
        if(!(csf & CSF_TT)) {
            // Check for BTTFN/MQTT-induced TT
            if(networkTimeTravel) {
                networkTimeTravel = false;
                if(!networkAbort && !ignoreTT) {
                    timeTravel(networkTCDTT, networkLead, networkP1);
                }
            }
        }

    }

    now = millis();

    // The time travel sequence
    
    if(csf & CSF_TT) {

        if(csf & CSF_TTP0) {   // Acceleration - runs for ETTO_LEAD ms by default

            if(networkAbort || (now - TTstart > P0duration)) {

                csf &= ~CSF_TTP0;
                csf |= CSF_TTP1;
                TTstart = now;
                
                if(!networkAbort) {
                    TTSeqActive = true;
                    bluelight(1, 1);
                    toplights(0, 1);
                }
            }
        }
        if(csf & CSF_TTP1) {   // Peak/"time tunnel" - ends with pin going LOW or BTTFN/MQTT "REENTRY" (or a long timeout)

            if(((networkTCDTT && (networkReentry || networkAbort))) ||
                (now - TTstart >  P1_maxtimeout)) {

                csf &= ~CSF_TTP1;
                csf |= CSF_TTP2;
                TTstart = now;
            }
                
        }
        if(csf & CSF_TTP2) {   // Reentry - up to us

            if(now - TTstart > 1000) {
              
                csf &= ~(CSF_TT|CSF_TTP2);

                TTSeqActive = false;
                
                bluelight(0, 1);
                toplights(1, 1);
            }
            
        }
    }

    // Follow TCD/Manual night mode
    if(tcdNM != nmOld) {
        if(tcdNM) {
            // NM on: 
            csf |= CSF_NM;
            nightmode(true);
        } else {
            // NM off: 
            csf &= ~CSF_NM;
            nightmode(false);
        }
        nmOld = tcdNM;
    }

    now = millis();

    // If network is interrupted, return to stand-alone
    if(useBTTFN) {
        if( !bttfnDataNotEnabled &&
            ((lastBTTFNpacket && (now - lastBTTFNpacket > 30*1000)) ||
             (!BTTFNBootTO && !lastBTTFNpacket && (now - powerupMillis > 60*1000))) ) {
            if(useNM) tcdNM = false;
            lastBTTFNpacket = 0;
            BTTFNBootTO = true;
        }
    }

    // Save volume 10 seconds after last change
    if(volchgnow && (now - volchgnow > 10000)) {
        volchgnow = 0;
        saveCurVolume();
    } else if(opmchgnow && (now - opmchgnow > 10000)) {
        opmchgnow = 0;
        saveOpMode();
    } else if(strchgnow && (now - strchgnow > 10000)) {
        strchgnow = 0;
        saveCurStream();
    }

    // Alarm from TCD
    // Alarm sound is only played if nothing else is
    if(networkAlarm) {
        networkAlarm = false;
        jbLED.specialSignal(JBSEQ_ALARM);
        if(evalBool(settings.playALsnd) && checkAudioDone()) {
            play_file("/alarm.mp3", PA_ALLOWSD|PA_DYNVOL, 1.0f);
        }
    }
    
}

void flushDelayedSave()
{
    if(volchgnow) {
        volchgnow = 0;
        saveCurVolume();
    }
    if(opmchgnow) {
        opmchgnow = 0;
        saveOpMode();
    }
    if(strchgnow) {
        strchgnow = 0;
        saveCurStream();
    }
}

void update_volume()
{
    int oldVol;
    
    if((!(csf & CSF_OFF)) && (csf & CSF_URotEncVol)) {
        switch(opMode) {
        case 0:
        case 1:
            oldVol = aud_state.curVolume;
            aud_state.curVolume += rotEncV.pollSimple(false);
            if(aud_state.curVolume < 0) aud_state.curVolume = 0;
            else if(aud_state.curVolume > VOL_LEVELS - 1) aud_state.curVolume = VOL_LEVELS - 1;
            if(oldVol != aud_state.curVolume) {
                storeCurVolume();
                volchgnow = millisNonZero();
                newVolume = 1;
                #ifdef JB_DBG_GEN
                Serial.printf("Heap %d\n", ESP.getFreeHeap());
                #endif
            }
            break;
        case 2:
            rem_vol_chg(rotEncV.pollSimple(false));
            break;
        }
    }
}

/*
 * Streaming
 * 
 */

static void init_streams()
{    
    numStreams = maxStream = 0;
    for(int i = 0; i < NUM_STREAMS; i++) streamStatus[i] = 1;
    if(WiFi.status() == WL_CONNECTED) {
        for(int i = 0; i < NUM_STREAMS; i++) {
            if(*settings.streams[i]) {
                streamStatus[i] = 0;
                numStreams++;
            }
        }
    }
    if(!numStreams) {
        if(opMode == 1) opMode = 0;
    } else {
        for(int i = NUM_STREAMS-1; i >= 0; i--) {
            if(!streamStatus[i]) {
                maxStream = i;
                break;
            }
        }

        if(curStream > maxStream) curStream = 0;

        while(streamStatus[curStream]) {
            curStream++;
            if(curStream >= NUM_STREAMS) curStream = 0;
        }
    }
    targetStream = curStream;
}

static bool play_current_stream()
{
    bool ret = true;

    int res = play_stream(settings.streams[curStream], PA_STREAM|PA_CHECKNM, 1.0f);

    // Don't store memory error as status
    if(res != STRS_NOMEM) streamStatus[curStream] = res;
    
    if(res) {
        jbLED.specialSignal(JBSEQ_NOMUSIC);
        play_file("/strfail.mp3", PA_CHECKNM|PA_ALLOWSD);
        ret = false;
    }
    triggerUpdatePLEDs = true;
    
    storeCurStream();
    strchgnow = millisNonZero();

    return ret;
}

static void find_next_stream()
{
    curStream++;
    if(curStream >= NUM_STREAMS) curStream = 0;
    while(streamStatus[curStream] > 0) {
        curStream++;
        if(curStream >= NUM_STREAMS) curStream = 0;
    }
}

static void find_prev_stream()
{                
    curStream--;
    if(curStream < 0) curStream = NUM_STREAMS - 1;
    while(streamStatus[curStream] > 0) {
        curStream--;
        if(curStream < 0) curStream = NUM_STREAMS - 1;
    }
}

// 0-9, 0-9
static bool goto_stream(int ltr, int num, bool doPlay)
{
    int tStr = (ltr * 10) + num;
    
    if(tStr >= NUM_STREAMS) return false;
    
    if(streamStatus[tStr] <= 0) {
        curStream = tStr;
        if(doPlay) play_current_stream();
        return true;
    }
    return false;
}

/*
 * HA/MQTT Remote mode
 */

static void rem_is_off()
{
    jbLED.specialSignal(JBSEQ_NOMUSIC);
    play_file("/remoff.mp3", PA_CHECKNM|PA_ALLOWSD);
}

static void rem_play()
{
    mqttPublishQuick(settings.mqmpt[MQ_PLAY], settings.mqmp[MQ_PLAY]);
    remActiveInt = true;
}

static void rem_stop()
{
    mqttPublishQuick(settings.mqmpt[MQ_STOP], settings.mqmp[MQ_STOP]);
    remActiveInt = false;
}

static void rem_next()
{
    mqttPublishQuick(settings.mqmpt[MQ_NEXT], settings.mqmp[MQ_NEXT]);
}

static void rem_prev()
{
    mqttPublishQuick(settings.mqmpt[MQ_PREV], settings.mqmp[MQ_PREV]);
}

// input: 0-9, 0-9
static bool rem_goto(int ltr, int num)
{
    if(!haveGoto)
        return false;

    String top, pl;
    char tltr[4]  = "A";
    char tnum[4]  = "10";
    char tnum2[4] = "10";
    char tnumber[8];
    char tnumber3[8];
    char tnumber4[8];

    int songNum = ((ltr * 10) + num) + remFirstTrack;
    
    sprintf(tnumber, "%d", songNum);
    sprintf(tnumber3, "%03d", songNum);
    sprintf(tnumber4, "%04d", songNum);
    
    sprintf(tnum, "%d", num + 1);
    sprintf(tnum2, "%02d", num + 1);

    *tltr = remIncI ? (ltr + 'A') : jbltrs[ltr];
        
    top.reserve(strlen(settings.mqmpt[MQ_GOTO]) + 32);
    pl.reserve(strlen(settings.mqmp[MQ_GOTO]) + 32);

    top = settings.mqmpt[MQ_GOTO];
    pl = settings.mqmp[MQ_GOTO];

    for(String *i : { &top, &pl }) {
        i->replace("{NNNN}", tnumber4);
        i->replace("{NNN}", tnumber3);
        i->replace("{N}", tnumber);
        i->replace("{JL}", tltr);
        i->replace("{JNN}", tnum2);
        i->replace("{JN}", tnum);
    }

    mqttPublishQuick(top.c_str(), pl.c_str());

    // Avoid flash of old current on LED update
    rem_stat.curTrack = songNum;
    rem_stat.curLtr = ltr;
    rem_stat.curNum = num;

    return true;
}

static void rem_shuffle(bool shuffle)
{
    if(shuffle) {
        mqttPublishQuick(settings.mqmpt[MQ_SHU1], settings.mqmp[MQ_SHU1]);
    } else {
        mqttPublishQuick(settings.mqmpt[MQ_SHU0], settings.mqmp[MQ_SHU0]);
    }
}

void rem_vol_set(int v)
{
    if(*settings.mqmpt[MQ_VOLU] && (rem_stat.validFields & RMPS_VOL)) {
      
        String vst, vsp;
        char buf[8];
        int t;
        vst.reserve(strlen(settings.mqmpt[MQ_VOLU]) + 8);
        vst = settings.mqmpt[MQ_VOLU];
        vsp.reserve(strlen(settings.mqmp[MQ_VOLU]) + 8);
        vsp = settings.mqmp[MQ_VOLU];

        sprintf(buf, "%d", v);
        vst.replace("{P}", buf);
        vsp.replace("{P}", buf);
        t = v / 100;
        buf[0] = t + '0';
        buf[1] = '.';
        v -= (t * 100);
        t = v / 10;
        v -= (t * 10);
        buf[2] = t + '0';
        buf[3] = v + '0';
        buf[4] = 0;
        vst.replace("{F}", buf);
        vsp.replace("{F}", buf);
        mqttPublishQuick(vst.c_str(), vsp.c_str());
    }
}

void rem_vol_chg(int chg)
{
    if(!chg) return;

    if(!remHaveStatus || !rem_stat.off) {

        if(*settings.mqmpt[MQ_VOLD]) {
            
            if(chg < 0) {
                mqttPublishQuick(settings.mqmpt[MQ_VOLD], settings.mqmp[MQ_VOLD]);
            } else {
                mqttPublishQuick(settings.mqmpt[MQ_VOLU], settings.mqmp[MQ_VOLU]);
            }
            
        } else if(*settings.mqmpt[MQ_VOLU] && (rem_stat.validFields & RMPS_VOL)) {
            
            int v = rem_stat.volumePerc + (chg * 5);
            if(v < 0) v = 0;
            else if(v > 100) v = 100;
            
            rem_vol_set(v);
            
        }
    }
}

static void rem_request_status()
{
    mqttPublishQuick(settings.mqmpt[MQ_REQS], settings.mqmp[MQ_REQS]);
}

/*
 * Switch operating mode
 */

static void switch_opMode(int prevOpMode)
{
    bool active = mp_stop(true) | st_stop(true);
    
    stopAudio();

    storeOpMode();
    opmchgnow = millisNonZero();

    if(prevOpMode == 2) {
        re_vol_reset();
    } else if(opMode == 2) {
        re_vol_reset();
        rem_request_status();
    }

    jbLED.specialSignal(JBSEQ_MPMODE + opMode);

    if(active) {
        switch(opMode) {
        case 0:
            mp_play(true);
            break;
        case 1:
            mp_sendStatus();  // Here to not disturb stream by sending later
            while(!jbLED.specialDone()) {
               mydelay(100);
            }
            while(streamStatus[curStream] > 0) {
                curStream++;
                if(curStream >= NUM_STREAMS) curStream = 0;
            }
            play_current_stream();
            break;
        case 2:
            break;
        }
    }

    mp_sendStatus();
    
    panelLEDsLock = false;
    triggerUpdatePLEDs = true;
}

/*
 * Time travel, sort of
 */

static void timeTravel(bool TCDtriggered, uint16_t P0Dur, uint16_t P1Dur)
{    
    if(csf & CSF_TT)
        return;

    flushDelayedSave();

    csf |= (CSF_TT|CSF_TTP0);
    csf &= ~(CSF_TTP1|CSF_TTP2);
    TTstart = millis();

    P0duration = P0Dur;

    if(!P1Dur) {
        P1Dur = P1_DUR_TCD;
    }
    P1_maxtimeout = P1Dur + 3000;
}

/*
 * Switch music folder
 * ATTN: This can be called anytime (on, off)
 */
 
bool switchMusicFolder(unsigned int nmf, bool isSetup)
{
    bool wasSActive = false;
    bool waitShown = false;
    bool doSuccessSignal = true;

    if(nmf > NUM_MUSIC_FOLDERS - 1)
        return false;
    
    if((musFolderNum != nmf) || isSetup) {
        
        csf |= CSF_BUSY;
        
        if(!isSetup) {
            musFolderNum = nmf;
            // Initializing the MP can take a while;
            // need to stop all audio before calling
            // mp_init()
            mp_stop(true, true);
            wasSActive = st_stop(true);
            stopAudio();
        }
        if(haveSD) {
            if(mp_checkForFolder(musFolderNum) == -1) {
                doSuccessSignal = false;
                flushDelayedSave();
                showWaitSequence();
                waitShown = true;
                play_file("/renaming.mp3", PA_INTRMUS|PA_ALLOWSD);
                waitAudioDone();
                if(csf & CSF_OFF) {
                    pLEDs.onOff(1);
                }
            }
        }
        if(!isSetup) {
            saveMusFoldNum();
        }
        
        mp_init(isSetup);
        
        if(waitShown) {
            endWaitSequence();
            if(csf & CSF_OFF) {
                pLEDs.onOff(0);
            }
        }

        if(csf & CSF_NOMUSIC) {
            if(!isSetup) {
                doSuccessSignal = false;
            }
            jbLED.specialSignal(JBSEQ_NOMUSIC);
        }

        numnomusic = 0;

        if(wasSActive) {
            play_current_stream();
        }
        
        re_vol_reset();
        re_jd_reset();

        triggerUpdatePLEDs = true;
        panelLEDsLock = false;

        csf &= ~CSF_BUSY;
    }

    return doSuccessSignal;
}

/*
 * Say IP address
 * ATTN: This can be called qnytime (on, off)
 */
static void say_ip_address()
{
    uint8_t a, b, c, d;
    char ipbuf[16];
    char numfname[] = "/x.mp3";

    csf |= CSF_BUSY;
    
    bool wasActiveM = mp_stop(true, true);
    bool wasActiveS = st_stop(true);
    
    stopAudio();
    flushDelayedSave();
    
    wifi_getIP(a, b, c, d);
    sprintf(ipbuf, "%d.%d.%d.%d", a, b, c, d);
    numfname[1] = ipbuf[0];
    
    play_file(numfname, PA_INTRMUS|PA_ALLOWSD);
    for(int i = 1; i < strlen(ipbuf); i++) {
        if(ipbuf[i] == '.') {
            append_file("/dot.mp3", PA_INTRMUS|PA_ALLOWSD);
        } else {
            numfname[1] = ipbuf[i];
            append_file(numfname, PA_INTRMUS|PA_ALLOWSD);
        }
        while(append_pending()) {
            mydelay(10);
        }
    }
    waitAudioDone();

    csf &= ~CSF_BUSY;
    
    if(wasActiveM) mp_play();
    else if(wasActiveS) play_current_stream();
}

/*
 * Reset static IP address to DHCP; reset AP password, and reboot if anything changed
 * ATTN: This can be called anytime (on, off)
 */
static void reset_IP_and_WIFIAPPW()
{
    bool didST;
    
    flushDelayedSave();

    didST = deleteIpSettings();
    
    if(settings.appw[0]) {
        settings.appw[0] = 0;
        mp_stop(true);
        st_stop(true);
        stopAudio();
        write_settings();
        didST = true;
    }

    jbLED.specialSignal(JBSEQ_OK);
    while(!jbLED.specialDone()) {
        mydelay(100);
    }

    if(didST) {
        prepareReboot();                        
        delay(1000);
        esp_restart();
    }
}

static void reconnect_WiFi()
{
    bool wasActiveM = false, waitShown = false;

    // Streams can't ever be active if WiFi is to be re-enabled/re-connected

    if(wifiOnWillBlock()) {
        wasActiveM = mp_stop(true);
        stopAudio();
        showWaitSequence();
        waitShown = true;
        flushDelayedSave();
    }
    
    // Enable WiFi / even if in AP mode / with CP
    wifiOn(0, true, false);
    
    if(waitShown) endWaitSequence();
    
    jbLED.specialSignal(JBSEQ_OK);

    init_streams();

    // This should not happen. If we were connected before,
    // we really ought to be now...
    if(opMode == 1 && (WiFi.status() != WL_CONNECTED)) {
        opMode = 0;
        switch_opMode(1);
    }
    
    // Restart mp if active before
    if(wasActiveM) mp_play();
}


/*
 * Remote command handling
 * 
 */

static uint8_t read2digs(uint8_t idx)
{
    return ((inputBuffer[idx] - '0') * 10) + (inputBuffer[idx+1] - '0');
}

void doKeySound(int key)
{   
    //play_key(key);
}

void doStopKeySound()
{
    //stop_key();
}

static void doMPPlay()
{ 
    switch(opMode) {
    case 0:
        if(!(csf & CSF_NOMUSIC)) {
            mp_play();
        } else {
            play_nomusic();
        }
        break;
    case 1:
        if(streamStatus[curStream] > 0) {
            find_next_stream();
        }
        play_current_stream();
        break;
    case 2:
        if(!remHaveStatus || !rem_stat.off) {
            rem_play();
        } else {
            rem_is_off();
        }
        break;
    }
}

static void doMPStop()
{
    switch(opMode) {
    case 0:
        mp_stop();
        break;
    case 1:
        st_stop();
        break;
    case 2:
        if(!remHaveStatus || !rem_stat.off) {
            rem_stop();
        } else {
            rem_is_off();
        }
        break;
    }
}

static void doMPNext()
{
    int b;
    
    switch(opMode) {
    case 0:
        if(!(csf & CSF_NOMUSIC)) {
            mp_next(mpActive);
        } else {
            play_nomusic();
        }
        break;
    case 1:
        b = curStream;
        find_next_stream();
        if((b != curStream) || !st_active()) {
            play_current_stream();
        }
        break;
    case 2:
        if(!remHaveStatus || !rem_stat.off) {
            rem_next();
        } else {
            rem_is_off();
        }
        break;
    }
}

static void doMPPrev()
{
    int b;
    
    switch(opMode) {
    case 0:
        if(!(csf & CSF_NOMUSIC)) {
            mp_prev(mpActive);
        } else {
            play_nomusic();
        }
        break;
    case 1:
        b = curStream;
        find_prev_stream();
        if((b != curStream) || !st_active()) {
            play_current_stream();
        }
        break;
    case 2:
        if(!remHaveStatus || !rem_stat.off) {
            rem_prev();
        } else {
            rem_is_off();
        }
        break;
    }
}

static void doKey1()
{
    doKeySound(1);
}
static void doKey2()
{
    doMPPrev();
}
static void doKey3()
{
    doKeySound(3);
}
static void doKey4()
{
    doKeySound(4);
}
static void doKey5()
{
    switch(opMode) {
    case 0:
        if(!(csf & CSF_NOMUSIC)) {
            if(mpActive) {
                doMPStop();
            } else {
                doMPPlay();
            }
        }
        break;
    case 1:
        if(st_active()) {
            doMPStop();
        } else {
            doMPPlay();
        }
        break;
    case 2:
        if(remHaveStatus) {
            if(rem_stat.off) {
                rem_is_off();
            } else if(rem_stat.playing) {
                doMPStop();
            } else {
                doMPPlay();
            }
        } else {
            if(remActiveInt) {
                doMPStop();
            } else {
                doMPPlay();
            }
        }
        break;
    }
}
static void doKey6()
{
    doKeySound(6);
}
static void doKey7()
{
    doKeySound(7);
}
static void doKey8()
{
    doMPNext();
}
static void doKey9()
{
    doKeySound(9);
}

static void handleRemoteCommand()
{
    uint32_t command = commandQueue[oCmdIdx];
    int      doInpReaction = 0;
    bool     injected = false;

    if(!command)
        return;

    commandQueue[oCmdIdx] = 0;
    oCmdIdx++;
    oCmdIdx &= 0x0f;

    if(command & 0x80000000) {
        injected = true;
        command &= ~0x80000000;
        // Allow user to use our TCD code
        if(command >= 5000 && command <= 5999) {
            command -= 5000;
        } else if(command >= 5000000 && command <= 5999999) {
            command -= 5000000;
        }
        if(!command) return;
    }

    // Some translation
    if(command < 10) {

        if(!(csf & CSF_OFF)) {

            // <10 are translated to direct-key-actions.
            switch(command) {
            case 1:
                doKey1();
                break;
            case 2:
                doKey2();
                break;
            case 3:
                doKey3();
                break;
            case 4:
                doKey4();
                break;
            case 5:
                doKey5();
                break;
            case 6:
                doKey6();
                break;
            case 7:
                doKey7();
                break;
            case 8:
                doKey8();
                break;
            case 9:
                doKey9();
                break;
            }

        }
        return;
        
    }
     
    if(command < 100) {
  
        sprintf(inputBuffer, "%02d", command);
        
    } else if(command < 1000) {
      
        sprintf(inputBuffer, "%03d", command);

    } else if(command < 10000) {
      
        sprintf(inputBuffer, "%04d", command);
        
    } else if(command < 100000) {

        sprintf(inputBuffer, "%05d", command);
        
    } else {
      
        sprintf(inputBuffer, "%06d", command);

    }

    doInpReaction = execute(injected);

}

static int execute(bool injected)
{
    int  doInpReaction = 0;
    uint16_t temp;
    unsigned long now = millisNonZero();

    switch(strlen(inputBuffer)) {

    case 1:
        // Single digit handled as single key actions above
        doInpReaction = -1;
        break;

    case 2:
        temp = atoi(inputBuffer);
        switch(temp) {
        case 20:                                      // 20: Switch to Music Player mode
            if(!(csf & CSF_OFF)) {
                if(opMode != 0) {
                    int o = opMode;
                    opMode = 0;
                    switch_opMode(o);
                }
            }
            break;
        case 21:                                      // 21: Switch to streaming mode
            if(!(csf & CSF_OFF)) {
                if((opMode != 1) && numStreams) {
                    int o = opMode;
                    opMode = 1;
                    switch_opMode(o);
                }
            }
            break;
        case 22:                                      // 22: Switch to remote mode
            if(!(csf & CSF_OFF)) {
                if((opMode != 2) && haveRem) {
                    int o = opMode;
                    opMode = 2;
                    switch_opMode(o);
                }
            }
            break;
        case 90:                                      // 90 say IP address
            if(csf & CSF_OFF) {
                say_ip_address();
            }
            break;
        default:                                      // 50 - 69 Set music folder number (50-59=A-K, 60-69=1-10)
            if(haveSD && temp >= 50 && temp <= 69) {
                if(switchMusicFolder(temp - 50)) {
                    doInpReaction = 1;
                }
            } else {
                doInpReaction = -1;
            }
            break;
        }
        break;

    case 3:
        temp = atoi(inputBuffer);
        if(temp >= 300 && temp < (300 + VOL_LEVELS)) {

            if(!(csf & CSF_OFF)) {
              
                temp -= 300;                              // 300-320: Set volume level
    
                switch(opMode) {
                case 0:
                case 1:
                    if(aud_state.curVolume != temp) {
                        aud_state.curVolume = temp;
                        volWasChanged();
                    }
                    break;
                case 2:
                    // Handled separately since we use 0-20, remote expects 0-100
                    break;
                }
    
                doInpReaction = 1;

            }
            
        } else if(temp >= 501 && temp <= 509) {       // 501-509 play keyX [disabled]

            if(!(csf & CSF_OFF) && !mpActive && !st_active()) {
                doKeySound(temp - 500);
            }
            doInpReaction = 1;
            
        } else {

            if(!(csf & CSF_OFF)) {
            
                switch(temp) {
                case 222:                             // 222/555 Disable/enable shuffle
                case 555:
                    switch(opMode) {
                    case 0:
                        mp_makeShuffle((temp == 555));
                        jbLED.specialSignal(aud_state.mpShuffle ? JBSEQ_SHUFFLE_ON : JBSEQ_SHUFFLE_OFF);
                        play_file(aud_state.mpShuffle ? "/shuffleon.mp3" : "/shuffleoff.mp3", PA_CHECKNM|PA_ALLOWSD);
                        if(!(csf & CSF_NOMUSIC)) {
                            doInpReaction = 1;
                        } else doInpReaction = -1;
                        break;
                    case 1:
                        break;
                    case 2:
                        if(*settings.mqmpt[MQ_SHU1] && *settings.mqmpt[MQ_SHU0]) {
                            if(!remHaveStatus || !rem_stat.off) {
                                remShuffleInt = (temp == 555);
                                rem_shuffle(remShuffleInt);
                                if(rem_stat.validFields & RMPS_SHUFFLE) {
                                    remAllowShuffleSignal = true;
                                } else {
                                    jbLED.specialSignal(remShuffleInt ? JBSEQ_SHUFFLE_ON : JBSEQ_SHUFFLE_OFF);
                                }
                            } else {
                                rem_is_off();
                            }
                        } else doInpReaction = -1;
                        break;
                    }
                    
                    break;
                case 888:                             // 888 go to track/stream A 1
                    doInpReaction = -1;
                    switch(opMode) {
                    case 0:
                        if(!(csf & CSF_NOMUSIC)) {
                            mp_gotonum(0, true);
                            doInpReaction = 1;
                        } else {
                            play_nomusic();
                        }
                        break;
                    case 1:
                        if(goto_stream(0, 0, true)) {
                            doInpReaction = 1;
                        }
                        break;
                    case 2:
                        if(!remHaveStatus || !rem_stat.off) {
                            if(rem_goto(0, 0)) {
                                doInpReaction = 1;
                            }
                        } else {
                            rem_is_off();
                        }
                        break;
                    }
                    break;
                
                default:                              
                    doInpReaction = -1;
                }

            }
        
        }
        break;

    case 4:                                           // 1000 - 9999 MQTT commands
        if(!injected) {

            if(!(csf & CSF_OFF)) {
              
                temp = atoi(inputBuffer) - 1000;

                if(temp >= 8000 && temp <= 8100) {

                    if(opMode == 2) rem_vol_set(temp - 8000);

                } else {

                    switch(temp) {
                    case 2: 
                        doMPPlay();
                        break;
                    case 3:
                        doMPStop();
                        break;
                    case 4:
                        doMPNext();
                        break;
                    case 5:
                        doMPPrev();
                        break;
                    case 9:
                        doStopKeySound();
                        break;
                    case 18:
                    case 19:
                        if(!useNM || !bttfn_connected()) {
                            tcdNM = (temp == 19);
                        }
                        break;
                    case 8101:
                        if(opMode == 2) rem_vol_chg(1);
                        break;
                    case 8102:
                        if(opMode == 2) rem_vol_chg(-1);
                        break;
                    }
                }
            }
        }

        break;
        
    case 5:
        switch(atoi(inputBuffer)) {
        case 53281:
            showUpdAvail = !showUpdAvail;
            saveUpdAvail();
            doInpReaction = 1;
            break;
        case 64738:
            if(!injected) {
                prepareReboot();
                delay(1000);
                esp_restart();
            }
            // fall through
        default:
            doInpReaction = -1;
        }
        break;

    case 6:
        if(!strncmp(inputBuffer, "888", 3)) {        // 888xxx go to song/stream xxx
            if(!(csf & CSF_OFF)) {
                int tnum = ((inputBuffer[3] - '0') * 100) + read2digs(4);
                if(tnum >= 0 && tnum <= 99) {
                    int ltr = tnum / 10;
                    int num = tnum % 10;
                    switch(opMode) {
                    case 0:
                        if(!(csf & CSF_NOMUSIC)) {
                            mp_gotonum(tnum, true);
                            doInpReaction = 1;
                        } else {
                            play_nomusic();
                        }
                        doInpReaction = 1;
                        break;
                    case 1:
                        if(goto_stream(ltr, num, true)) {
                            doInpReaction = 1;
                        }
                        break;
                    case 2:
                        if(!remHaveStatus || !rem_stat.off) {
                            if(rem_goto(ltr, num)) {
                                doInpReaction = 1;
                            }
                        } else {
                            rem_is_off();
                        }
                        break;
                    }
                }
            } else doInpReaction = -1;
        } else if(!strncmp(inputBuffer, "88", 2)) {  // 88xxyy go to song/stream: xx = letter (01-10), yy = number (01-10)
            if(!(csf & CSF_OFF)) {
                int ltr = read2digs(2);
                int num = read2digs(4);
                doInpReaction = -1;
                if(ltr > 0 && ltr <= 10 && num > 0 && num <= 10) {
                    ltr--; num--;
                    switch(opMode) {
                    case 0:
                        if(!(csf & CSF_NOMUSIC)) {
                            mp_gotonum((ltr * 10) + num, true);
                            doInpReaction = 1;
                        } else {
                            play_nomusic();
                        }
                        break;
                    case 1:
                        if(goto_stream(ltr, num, true)) {
                            doInpReaction = 1;
                        }
                        break;
                    case 2:
                        if(!remHaveStatus || !rem_stat.off) {
                            if(rem_goto(ltr, num)) {
                                doInpReaction = 1;
                            }
                        } else {
                            rem_is_off();
                        }
                        break;
                    }
                }
            } else doInpReaction = -1;
        } else if(!strcmp(inputBuffer, "123456") && !injected) {
            reset_IP_and_WIFIAPPW();                  // 123456 deletes IP settings and clears WiFi PW
            doInpReaction = 1;                        // (and reboots and anything actually changed)
        } else {
            doInpReaction = -1;
        }
        break;

    default:
        doInpReaction = -1;
    }

    return doInpReaction;
}

/*
 * Helpers
 */

/*
void showMPRProgress(int perc)
{
    if(perc >= 100) perc = 99;
    else if(perc < 0) perc = 0;

    pLEDs.setLEDs(-1, perc / 10, 1);
}
*/

void showWaitSequence()
{
    jbLED.specialSignal(JBSEQ_WAIT);
}

void endWaitSequence()
{
    if(jbLED.curSignal() == JBSEQ_WAIT) {
        jbLED.specialSignal(0);
    }
}

void showCopyError()
{
    jbLED.specialSignal(JBSEQ_ERROR);
}

static void play_nomusic()
{
    jbLED.specialSignal(JBSEQ_NOMUSIC);
    
    play_file(numnomusic ? "/nomusic.mp3" : "/nomusic2.mp3", PA_INTRMUS|PA_CHECKNM|PA_ALLOWSD);
    numnomusic = 1;
}

static void play_startup()
{   
    play_file("/startup.mp3", PA_INTRMUS|PA_ALLOWSD, 1.0f);

    pLEDs.setLEDs(-1, -1);
    pLEDs.onOff(1);

    jbLED.specialSignal(JBSEQ_MPMODE + opMode);
    
    for(int i = 0; i < 10; i++) {
        pLEDs.setLEDs(i, 9-i, 1);
        mydelay(20);
    }
    for(int i = 1; i < 10; i++) {
        pLEDs.setLEDs(9-i, i, 1);
        mydelay(30);
    }

    pLEDs.setLEDs(-1, -1, 1);

    mydelay(300);

    for(int i = 0; i < 3; i++) {
        illuminateMusicFolder(musFolderNum, 1);
        mydelay(70);
        pLEDs.setLEDs(-1, -1, 1);
        mydelay(70);
    }
}

static void illuminateMusicFolder(int t, int direct)
{
    if(t < 10) {
        pLEDs.setLEDs(t, -1, direct);
    } else if(t < 20) {
        pLEDs.setLEDs(-1, t - 10, direct);
    } else {
        pLEDs.setLEDs(-1, -1, direct);
    }
}

static void bluelight(int onOff, int useFader)
{
    if(onOff) {
        if(((csf & CSF_NM) && NM_blue) || TTSeqActive) {
            if(!useFader) {
                digitalWrite(BLUELIGHT_PIN, HIGH);
                blueLED.setDC(blueMaxBri);
                blueFade = 0;
            } else {
                blueFade = millisNonZero();
                bfDelay = 10;
                bfStep = 4;
            }
        }
    } else {
        if((!((csf & CSF_NM) && NM_blue)) && !TTSeqActive) {
            if(!useFader) {
                digitalWrite(BLUELIGHT_PIN, LOW);
                blueLED.setDC(0);
                blueFade = 0;
            } else {
                blueFade = millisNonZero();
                bfDelay = 10;
                bfStep = -4;
            }
        }
    }
}

static void toplights(int onOff, int useFader)
{
    if((csf & CSF_NM) || TTSeqActive) onOff = 0;
    digitalWrite(TOPLIGHT_PIN, onOff ? HIGH : LOW);
    if(!useFader) {
        whiteLED.setDC(onOff ? whiteMaxBri : 0);
        whiteFade = 0;
    } else {
        whiteFade = millisNonZero();
        wfDelay = 10;
        wfStep = onOff ? 4 : -4;
    }
}

void allOff()
{
    jbLED.specialSignal(0);
    pLEDs.onOff(0);
    toplights(0, 0);
    digitalWrite(BLUELIGHT_PIN, LOW);
    blueLED.setDC(0);
    blueFade = 0;
}

static void nightmode(bool onOff)
{
    // pLEDs are not switched off in NM, can't use the unit without them.
    // jbLED is not touched, only used for signals

    if(onOff) {
        // Nightmode on
        blueMaxBri = 128;
        if(csf & CSF_OFF) return;
        toplights(0, 1);
        bluelight(1, 1);
    } else {
        // Nightmode off
        blueMaxBri = 255;
        if(csf & CSF_OFF) return;
        toplights(1, 1);
        bluelight(0, 1);
    }
}

void prepareReboot()
{
    csf |= CSF_BUSY;
    st_stop(true);
    mp_stop(true);
    stopAudio();
    allOff();
    flushDelayedSave();
    delay(1000);
    unmount_fs();
    delay(100);
}

/*
 * Callbacks for fake power control
 */
void mqttFakePowerControl(bool isPwrMaster)
{
    if((!!(csf & CSF_MQTTPM)) != isPwrMaster) {
        if(!isPwrMaster) {
            csf &= ~CSF_MQTTPM;
            if(!useFakePowerSwitch || !evalBool(settings.waitForPower)) {
                if(csf & CSF_OFF) {
                    csf |= CSF_RESTOREFP;
                    isFPBKeyPressed = isFPBKeyChange = true;
                    isFPBKeyHeld = false;
                }
            }
        } else {
            csf |= CSF_MQTTPM;
            if((!!(csf & CSF_OFF)) != (!!(csf & CSF_MQTTPWROFF))) {
                if(csf & CSF_OFF) {
                    isFPBKeyPressed = isFPBKeyChange = true;
                    isFPBKeyHeld = false;
                } else {
                    isFPBKeyHeld = isFPBKeyChange = true;
                    isFPBKeyPressed = false;
                }
            }
        }          
    }
}

void mqttFakePowerOn()
{
    csf &= ~CSF_MQTTPWROFF;
    if(!(csf & CSF_MQTTPM)) return;
    if(csf & CSF_OFF) {
        isFPBKeyPressed = true;
        isFPBKeyHeld = false;
        isFPBKeyChange = true;
    }
}

void mqttFakePowerOff()
{
    csf |= CSF_MQTTPWROFF;
    if(!(csf & CSF_MQTTPM)) return;
    if(!(csf & CSF_OFF)) {
        isFPBKeyPressed = false;
        isFPBKeyHeld = true;
        isFPBKeyChange = true;
    }
}

static void fpbKeyPressed()
{
    if(csf & CSF_OFF) {
        if(csf & CSF_MQTTPM) return;
    }
    isFPBKeyPressed = true;
    isFPBKeyHeld = false;
    isFPBKeyChange = true;
}

static void fpbKeyLongPressStart()
{
    if(!(csf & CSF_OFF)) {
        if(csf & CSF_MQTTPM) return;
    }
    isFPBKeyPressed = false;
    isFPBKeyHeld = true;
    isFPBKeyChange = true;
}

static void jdKeyPressed()
{
    jbButtonEventHandler(0, TCBS_PRESSED);
}

static void jdKeyLongPressStart()
{
    jbButtonEventHandler(0, TCBS_LONGPRESS);
}

static void jd2KeyPressed()
{
    jbButtonEventHandler(1, TCBS_PRESSED);
}

static void jd2KeyLongPressStart()
{
    jbButtonEventHandler(1, TCBS_LONGPRESS);
}

static void jd2KeyLongPressStop()
{
    jbButtonEventHandler(1, TCBS_LONGPRESSEND);
}

static void jd2KeyELongPressStart()
{
    jbButtonEventHandler(1, TCBS_ELONGPRESS);
}

static void jbButtonEventHandler(int idx, ButtonState bstate)
{
    switch(bstate) {
    case TCBS_PRESSED:
        triggerOpAnnounce = 0;
        switch(idx) {
        case 0:   // The button on Jog Dial 1
            if(csf & CSF_OFF) {
                say_ip_address();
            } else {
                panelLEDsLock = false;
                switch(opMode) {
                case 0:
                    if(!(csf & CSF_NOMUSIC)) {
                        if(mpActive && (targetTrack == mp_get_currently_playing())) {
                            mp_stop(false);
                        } else if(targetTrack != mp_get_currently_playing()) {
                            mp_gotonum(targetTrack, true);
                        } else {
                            mp_play();
                        }
                    } else {
                        play_nomusic();
                    }
                    break;
                case 1:
                    if(st_active() && (targetStream == curStream)) {
                        st_stop(false);
                    } else {
                        if(targetStream != curStream) curStream = targetStream;
                        if(streamStatus[curStream] <= 0) {
                            play_current_stream();
                        }
                    }
                    break;
                case 2:
                    if(remHaveFullStatus && remHaveMusic) {
                        bool curIsTarget = false;
                        if(rem_stat.validFields & RMPS_CURLN) {
                            curIsTarget = ((remTargetLtr == rem_stat.curLtr) && (remTargetNum == rem_stat.curNum));
                        } else {
                            curIsTarget = (remTargetTrack == rem_stat.curTrack);
                        }
                        if(!rem_stat.off) {
                            if(rem_stat.playing && curIsTarget) {
                                rem_stop();
                            } else if(!curIsTarget) {
                                rem_goto(remTargetLtr, remTargetNum);
                            } else {
                                rem_play();
                            }
                        } else {
                            rem_is_off();
                        }
                    } else if(remHaveMusic) {
                        remActiveInt ? rem_stop() : rem_play();   // FIXME untested
                    }
                    break;
                }
            }
            break;
        case 1:   // The button on Jog Dial 2
            if(csf & CSF_OFF) {
                if(MFSelectMode) {
                    MFSelectMode = 0;
                    switchMusicFolder(targetFolder, false);
                    pLEDs.onOff(0);
                    re_jd_reset();
                } else {
                    MFSelectMode = 1;
                    targetFolder = musFolderNum;
                    illuminateMusicFolder(targetFolder);
                    pLEDs.onOff(1);
                    re_jd_reset();
                    lastMFsel = millisNonZero();
                }
            } else {
                int b;
                switch(opMode) {
                case 0:
                    if(!(csf & CSF_NOMUSIC)) {
                        panelLEDsLock = false;
                        if(mpActive && (targetTrack == mp_get_currently_playing())) {
                            mp_next(true);
                        } else if(targetTrack != mp_get_currently_playing()) {
                            mp_gotonum(targetTrack, true);
                        } else {
                            mp_next(true);
                        }
                    } else {
                        play_nomusic();
                    }
                    break;
                case 1:
                    b = curStream;
                    if(curStream != targetStream) {
                        curStream = targetStream;
                    } else {
                        find_next_stream();
                    }
                    if((b != curStream) || !st_active()) {
                        play_current_stream();
                    }
                    break;
                case 2:
                    panelLEDsLock = false;
                    if(remHaveFullStatus && remHaveMusic) {
                        bool curIsTarget = false;
                        if(rem_stat.validFields & RMPS_CURLN) {
                            curIsTarget = ((remTargetLtr == rem_stat.curLtr) && (remTargetNum == rem_stat.curNum));
                        } else {
                            curIsTarget = (remTargetTrack == rem_stat.curTrack);
                        }
                        if(!rem_stat.off) {
                            if(rem_stat.playing && curIsTarget) {
                                rem_next();
                            } else if(!curIsTarget) {
                                rem_goto(remTargetLtr, remTargetNum);
                            } else {
                                rem_next();
                            }
                        } else {
                            rem_is_off();
                        }
                    } else if(remHaveMusic) {
                        rem_next();
                    }
                    break;
                }
            }
            break;
        }
        break;

    case TCBS_LONGPRESS:
        switch(idx) {
        case 0:   // The button on Jog Dial 1
            if(csf & CSF_OFF) {
                reconnect_WiFi();
            } else {
                switch(opMode) {
                case 0:
                    mp_makeShuffle(!!!aud_state.mpShuffle);
                    jbLED.specialSignal(aud_state.mpShuffle ? JBSEQ_SHUFFLE_ON : JBSEQ_SHUFFLE_OFF);
                    play_file(aud_state.mpShuffle ? "/shuffleon.mp3" : "/shuffleoff.mp3", PA_CHECKNM|PA_ALLOWSD);
                    break;
                case 1:
                    break;
                case 2:
                    if(*settings.mqmpt[MQ_SHU1] && *settings.mqmpt[MQ_SHU0]) {
                        if(!remHaveStatus || !rem_stat.off) {
                            if(rem_stat.validFields & RMPS_SHUFFLE) {
                                rem_shuffle(!rem_stat.shuffle);
                                remShuffleInt = !rem_stat.shuffle;
                                remAllowShuffleSignal = true;
                            } else {
                                remShuffleInt = !remShuffleInt;
                                rem_shuffle(remShuffleInt);
                                jbLED.specialSignal(remShuffleInt ? JBSEQ_SHUFFLE_ON : JBSEQ_SHUFFLE_OFF);
                            }
                        } else {
                            rem_is_off();
                        }
                    }
                    break;
                }
            }
            break;
        case 1:   // The button on Jog Dial 2
            if(csf & CSF_OFF) {
                // Start signal for "reset ip and AP-PW"
                jbLED.specialSignal(JBSEQ_RESETIPPW);
            } else {
                int b;
                switch(opMode) {
                case 0:
                    if(!(csf & CSF_NOMUSIC)) {
                        mp_prev(true);
                    } else {
                        play_nomusic();
                    }
                    break;
                case 1:
                    b = curStream;
                    find_prev_stream();
                    if((b != curStream) || !st_active()) {
                        play_current_stream();
                    }
                    break;
                case 2:
                    if(remHaveMusic) {
                        if(!rem_stat.off) {
                            rem_prev();
                        } else {
                            rem_is_off();
                        }
                    }
                    break;
                }
            }
            break;
        }
        break;

    case TCBS_LONGPRESSEND:
        switch(idx) {
        case 0:   // The button on Jog Dial 1
            break;
        case 1:   // The button on Jog Dial 2
            if(csf & CSF_OFF) {
                // Cancel signal for "reset ip and AP-PW"
                jbLED.specialSignal(0);         
            }
            break;
        }
        break;

    case TCBS_ELONGPRESS:
        switch(idx) {
        case 0:   // The button on Jog Dial 1
            break;
        case 1:   // The button on Jog Dial 2
            if(csf & CSF_OFF) {
                jbLED.specialSignal(0);
                reset_IP_and_WIFIAPPW();
            } 
            break;
        }
        break;
    }
}

void prepareTT()
{
    doPrepareTT = false;
}

void wakeup()
{
    doWakeup = false;
}

static void volWasChanged()
{
    re_vol_reset();
    
    volchgnow = millisNonZero();
    storeCurVolume();
}

static void waitAudioDone()
{
    int timeout = 400;

    while(!checkAudioDone() && timeout--) {
        mydelay(10);
    }
}

static void re_vol_reset()
{
    if(csf & CSF_URotEncVol) {
        rotEncV.setZeroPos();
    }
}

static void re_jd_reset()
{
    if(csf & CSF_URotEncJD) {
        rotEncJ1.setZeroPos();
        rotEncJ2.setZeroPos();
    }
}

void musicPlayCallback()
{
    triggerUpdatePLEDs = true;
}

void updateRemotePlayer(const char *statusJSON)
{
    bool oldStatus = remHaveFullStatus;
    bool oldHM = remHaveMusic;

    #ifdef JB_DBG_MQTT
    mqttPublishQuick("bttf/jb/debug", statusJSON);
    #endif
    
    if(parse_mp_backchannel(statusJSON, &rem_stat)) {

        if(rem_stat.validFields & RMPS_STATE) {
            remHaveStatus = true;
            remActiveInt = rem_stat.playing;
        } else {
            remHaveStatus = false;
        }

        if(rem_stat.validFields & RMPS_FIRST) {
            remFirstTrack = rem_stat.firstTrack;
        }

        if(rem_stat.validFields & RMPS_LAST) {
            remLastTrack = rem_stat.lastTrack;
        }

        if(rem_stat.validFields & RMPS_SHUFFLE) {
            remShuffleInt = rem_stat.shuffle;
            if(remAllowShuffleSignal) {
                remAllowShuffleSignal = false;
                jbLED.specialSignal(remShuffleInt ? JBSEQ_SHUFFLE_ON : JBSEQ_SHUFFLE_OFF);
            }
        }
    }

    remHaveFullStatus = ((rem_stat.validFields & RMPS_CURLN) || 
                         ((rem_stat.validFields & (RMPS_CURTRCK|RMPS_FIRST|RMPS_LAST)) == (RMPS_CURTRCK|RMPS_FIRST|RMPS_LAST)));

    if(remHaveFullStatus) {
        if(rem_stat.validFields & RMPS_CURLN) {
            if(rem_stat.validFields & RMPS_MAXLN) {
                if((rem_stat.curLtr > rem_stat.maxLtr) ||
                   ((rem_stat.curLtr == rem_stat.maxLtr) && (rem_stat.curNum > rem_stat.maxNum))) {
                    remHaveFullStatus = false;
                }
            }
        } else {
            if((remFirstTrack > remLastTrack)      ||
               (rem_stat.curTrack < remFirstTrack) ||
               (rem_stat.curTrack > remLastTrack)) {
                remHaveFullStatus = false;
            }
        }
    }

    if(remHaveStatus) {
        remHaveMusic = !rem_stat.off;
    } else {
        remHaveMusic = true;      // Default to assuming we have sth to play
    }

    #ifdef JB_DBG_MQTT
    {
        char buf[128];
        sprintf(buf, "remHaveFullStatus %d remHaveMusic %d  vf 0x%x",
            remHaveFullStatus,
            remHaveMusic, 
            rem_stat.validFields);
        mqttPublishQuick("bttf/jb/debug", buf);
    }
    #endif
    
    // Before we have a status, user might have moved Jog Dials,
    // and we didn't poll. Reset dials as soon as we get a status.
    if(oldStatus != remHaveFullStatus || oldHM != remHaveMusic) {
        re_jd_reset();
    }

    triggerUpdatePLEDs = true;
}

void mqttConnectCallback()
{
    if(opMode == 2) {
        rem_request_status();
    }
}

void fade_loop()
{
    unsigned long now = millisNonZero();

    if(whiteFade && (now - whiteFade > wfDelay)) {
        uint32_t c = whiteLED.getDC();
        c += (uint32_t)wfStep;
        if((int32_t)c < 0)       { c = 0;           whiteFade = 0; }
        else if(c > whiteMaxBri) { c = whiteMaxBri; whiteFade = 0; }
        else whiteFade = now;
        whiteLED.setDC(c);
    }
    if(blueFade && (now - blueFade > bfDelay)) {
        uint32_t c = blueLED.getDC();
        c += (uint32_t)bfStep;
        if((int32_t)c < 0)      { c = 0;          blueFade = 0; }
        else if(c > blueMaxBri) { c = blueMaxBri; blueFade = 0; }
        else blueFade = now;
        blueLED.setDC(c);
    }
}

/*
 *  Do this whenever we are caught in a while() loop
 *  This allows other stuff to proceed
 */
static void myloop()
{
    wifi_loop();
    audio_loop();
    fade_loop();
    bttfn_loop_quick();
}

/*
 * MyDelay:
 * Calls myloop() periodically
 */
void mydelay(unsigned long mydel)
{
    unsigned long startNow = millis();
    myloop();
    while(millis() - startNow < mydel) {
        delay(10);
        myloop();
    }
}

unsigned long millisNonZero()
{
    unsigned long now = millis();
    if(!now) now--;
    return now;
}

/*
 * Basic Telematics Transmission Framework (BTTFN)
 */

static bool check_packet(uint8_t *buf)
{
    // Basic validity check
    if(memcmp(buf, BTTFUDPHD, 4))
        return false;

    uint8_t a = 0;
    for(int i = 4; i < BTTF_PACKET_SIZE - 1; i++) {
        a += buf[i] ^ 0x55;
    }

    return (buf[BTTF_PACKET_SIZE - 1] == a);
}

void addCmdQueue(uint32_t command)
{
    if(!command) return;

    commandQueue[iCmdIdx] = command;
    iCmdIdx++;
    iCmdIdx &= 0x0f;
}

static void bttfn_eval_response(uint8_t *buf, bool checkCaps)
{
    if(checkCaps && (buf[5] & 0x40)) {
        bttfnReqStatus &= ~0x40;     // Do no longer poll capabilities
        if(buf[31] & 0x10) {
            TCDSupportsNOTData = true;
            TCDSupportsSSID = !!(buf[31] & 0x40);
        }
    }

    if(buf[5] & 0x10) {
        if(useNM) tcdNM = !!(buf[26] & 0x01);
        tcdIsBusy = !!(buf[26] & 0x10);
    } else {
        if(useNM) tcdNM = false;
    }

    if(!bttfnHaveTCDSSID && !checkCaps && TCDSupportsSSID) {
        bttfnHaveTCDSSID = 1;
        memcpy((void *)TCDSSID, (void *)&buf[41], 6);
        TCDSSID[6] = buf[18];
        TCDpwMarker = buf[19] & 0x01;
    }
}

static void handle_tcd_notification(uint8_t *buf)
{
    uint32_t seqCnt;

    // Note: This might be called while we are in a
    // wait-delay-loop. Best to just set flags here
    // that are evaluated synchronously (=later).
    // Do not mess with display, input, etc.

    if(buf[5] & BTTFN_NOT_DATA) {
        if(TCDSupportsNOTData) {
            bttfnDataNotEnabled = true;
            bttfnLastNotData = millis();
            seqCnt = GET32(buf, 27);
            if(bttfnSessionID && (bttfnSessionID != seqCnt)) {
                lastBTTFNKA = bttfnLastNotData - BTTFN_KA_INTERVAL + (BTTFN_KA_OFFSET*1000);
                bttfnTCDDataSeqCnt = 1;
                bttfnHaveTCDSSID = 0;
            }
            bttfnSessionID = seqCnt;
            seqCnt = GET32(buf, 6);
            if(seqCnt > bttfnTCDDataSeqCnt || seqCnt == 1) {
                #ifdef JB_DBG_NET
                Serial.println("Valid NOT_DATA packet received");
                #endif
                bttfn_eval_response(buf, false);
            } else {
                #ifdef JB_DBG_NET
                Serial.printf("Out-of-sequence NOT_DATA packet received %d %d\n", seqCnt, bttfnTCDDataSeqCnt);
                #endif
            }
            bttfnTCDDataSeqCnt = seqCnt;
        }
        return;
    }
    
    switch(buf[5]) {
    case BTTFN_NOT_SPD:
        break;
    case BTTFN_NOT_PREPARE:
        // Prepare for TT. Comes at some undefined point,
        // an undefined time before the actual tt, and
        // may not come at all.
        // We disable our Screen Saver and start the flux
        // sound (if to be played)
        // We don't ignore this if TCD is connected by wire,
        // because this signal does not come via wire.
        doPrepareTT = true;
        break;
    case BTTFN_NOT_TT:
        // Trigger Time Travel (if not running already)
        if(!(csf & (CSF_TT|CSF_BUSY))) {
            networkTimeTravel = true;
            networkTCDTT = true;
            networkReentry = false;
            networkAbort = false;
            networkLead = buf[6] | (buf[7] << 8);
            networkP1 = buf[8] | (buf[9] << 8);
        }
        break;
    case BTTFN_NOT_REENTRY:
        // Start re-entry (if TT currently running)
        if(((csf & CSF_TT) || networkTimeTravel) && networkTCDTT) {
            networkReentry = true;
        }
        break;
    case BTTFN_NOT_ABORT_TT:
        // Abort TT (if TT currently running)
        if(((csf & CSF_TT) || networkTimeTravel) && networkTCDTT) {
            networkAbort = true;
        }
        break;
    case BTTFN_NOT_ALARM:
        networkAlarm = true;
        break;
    case BTTFN_NOT_AUX_CMD:
        if(!(csf & CSF_BUSY)) {
            addCmdQueue(GET32(buf, 6));
        }
        break;
    case BTTFN_NOT_WAKEUP:
        doWakeup = true;
        break;
    case BTTFN_NOT_INFO:
        {
            uint16_t tcdi1 = buf[6] | (buf[7] << 8);
            uint16_t tcdi2 = buf[8] | (buf[9] << 8);
            if(tcdi1 & BTTFN_TCDI1_EXT) {
                if(useNM) tcdNM  = !!(tcdi1 & BTTFN_TCDI1_NM);
            }
            tcdIsBusy = !!(tcdi2 & BTTFN_TCDI2_BUSY);
        }
        break;
    }
}

// Check for pending MC packet and parse it
static bool bttfn_checkmc()
{
    int psize = jbMcUDP->parsePacket();

    if(!psize)
        return false;

    // This returns true as long as a packet was received
    // regardless whether it was for us or not. Point is
    // to clear the receive buffer.
    
    jbMcUDP->read(BTTFMCBuf, BTTF_PACKET_SIZE);

    #ifdef JB_DBG_NET
    Serial.printf("Received multicast packet from %s\n", jbMcUDP->remoteIP().toString());
    #endif

    if(haveTCDIP) {
        if(bttfnTcdIP != jbMcUDP->remoteIP())
            return true;
    } else {
        // Do not use tcdHostNameHash; let DISCOVER do its work
        // and wait for a result.
        return true;
    }

    if(!check_packet(BTTFMCBuf))
        return true;

    if((BTTFMCBuf[4] & 0x4f) == (BTTFN_VERSION | 0x40)) {

        // A notification from the TCD
        handle_tcd_notification(BTTFMCBuf);
    
    }

    return true;
}

// Check for pending packet and parse it
static void BTTFNCheckPacket()
{
    unsigned long mymillis = millisNonZero();
    
    int psize = bUDP->parsePacket();
    if(!psize) {
        if(!bttfnDataNotEnabled && BTTFNPacketDue) {
            if((mymillis - BTTFNTSRQAge) > BTTFN_RESPONSE_TO) {
                // Packet timed out
                BTTFNPacketDue = false;
                // Immediately trigger new request for
                // the first 10 timeouts, after that
                // the new request is only triggered
                // in greater intervals via bttfn_loop().
                if(haveTCDIP && BTTFNfailCount < 10) {
                    BTTFNfailCount++;
                    BTTFNUpdateNow = 0;
                }
            }
        }
        return;
    }
    
    bUDP->read(BTTFUDPBuf, BTTF_PACKET_SIZE);

    if(!check_packet(BTTFUDPBuf))
        return;

    if((BTTFUDPBuf[4] & 0x4f) == (BTTFN_VERSION | 0x40)) {

        // A notification from the TCD
        handle_tcd_notification(BTTFUDPBuf);

    } else {

        // (Possibly) a response packet

        if(GET32(BTTFUDPBuf, 6) != BTTFUDPID)
             return;

        // Response marker missing or wrong version, bail
        if((BTTFUDPBuf[4] & 0x8f) != (BTTFN_VERSION | 0x80))
            return;

        BTTFNfailCount = 0;
    
        // If it's our expected packet, no other is due for now
        BTTFNPacketDue = false;

        if(BTTFUDPBuf[5] & 0x80) {
            if(!haveTCDIP) {
                bttfnTcdIP = bUDP->remoteIP();
                haveTCDIP = true;
                #ifdef JB_DBG_NET
                Serial.printf("Discovered TCD IP %d.%d.%d.%d\n", bttfnTcdIP[0], bttfnTcdIP[1], bttfnTcdIP[2], bttfnTcdIP[3]);
                #endif
            } else {
                #ifdef JB_DBG_NET
                Serial.println("Internal error - received unexpected DISCOVER response");
                #endif
            }
        }

        // TCD did register us, so use current millis as
        // baseline for KEEP_ALIVE (lastBTTFNKA)
        lastBTTFNpacket = lastBTTFNKA = mymillis;

        bttfn_eval_response(BTTFUDPBuf, true);
    }
}

static void BTTFNPreparePacketTemplate()
{
    memset(BTTFUDPTBuf, 0, BTTF_PACKET_SIZE);

    // ID
    memcpy(BTTFUDPTBuf, BTTFUDPHD, 4);

    // Tell the TCD about our hostname
    // 13 bytes total. If hostname is longer, last in buf is '.'
    memcpy(BTTFUDPTBuf + 10, settings.hostName, 13);
    if(strlen(settings.hostName) > 13) BTTFUDPTBuf[10+12] = '.';

    BTTFUDPTBuf[10+13] = BTTFN_TYPE_AUX;

    // Version, MC-marker, ND-marker
    BTTFUDPTBuf[4] = BTTFN_VERSION | BTTFN_SUP_MC | BTTFN_SUP_ND;                
}

static void BTTFNPreparePacket()
{
    memcpy(BTTFUDPBuf, BTTFUDPTBuf, BTTF_PACKET_SIZE);                
}

static void BTTFNDispatch()
{
    uint8_t a = 0;
    for(int i = 4; i < BTTF_PACKET_SIZE - 1; i++) {
        a += BTTFUDPBuf[i] ^ 0x55;
    }
    BTTFUDPBuf[BTTF_PACKET_SIZE - 1] = a;

    if(haveTCDIP) {
        bUDP->beginPacket(bttfnTcdIP, BTTF_DEFAULT_LOCAL_PORT);
    } else {
        #ifdef JB_DBG_NET
        Serial.printf("Sending multicast (hostname hash %x)\n", tcdHostNameHash);
        #endif
        bUDP->beginPacket(bttfnMcIP, BTTF_DEFAULT_LOCAL_PORT + 1);
    }
    bUDP->write(BTTFUDPBuf, BTTF_PACKET_SIZE);
    bUDP->endPacket();
}

// Send a new data request
static bool BTTFNSendRequest()
{
    BTTFNPacketDue = false;

    BTTFNUpdateNow = millisNonZero();

    if(WiFi.status() != WL_CONNECTED) {
        BTTFNWiFiUp = false;
        return false;
    }

    BTTFNWiFiUp = true;

    // Send new packet
    BTTFNPreparePacket();
    
    // Serial
    BTTFUDPID = (uint32_t)millis();
    SET32(BTTFUDPBuf, 6, BTTFUDPID);

    // Request flags
    BTTFUDPBuf[5] = bttfnReqStatus;

    if(!haveTCDIP) {
        BTTFUDPBuf[5] |= 0x80;
        SET32(BTTFUDPBuf, 31, tcdHostNameHash);
    }

    BTTFNDispatch();

    BTTFNTSRQAge = millis();
    
    BTTFNPacketDue = true;
    
    return true;
}

static bool bttfn_connected()
{
    if(!useBTTFN)
        return false;

    if(!haveTCDIP)
        return false;

    if(WiFi.status() != WL_CONNECTED)
        return false;

    if(!lastBTTFNpacket)
        return false;

    return true;
}

static bool bttfn_send_command(uint8_t cmd, uint8_t p1, uint8_t p2)
{
    if(cmd <= BTTFN_REM_MAX_COMMAND)
        return false;
        
    if(!bttfn_connected())
        return false;

    BTTFNPreparePacket();
    
    //BTTFUDPBuf[5] = 0x00; // 0 already

    BTTFUDPBuf[25] = cmd;                           // Cmd + parms
    BTTFUDPBuf[26] = p1;
    BTTFUDPBuf[27] = p2;

    BTTFNDispatch();

    #ifdef JB_DBG_NET
    Serial.printf("Sent command %d\n", cmd);
    #endif

    BTTFNLastCmdSent = millisNonZero();

    return true;
}


static void bttfn_setup()
{
    useBTTFN = false;

    // string empty? Disable BTTFN.
    if(!settings.tcdIP[0])
        return;

    haveTCDIP = isIp(settings.tcdIP);
    
    if(!haveTCDIP) {
        tcdHostNameHash = 0;
        unsigned char *s = (unsigned char *)settings.tcdIP;
        for ( ; *s; ++s) tcdHostNameHash = 37 * tcdHostNameHash + tolower(*s);
    } else {
        bttfnTcdIP.fromString(settings.tcdIP);
    }
    
    bUDP = &bttfUDP;
    bUDP->begin(BTTF_DEFAULT_LOCAL_PORT);

    jbMcUDP = &bttfMcUDP;
    jbMcUDP->beginMulticast(bttfnMcIP, BTTF_DEFAULT_LOCAL_PORT + 2);

    BTTFNPreparePacketTemplate();
    
    BTTFNfailCount = 0;
    useBTTFN = true;
}

void bttfn_loop()
{
    if(!useBTTFN)
        return;

    int t = 100;
    
    while(bttfn_checkmc() && t--) {}

    unsigned long now = millisNonZero();
            
    BTTFNCheckPacket();

    if(bttfnDataNotEnabled) {
        if(now - lastBTTFNKA > BTTFN_KA_INTERVAL) {
            if(!BTTFNLastCmdSent || (now - BTTFNLastCmdSent > (BTTFN_KA_INTERVAL/2))) {
                bttfn_send_command(BTTFN_REMCMD_KEEPALIVE, 0, 0);
                #ifdef JB_DBG_NET
                Serial.println("Sent KEEP-ALIVE");
                #endif
            } else {
                #ifdef JB_DBG_NET
                Serial.println("Skipped KEEP-ALIVE");
                #endif
            }
            BTTFNLastCmdSent = 0;
            do {
                lastBTTFNKA += BTTFN_KA_INTERVAL;
            } while(now - lastBTTFNKA >= BTTFN_KA_INTERVAL);
        }
        if(now - bttfnLastNotData > BTTFN_DATA_TO) {
            // Return to polling if no NOT_DATA for too long
            bttfnDataNotEnabled = false;
            bttfnTCDDataSeqCnt = 1;
            // Re-do DISCOVER, TCD might have got new IP address
            if(tcdHostNameHash) haveTCDIP = false;
            // Don't assume TCD comes back with same SSID/pwMarker
            bttfnHaveTCDSSID = 0;
            // Avoid immediate return to stand-alone in main_loop()
            lastBTTFNpacket = now;
            #ifdef JB_DBG_NET
            Serial.println("NOT_DATA timeout, returning to polling");
            #endif
        }
    } else if(!BTTFNPacketDue) {
        // If WiFi status changed, trigger immediately
        if(!BTTFNWiFiUp && (WiFi.status() == WL_CONNECTED)) {
            BTTFNUpdateNow = 0;
        }
        if((!BTTFNUpdateNow) || (now - BTTFNUpdateNow > bttfnJBPollInt)) {
            BTTFNSendRequest();
        }
    }
}

static void bttfn_loop_quick()
{
    if(!useBTTFN)
        return;
    
    int t = 100;
    
    while(bttfn_checkmc() && t--) {}
}
