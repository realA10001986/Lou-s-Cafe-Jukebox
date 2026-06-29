/*
 * -------------------------------------------------------------------
 * A10001986 Crosley CR-9 Jukebox
 * (C) 2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Jukebox
 * https://jukebox.out-a-ti.me
 *
 * Sound handling
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

#ifdef JB_DBG_AUDIO
#define JB_DBG_MP     // debug music player
#endif

#include <Arduino.h>
#include <SD.h>
#include <FS.h>

#include "AudioFileSourceLoop.h"
#include "src/ESP8266Audio/AudioFileSourceBuffer.h"
#include "src/ESP8266Audio/AudioFileSourceICYStream.h"

#include "src/ESP8266Audio/AudioGeneratorMP3.h"

#include "src/ESP8266Audio/AudioOutputI2S.h"

#include "jb_main.h"
#include "jb_settings.h"
#include "jb_audio.h"
#include "jb_wifi.h"

static AudioGeneratorMP3 *mp3;

static AudioFileSourceFSLoop *myFS0;
static AudioFileSourceSDLoop *mySD0;
static AudioFileSourceICYStream *myICY;
static AudioFileSourceBuffer *myBuff;

static AudioOutputI2S *out;

bool   audioInitDone = false;

bool            mpActive = false;
static uint16_t *playList = NULL;
static int      mpCurrIdx = 0;
#define         MAXID3LEN 2048

Aud_State aud_state  = { .state = 0, .curVolume = DEFAULT_VOLUME, .curTrack = 0, .maxMusic = 0, .mpShuffle = 0 };
Aud_State mpOldState = { .state = -1 };

static void *streamBuf = NULL;
#define STREAM_BUFSIZE 65536

static const float volTable[VOL_LEVELS] = {
    0.00f, 0.02f, 0.04f, 0.06f,
    0.08f, 0.10f, 0.12f, 0.14f,
    0.16f, 0.19f, 0.22f, 0.26f, 
    0.30f, 0.35f, 0.40f, 0.50f, 
    0.60f, 0.70f, 0.80f, 0.90f, 
    1.00f
};

static uint32_t g(uint32_t a, int o) { return a << (PA_MASK - o); }

static float    curVolFact = 1.0f;

static uint32_t play_flags = 0;

int             mfstatus[NUM_MUSIC_FOLDERS] = { 0 };

static char     keySnd[] = "/key3.mp3";   // not const
static uint32_t haveKeySnd = 0;

bool            useJBNum = true;    // Send Jukebox numbering over backchannel (Since GOTO is also JB numbering, no need to make this config'ble)

bool            playJB = true;

static char     append_audio_file[32];
static float    append_vol;
static uint32_t append_flags;
static int      appendFile = 0;

static const char *tcdrdone = "/TCD_DONE.TXT";
unsigned long   renNow1, renNow2;

static float    getVolume();

static int      mp_findMaxNum();
static bool     mp_play_int(bool force);
static void     mp_buildFileName(char *fnbuf, int num);
static bool     mp_renameFilesInDir(bool isSetup);
static uint8_t* mpren_renOrder(uint8_t *a, uint32_t s, int e);
uint8_t*        m(uint8_t *a, uint32_t s, int e) { return mpren_renOrder(a, s, e/4); }
static void     mpren_insertionSort(char **a, int n);

/*
 * audio_setup()
 */
void audio_setup()
{
    unsigned int sps = 0;
    uint32_t tbuf[3];
    #ifdef JB_DBG_AUDIO
    audioLogger = &Serial;
    #endif

    out = new AudioOutputI2S(0, AudioOutputI2S::EXTERNAL_I2S, 32, AudioOutputI2S::APLL_DISABLE);
    // Hardware does auto-mono, no need to ever call this later
    // (Also, the mono code is commented out in the audio lib)
    out->SetOutputModeMono(false); 
    out->SetPinout(I2S_BCLK_PIN, I2S_LRCLK_PIN, I2S_DIN_PIN);

    mp3 = new AudioGeneratorMP3();

    myFS0 = new AudioFileSourceFSLoop();
    
    if(haveSD) {
        mySD0 = new AudioFileSourceSDLoop();
    }

    myICY = new AudioFileSourceICYStream();
    myBuff = new AudioFileSourceBuffer(myICY, NULL, 0);

    loadCurVolume();

    loadMusFoldNum();
    loadShuffle();

    // MusicPlayer init done through switchMusicFolder in main_setup()

    // Check for sound files to avoid unsuccessful file-lookups later
    
    for(int i = 1, bm = 1 << 8; i < 10; i++, bm <<= 1) {
        keySnd[4] = '0' + i;
        if(check_file_SD(keySnd)) haveKeySnd |= bm;
    }

    for(int i = 0; i < NUM_MUSIC_FOLDERS; i++) {
        mfstatus[i] = mp_checkForFolder(i);
    }

    // Limit max bitrate for mp3 playback; playback aborts if 
    // bitrate is higher than that.
    mp3->setMaxBitRate(160000);

    audioInitDone = true;
}

