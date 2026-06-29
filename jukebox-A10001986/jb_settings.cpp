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

#include "jb_global.h"

#define ARDUINOJSON_USE_LONG_LONG 0
#define ARDUINOJSON_USE_DOUBLE 0
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 0
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_STD_STREAM 0
#define ARDUINOJSON_ENABLE_STD_STRING 0
#define ARDUINOJSON_ENABLE_NAN 0
#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#define MYNVS LittleFS
#include <LittleFS.h>
#include <Update.h>

#include "jb_settings.h"
#include "jb_audio.h"
#include "jb_main.h"
#include "jb_wifi.h"

// Size of main config JSON
// Needs to be adapted when config grows
#define JSON_SIZE 5000
#if ARDUINOJSON_VERSION_MAJOR >= 7
#define DECLARE_S_JSON(x,n) JsonDocument n;
#define DECLARE_D_JSON(x,n) JsonDocument n;
#else
#define DECLARE_S_JSON(x,n) StaticJsonDocument<x> n;
#define DECLARE_D_JSON(x,n) DynamicJsonDocument n(x);
#endif 

#define NUM_AUDIOFILES 28
#define AC_FMTV 2
#define AC_OHSZ (14 + ((NUM_AUDIOFILES+1)*(32+4)))
#define SND_REQ_VERSION "JB01"
#define AC_TS 868924

// Secondary settings
// Do not change or insert new values, this
// struct is saved as such. Append new stuff.
static struct [[gnu::packed]] {
    uint8_t curVolume       = 8;
    uint8_t showUpdAvail    = 1;
    uint8_t updateV         = 0;
    uint8_t updateR         = 0;
    uint8_t carMode         = 0;
} secSettings;

// Tertiary settings (SD only)
// Do not change or insert new values, this
// struct is saved as such. Append new stuff.
static struct [[gnu::packed]] {
    uint8_t musFolderNum    = 0;
    uint8_t mpShuffle       = 0;
    uint8_t opMode          = 0;
    uint8_t curStream       = 0;
    uint8_t curVolume       = 8;
} terSettings;

static int      secSetValidBytes = 0;
static uint32_t secSettingsHash  = 0;
static bool     haveSecSettings  = false;
static int      terSetValidBytes = 0;
static uint32_t terSettingsHash  = 0;
static bool     haveTerSettings  = false;

static uint32_t mainConfigHash = 0;
static uint32_t ipHash = 0;

static const char *CONFN  = "/JBA.bin";
static const char *CONFND = "/JBA.old";
static const char *CONID  = "JBAA";
const  char       rspv[]  = SND_REQ_VERSION;
static uint32_t   soa = AC_TS;
static bool       ic = false;
static uint8_t*   f(uint8_t *d, uint32_t m, int y) { return d; }
static char       *uploadFileNames[MAX_SIM_UPLOADS] = { NULL };
static char       *uploadRealFileNames[MAX_SIM_UPLOADS] = { NULL };

static const char *cfgName     = "/jbconfig.json"; // Main config (flash)
static const char *ipCfgName   = "/jbipcfg";    // IP config (flash)
static const char *secCfgName  = "/jb2cfg";     // Secondary settings (flash/SD)
static const char *terCfgName  = "/jb3cfg";     // Tertiary settings (SD)

static const char fwfn[]    = "/jbfw.bin";
static const char fwfnold[] = "/jbfw.old";

static const char *fsNoAvail     = "File System not available";
static const char *failFileWrite = "Failed to open file for writing";
#ifdef JB_DBG_BOOT
static const char *badConfig     = "Settings bad/missing/incomplete; writing new file";
#endif

// If LittleFS/SPIFFS is mounted
bool haveFS = false;

// If a SD card is found
bool haveSD = false;

// Save sedondary settings on SD?
bool configOnSD = false;

// Paranoia: No writes Flash-FS
bool FlashROMode = false;

// If SD contains default audio files
static bool allowCPA = false;

// If current audio data is installed
bool haveAudioFiles = false;

// Music Folder Number
unsigned int musFolderNum = 0;

static void loadCarMode();
static void loadUpdAvail();

static uint8_t* (*r)(uint8_t *, uint32_t, int);

static bool audio_files_present(int& alienVER);

static void firmware_update();

// Helpers from tc_keypad
extern void start_file_copy();
extern void file_copy_progress(uint32_t ts, uint32_t tw);
extern void file_copy_done(int err);

/*
 * Format Flash FS
 */

static bool formatFlashFS(bool userSignal)
{
    bool ret = false;

    if(userSignal) {
        // Show the user some action
    } else {
        #ifdef JB_DBG_BOOT
        Serial.println("Formatting flash FS");
        #endif
    }

    MYNVS.format();
    if(MYNVS.begin()) ret = true;

    if(userSignal) {
        //
    }

    return ret;
}

/*
 * Unmount filesystems
 */

void unmount_fs()
{
    if(haveFS) {
        MYNVS.end();
        #ifdef JB_DBG_GEN
        Serial.println("Unmounted Flash FS");
        #endif
        haveFS = false;
    }
    if(haveSD) {
        SD.end();
        #ifdef JB_DBG_GEN
        Serial.println("Unmounted SD card");
        #endif
        haveSD = false;
    }
}


/*
 * Generic file readers/writers
 */

static bool readFile(File& myFile, uint8_t *buf, int len)
{
    if(myFile) {
        size_t bytesr = myFile.read(buf, len);
        myFile.close();
        return (bytesr == len);
    } else
        return false;
}

static bool readFileU(File& myFile, uint8_t*& buf, int& len)
{
    if(myFile) {
        len = myFile.size();
        buf = (uint8_t *)malloc(len+1);
        if(buf) {
            buf[len] = 0;
            return readFile(myFile, buf, len);
        } else {
            myFile.close();
        }
    }
    return false;
}

// Read file of unknown size from SD
static bool readFileFromSDU(const char *fn, uint8_t*& buf, int& len)
{   
    if(!haveSD)
        return false;

    File myFile = SD.open(fn, FILE_READ);
    return readFileU(myFile, buf, len);
}

// Read file of unknown size from NVS
static bool readFileFromFSU(const char *fn, uint8_t*& buf, int& len)
{   
    if(!haveFS || !MYNVS.exists(fn))
        return false;

    File myFile = MYNVS.open(fn, FILE_READ);
    return readFileU(myFile, buf, len);
}

// Read file of known size from SD
static bool readFileFromSD(const char *fn, uint8_t *buf, int len)
{   
    if(!haveSD)
        return false;

    File myFile = SD.open(fn, FILE_READ);
    return readFile(myFile, buf, len);
}

// Read file of known size from NVS
static bool readFileFromFS(const char *fn, uint8_t *buf, int len)
{
    if(!haveFS || !MYNVS.exists(fn))
        return false;

    File myFile = MYNVS.open(fn, FILE_READ);
    return readFile(myFile, buf, len);
}

static bool writeFile(File& myFile, uint8_t *buf, int len)
{
    if(myFile) {
        size_t bytesw = myFile.write(buf, len);
        myFile.close();
        return (bytesw == len);
    } else
        return false;
}

// Write file to SD
static bool writeFileToSD(const char *fn, uint8_t *buf, int len)
{
    if(!haveSD)
        return false;

    File myFile = SD.open(fn, FILE_WRITE);
    return writeFile(myFile, buf, len);
}

// Write file to NVS
static bool writeFileToFS(const char *fn, uint8_t *buf, int len)
{
    if(!haveFS)
        return false;

    File myFile = MYNVS.open(fn, FILE_WRITE);
    return writeFile(myFile, buf, len);
}

/*
 * Config file handling
 */

static uint8_t cfChkSum(const uint8_t *buf, int len)
{
    uint16_t s = 0;
    while(len--) {
        s += *buf++;
    }
    s = (s >> 8) + (s & 0xff);
    s += (s >> 8);
    return (uint8_t)(~s);
}

