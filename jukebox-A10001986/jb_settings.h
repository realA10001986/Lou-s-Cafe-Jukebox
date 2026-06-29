/*
 * -------------------------------------------------------------------
 * A10001986 Crosley CR-9 Jukebox
 * (C) 2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Jukebox
 * https://jukebox.out-a-ti.me
 *
 * Settings & file handling
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

#ifndef _JB_SETTINGS_H
#define _JB_SETTINGS_H

#include <FS.h>

#define MS(s) XMS(s)
#define XMS(s) #s

void unmount_fs();

bool evalBool(char *s);
bool evalBoolSetClear(char *s, uint32_t& ff, uint32_t fl);

void clearWiFiCredentials();

void write_settings();

void settings_setup();

void loadCurVolume();
void storeCurVolume();
void saveCurVolume();

void saveCarMode();

void saveUpdAvail();

void loadUpdVers(int &v, int& r);
void saveUpdVers(int v, int r);

void loadMusFoldNum();
void saveMusFoldNum();

void loadShuffle();
void saveShuffle();

void loadOpMode();
void storeOpMode();
void saveOpMode();

void loadCurStream();
void storeCurStream();
void saveCurStream();

bool loadIpSettings();
void writeIpSettings();
bool deleteIpSettings();

void reInstallFlashFS();
void moveSettings();

bool check_allow_CPA();
bool check_if_default_audio_present();
bool prepareCopyAudioFiles();
void doCopyAudioFiles();
void delete_ID_file();

#define MAX_SIM_UPLOADS 16
#define UPL_OPENERR 1
#define UPL_NOSDERR 2
#define UPL_WRERR   3
#define UPL_BADERR  4
#define UPL_MEMERR  5
#define UPL_UNKNOWN 6
#define UPL_DPLBIN  7
bool   openUploadFile(String& fn, File& file, int idx, bool haveAC, int& opType, int& errNo);
extern uint32_t (*t)(uint8_t *, uint32_t, uint32_t);
size_t writeACFile(File& file, uint8_t *buf, size_t len);
void   closeACFile(File& file);
void   removeACFile(int idx);
void   renameUploadFile(int idx);
char   *getUploadFileName(int idx);
int    getUploadFileNameLen(int idx);
void   freeUploadFileNames();

bool parse_mp_backchannel(const char *msg, struct mp_rem_stat *mp_stat);

// Default settings
#define DEF_HOSTNAME        "jb"
#define DEF_WIFI_RETRY      3     // 1-10; Default: 3 retries
#define DEF_AP_CHANNEL      1     // 1-13; 0 = random(1-13)
#define DEF_WIFI_APOFFDELAY 0     // 0/10-99; Default 0 = Never power down WiFi in AP-mode

#define DEF_PLAY_JB_SND     1     // 1: Play jukebox sounds, 0: do not
#define DEF_PLAY_ALM_SND    0     // 1: Play TCD-alarm sound, 0: do not

#define DEF_TCD_IP          ""    // TCD hostname (or ip address) for BTTFN
#define DEF_USE_NM          0     // 0: Ignore TCD night mode; 1: Follow TCD night mode
#define DEF_NMB             1     // 1: Turn on blue light in NM, 0: Don't
#define DEF_IGN_TT          0     // 1: Ignore TCD-TTs, 0: Do "TT Sequence"

#define DEF_CFG_ON_SD       1     // 1: Save secondary settings on SD, 0: Do not (use internal Flash)
#define DEF_SD_FREQ         0     // 0: SD/SPI frequency: Default 16MHz

#define DEF_WAIT_PWR        1     // 0: Power up directly; 1: Wait for fake-power-on

#define MQ_PLAY 0
#define MQ_STOP 1
#define MQ_NEXT 2
#define MQ_PREV 3
#define MQ_GOTO 4
#define MQ_VOLU 5
#define MQ_VOLD 6
#define MQ_REQS 7
#define MQ_SHU1 8
#define MQ_SHU0 9
#define MQ_NUM  MQ_SHU0 + 1

struct Settings {
    char ssid[34]           = "";
    char pass[66]           = "";
    char bssid[18]          = "";

    char cm_ssid[14]        = "TCD-AP";
    char cm_pass[10]        = "";
    char cm_bssid[18]       = "";

    char hostName[32]       = DEF_HOSTNAME;
    char wifiConRetries[4]  = MS(DEF_WIFI_RETRY);
    char systemID[8]        = "";
    char appw[10]           = "";
    char apChnl[4]          = MS(DEF_AP_CHANNEL);
    char wifiAPOffDelay[4]  = MS(DEF_WIFI_APOFFDELAY);

    char playJB[2]          = MS(DEF_PLAY_JB_SND);
    char playALsnd[2]       = MS(DEF_PLAY_ALM_SND);
    char NMB[2]             = MS(DEF_NMB);

    char tcdIP[32]          = DEF_TCD_IP;
    char useNM[2]           = MS(DEF_USE_NM);
    char ignTT[2]           = MS(DEF_IGN_TT);

    char CfgOnSD[2]         = MS(DEF_CFG_ON_SD);
    //char sdFreq[2]          = MS(DEF_SD_FREQ);

    char waitForPower[2]    = MS(DEF_WAIT_PWR);

    char streams[NUM_STREAMS][128] = { 0 };
    
    char useMQTT[2]         = "0";
    char mqttVers[2]        = "0"; // 0 = 3.1.1, 1 = 5.0
    char mqttServer[80]     = "";  // ip or domain [:port]  
    char mqttUser[64]       = "";  // user[:pass] (UTF8) [limited to 63 bytes through WM]

    char pubMP[2]           = "0"; // 1:Publish music player status to bttf/<hostname>/mpstatus, 0: Don't
    
    char mqttPwr[2]         = "0"; // Do not start with MQTT having control over fake-power
    char mqttPwrOn[2]       = "0"; // Do not wait for POWER_ON at startup

    char mqmpii[2]          = "0"; // Remote player expects/delivers Jukebox track selection including letters "I" and "O"

    char mqmpbackchannel[64] = "";
    char mqmpbc_state_key[16]        = "S";   // key for state
    char mqmpbc_state_val_play[16]   = "P";   // val for state 'playing'; if empty, state key is checked for boolean true
    char mqmpbc_state_val_off[16]    = "O";   // val for state 'off'; if empty, only "playing" and "idle" are assumed/used
    char mqmpbc_curTrack_key[16]     = "C";   // key for current track (number or "Jukebox" [L-N]). If this is unavailable, jog dials and panel LEDs are off.
    char mqmpbc_firstTrack_key[16]   = "F";   // key for first track number (0 or 1), or =0/=1 for substituting with a fixed number
    char mqmpbc_lastTrack_key[16]    = "L";   // key for last track number, or =xxx for substituting with a fixed number or LTR-NUM
    char mqmpbc_volume_key[16]       = "V";   // volume key; integer is interpreted as percentage, float as factor 0.0-1.0
    char mqmpbc_shuffle_key[16]      = "SH";  // key for shuffle mode state
    char mqmpbc_shuffle_val_on[16]   = "1";   // value for "shuffle is on"; if empty, key is checked for boolean true
    
    char mqmpt[MQ_NUM][64]   = { 0 };
    char mqmp[MQ_NUM][64]    = { 0 };

    // Kludge
    char ecmKludge[2]       = "0";  // MUST BE 0
};

struct IPSettings {
    char ip[20]       = "";
    char gateway[20]  = "";
    char netmask[20]  = "";
    char dns[20]      = "";
};

extern struct     Settings    settings;
extern struct     IPSettings  ipsettings;

extern bool       haveFS;
extern bool       haveSD;
extern bool       FlashROMode;
extern bool       configOnSD;
extern const char rspv[];

extern bool       haveAudioFiles;

extern unsigned int musFolderNum;

#endif