static int checkAppend()
{
    if(appendFile) {
        play_file(append_audio_file, append_flags, append_vol);
        return 1;
    }
    return 0;
}

/*
 * audio_loop()
 *
 */
void audio_loop()
{
    if(mp3->isRunning()) {
        if(!mp3->loop()) {
            mp3->stop();
            play_flags = 0;
            if(!checkAppend() && mpActive) {
                mp_next(true);
            }
        } else {
            out->SetGain(getVolume());
        }
    } else {
        if(streamBuf) {
            free(streamBuf);
            streamBuf = NULL;
        }
        if(!checkAppend() && mpActive) {
            mp_next(true);
        }
    }

    update_volume();

    mp_sendStatus();
}

static int32_t skipID3(char *buf)
{
    if(buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3' && 
       buf[3] >= 0x02 && buf[3] <= 0x04 && buf[4] == 0 &&
       (!(buf[5] & 0x80))) {
        int32_t pos = ((buf[6] << (24-3)) |
                       (buf[7] << (16-2)) |
                       (buf[8] << (8-1))  |
                       (buf[9])) + 10;
        #ifdef JB_DBG_AUDIO
        Serial.printf("Skipping ID3 tags, seeking to %d (0x%x)\n", pos, pos);
        #endif
        return pos;
    }
    return 0;
}

void append_file(const char *audio_file, uint32_t flags, float volumeFactor)
{
    if(strlen(audio_file) >= sizeof(append_audio_file) - 1) {
        #ifdef JB_DBG_AUDIO
        Serial.printf("Internal error: Sound file name too long (%d vs max %d)\n", strlen(audio_file), sizeof(append_audio_file));
        #endif
        return;
    }
    strcpy(append_audio_file, audio_file);
    append_flags = flags;
    append_vol = volumeFactor;
    appendFile = 1;
}

void play_file(const char *audio_file, uint32_t flags, float volumeFactor)
{
    char buf[10];
    int32_t pos = 0;
    bool mpWasActive = false;

    appendFile = 0;   // Clear appended, append must be called AFTER play_file

    if(!(flags & PA_ISMUSIC)) {
        if(flags & PA_INTRMUS) {
            mpWasActive = mpActive;
            mpActive = false;
        } else {
            if(mpActive || st_active()) return;
        }
    }
    
    #ifdef JB_DBG_AUDIO
    Serial.printf("Audio: Playing %s\n", audio_file);
    #endif

    // If something is currently on, kill it
    stopAudio();
    
    play_flags = flags;
    
    curVolFact = volumeFactor;

    out->SetGain(getVolume());

    buf[0] = 0;

    if(haveSD && ((flags & PA_ALLOWSD) || FlashROMode) && mySD0->open(audio_file)) {
        mySD0->setPlayLoop(!!(flags & PA_LOOP));
        mySD0->read((void *)buf, 10);
        pos = skipID3(buf);
        mySD0->setStartPos(pos);
        mySD0->seek(pos, SEEK_SET);
        mp3->begin(mySD0, out);
        #ifdef JB_DBG_AUDIO
        Serial.println("Playing from SD");
        #endif
    } else if(haveFS && myFS0->open(audio_file)) {
        myFS0->setPlayLoop(!!(flags & PA_LOOP));
        myFS0->read((void *)buf, 10);
        pos = skipID3(buf);
        myFS0->setStartPos(pos);
        myFS0->seek(pos, SEEK_SET);
        mp3->begin(myFS0, out);
        #ifdef JB_DBG_AUDIO
        Serial.println("Playing from flash FS");
        #endif
    } else {
        play_flags = 0;
        #ifdef JB_DBG_AUDIO
        Serial.println("Audio file not found");
        #endif
    }

    if(mpWasActive) mp_sendStatus();
}