static bool loadConfigFile(const char *fn, uint8_t *buf, int len, int& validBytes, int forcefs = 0)
{
    bool haveConfigFile = false;
    int fl;
    uint8_t *bbuf = NULL;

    // forcefs: > 0: SD only; = 0 either (configOnSD); < 0: Flash if !FlashROMode, SD if FlashROMode

    if(haveSD && ((!forcefs && configOnSD) || forcefs > 0 || (forcefs < 0 && FlashROMode))) {
        haveConfigFile = readFileFromSDU(fn, bbuf, fl);
    }
    if(!haveConfigFile && haveFS && (!forcefs || (forcefs < 0 && !FlashROMode))) {
        haveConfigFile = readFileFromFSU(fn, bbuf, fl);
    }
    if(haveConfigFile) {
        uint8_t chksum = cfChkSum(bbuf, fl - 1);
        if((haveConfigFile = (bbuf[fl - 1] == chksum))) {
            validBytes = bbuf[0] | (bbuf[1] << 8);
            memcpy(buf, bbuf + 2, min(len, validBytes));
            haveConfigFile = true; //(len <= validBytes);
            #ifdef JB_DBG_BOOT
            Serial.printf("loadConfigFile: loaded %s: need %d, got %d bytes: ", fn, len, validBytes);
            for(int k = 0; k < len; k++) Serial.printf("%02x ", buf[k]);
            Serial.printf("chksum %02x\n", chksum);
            #endif
        } else {
            #ifdef JB_DBG_BOOT
            Serial.printf("loadConfigFile: Bad checksum %02x %02x\n", chksum, bbuf[fl - 1]);
            #endif
        }
    }

    if(bbuf) free(bbuf);

    return haveConfigFile;
}

static bool saveConfigFile(const char *fn, uint8_t *buf, int len, int forcefs = 0)
{
    uint8_t *bbuf;
    bool ret = false;

    if(!(bbuf = (uint8_t *)malloc(len + 3)))
        return false;

    bbuf[0] = len & 0xff;
    bbuf[1] = len >> 8;
    memcpy(bbuf + 2, buf, len);
    bbuf[len + 2] = cfChkSum(bbuf, len + 2);
    
    #ifdef JB_DBG_BOOT
    Serial.printf("saveConfigFile: %s: ", fn);
    for(int k = 0; k < len + 3; k++) Serial.printf("0x%02x ", bbuf[k]);
    Serial.println("");
    #endif

    if((!forcefs && configOnSD) || forcefs > 0 || (forcefs < 0 && FlashROMode)) {
        ret = writeFileToSD(fn, bbuf, len + 3);
    } else if(haveFS) {
        ret = writeFileToFS(fn, bbuf, len + 3);
    }

    free(bbuf);

    return ret;
}

static uint32_t calcHash(uint8_t *buf, int len)
{
    uint32_t hash = 2166136261UL;
    for(int i = 0; i < len; i++) {
        hash = (hash ^ buf[i]) * 16777619;
    }
    return hash;
}

static bool saveSecSettings(bool useCache)
{
    uint32_t oldHash = secSettingsHash;

    secSettingsHash = calcHash((uint8_t *)&secSettings, sizeof(secSettings));
    
    if(useCache) {
        if(oldHash == secSettingsHash) {
            #ifdef JB_DBG_BOOT
            Serial.printf("saveSecSettings: Data up to date, not writing (%x)\n", secSettingsHash);
            #endif
            return true;
        }
    }
    
    return saveConfigFile(secCfgName, (uint8_t *)&secSettings, sizeof(secSettings), 0);
}

static bool saveTerSettings(bool useCache)
{
    if(!haveSD)
        return false;

    uint32_t oldHash = terSettingsHash;
    
    terSettingsHash = calcHash((uint8_t *)&terSettings, sizeof(terSettings));
    
    if(useCache) {
        if(oldHash == terSettingsHash) {
            #ifdef JB_DBG_BOOT
            Serial.printf("saveTerSettings: Data up to date, not writing (%x)\n", terSettingsHash);
            #endif
            return true;
        }
    }
    
    return saveConfigFile(terCfgName, (uint8_t *)&terSettings, sizeof(terSettings), 1);
}

#ifdef SETTINGS_TRANSITION
static void removeOldFiles(const char *oldfn)
{
    if(haveSD) SD.remove(oldfn);
    if(haveFS) MYNVS.remove(oldfn);
    #ifdef JB_DBG_BOOT
    Serial.printf("removeOldFiles: Removing %s\n", oldfn);
    #endif
}
#endif

/*
 * Helpers for JSON config files
 */

static DeserializationError readJSONCfgFile(JsonDocument& json, File& configFile, uint32_t *readHash = NULL)
{
    const char *buf = NULL;   // const to avoid ArduinoJSON's "zero-copy mode".
    size_t bufSize = configFile.size();
    DeserializationError ret;

    if(!(buf = (const char *)malloc(bufSize + 1))) {
        Serial.printf("rJSON: malloc failed (%d)\n", bufSize);
        return DeserializationError::NoMemory;
    }

    memset((void *)buf, 0, bufSize + 1);

    configFile.read((uint8_t *)buf, bufSize);

    #ifdef JB_DBG_BOOT
    Serial.println(buf);
    #endif

    if(readHash) {
        *readHash = calcHash((uint8_t *)buf, bufSize);
    }
    
    ret = deserializeJson(json, buf);

    free((void *)buf);

    return ret;
}

static bool writeJSONCfgFile(const JsonDocument& json, const char *fn, bool useSD, uint32_t oldHash = 0, uint32_t *newHash = NULL)
{
    char *buf;
    size_t bufSize = measureJson(json);
    bool success = false;

    if(!(buf = (char *)malloc(bufSize + 1))) {
        Serial.printf("wJSON: malloc failed (%d) (%s)\n", bufSize, fn);
        return false;
    }

    memset(buf, 0, bufSize + 1);
    serializeJson(json, buf, bufSize);

    #ifdef JB_DBG_BOOT
    Serial.printf("Writing %s to %s, %d bytes\n", fn, useSD ? "SD" : "FS", bufSize);
    Serial.println((const char *)buf);
    #endif

    if(oldHash || newHash) {
        uint32_t newH = calcHash((uint8_t *)buf, bufSize);
        
        if(newHash) *newHash = newH;
    
        if(oldHash) {
            if(oldHash == newH) {
                #ifdef JB_DBG_BOOT
                Serial.printf("Not writing %s, hash identical (%x)\n", fn, oldHash);
                #endif
                free(buf);
                return true;
            }
        }
    }

    if(useSD) {
        success = writeFileToSD(fn, (uint8_t *)buf, (int)bufSize);
    } else {
        success = writeFileToFS(fn, (uint8_t *)buf, (int)bufSize);
    }

    free(buf);

    if(!success) {
        Serial.printf("wJSON: %s - %s\n", fn, failFileWrite);
    }

    return success;
}

/*
 *  Helpers for parm copying & checking
 */

static bool checkValidNumParm(char *text, int lowerLim, int upperLim, int setDefault)
{
    int i, len = strlen(text);
    bool ret = false;

    if(!len) {
        i = setDefault;
        ret = true;
    } else {
        for(int j = 0; j < len; j++) {
            if(text[j] < '0' || text[j] > '9') {
                i = setDefault;
                ret = true;
                break;
            }
        }
        if(!ret) {
            i = atoi(text);   
            if(i < lowerLim) {
                i = lowerLim;
                ret = true;
            } else if(i > upperLim) {
                i = upperLim;
                ret = true;
            }
        }
    }

    // Re-do to get rid of formatting errors (eg "000")
    sprintf(text, "%d", i);

    return ret;
}

static bool checkValidNumParmF(char *text, float lowerLim, float upperLim, float setDefault)
{
    int i, len = strlen(text);
    bool ret = false;
    float f;

    if(!len) {
        f = setDefault;
        ret = true;
    } else {
        for(i = 0; i < len; i++) {
            if(text[i] != '.' && text[i] != '-' && (text[i] < '0' || text[i] > '9')) {
                f = setDefault;
                ret = true;
                break;
            }
        }
        if(!ret) {
            f = strtof(text, NULL);
            if(f < lowerLim) {
                f = lowerLim;
                ret = true;
            } else if(f > upperLim) {
                f = upperLim;
                ret = true;
            }
        }
    }
    // Re-do to get rid of formatting errors (eg "0.")
    sprintf(text, "%.1f", f);

    return ret;
}

