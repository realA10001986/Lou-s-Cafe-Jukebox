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

#ifndef _JB_MAIN_H
#define _JB_MAIN_H

void main_boot();
void main_setup();
void main_loop();

void update_volume();

void rem_vol_chg(int chg);
void rem_vol_set(int v);

void flushDelayedSave();

void showWaitSequence();
void endWaitSequence();
void showCopyError();

void allOff();
void prepareReboot();

void doKeySound(int key);
void doStopKeySound();

bool switchMusicFolder(unsigned int nmf, bool isSetup = false);
//void showMPRProgress(int perc);
void musicPlayCallback();

void updateRemotePlayer(const char *statusJSON);
void mqttConnectCallback();

void fade_loop();

void  mqttFakePowerControl(bool);
void  mqttFakePowerOn();
void  mqttFakePowerOff();

void mydelay(unsigned long mydel);
unsigned long millisNonZero();

void prepareTT();
void wakeup();

void addCmdQueue(uint32_t command);
void bttfn_loop();

struct mp_rem_stat {
    uint32_t validFields;
    int  curTrack;
    int  curLtr, curNum;
    int  volumePerc;
    int  firstTrack;
    int  lastTrack;
    int  maxLtr, maxNum;
    bool playing;
    bool off;
    bool shuffle;
};
#define RMPS_STATE      1
#define RMPS_CURTRCK    2
#define RMPS_CURLN      4
#define RMPS_VOL        8
#define RMPS_FIRST     16
#define RMPS_LAST      32
#define RMPS_SHUFFLE   64
#define RMPS_MAXLN    128

extern bool remIncI;

extern unsigned long powerupMillis;

#define CSF_NM         0x00000001    // DO NOT CHANGE - MUST MATCH BTTFN BITS. Night mode
#define CSF_OFF        0x00000002    // DO NOT CHANGE - MUST MATCH BTTFN BITS. Night mode
#define CSF_NS         0x00000020
#define CSF_NOMUSIC    0x00000100
#define CSF_MQTTPM     0x00001000
#define CSF_MQTTPWROFF 0x00002000
#define CSF_RESTOREFP  0x00008000
#define CSF_BUSY       0x00010000
#define CSF_TT         0x00100000
#define CSF_TTP0       0x00200000
#define CSF_TTP1       0x00400000
#define CSF_TTP2       0x00800000
#define CSF_URotEncVol 0x10000000
#define CSF_URotEncJD  0x20000000
extern uint32_t csf;

extern bool networkTimeTravel;
extern bool networkTCDTT;
extern bool networkReentry;
extern bool networkAbort;
extern bool networkAlarm;
extern uint16_t networkLead;
extern uint16_t networkP1;

extern bool doPrepareTT;
extern bool doWakeup;

extern bool showUpdAvail;

extern int  opMode;
extern const char jbltrs[];
extern int  curStream;
extern int  streamStatus[NUM_STREAMS];
#define STRS_NOFILE   1
#define STRS_OK       0
#define STRS_NOMEM   -1
#define STRS_HTTPERR -2
#define STRS_BAD     -3

extern int     bttfnHaveTCDSSID;
extern char    TCDSSID[];
extern uint8_t TCDpwMarker;

#endif