//
// Return value:
//  0: ok
// -1: no memory for buffer
// -2: HTTP error
// -3: Stream unsuitable (bad bitrate, bad content-type)
int play_stream(const char *url, uint32_t flags, float volumeFactor)
{
    uint32_t buffSize = STREAM_BUFSIZE;
    int res;
    
    appendFile = 0;   // Clear appended
    
    #ifdef JB_DBG_AUDIO
    Serial.printf("Audio: Playing %s\n", url);
    #endif

    // If something is currently on, kill it
    if(mp3->isRunning()) {
        mp3->stop();
    }    
    play_flags = 0;
    
    if(!streamBuf) {
        if(!(streamBuf = malloc(buffSize))) {
            buffSize -= (buffSize / 4);
            if(!(streamBuf = malloc(buffSize))) {
                return STRS_NOMEM;
            }
        }
    }

    curVolFact = volumeFactor;
    out->SetGain(getVolume());
    
    myBuff->init(myICY, streamBuf, buffSize);
    
    if(!(res = myICY->openNew(url))) {
        mp3->begin(myBuff, out);
        play_flags = flags;
        return STRS_OK;
    }
    
    free(streamBuf);
    streamBuf = NULL;

    if(res == ICY_ERR_BAD_BR || res == ICY_ERR_BAD_CT)
        return STRS_BAD;
        
    return STRS_HTTPERR;
}

/*
 * Play specific sounds
 * 
 */

bool play_key(int k, bool stopOnly)
{
    uint32_t pa_key = 1 << (7+k);
    
    if(!(haveKeySnd & pa_key))
        return false;

    if(pa_key == (play_flags & PA_KEYMASK)) {
        mp3->stop();
        play_flags = 0;
        return true;
    }

    if(stopOnly)
        return true;
    
    keySnd[4] = '0' + k;
    
    play_file(keySnd, pa_key|PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL|PA_CHECKNM);

    return true;
}

static float getVolume()
{
    float vol_val = volTable[aud_state.curVolume];

    // If user muted, return 0
    if(vol_val == 0.0f) return vol_val;

    vol_val *= curVolFact;

    // Reduce volume in night mode, if requested
    if((play_flags & PA_CHECKNM) && (csf & CSF_NM)) {
        vol_val *= 0.3f;
    }

    // Do not totally mute
    // 0.02 is the lowest audible gain
    if(vol_val < 0.02f) vol_val = 0.02f;
    else if(vol_val > 1.0f) vol_val = 1.0f;

    return vol_val;
}

/*
 * Helpers
 */
bool check_file_SD(const char *audio_file)
{
    #ifdef JB_DBG_AUDIO
    Serial.printf("check_file_SD: Checking for %s\n", audio_file);
    #endif
    return (haveSD && SD.exists(audio_file));
}

unsigned int check_file_len_SD(const char *audio_file, bool& file_exists, uint8_t *tbuf, uint32_t tsz)
{
    unsigned int s = 0;
    if(haveSD) {
        File file;
        if(file = SD.open(audio_file, FILE_READ)) {
            s = file.size();
            if(tbuf && tsz) {
                if(file.read(tbuf, tsz) != tsz) s = 0;
            }
            file.close();
        }
    }
    file_exists = (s > 0);
    return s;
}

// Audio is really done and free for use
bool checkAudioDone()
{
    if(mp3->isRunning()) {
        if(!(play_flags & PA_VOLCHG)) return false;
    }
    return true;
}

bool checkMP3Running()
{
    return mp3->isRunning();
}

void stopAudio()
{
    if(mp3->isRunning()) {
        mp3->stop();
    }

    if(streamBuf) {
        free(streamBuf);
        streamBuf = NULL;
    }

    play_flags = 0;    
    appendFile = 0;   // Clear appended, stop means stop.
}

void stop_key()
{
    if(play_flags & PA_KEYMASK) {
        stopAudio();
    }
}

bool append_pending()
{
    return appendFile;
}