static bool CopyTextParm(const char *json, char *setting, int setSize)
{
    if(!json) return true;
    
    memset(setting, 0, setSize);
    strncpy(setting, json, setSize - 1);
    return false;
}

static bool CopyCheckValidNumParm(const char *json, char *text, int psize, int lowerLim, int upperLim, int setDefault)
{
    if(!json) return true;

    memset(text, 0, psize);
    strncpy(text, json, psize-1);
    return checkValidNumParm(text, lowerLim, upperLim, setDefault);
}

static bool CopyCheckValidNumParmF(const char *json, char *text, int psize, float lowerLim, float upperLim, float setDefault)
{
    if(!json) return true;

    memset(text, 0, psize);
    strncpy(text, json, psize-1);
    return checkValidNumParmF(text, lowerLim, upperLim, setDefault);
}

bool evalBool(char *s)
{
    if(*s == '0') return false;
    return true;
}

bool evalBoolSetClear(char *s, uint32_t& ff, uint32_t fl)
{
    if(*s == '0') {
        ff &= ~fl;
        return false;
    }
    ff |= fl;
    return true;
}

void clearWiFiCredentials()
{
    memset(settings.ssid, 0, sizeof(settings.ssid));
    memset(settings.pass, 0, sizeof(settings.pass));
    memset(settings.bssid, 0, sizeof(settings.bssid));
}

static bool read_settings(File configFile, int cfgReadCount)
{
    #ifdef JB_DBG_BOOT
    const char *funcName = "read_settings";
    #endif
    bool wd = false;
    size_t jsonSize = 0;
    char jid[8];
    
    DECLARE_D_JSON(JSON_SIZE,json);

    DeserializationError error = readJSONCfgFile(json, configFile, &mainConfigHash);

    #if ARDUINOJSON_VERSION_MAJOR < 7
    jsonSize = json.memoryUsage();
    if(jsonSize > JSON_SIZE) {
        Serial.printf("ERROR: Config too large (%d vs %d)\n", jsonSize, JSON_SIZE);
    }
    
    #ifdef JB_DBG_BOOT
    if(jsonSize > JSON_SIZE - 256) {
          Serial.printf("%s: WARNING: JSON_SIZE needs to be adapted **************\n", funcName);
    }
    Serial.printf("%s: Size of document: %d (JSON_SIZE %d)\n", funcName, jsonSize, JSON_SIZE);
    #endif
    #endif

    if(!error) {

        // WiFi Configuration

        if(!cfgReadCount) {
            clearWiFiCredentials();
        }

        if(json["ssid"]) {
            clearWiFiCredentials();
            strncpy(settings.ssid, json["ssid"], sizeof(settings.ssid) - 1);
            if(json["pass"]) {
                strncpy(settings.pass, json["pass"], sizeof(settings.pass) - 1);
            }
            if(json["bssid"]) {
                strncpy(settings.bssid, json["bssid"], sizeof(settings.bssid) - 1);
            }
        } else {
            if(!cfgReadCount) {
                // Set a marker for "no ssid tag in config file", ie read from NVS.
                settings.ssid[1] = 'X';
            } else if(settings.ssid[0] || settings.ssid[1] != 'X') {
                // FlashRO: If flash-config didn't set the marker, write new file 
                // with ssid/pass from flash-config
                wd = true;
            }
        }

        wd |= CopyTextParm(json["cmsid"], settings.cm_ssid, sizeof(settings.cm_ssid));
        wd |= CopyTextParm(json["cmpwd"], settings.cm_pass, sizeof(settings.cm_pass));
        wd |= CopyTextParm(json["cmbid"], settings.cm_bssid, sizeof(settings.cm_bssid));

        wd |= CopyTextParm(json["hn"], settings.hostName, sizeof(settings.hostName));
        
        wd |= CopyCheckValidNumParm(json["wCR"], settings.wifiConRetries, sizeof(settings.wifiConRetries), 1, 10, DEF_WIFI_RETRY);

        wd |= CopyTextParm(json["sID"], settings.systemID, sizeof(settings.systemID));
        wd |= CopyTextParm(json["appw"], settings.appw, sizeof(settings.appw));
        wd |= CopyCheckValidNumParm(json["apch"], settings.apChnl, sizeof(settings.apChnl), 0, 11, DEF_AP_CHANNEL);
        wd |= CopyCheckValidNumParm(json["wAOD"], settings.wifiAPOffDelay, sizeof(settings.wifiAPOffDelay), 0, 99, DEF_WIFI_APOFFDELAY);

        // Settings

        wd |= CopyCheckValidNumParm(json["playJB"], settings.playJB, sizeof(settings.playJB), 0, 1, DEF_PLAY_JB_SND);
        wd |= CopyCheckValidNumParm(json["playALsnd"], settings.playALsnd, sizeof(settings.playALsnd), 0, 1, DEF_PLAY_ALM_SND);
        wd |= CopyCheckValidNumParm(json["NMB"], settings.NMB, sizeof(settings.NMB), 0, 1, DEF_NMB);

        wd |= CopyTextParm(json["tcdIP"], settings.tcdIP, sizeof(settings.tcdIP));
        wd |= CopyCheckValidNumParm(json["useNM"], settings.useNM, sizeof(settings.useNM), 0, 1, DEF_USE_NM);
        wd |= CopyCheckValidNumParm(json["ignTT"], settings.ignTT, sizeof(settings.ignTT), 0, 1, DEF_IGN_TT);

        wd |= CopyCheckValidNumParm(json["CoSD"], settings.CfgOnSD, sizeof(settings.CfgOnSD), 0, 1, DEF_CFG_ON_SD);
        //wd |= CopyCheckValidNumParm(json["sdFreq"], settings.sdFreq, sizeof(settings.sdFreq), 0, 1, DEF_SD_FREQ);
        
        wd |= CopyCheckValidNumParm(json["wPwr"], settings.waitForPower, sizeof(settings.waitForPower), 0, 1, DEF_WAIT_PWR);

        for(int i = 0; i < NUM_STREAMS; i++) {
            sprintf(jid, "str%d", i);
            memset(settings.streams[i], 0, sizeof(settings.streams[0]));
            if(json[jid]) {
                strncpy(settings.streams[i], json[jid], sizeof(settings.streams[0]));
            }
        }
        
        wd |= CopyCheckValidNumParm(json["uMQTT"], settings.useMQTT, sizeof(settings.useMQTT), 0, 1, 0);
        wd |= CopyTextParm(json["mqttS"], settings.mqttServer, sizeof(settings.mqttServer));
        wd |= CopyCheckValidNumParm(json["mqttV"], settings.mqttVers, sizeof(settings.mqttVers), 0, 1, 0);
        wd |= CopyTextParm(json["mqttU"], settings.mqttUser, sizeof(settings.mqttUser));

        wd |= CopyCheckValidNumParm(json["pMP"], settings.pubMP, sizeof(settings.pubMP), 0, 1, 0);
        
        wd |= CopyCheckValidNumParm(json["mqP"], settings.mqttPwr, sizeof(settings.mqttPwr), 0, 1, 0);
        wd |= CopyCheckValidNumParm(json["mqPO"], settings.mqttPwrOn, sizeof(settings.mqttPwrOn), 0, 1, 0);

        wd |= CopyCheckValidNumParm(json["mqII"], settings.mqmpii, sizeof(settings.mqmpii), 0, 1, 1);
        
        wd |= CopyTextParm(json["mqmpBC"], settings.mqmpbackchannel, sizeof(settings.mqmpbackchannel));

        wd |= CopyTextParm(json["mqmpSK"], settings.mqmpbc_state_key, sizeof(settings.mqmpbc_state_key));
        wd |= CopyTextParm(json["mqmpSV"], settings.mqmpbc_state_val_play, sizeof(settings.mqmpbc_state_val_play));
        wd |= CopyTextParm(json["mqmpSO"], settings.mqmpbc_state_val_off, sizeof(settings.mqmpbc_state_val_off));
        wd |= CopyTextParm(json["mqmpC"], settings.mqmpbc_curTrack_key, sizeof(settings.mqmpbc_curTrack_key));
        wd |= CopyTextParm(json["mqmpF"], settings.mqmpbc_firstTrack_key, sizeof(settings.mqmpbc_firstTrack_key));
        wd |= CopyTextParm(json["mqmpL"], settings.mqmpbc_lastTrack_key, sizeof(settings.mqmpbc_lastTrack_key));
        wd |= CopyTextParm(json["mqmpV"], settings.mqmpbc_volume_key, sizeof(settings.mqmpbc_volume_key));
        wd |= CopyTextParm(json["mqmpSHK"], settings.mqmpbc_shuffle_key, sizeof(settings.mqmpbc_shuffle_key));
        wd |= CopyTextParm(json["mqmpSHV"], settings.mqmpbc_shuffle_val_on, sizeof(settings.mqmpbc_shuffle_val_on));
        
        for(int i = 0; i < MQ_NUM; i++) {
            memset(settings.mqmpt[i], 0, sizeof(settings.mqmpt[0]));
            memset(settings.mqmp[i], 0, sizeof(settings.mqmp[0]));
            sprintf(jid, "mqmpt%d", i);
            if(json[jid]) {
                strncpy(settings.mqmpt[i], json[jid], sizeof(settings.mqmpt[0]));
            }
            sprintf(jid, "mqmp%d", i);
            if(json[jid]) {
                strncpy(settings.mqmp[i], json[jid], sizeof(settings.mqmp[0]));
            }
        }
        
    } else {

        wd = true;

    }

    return wd;
}

void write_settings()
{
    char jid[12];
    #ifdef JB_DBG_BOOT
    const char *funcName = "write_settings";
    #endif
    DECLARE_D_JSON(JSON_SIZE,json);

    if(!haveFS && !FlashROMode) {
        Serial.printf("%s\n", fsNoAvail);
        return;
    }

    #ifdef JB_DBG_BOOT
    Serial.printf("%s: Writing config file\n", funcName);
    #endif

    // Write this only if either set, or also present in file read earlier
    if(settings.ssid[0] || settings.ssid[1] != 'X') {
        json["ssid"] = (const char *)settings.ssid;
        json["pass"] = (const char *)settings.pass;
        json["bssid"] = (const char *)settings.bssid;
    }

    json["cmsid"] = (const char *)settings.cm_ssid;
    json["cmpwd"] = (const char *)settings.cm_pass;
    json["cmbid"] = (const char *)settings.cm_bssid;

    json["hn"] = (const char *)settings.hostName;
    json["wCR"] = (const char *)settings.wifiConRetries;

    json["sID"] = (const char *)settings.systemID;
    json["appw"] = (const char *)settings.appw;
    json["apch"] = (const char *)settings.apChnl;
    json["wAOD"] = (const char *)settings.wifiAPOffDelay;

    json["playJB"] = (const char *)settings.playJB;
    json["playALsnd"] = (const char *)settings.playALsnd;
    json["NMB"] = (const char *)settings.NMB;

    json["tcdIP"] = (const char *)settings.tcdIP;
    json["useNM"] = (const char *)settings.useNM;
    json["ignTT"] = (const char *)settings.ignTT;

    json["CoSD"] = (const char *)settings.CfgOnSD;
    //json["sdFreq"] = (const char *)settings.sdFreq;

    json["wPwr"] = (const char *)settings.waitForPower;

    for(int i = 0; i < NUM_STREAMS; i++) {
        sprintf(jid, "str%d", i);
        json[jid] = (const char *)settings.streams[i];
    }

    json["uMQTT"] = (const char *)settings.useMQTT;
    json["mqttS"] = (const char *)settings.mqttServer;
    json["mqttV"] = (const char *)settings.mqttVers;
    json["mqttU"] = (const char *)settings.mqttUser;
    
    json["pMP"] = (const char *)settings.pubMP;
    
    json["mqP"] = (const char *)settings.mqttPwr;
    json["mqPO"] = (const char *)settings.mqttPwrOn;

    json["mqII"] = (const char *)settings.mqmpii;
    
    json["mqmpBC"] = (const char *)settings.mqmpbackchannel;
    
    json["mqmpSK"] = (const char *)settings.mqmpbc_state_key;
    json["mqmpSV"] = (const char *)settings.mqmpbc_state_val_play;
    json["mqmpSO"] = (const char *)settings.mqmpbc_state_val_off;
    json["mqmpC"] = (const char *)settings.mqmpbc_curTrack_key;
    json["mqmpF"] = (const char *)settings.mqmpbc_firstTrack_key;
    json["mqmpL"] = (const char *)settings.mqmpbc_lastTrack_key;
    json["mqmpV"] = (const char *)settings.mqmpbc_volume_key;
    json["mqmpSHK"] = (const char *)settings.mqmpbc_shuffle_key;
    json["mqmpSHV"] = (const char *)settings.mqmpbc_shuffle_val_on;
    
    for(int i = 0; i < MQ_NUM; i++) {
        sprintf(jid, "mqmpt%d", i);
        json[jid] = (const char *)settings.mqmpt[i];
        sprintf(jid, "mqmp%d", i);
        json[jid] = (const char *)settings.mqmp[i];
    }

    writeJSONCfgFile(json, cfgName, FlashROMode, mainConfigHash, &mainConfigHash);
}

/*
 * settings_setup()
 * 
 * Mount LittleFS/SPIFFS and SD (if available).
 * Read configuration from JSON config file
 * If config file not found, create one with default settings
 *
 * If the device is powered on or reset while ENTER is held down, 
 * the IP settings file will be deleted and the device will use DHCP.
 */