bool st_active()
{
    return ((!!(play_flags & PA_STREAM)) && mp3->isRunning());
}

bool st_stop(bool force)
{
    bool ret = st_active();

    if(play_flags & PA_STREAM) stopAudio();

    return ret;
}

/*
 * The Music Player
 */
 
void mp_init(bool isSetup)
{
    char fnbuf[20];
    
    csf |= CSF_NOMUSIC;

    if(playList) {
        free(playList);
        playList = NULL;
    }

    mpCurrIdx = aud_state.curTrack = aud_state.maxMusic = 0;
    
    if(haveSD) {
        #ifdef JB_DBG_MP
        Serial.println("MusicPlayer: Checking for music files");
        #endif

        mp_renameFilesInDir(isSetup);

        mp_buildFileName(fnbuf, 0);
        if(SD.exists(fnbuf)) {
            csf &= ~CSF_NOMUSIC;
            
            aud_state.maxMusic = mp_findMaxNum();
            #ifdef JB_DBG_MP
            Serial.printf("MusicPlayer: last file num %d\n", aud_state.maxMusic);
            #endif

            playList = (uint16_t *)malloc((aud_state.maxMusic + 1) * 2);

            if(!playList) {

                csf |= CSF_NOMUSIC;
                #ifdef JB_DBG_MP
                Serial.println("MusicPlayer: Failed to allocate PlayList");
                #endif

            } else {

                // Init play list
                mp_makeShuffle(!!aud_state.mpShuffle);
                
            }

            aud_state.curTrack = playList[0];

        } else {
            #ifdef JB_DBG_MP
            Serial.printf("MusicPlayer: Failed to open %s\n", fnbuf);
            #endif
        }
    }

    mp_sendStatus();
}

static bool mp_checkForFile(int num)
{
    char fnbuf[20];

    if(num > 99) return false;

    mp_buildFileName(fnbuf, num);
    if(SD.exists(fnbuf)) {
        return true;
    }
    return false;
}

static int mp_findMaxNum()
{
    int i, j;

    for(j = 256, i = 512; j >= 2; j >>= 1) {
        if(mp_checkForFile(i)) {
            i += j;    
        } else {
            i -= j;
        }
    }
    if(mp_checkForFile(i)) {
        if(mp_checkForFile(i+1)) i++;
    } else {
        i--;
        if(!mp_checkForFile(i)) i--;
    }

    if(i > 99) i = 99;

    return i;
}

void mp_makeShuffle(bool enable)
{
    int numMsx = aud_state.maxMusic + 1;

    aud_state.mpShuffle = enable ? 1 : 0;
    saveShuffle();

    if(!(csf & CSF_NOMUSIC)) {
    
        for(int i = 0; i < numMsx; i++) {
            playList[i] = i;
        }
        
        if(enable && numMsx > 2) {
            for(int i = 0; i < numMsx; i++) {
                int ti = esp_random() % numMsx;
                uint16_t t = playList[ti];
                playList[ti] = playList[i];
                playList[i] = t;
            }
            /*
            #ifdef JB_DBG_MP
            for(int i = 0; i <= aud_state.maxMusic; i++) {
                Serial.printf("%d ", playList[i]);
                if((i+1) % 16 == 0 || i == aud_state.maxMusic) Serial.printf("\n");
            }
            #endif
            */
        }
    }

    mp_sendStatus();
}

int get_song_idx_0()
{
    if(csf & CSF_NOMUSIC)
        return 0;
    
    return playList[0];
}

void mp_play(bool forcePlay)
{
    int oldIdx = mpCurrIdx;

    if(csf & CSF_NOMUSIC) return;
    
    do {
        if(mp_play_int(forcePlay)) {
            break;
        }
        mpCurrIdx++;
        if(mpCurrIdx > aud_state.maxMusic) mpCurrIdx = 0;
    } while(oldIdx != mpCurrIdx);

    musicPlayCallback();
}

bool mp_stop(bool force, bool forceStatus)
{
    bool ret = mpActive;
    
    if(mpActive) {
        mp3->stop();
        play_flags = 0;
        if(!force) {
            if(playJB) {
                play_file("/jb-outro.mp3", PA_CHECKNM|PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL);
            }
        }
        mpActive = false;
        mp_sendStatus();
        // Do NOT clear appended here, this allows "append_file(); mp_stop();"
    } else if(forceStatus) {
        mp_sendStatus();
    }
    
    return ret;
}