void settings_setup()
{
    #ifdef JB_DBG_BOOT
    const char *funcName = "settings_setup";
    #endif
    bool writedefault = false;
    bool freshFS = false;
    bool SDres = false;
    int alienVER = -1;
    int cfgReadCount = 0;

    #ifdef JB_DBG_BOOT
    Serial.printf("%s: Mounting flash FS... ", funcName);
    #endif

    if(MYNVS.begin()) {

        haveFS = true;

    } else {

        #ifdef JB_DBG_BOOT
        Serial.print("failed, formatting... ");
        #endif

        haveFS = formatFlashFS(true);
        freshFS = true;

    }

    if(haveFS) {

        #ifdef JB_DBG_BOOT
        Serial.printf("ok.\nFlashFS: %d total, %d used, %d free\n", MYNVS.totalBytes(), MYNVS.usedBytes(), MYNVS.totalBytes() - MYNVS.usedBytes());
        #endif
        
        if(MYNVS.exists(cfgName)) {
            File configFile = MYNVS.open(cfgName, "r");
            if(configFile) {
                writedefault = read_settings(configFile, cfgReadCount);
                cfgReadCount++;
                configFile.close();
            } else {
                writedefault = true;
            }
        } else {
            writedefault = true;
        }

        // Write new config file after mounting SD and determining FlashROMode

    } else {

        Serial.println("failed.\n*** Mounting flash FS failed. Using SD (if available)");

    }
    
    // Set up SD card
    SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);

    haveSD = false;

    //uint32_t sdfreq = (settings.sdFreq[0] == '0') ? 16000000 : 4000000;
    uint32_t sdfreq = 16000000;
    #ifdef JB_DBG_BOOT
    Serial.printf("%s: SD/SPI frequency %dMHz\n", funcName, sdfreq / 1000000);
    #endif

    #ifdef JB_DBG_BOOT
    Serial.printf("%s: Mounting SD... ", funcName);
    #endif

    if(!(SDres = SD.begin(SD_CS_PIN, SPI, sdfreq))) {
        #ifdef JB_DBG_BOOT
        Serial.printf("Retrying at 25Mhz... ");
        #endif
        SDres = SD.begin(SD_CS_PIN, SPI, 25000000);
    }

    if(SDres) {

        #ifdef JB_DBG_BOOT
        Serial.println("ok");
        #endif

        uint8_t cardType = SD.cardType();
       
        #ifdef JB_DBG_BOOT
        const char *sdTypes[5] = { "No card", "MMC", "SD", "SDHC", "unknown (SD not usable)" };
        Serial.printf("SD card type: %s\n", sdTypes[cardType > 4 ? 4 : cardType]);
        #endif

        haveSD = ((cardType != CARD_NONE) && (cardType != CARD_UNKNOWN));

    }

    if(haveSD) {

        firmware_update();

        if(SD.exists("/JB_FLASH_RO") || !haveFS) {
            bool writedefault2 = true;
            FlashROMode = true;
            Serial.println("Flash-RO mode: Using SD only.");
            if(SD.exists(cfgName)) {
                File configFile = SD.open(cfgName, "r");
                if(configFile) {
                    writedefault2 = read_settings(configFile, cfgReadCount);
                    configFile.close();
                }
            }
            if(writedefault2) {
                #ifdef JB_DBG_BOOT
                Serial.printf("%s: %s\n", funcName, badConfig);
                #endif
                mainConfigHash = 0;
                write_settings();
            }
        }

    } else {
        Serial.println("No SD card found");
    }

    // Check if (current) audio data is installed
    haveAudioFiles = audio_files_present(alienVER);

    // Re-format flash FS if either alien VER found, or
    // neither VER nor our config file exist.
    // (Note: LittleFS crashes when flash FS is full.)
    if(!haveAudioFiles && haveFS && !FlashROMode) {
        if((alienVER > 0) || 
           (alienVER < 0 && !freshFS && !cfgReadCount)) {
            #ifdef JB_DBG_BOOT
            Serial.printf("Reformatting. Alien VER: %d, used space %d", alienVER, MYNVS.usedBytes());
            #endif
            writedefault = true;
            formatFlashFS(true);
        }
    }

    // Now write new config to flash FS if old one somehow bad
    // Only write this file if FlashROMode is off
    if(haveFS && writedefault && !FlashROMode) {
        #ifdef JB_DBG_BOOT
        Serial.printf("%s: %s\n", funcName, badConfig);
        #endif
        mainConfigHash = 0;
        write_settings();
    }

    // Determine if secondary settings are to be stored on SD
    configOnSD = (haveSD && (evalBool(settings.CfgOnSD) || FlashROMode));

    // Load secondary config file
    if(loadConfigFile(secCfgName, (uint8_t *)&secSettings, sizeof(secSettings), secSetValidBytes)) {
        secSettingsHash = calcHash((uint8_t *)&secSettings, sizeof(secSettings));
        haveSecSettings = true;
    }

    // Load tertiary config file (SD only)
    if(haveSD) {
        if(loadConfigFile(terCfgName, (uint8_t *)&terSettings, sizeof(terSettings), terSetValidBytes, 1)) {
            terSettingsHash = calcHash((uint8_t *)&terSettings, sizeof(terSettings));
            haveTerSettings = true;
        }
    }

    // Load car mode
    if(*settings.cm_ssid) {
        loadCarMode();
    }

    loadUpdAvail();
    
    // Check if SD contains the default sound files
    if((r  = m) && haveSD && (FlashROMode || haveFS)) {
        allowCPA = check_if_default_audio_present();
    }

    for(int i = 0; i < MAX_SIM_UPLOADS; i++) {
        uploadFileNames[i] = uploadRealFileNames[i] = NULL;
    }

}

/*
 *  Load/save audio volume
 */

void loadCurVolume()
{
    if(haveTerSettings) {
        #ifdef JB_DBG_BOOT
        Serial.println("loadCurVolume: extracting from terSettings");
        #endif
        if(terSettings.curVolume < VOL_LEVELS) {
            aud_state.curVolume = terSettings.curVolume;
        }
    } else if(haveSecSettings) {
        #ifdef JB_DBG_BOOT
        Serial.println("loadCurVolume: extracting from secSettings");
        #endif
        if(secSettings.curVolume < VOL_LEVELS) {
            aud_state.curVolume = secSettings.curVolume;
        }
    }
}

void storeCurVolume()
{
    // Used to keep secSettings up-to-date in case
    // of delayed save
    secSettings.curVolume = terSettings.curVolume = aud_state.curVolume;
}

void saveCurVolume()
{
    storeCurVolume();
    if(haveSD) {
        saveTerSettings(true);
    } else {
        saveSecSettings(true);
    }
}

/*
 *  Load/save carMode
 */

static void loadCarMode()
{
    if(haveSecSettings) {
        carMode = !!secSettings.carMode;
    }
}

void saveCarMode()
{
    secSettings.carMode = carMode ? 1 : 0;
    saveSecSettings(true);
}


/*
 *  Load/save "show update notification at boot"
 */

static void loadUpdAvail()
{
    if(haveSecSettings) {
        showUpdAvail = !!secSettings.showUpdAvail;
    }
}

void saveUpdAvail()
{
    secSettings.showUpdAvail = showUpdAvail ? 1 : 0;
    saveSecSettings(true);
}

/*
 *  Load/save curr version
 */

void loadUpdVers(int &v, int& r)
{
    if(haveSecSettings) {
        v = secSettings.updateV;
        r = secSettings.updateR;
    } else {
        v = r = 0;
    }
}

void saveUpdVers(int v, int r)
{
    secSettings.updateV = v;
    secSettings.updateR = r;
    saveSecSettings(true);
}

/*
 * Load/save Music Folder Number
 */

void loadMusFoldNum()
{
    if(!haveSD)
        return;

    if(haveTerSettings) {
        #ifdef JB_DBG_BOOT
        Serial.println("loadMusFoldNum: extracting from terSettings");
        #endif
        if(terSettings.musFolderNum < NUM_MUSIC_FOLDERS) {
            musFolderNum = terSettings.musFolderNum;
        }
    } else {
        #ifdef SETTINGS_TRANSITION
        char temp[4];
        musFolderNum = 0;
        if(SD.exists(musCfgName)) {
            File configFile = SD.open(musCfgName, "r");
            if(configFile) {
                DECLARE_S_JSON(512,json);
                if(!readJSONCfgFile(json, configFile)) {
                    if(!CopyCheckValidNumParm(json["folder"], NULL, temp, sizeof(temp), 0, NUM_MUSIC_FOLDERS - 1, 0)) {
                        musFolderNum = atoi(temp);
                    }
                }
                configFile.close();
                saveMusFoldNum();
            }
        }
        removeOldFiles(musCfgName);
        #endif
        
    }
}

void saveMusFoldNum()
{
    terSettings.musFolderNum = musFolderNum;
    saveTerSettings(true);
}

/*
 * Load/save shuffle setting
 */

void loadShuffle()
{
    if(haveSD && haveTerSettings) {
        aud_state.mpShuffle = terSettings.mpShuffle;
    }
}

void saveShuffle()
{
    terSettings.mpShuffle = aud_state.mpShuffle;
    saveTerSettings(true);
}

/*
 * Load/save opMode
 */

void loadOpMode()
{
    if(haveSD && haveTerSettings) {
        opMode = terSettings.opMode;
    }
}

void storeOpMode()
{
    // Used to keep terSettings up-to-date in case
    // of delayed save
    terSettings.opMode = opMode;
}

void saveOpMode()
{
    terSettings.opMode = opMode;
    saveTerSettings(true);
}

/*
 * Load/save curStream
 */

void loadCurStream()
{
    if(haveSD && haveTerSettings) {
        curStream = terSettings.curStream;
    }
}

void storeCurStream()
{
    // Used to keep terSettings up-to-date in case
    // of delayed save
    terSettings.curStream = curStream;
}

void saveCurStream()
{
    terSettings.curStream = curStream;
    saveTerSettings(true);
}

/*
 * Load/save/delete settings for static IP configuration
 */

bool loadIpSettings()
{
    memset((void *)&ipsettings, 0, sizeof(ipsettings));

    if(!haveFS && !FlashROMode)
        return false;

    int vb = 0;
    if(loadConfigFile(ipCfgName, (uint8_t *)&ipsettings, sizeof(ipsettings), vb, -1)) {
        if(*ipsettings.ip) {
            if(checkIPConfig()) {
                ipHash = calcHash((uint8_t *)&ipsettings, sizeof(ipsettings));
                return true;
            } else {
                #ifdef JB_DBG_BOOT
                Serial.println("loadIpSettings: IP settings invalid; deleting file");
                #endif
                memset((void *)&ipsettings, 0, sizeof(ipsettings));
                deleteIpSettings();
            }
        }
    }

    ipHash = 0;
    return false;
}

void writeIpSettings()
{
    if(!haveFS && !FlashROMode)
        return;

    if(!*ipsettings.ip)
        return;

    uint32_t nh = calcHash((uint8_t *)&ipsettings, sizeof(ipsettings));
    
    if(ipHash) {
        if(nh == ipHash) {
            #ifdef JB_DBG_BOOT
            Serial.printf("writeIpSettings: Not writing, hash identical (%x)\n", ipHash);
            #endif
            return;
        }
    }

    ipHash = nh;
    
    saveConfigFile(ipCfgName, (uint8_t *)&ipsettings, sizeof(ipsettings), -1);
}

bool deleteIpSettings()
{
    bool ret;
    
    #ifdef JB_DBG_BOOT
    Serial.println("deleteIpSettings: Deleting ip config");
    #endif

    ipHash = 0;

    if(FlashROMode) {
        ret = SD.exists(ipCfgName);
        SD.remove(ipCfgName);
    } else if(haveFS) {
        ret = MYNVS.exists(ipCfgName);
        MYNVS.remove(ipCfgName);
    }

    return ret;
}

/*
 * Re-format flash FS and write back all settings.
 * Used during audio file installation when flash FS needs
 * to be re-formatted.
 * Is never called in FlashROmode.
 * Needs a reboot afterwards!
 */
void reInstallFlashFS()
{
    // Format partition
    formatFlashFS(false);

    // Rewrite all settings residing in NVS
    #ifdef JB_DBG_BOOT
    Serial.println("Re-writing main, ip settings and clockstate");
    #endif
    
    mainConfigHash = 0;
    write_settings();

    ipHash = 0;
    writeIpSettings();

    if(!configOnSD) {
        #ifdef JB_DBG_BOOT
        Serial.println("Re-writing clockdata and secondary settings");
        #endif
        saveSecSettings(false);
    }
}

/* 
 * Move settings from/to SD if user changed "save to SD"-option in CP
 * Needs a reboot afterwards!
 */
void moveSettings()
{       
    if(!haveSD || !haveFS) 
        return;

    if(configOnSD && FlashROMode) {
        #ifdef JB_DBG_BOOT
        Serial.println("moveSettings: Writing to flash prohibted (FlashROMode), aborting.");
        #endif
    }

    // Flush pending saves
    flushDelayedSave();

    configOnSD = !configOnSD;
    
    #ifdef JB_DBG_BOOT
    Serial.printf("moveSettings: Storing secondary settings %s\n", configOnSD ? "on SD" : "in Flash FS");
    #endif
    saveSecSettings(false);

    configOnSD = !configOnSD;

    if(configOnSD) {
        SD.remove(secCfgName);
    } else {
        MYNVS.remove(secCfgName);
    }
}


/*
 * Sound pack installer
 *
 */

static bool audio_files_present(int& alienVER)
{
    File file;
    uint8_t buf[4];
    const char *fn = "/VER";

    // alienVER is -1 if no VER found,
    //              0 if our VER-type found,
    //              1 if alien VER-type found
    alienVER = -1;

    if(FlashROMode) {
        if(!(file = SD.open(fn, FILE_READ)))
            return false;
    } else {
        // No SD, no FS - don't even bother....
        if(!haveFS)
            return true;
        if(!MYNVS.exists(fn))
            return false;
        if(!(file = MYNVS.open(fn, FILE_READ)))
            return false;
    }

    file.read(buf, 4);
    file.close();

    if(!FlashROMode) {
        alienVER = memcmp(buf, rspv, 2) ? 1 : 0;
    }

    return (!memcmp(buf, rspv, 4));
}

static uint32_t getuint32(uint8_t *buf)
{
    uint32_t t = 0;
    for(int i = 3; i >= 0; i--) {
        t <<= 8;
        t += buf[i];
    }
    return t;
}

static bool dfile_open(File& file, bool tSD, const char *fn, const char *md)
{
    if(tSD || FlashROMode) {
        return (file = SD.open(fn, md));
    }
    return (file = MYNVS.open(fn, md));
}

static void cfc(File& sfile, bool doCopy, int& haveErr, int& haveWriteErr)
{
    const char *funcName = "cfc";
    uint8_t buf1[1+32+4];
    uint8_t buf2[1024];
    uint32_t s;
    bool skip = false, tSD = false;
    File dfile;

    buf1[0] = '/';
    sfile.read(buf1 + 1, 32+4);   
    s = getuint32((*r)(buf1 + 1, soa, 32) + 32);
    if(buf1[1] == '_') {
        tSD = true;
        skip = doCopy;
    } else {
        skip = !doCopy;
    }
    if(!skip) {
        if((dfile = (tSD || FlashROMode) ? SD.open((const char *)buf1, FILE_WRITE) : MYNVS.open((const char *)buf1, FILE_WRITE))) {
            uint32_t t = 1024;
            #ifdef JB_DBG_BOOT
            Serial.printf("%s: Opened destination file: %s, length %d\n", funcName, (const char *)buf1, s);
            #endif
            while(s > 0) {
                t = (s < t) ? s : t;
                if(sfile.read(buf2, t) != t) {
                    haveErr++;
                    break;
                }
                if(dfile.write((*r)(buf2, soa, t), t) != t) {
                    #ifdef JB_DBG_BOOT
                    Serial.printf("%s: Write error\n", funcName);
                    #endif
                    haveErr++;
                    haveWriteErr++;
                    break;
                }
                s -= t;
            }
        } else {
            haveErr++;
            haveWriteErr++;
            Serial.printf("%s: Error opening destination file: %s\n", funcName, buf1);
        }
    } else {
        #ifdef JB_DBG_BOOT
        Serial.printf("%s: Skipped file: %s, length %d\n", funcName, (const char *)buf1, s);
        #endif
        sfile.seek(sfile.position() + s);
    }
}

bool check_allow_CPA()
{
    return allowCPA;
}

bool check_if_default_audio_present()
{
    uint8_t dbuf[16]; 
    File file;
    size_t ts;

    ic = false;

    if(!haveSD)
        return false;

    if(SD.exists(CONFN)) {
        if(file = SD.open(CONFN, FILE_READ)) {
            ts = file.size();
            file.read(dbuf, 14);
            file.close();
            if((!memcmp(dbuf, CONID, 4))             && 
               ((*(dbuf+4) & 0x7f) == AC_FMTV)       &&
               (!memcmp(dbuf+5, rspv, 4))            &&
               (*(dbuf+9) == (NUM_AUDIOFILES + 1))   &&
               (getuint32(dbuf+10) == soa)           &&
               (ts > soa + AC_OHSZ)) {
                ic = true;
                if(!(*(dbuf+4) & 0x80)) r=f;
            }
        }
    }

    return ic;
}

bool prepareCopyAudioFiles()
{
    int i, haveErr = 0, haveWriteErr = 0;
    
    if(!ic)
        return true;

    File sfile;
    if(sfile = SD.open(CONFN, FILE_READ)) {
        sfile.seek(14);
        for(i = 0; i < NUM_AUDIOFILES+1; i++) {
           cfc(sfile, false, haveErr, haveWriteErr);
           if(haveErr) break;
        }
        sfile.close();
    } else {
        return false;
    }

    return (haveErr == 0);
}

// Returns false if copy failed because of a write error (which 
//    might be cured by a reformat of the FlashFS)
// Returns true if ok or source error (file missing, read error)
// Sets delIDfile to true in any case (to avoid repeated flash-hurting attempts)
static bool copy_audio_files(bool& delIDfile)
{
    int i, haveErr = 0, haveWriteErr = 0;
    uint32_t tw = 0, ts;

    if(!allowCPA) {
        delIDfile = false;
        return true;
    }

    if(ic) {
        File sfile;
        if(sfile = SD.open(CONFN, FILE_READ)) {
            sfile.seek(14);
            for(i = 0; i < NUM_AUDIOFILES + 1; i++) {
               cfc(sfile, true, haveErr, haveWriteErr);
               if(haveErr) break;
            }
            sfile.close();
        } else {
            haveErr++;
        }
    } else {
        haveErr++;
    }

    delIDfile = true;   // (haveErr == 0);

    return (haveWriteErr == 0);
}