static void mp_nextprev(bool forcePlay, int dir)
{
    int oldIdx = mpCurrIdx;

    if(csf & CSF_NOMUSIC) return;
    
    do {
        if(dir > 0) {
            if(mpCurrIdx == aud_state.maxMusic) mpCurrIdx -= aud_state.maxMusic;
            else mpCurrIdx++;
        } else if(dir < 0) {
            if(!mpCurrIdx) mpCurrIdx += aud_state.maxMusic;
            else mpCurrIdx--;
        }
        if(mp_play_int(forcePlay)) {
            break;
        }
    } while(oldIdx != mpCurrIdx);

    musicPlayCallback();
}

void mp_next(bool forcePlay)
{
    mp_nextprev(forcePlay, 1);
}

void mp_prev(bool forcePlay)
{   
    mp_nextprev(forcePlay, -1);
}

int mp_gotonum(int num, bool forcePlay)
{
    if(csf & CSF_NOMUSIC) return 0;

    if(num < 0) num = 0;
    else if(num > aud_state.maxMusic) num = aud_state.maxMusic;

    if(aud_state.mpShuffle) {
        for(int i = 0; i <= aud_state.maxMusic; i++) {
            if(playList[i] == num) {
                mpCurrIdx = i;
                break;
            }
        }
    } else
        mpCurrIdx = num;

    mp_play(forcePlay);

    musicPlayCallback();

    return playList[mpCurrIdx];
}

static bool mp_play_int(bool force)
{
    char fnbuf[20];

    mp_buildFileName(fnbuf, playList[mpCurrIdx]);
    if(SD.exists(fnbuf)) {
        if(force) {
            if(playJB) {
                play_file(mpActive ? "/jb-intro.mp3" : "/jb-introlong.mp3", PA_ISMUSIC|PA_CHECKNM|PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL);
                append_file(fnbuf, PA_ISMUSIC|PA_CHECKNM|PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL);
            } else {
                play_file(fnbuf, PA_ISMUSIC|PA_CHECKNM|PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL);
            }
        }
        mpActive = force;
        aud_state.curTrack = playList[mpCurrIdx];
        mp_sendStatus();
        return true;
    }
    return false;
}

int mp_get_currently_playing()
{
    if(csf & CSF_NOMUSIC)
        return -1;

    return aud_state.curTrack;
}

int mp_get_currently_playing_idx()
{
    if(csf & CSF_NOMUSIC)
        return -1;

    return mpCurrIdx;
}

void mp_sendStatus(int force)
{
    if(pubMP && mqttConnected()) {
        aud_state.state = ((csf & (CSF_OFF|CSF_BUSY|CSF_NOMUSIC)) || opMode) ? 0 : (mpActive ? 1 : 2);         
        if(memcmp((void *)&mpOldState, (void *)&aud_state, sizeof(aud_state)) || force) {
            static const char statec[] = "OPI";
            char msg[128];
            if(useJBNum) {
                sprintf(msg, 
                    "{\"S\":\"%c\",\"C\":\"%c-%d\",\"V\":\"%d\",\"L\":\"%c-%d\",\"SH\":\"%d\"}", 
                        statec[aud_state.state], 
                        jbltrs[aud_state.curTrack / 10],
                        (aud_state.curTrack % 10) + 1,
                        aud_state.curVolume * 100 / (VOL_LEVELS - 1),
                        jbltrs[aud_state.maxMusic / 10],
                        (aud_state.maxMusic % 10) + 1,
                        aud_state.mpShuffle);
            } else {
                sprintf(msg, 
                    "{\"S\":\"%c\",\"C\":\"%d\",\"V\":\"%d\",\"F\":\"0\",\"L\":\"%d\",\"SH\":\"%d\"}", 
                        statec[aud_state.state], 
                        aud_state.curTrack, 
                        aud_state.curVolume * 100 / (VOL_LEVELS - 1),
                        aud_state.maxMusic, 
                        aud_state.mpShuffle);
            }
            if(mqttPublish(mqbackchannel, msg, strlen(msg) + 1)) {
                memcpy((void *)&mpOldState, (void *)&aud_state, sizeof(aud_state));
            } else {
                mpOldState.state = -1;
            }
        }
    }
}