void doCopyAudioFiles()
{
    bool delIDfile = false;

    if((!copy_audio_files(delIDfile)) && !FlashROMode) {
        // If copy fails because of a write error, re-format flash FS
        reInstallFlashFS();
        copy_audio_files(delIDfile);// Retry copy
    }

    if(haveSD) {
        SD.remove("/_installing.mp3");
    }

    if(delIDfile) {
        delete_ID_file();
    } else {
        showCopyError();
        mydelay(5000);
    }

    mydelay(500);
    allOff();

    flushDelayedSave();

    unmount_fs();
    delay(1000);
    
    esp_restart();
}

void delete_ID_file()
{
    if(haveSD && ic) {
        SD.remove(CONFND);
        SD.rename(CONFN, CONFND);
    }
}

/*
 * File upload
 */

static char *allocateUploadFileName(const char *fn, int idx)
{
    if(uploadFileNames[idx]) {
        free(uploadFileNames[idx]);
    }
    if(uploadRealFileNames[idx]) {
        free(uploadRealFileNames[idx]);
    }
    uploadFileNames[idx] = uploadRealFileNames[idx] = NULL;

    if(!strlen(fn))
        return NULL;
  
    if(!(uploadFileNames[idx] = (char *)malloc(strlen(fn)+4)))
        return NULL;

    if(!(uploadRealFileNames[idx] = (char *)malloc(strlen(fn)+4))) {
        free(uploadFileNames[idx]);
        uploadFileNames[idx] = NULL;
        return NULL;
    }

    return uploadRealFileNames[idx];
}

bool openUploadFile(String& fn, File& file, int idx, bool haveAC, int& opType, int& errNo)
{
    char *uploadFileName = NULL;
    bool ret = false;
    
    if(haveSD) {

        errNo = 0;
        opType = 0;  // 0=normal, 1=AC, -1=deletion

        if(!(uploadFileName = allocateUploadFileName(fn.c_str(), idx))) {
            errNo = UPL_MEMERR;
            return false;
        }
        strcpy(uploadFileNames[idx], fn.c_str());
        
        uploadFileName[0] = '/';
        uploadFileName[1] = '-';
        uploadFileName[2] = 0;

        if(fn.length() > 4 && fn.endsWith(".mp3")) {

            strcat(uploadFileName, fn.c_str());

            if((strlen(uploadFileName) > 9) &&
               (strstr(uploadFileName, "/-delete-") == uploadFileName)) {

                #ifdef JB_DBG_BOOT
                char t = uploadFileName[8];
                #endif
                
                uploadFileName[8] = '/';
                
                #ifdef JB_DBG_BOOT
                Serial.printf("openUploadFile: Deleting %s\n", uploadFileName+8);
                #endif
                
                SD.remove(uploadFileName+8);
                
                #ifdef JB_DBG_BOOT
                uploadFileName[8] = t;
                #endif
                
                opType = -1;
            }

        } else if(fn.endsWith(".bin")) {

            if(!haveAC) {
                strcat(uploadFileName, CONFN+1);  // Skip '/', already there
                opType = 1;
            } else {
                errNo = UPL_DPLBIN;
                opType = -1;
            }

        } else {

            errNo = UPL_UNKNOWN;
            opType = -1;
            // ret must be false!

        }

        #ifdef JB_DBG_BOOT
        Serial.printf("openUploadFile: uploadFilename: %s opType %d\n", uploadFileName, opType);
        #endif

        if(opType >= 0) {
            if((file = SD.open(uploadFileName, FILE_WRITE))) {
                ret = true;
            } else {
                errNo = UPL_OPENERR;
            }
        }

    } else {
      
        errNo = UPL_NOSDERR;
        
    }

    return ret;
}

size_t writeACFile(File& file, uint8_t *buf, size_t len)
{
    return file.write(buf, len);
}

void closeACFile(File& file)
{
    file.close();
}

void removeACFile(int idx)
{
    if(haveSD) {
        if(uploadRealFileNames[idx]) {
            #ifdef JB_DBG_BOOT
            Serial.printf("removeACFile: Deleting %s\n", uploadRealFileNames[idx]);
            #endif
            SD.remove(uploadRealFileNames[idx]);
        }
    }
}

int getUploadFileNameLen(int idx)
{
    if(idx >= MAX_SIM_UPLOADS) return 0; 
    if(!uploadFileNames[idx]) return 0;
    return strlen(uploadFileNames[idx]);
}

char *getUploadFileName(int idx)
{
    if(idx >= MAX_SIM_UPLOADS) return NULL; 
    return uploadFileNames[idx];
}

void freeUploadFileNames()
{
    for(int i = 0; i < MAX_SIM_UPLOADS; i++) {
        if(uploadFileNames[i]) {
            free(uploadFileNames[i]);
            uploadFileNames[i] = NULL;
        }
        if(uploadRealFileNames[i]) {
            free(uploadRealFileNames[i]);
            uploadRealFileNames[i] = NULL;
        }
    }
}

void renameUploadFile(int idx)
{
    char *uploadFileName = uploadRealFileNames[idx];
    
    if(haveSD && uploadFileName) {

        char *t = (char *)malloc(strlen(uploadFileName)+4);
        t[0] = uploadFileName[0];
        t[1] = 0;
        strcat(t, uploadFileName+2);
        
        #ifdef JB_DBG_BOOT
        Serial.printf("renameUploadFile [1]: Deleting %s\n", t);
        #endif
        
        SD.remove(t);
        
        #ifdef JB_DBG_BOOT
        Serial.printf("renameUploadFile [2]: Renaming %s to %s\n", uploadFileName, t);
        #endif
        
        SD.rename(uploadFileName, t);

        // Real name is now changed
        strcpy(uploadFileName, t);
        
        free(t);
    }
}

// Emergency firmware update from SD card
static void fw_error_blink(int n)
{
    bool leds = false;

    for(int i = 0; i < n; i++) {
        leds = !leds;
        digitalWrite(SIGNALLIGHT_PIN, leds ? HIGH : LOW);
        delay(500);
    }
    digitalWrite(SIGNALLIGHT_PIN, LOW);
}

static void firmware_update()
{
    const char *upderr = "Firmware update error %d\n";
    uint8_t  buf[1024];
    unsigned int lastMillis = millis();
    bool     leds = false;
    size_t   s;

    if(!SD.exists(fwfn))
        return;
    
    File myFile = SD.open(fwfn, FILE_READ);
    
    if(!myFile)
        return;

    pinMode(SIGNALLIGHT_PIN, OUTPUT);
    
    if(!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Serial.printf(upderr, Update.getError());
        fw_error_blink(5);
        return;
    }

    while((s = myFile.read(buf, 1024))) {
        if(Update.write(buf, s) != s) {
            break;
        }
        if(millis() - lastMillis > 1000) {
            leds = !leds;
            digitalWrite(SIGNALLIGHT_PIN, leds ? HIGH : LOW);
            lastMillis = millis();
        }
    }
    
    if(Update.hasError() || !Update.end(true)) {
        Serial.printf(upderr, Update.getError());
        fw_error_blink(5);
    } 
    myFile.close();
    // Rename/remove in any case, we don't
    // want an update loop hammer our flash
    SD.remove(fwfnold);
    SD.rename(fwfn, fwfnold);
    unmount_fs();
    delay(1000);
    fw_error_blink(0);
    esp_restart();
}

/*
 * Backchannel JSON parsing
 * 
 */

static bool getJSONJBLN(const JsonDocument& json, const char *key, int& jltr, int& jnum, bool allowFixed)
{
    const char *value = NULL;

    #ifdef JB_DBG_MQTT
    mqttPublishQuick("bttf/jb/debug", key);
    #endif
    
    if(*key && (*key != '=')) {
        if(json[key]) {
            if(json[key].is<const char*>()) {
                if(strlen(json[key]) >= 3) {
                    value = json[key];
                }
            }
        }
    } else if(allowFixed && (*key == '=') && (strlen(key) >= 4)) {
        value = (const char *)&key[1];
    }
    if(value) {
        if(value[1] == ' ' || value[1] == '_' || value[1] == '-') {
            jltr = value[0] - 'A';
            jnum = atoi(&value[2]);
            return true;
        }
    }

    return false;
    
}

static bool getJSONNumber(const JsonDocument& json, const char *key, int& val, bool allowFixed, bool allowFloat = false)
{
    #ifdef JB_DBG_MQTT
    mqttPublishQuick("bttf/jb/debug", key);
    #endif
    
    if(*key && *key != '=') {
        if(json[key]) {
            if(json[key].is<int>()) {
                val = json[key];
                return true;
            } else if(json[key].is<const char*>()) {
                if(strpbrk(json[key], ".")) {
                    if(!allowFloat) return false;
                    val = (int)((float)strtof(json[key], NULL) * 100.0f);
                } else {
                    val = atoi(json[key]);
                }
                return true;
            } else if(allowFloat && json[key].is<float>()) {
                val = (int)((float)json[key] * 100.0f);
                return true;
            } else {
                val = 0;
                return true;
            }
        } else if(json.containsKey(key)) {
            val = 0;
            return true;
        }
    } else if(allowFixed && (*key == '=') && (strlen(key) > 1)) {
        val = atoi(key + 1);
        return true;
    }
    
    return false;
}

static bool getJSONBool(const JsonDocument& json, const char *key, const char *yesVal, bool& val, bool allowBool)
{
    if(*key) {
        if(json[key]) {
            if(*yesVal && json[key].is<const char*>()) {
                val = !strcmp(json[key], yesVal);
                return true;
            } else if(allowBool) {
                if(json[key].is<const char*>()) {
                    val = !strcmp(json[key], "1");
                    return true;
                } else if(json[key].is<bool>()) {
                    val = json[key];
                    return true;
                } else if(json[key].is<int>()) {
                    val = (json[key] == 1);
                    return true;
                }
            }
        } else if(allowBool && json.containsKey(key)) {
            val = false;
            return true;
        }
    }
    return false;
}

static bool getJSONOption(const JsonDocument& json, const char *key, const char **olist, int& val)
{
    val = 0;
    if(*key && json[key]) {
        while(*olist) {
            if(**olist) {
                if(!strcmp(json[key], *olist)) {
                    return true;
                }
            }
            val++;
            olist++;
        }
        return true;
    }

    return false;
}

bool parse_mp_backchannel(const char *msg, struct mp_rem_stat *mp_stat)
{   
    int temp = 0, temp1 = 0;
    bool tempb = false;
    
    DECLARE_S_JSON(512,json);

    DeserializationError error = deserializeJson(json, msg);

    if(error) {
        return false;
    }

    // Don't clear validFields if sth is missing. This way we can have
    // partial status updates.

    // State: String or bool
    if(!*settings.mqmpbc_state_val_off) {
        if(getJSONBool(json, settings.mqmpbc_state_key, settings.mqmpbc_state_val_play, tempb, true)) {
            mp_stat->playing = tempb;
            mp_stat->off = false;
            mp_stat->validFields |= RMPS_STATE;
        }
    } else {
        char *olist[3] = {
            settings.mqmpbc_state_val_play,
            settings.mqmpbc_state_val_off,
            NULL
        };
        if(getJSONOption(json, settings.mqmpbc_state_key, (const char **)olist, temp)) {
            switch(temp) {
            case 0:
                mp_stat->playing = true;
                mp_stat->off = false;
                break;
            case 1:
                mp_stat->playing = false;
                mp_stat->off = true;
                break;
            default:
                mp_stat->playing = false;
                mp_stat->off = false;
                break;
            }
            mp_stat->validFields |= RMPS_STATE;
        }
    }

    // Current Track ("L-N" or int)
    if(getJSONJBLN(json, settings.mqmpbc_curTrack_key, temp, temp1, false)) {
        if((temp >= 0) && (temp <= 25) && (temp1 >= 1) && (temp1 <= 99)) {
            if(!remIncI) {
                if(temp >= 15) temp--;
                if(temp >= 9) temp--;
            }
            // curLtr/Num 0-X
            mp_stat->curLtr = temp;
            mp_stat->curNum = temp1 - 1;
            mp_stat->validFields |= RMPS_CURLN;
            mp_stat->validFields &= ~RMPS_CURTRCK;
        } else {
            // illegal values make field unavailable
            mp_stat->validFields &= ~RMPS_CURLN;
        }
    } else if(getJSONNumber(json, settings.mqmpbc_curTrack_key, temp, false)) {
        if((temp >= 0) && (temp <= 1000)) {
            mp_stat->curTrack = temp;
            mp_stat->validFields |= RMPS_CURTRCK;
            mp_stat->validFields &= ~RMPS_CURLN;
        } else {
            // illegal values make field unavailable
            mp_stat->validFields &= ~RMPS_CURTRCK;
        }
    }

    // First Track (int or =xxx)
    if(getJSONNumber(json, settings.mqmpbc_firstTrack_key, temp, true)) {
        if((temp == 0) || (temp == 1)) {
            mp_stat->firstTrack = temp;
            mp_stat->validFields |= RMPS_FIRST;
        } else {
            // illegal values make field unavailable
            mp_stat->validFields &= ~RMPS_FIRST;
        }
    }

    // Last Track (X-Y or int or "=X-Y" or "=int")  ABCDEFGHIJKLMNOP
    if(getJSONJBLN(json, settings.mqmpbc_lastTrack_key, temp, temp1, true)) {
        if((temp >= 0) && (temp <= 25) && (temp1 >= 1) && (temp1 <= 99)) {
            if(!remIncI) {
                if(temp >= 15) temp--;
                if(temp >= 9) temp--;
            }
            // curLtr/Num 0-X
            mp_stat->maxLtr = temp;
            mp_stat->maxNum = temp1 - 1;
            mp_stat->validFields |= RMPS_MAXLN;
        } else {
            // illegal values make field unavailable
            mp_stat->validFields &= ~RMPS_MAXLN;
        }
    } else if(getJSONNumber(json, settings.mqmpbc_lastTrack_key, temp, true)) {
        if((temp >= 0) && (temp <= 1000)) {
            mp_stat->lastTrack = temp;
            mp_stat->validFields |= RMPS_LAST;
        } else {
            // illegal values make field unavailable
            mp_stat->validFields &= ~RMPS_LAST;
        }
    }
    
    // Volume (percent 0-100 or float 0.0-1.0, converted to percent)
    if(getJSONNumber(json, settings.mqmpbc_volume_key, temp, true, true)) {
        if(temp >= 0 && temp <= 100) {
            mp_stat->volumePerc = temp;
            mp_stat->validFields |= RMPS_VOL;
        } else {
            // illegal values make field unavailable
            // (TCD can switch between knob and software vol; knob -> -1)
            mp_stat->validFields &= ~RMPS_VOL;
        }
    }

    // Shuffle state (string or bool)
    if(getJSONBool(json, settings.mqmpbc_shuffle_key, settings.mqmpbc_shuffle_val_on, tempb, true)) {
        mp_stat->shuffle = tempb;
        mp_stat->validFields |= RMPS_SHUFFLE;
    }

    #ifdef JB_DBG_MQTT
    {
        char buf[128];
        sprintf(buf, "C %d [%c %d] F %d L %d [%c %d] V %d     play %d, off %d, shuffle %d    vf %x",
            mp_stat->curTrack, 
            remIncI ? mp_stat->curLtr + 'A' : jbltrs[mp_stat->curLtr], 
            mp_stat->curNum, 
            mp_stat->firstTrack, 
            mp_stat->lastTrack, 
            remIncI ? mp_stat->maxLtr + 'A' : jbltrs[mp_stat->maxLtr], 
            mp_stat->maxNum,
            mp_stat->volumePerc,
            mp_stat->playing ? 1 : 0, 
            mp_stat->off ? 1 : 0, 
            mp_stat->shuffle ? 1 : 0,
            mp_stat->validFields);
        mqttPublishQuick("bttf/jb/debug", buf);
    }
    #endif

    return true;
}