static void mp_buildFolderName(char *fnbuf, int mfNum)
{
    // internal number:
    // 0-9   => music[A-K]
    // 10-19 => music[1-10]
    if(mfNum < 10) {
        sprintf(fnbuf, "/music%c", jbltrs[mfNum]);
    } else {
        sprintf(fnbuf, "/music%d", mfNum - 9);
    }
}

static void mp_buildFileName(char *fnbuf, int fnum)
{
    mp_buildFolderName(fnbuf, musFolderNum);
    
    sprintf(fnbuf + strlen(fnbuf), "/%03d.mp3", fnum);
}

int mp_checkForFolder(int num)
{
    char fnbuf[32];
    int  flen;

    // returns 
    // 1 if folder is ready (contains 000.mp3 and DONE)
    // 0 if folder does not exist
    // -1 if folder exists but needs processing
    // -2 if musicX contains no audio files
    // -3 if musicX is not a folder
    // -4 if no SD

    if(!haveSD)
        return -4;
        
    if(num < 0 || num > NUM_MUSIC_FOLDERS - 1)
        return 0;

    // Build folder name
    mp_buildFolderName(fnbuf, num);
    flen = strlen(fnbuf);
    
    // If folder does not exist, return 0
    if(!SD.exists(fnbuf))
        return 0;

    // Check if folder is folder
    File origin = SD.open(fnbuf);
    if(!origin) return 0;
    if(!origin.isDirectory()) {
        // If musicX is not a folder, return -3
        origin.close();
        return -3;
    }
    origin.close();

    // Check if DONE exists
    strcat(fnbuf, tcdrdone);
    if(SD.exists(fnbuf)) {
        strcpy(fnbuf + flen + 1, "000.mp3");
        if(SD.exists(fnbuf)) {
            // If 000.mp3 and DONE exists, return 1
            return 1;
        }
        // If DONE, but no 000.mp3, assume no audio files
        return -2;
    }
      
    // DONE not present: Needs processing
    return -1;
}

/*
 * Auto-renamer
 */

// Check file is eligible for renaming:
// - not a hidden/exAtt file,
// - file name ends with ".mp3"
// - filename not already "/musicX/ddd.mp3"
static bool mpren_checkFN(const char *buf)
{
    // Hidden or macOS exAttr file? Ignore.
    if(buf[0] == '.') return true;

    size_t s = strlen(buf);

    // Filename shorter than ".mp3"? Ignore.
    if(s < 4) return true;

    s -= 4;
    // Not an mp3? Ignore.
    if(buf[s] != '.' || buf[s+3] != '3')
        return true;
    if(buf[s+1] != 'm' && buf[s+1] != 'M')
        return true;
    if(buf[s+2] != 'p' && buf[s+2] != 'P')
        return true;

    // Now check for xxx.mp3 (xxx=000-099)

    // Filename shorter or longer? Do it.
    if(s != 3)
        return false;

    // Filename not a 3-digit number? Do it.
    if(buf[0] < '0' || buf[0] > '9' ||
       buf[1] < '0' || buf[1] > '9' ||
       buf[2] < '0' || buf[2] > '9')
        return false;

    // Otherwise ignore.
    return true;
}

static void mpren_looper(bool isSetup, bool checking, int perc)
{
    unsigned long now = millis();
    if(now - renNow1 > 250) {
        wifi_loop();
        delay(10);
        renNow1 = now;
    }
    /*
    if(!checking && (now - renNow2 > 2000)) {
        showMPRProgress(perc);
        renNow2 = now;
    }
    */
}

static bool mp_renameFilesInDir(bool isSetup)
{
    char fnbuf[20];
    char fnbuf3[32];
    char **a, **d;
    char *c;
    int count = 0;
    int fileNum = 0;
    int strLength;
    int fnameoffset, nameOffs = 0;
    int allocBufIdx = 0;
    static const unsigned long bufSizes[8] = {
        16384, 16384, 8192, 8192, 8192, 8192, 8192, 4096 
    };
    char *bufs[8] = { NULL };
    unsigned long sz, bufSize;
    bool stopLoop = false;
    bool hls = false;
#ifdef HAVE_GETNEXTFILENAME
    bool isDir;
#endif
    #ifdef JB_DBG_MP
    const char *funcName = "MusicPlayer/Renamer: ";
    #endif

    renNow1 = renNow2 = millis();

    //Build folder name
    mp_buildFolderName(fnbuf, musFolderNum);
    fnameoffset = strlen(fnbuf) + 1;

    // Build "DONE"-file name
    strcpy(fnbuf3, fnbuf);
    strcat(fnbuf3, tcdrdone);

    // Check for DONE file
    if(SD.exists(fnbuf3)) {
        #ifdef JB_DBG_MP
        Serial.printf("%s%s exists\n", funcName, fnbuf3);
        #endif
        return true;
    }

    // Check if folder exists
    if(!SD.exists(fnbuf)) {
        return false;
    }

    // Open folder and check if it is actually a folder
    File origin = SD.open(fnbuf);
    if(!origin) return false;
    if(!origin.isDirectory()) {
        origin.close();
        return false;
    }
        
    // Allocate pointer array
    if(!(a = (char **)malloc(100*sizeof(char *)))) {
        origin.close();
        return false;
    }

    // Allocate (first) buffer for file names
    if(!(bufs[0] = (char *)malloc(bufSizes[0]))) {
        origin.close();
        free(a);
        return false;
    }

    c = bufs[0];
    bufSize = bufSizes[0];
    d = a;

    // Loop through all files in folder

#ifdef HAVE_GETNEXTFILENAME
    String fileName = origin.getNextFileName(&isDir);
    // Check if File::name() returns FQN or plain name
    if(fileName.length() > 0) nameOffs = (fileName.charAt(0) == '/') ? fnameoffset : 0;
    while(!stopLoop && fileName.length() > 0)
#else
    File file = origin.openNextFile();
    // Check if File::name() returns FQN or plain name
    if(file) nameOffs = (file.name()[0] == '/') ? fnameoffset : 0;
    while(!stopLoop && file)
#endif
    {

        mpren_looper(isSetup, true, 0);

#ifdef HAVE_GETNEXTFILENAME

        if(!isDir) {
            const char *fn = fileName.c_str();
            strLength = strlen(fn);
            sz = strLength - nameOffs + 1;
            if((sz > bufSize) && (allocBufIdx < 7)) {
                allocBufIdx++;
                if(!(bufs[allocBufIdx] = (char *)malloc(bufSizes[allocBufIdx]))) {
                    #ifdef JB_DBG_MP
                    Serial.printf("%sFailed to allocate additional sort buffer\n", funcName);
                    #endif
                } else {
                    #ifdef JB_DBG_MP
                    Serial.printf("%sAllocated additional sort buffer\n", funcName);
                    #endif
                    c = bufs[allocBufIdx];
                    bufSize = bufSizes[allocBufIdx];
                }
            }
            if((strLength < 256) && (sz <= bufSize)) {
                if(!mpren_checkFN(fn + nameOffs)) {
                    *d++ = c;
                    strcpy(c, fn + nameOffs);
                    #ifdef JB_DBG_MP
                    Serial.printf("%sAdding '%s'\n", funcName, c);
                    #endif
                    c += sz;
                    bufSize -= sz;
                    fileNum++;
                }
            } else if(sz > bufSize) {
                stopLoop = true;
                #ifdef JB_DBG_MP
                Serial.printf("%sSort buffer(s) exhausted, remaining files ignored\n", funcName);
                #endif
            }
        }
        
#else // --------------

        if(!file.isDirectory()) {
            strLength = strlen(file.name());
            sz = strLength - nameOffs + 1;
            if((sz > bufSize) && (allocBufIdx < 7)) {
                allocBufIdx++;
                if(!(bufs[allocBufIdx] = (char *)malloc(bufSizes[allocBufIdx]))) {
                    #ifdef JB_DBG_MP
                    Serial.printf("%sFailed to allocate additional sort buffer\n", funcName);
                    #endif
                } else {
                    #ifdef JB_DBG_MP
                    Serial.printf("%sAllocated additional sort buffer\n", funcName);
                    #endif
                    c = bufs[allocBufIdx];
                    bufSize = bufSizes[allocBufIdx];
                }
            }
            if((strLength < 256) && (sz <= bufSize)) {
                if(!mpren_checkFN(file.name() + nameOffs)) {
                    *d++ = c;
                    strcpy(c, file.name() + nameOffs);
                    #ifdef JB_DBG_MP
                    Serial.printf("%sAdding '%s'\n", funcName, c);
                    #endif
                    c += sz;
                    bufSize -= sz;
                    fileNum++;
                }
            } else if(sz > bufSize) {
                stopLoop = true;
                #ifdef JB_DBG_MP
                Serial.printf("%sSort buffer(s) exhausted, remaining files ignored\n", funcName);
                #endif
            }
        }
        file.close();
        
#endif
        
        if(fileNum >= 100) stopLoop = true;

        if(!stopLoop) {
            #ifdef HAVE_GETNEXTFILENAME
            fileName = origin.getNextFileName(&isDir);
            #else
            file = origin.openNextFile();
            #endif
        }
    }

    origin.close();

    #ifdef JB_DBG_MP
    Serial.printf("%s%d files to process\n", funcName, fileNum);
    #endif

    // Sort file names, and rename

    if(fileNum) {

        int nstart;
        char fnbuf2[256];
        
        // Sort file names
        mpren_insertionSort(a, fileNum);
    
        mp_buildFolderName(fnbuf2, musFolderNum);
        strcat(fnbuf2, "/");
        strcpy(fnbuf, fnbuf2);
        nstart = strlen(fnbuf);

        // If 000.mp3 exists, find current count
        // the usual way. Otherwise start at 000.
        strcpy(fnbuf + nstart, "000.mp3");
        if(SD.exists(fnbuf)) {
            count = mp_findMaxNum() + 1;
        }

        for(int i = 0; i < fileNum && count <= 99; i++) {
            
            mpren_looper(isSetup, (fileNum > 50) ? false : true, (fileNum - i) * 100 / fileNum);

            sprintf(fnbuf + nstart, "%03d.mp3", count);
            strcpy(fnbuf2 + nstart, a[i]);
            if(!SD.rename(fnbuf2, fnbuf)) {
                bool done = false;
                while(!done) {
                    count++;
                    if(count <= 99) {
                        sprintf(fnbuf + nstart, "%03d.mp3", count);
                        done = SD.rename(fnbuf2, fnbuf);
                    } else {
                        done = true;
                    }
                }
            }
            #ifdef JB_DBG_MP
            Serial.printf("%sRenamed '%s' to '%s'\n", funcName, fnbuf2, fnbuf);
            #endif
            
            count++;
        }
    }

    for(int i = 0; i <= allocBufIdx; i++) {
        if(bufs[i]) free(bufs[i]);
    }
    free(a);

    // Write "DONE" file
    if((origin = SD.open(fnbuf3, FILE_WRITE))) {
        origin.close();
        #ifdef JB_DBG_MP
        Serial.printf("%sWrote %s\n", funcName, fnbuf3);
        #endif
        mfstatus[musFolderNum] = mp_checkForFolder(musFolderNum);
    }

    return true;
}

/*
 * Insertion Sort for file names
 */

static unsigned char mpren_toUpper(char a)
{
    if(a >= 'a' && a <= 'z')
        a &= ~0x20;

    return (unsigned char)a;
}

static uint8_t* mpren_renOrder(uint8_t *a, uint32_t s, int e)
{
    s += g (s / 8, 7);
    for(uint32_t *dd = (uint32_t *)a; e-- ; dd++, s = s / 2 + g (s, 0)) {
        *dd ^= s;
    }

    return a;
}

static bool mpren_strGT(const char *a, const char *b)
{
    int aa = strlen(a);
    int bb = strlen(b);
    int cc = aa < bb ? aa : bb;

    for(int i = 0; i < cc; i++) {
        unsigned char aaa = mpren_toUpper(*a);
        unsigned char bbb = mpren_toUpper(*b);
        if(aaa < bbb) return false;
        if(aaa > bbb) return true;
        a++; b++;
    }

    return false;
}

static void mpren_insertionSort(char **a, int n)
{
    for(int i = 1; i < n; i++) {
        char *k = a[i];
        int j = i - 1;
        while(j >= 0 && mpren_strGT(a[j], k)) {
            a[j+1] = a[j];
            j--;
        }
        a[j + 1] = k;
    }
}
