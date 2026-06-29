/*
 * -------------------------------------------------------------------
 * A10001986 Crosley CR-9 Jukebox
 * (C) 2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Jukebox
 * https://jukebox.out-a-ti.me
 *
 * WiFi and Config Portal handling
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

#include "src/WiFiManager/WiFiManager.h"

#ifndef WM_MDNS
#define TC_MDNS
#include <ESPmDNS.h>
#endif
#include "jb_main.h"
#include "jb_audio.h"
#include "jb_settings.h"
#include "jb_wifi.h"
#include "mqtt.h"

#define STRLEN(x) (sizeof(x)-1)

Settings settings;

IPSettings ipsettings;

WiFiManager wm;

WiFiClient mqttWClient;
PubSubClient mqttClient(mqttWClient);
unsigned long mqttInitialConnectNow = 0;

static const char R_updateacdone[] = "/uac";

static const char acul_part1[]  = "</style>";
static const char acul_part3[]  = "</head><body><div id='wrap'><h1 id='h1'>";
static const char acul_part5[]  = "</h1><h3 id='h3'>File upload</h3><div class='msg";
static const char acul_part7[]  = " S' id='lc'><strong>Upload complete.</strong><br>Device rebooting.";
static const char acul_part7a[] = "<br>Installation will proceed after reboot.";
static const char acul_part71[] = " D'><strong>Upload failed.</strong><br>";
static const char acul_part8[]  = "</div></div></body></html>";
static const char *acul_errs[]  = { 
    "Can't open file on SD",
    "No SD card found",
    "Write error",
    "Bad file",
    "Not enough memory",
    "Unrecognized type",
    "Extraneous .bin file"
};

static const char tcdList[] = "<datalist id='tcda'><option value='TCD-AP%s'></option></datalist><datalist id='hnl'><option value='flux'></option></datalist>";

static const char tcdSSIDp[] = "<div style='margin:0 0 10px 0;padding:0;font-size:80%%'>SSID of currently connected TCD is <b>TCD-AP%s</b> (%s password)</div>";
static const char tcdAPPW1[] = "no";
static const char tcdAPPW2[] = "with";

static const char *apChannelCustHTMLSrc[14] = {
    "'>WiFi channel",
    "apchnl",
    ">Random%s1'",
    ">1%s2'",
    ">2%s3'",
    ">3%s4'",
    ">4%s5'",
    ">5%s6'",
    ">6%s7'",
    ">7%s8'",
    ">8%s9'",
    ">9%s10'",
    ">10%s11'",
    ">11%s"
};

static const char *mqttpCustHTMLSrc[4] = {
    "'>Protocol version",
    "mprot",
    ">3.1.1%s1'",
    ">5.0%s"
};
static const char mqttMsgDisabled[] = "Disabled";
static const char mqttMsgConnecting[] = "Connecting...";
static const char mqttMsgTimeout[] = "Connection time-out";
static const char mqttMsgFailed[] = "Connection failed";
static const char mqttMsgDisconnected[] = "Disconnected";
static const char mqttMsgConnected[] = "Connected";
static const char mqttMsgBadProtocol[] = "Protocol error";
static const char mqttMsgUnavailable[] = "Server unavailable/busy";
static const char mqttMsgBadCred[] = "Login failed";
static const char mqttMsgGenError[] = "Error";
static const char msgResolvErr[] = "DNS lookup error";    // MQTT

static const char *wmBuildTCDAPList(const char *dest, int op);
static const char *wmBuildTCDSSID(const char *dest, int op);
static const char *wmBuildApChnl(const char *dest, int op);
static const char *wmBuildBestApChnl(const char *dest, int op);
static const char *wmBuildHaveSD(const char *dest, int op);
static const char *wmBuildStreams(const char *dest, int op);
static const char *wmBuildMQTTprot(const char *dest, int op);
static const char *wmBuildMQTTstate(const char *dest, int op);
static const char *wmBuildMQTTTM(const char *dest, int op);

static const char custHTMLHdr1[] = "<div class='cmp0";
static const char custHTMLHdrI[] = " ml20";
static const char custHTMLHdr2[] = "'><label class='mp0' for='";
static const char custHTMLSHdr[] = "</label><select class='sel0' value='";
static const char osde[] = "</option></select></div>";
static const char ooe[]  = "</option><option value='";
static const char custHTMLSel[] = " selected";
static const char custHTMLSelFmt[] = "' name='%s' id='%s' autocomplete='off'><option value='0'";
static const char col_g[] = "609b71";
static const char col_r[] = "dc3630";
static const char col_gr[] = "777";

static const char bannerStart[] = "<div class='c' style='background-color:#";
static const char bannerMid[] = ";color:#fff;font-size:80%;border-radius:5px'>";

static const char bestAP[]   = "%s%s%sProposed channel at current location: %d<br>%s(Non-WiFi devices not taken into account)</div>";
static const char badWiFi[]  = "<br><i>Operating in AP mode not recommended</i>";

static const char bannerGen[] = "%s%s%s<i>%s</i></div>";
static const char ntpOFF[] = "NTP is inactive";
static const char ntpUNR[] = "NTP server is unresponsive";
static const char haveNoSD[] = "No SD card present";

static const char mqttStatus[] = "%s%s%s%s%s (%d)</div>";

// WiFi Configuration

#ifdef WM_CCM
WiFiManagerParameter custom_asel(wmBuildTCDAPList);

WiFiManagerParameter custom_sectstart_cm("Car mode settings", WFM_SECTS_HEAD|WFM_HL);
WiFiManagerParameter custom_cmhint("<div style='margin:0 0 10px 0;padding:0;font-size:80%;white-space:break-spaces;'>In Car mode, the device connects to the TCD's access point instead of the WiFi network configured above.</div>");
WiFiManagerParameter custom_ssidcm("ssidcm", "Network name (SSID) of TCD-AP", settings.cm_ssid, 13, "pattern='[A-Za-z0-9\\-]+' placeholder='Example: TCD-AP' list='tcda'");
WiFiManagerParameter custom_passcm("passcm", "Password for TCD-AP", settings.cm_pass, 8, "minlength='8' pattern='[A-Za-z0-9\\-]+'");
WiFiManagerParameter custom_tcdssid(wmBuildTCDSSID);
WiFiManagerParameter custom_bssidcm("bsidcm", "TCD-AP BSSID (optional)", settings.cm_bssid, 17, "pattern='^([0-9A-Fa-f]{2}[:]){5}([0-9A-Fa-f]{2})$' placeholder='XX:XX:XX:XX:XX:XX'");
WiFiManagerParameter custom_ecm("ecm", "Enable Car Mode", settings.ecmKludge, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
#endif

#if defined(TC_MDNS) || defined(WM_MDNS)
#define HNTEXT "Hostname<br><span>The Config Portal is accessible at http://<i>hostname</i>.local<br>[Valid characters: a-z/0-9/-]</span>"
#else
#define HNTEXT "Hostname<br><span>(Valid characters: a-z/0-9/-)</span>"
#endif
WiFiManagerParameter custom_hostName("hostname", HNTEXT, settings.hostName, 31, "pattern='[A-Za-z0-9\\-]+' placeholder='Example: jb'", WFM_LABEL_BEFORE|WFM_SECTS_HEAD);

WiFiManagerParameter custom_sectstart_wifi("WiFi connection: Other settings", WFM_SECTS|WFM_HL);
WiFiManagerParameter custom_wifiConRetries("wifiret", "Connection attempts (1-10)", settings.wifiConRetries, 2, "type='number' min='1' max='10'");

WiFiManagerParameter custom_sectstart_ap("Access point (AP) mode settings", WFM_SECTS|WFM_HL);
WiFiManagerParameter custom_sysID("sysID", "Network name (SSID) appendix<br><span>Will be appended to \"JB-AP\" [a-z/0-9/-]</span>", settings.systemID, 7, "pattern='[A-Za-z0-9\\-]+'");
WiFiManagerParameter custom_appw("appw", "Password<br><span>Password to protect JB-AP. Empty or 8 characters [a-z/0-9/-]</span>", settings.appw, 8, "minlength='8' pattern='[A-Za-z0-9\\-]{8}'");
WiFiManagerParameter custom_apch(wmBuildApChnl);
WiFiManagerParameter custom_bapch(wmBuildBestApChnl);
WiFiManagerParameter custom_wifiAPOffDelay("wifiAPoff", "Power save timer<br><span>(10-99[minutes]; 0=off)</span>", settings.wifiAPOffDelay, 2, "type='number' min='0' max='99' title='WiFi-AP will be shut down after chosen period'");
WiFiManagerParameter custom_wifihint("<div style='margin:0;padding:0;font-size:80%;white-space:break-spaces;'>Hold middle jog dial button while fake-off to re-enable Wifi when in power save mode.</div>", WFM_FOOT);

// Settings

WiFiManagerParameter custom_playJBSnd("plyJB", "Play Jukebox sounds", settings.playJB, "title='Check to have the device play a sound then the TCD alarm sounds.'", WFM_LABEL_AFTER|WFM_IS_CHKBOX|WFM_SECTS_HEAD);
WiFiManagerParameter custom_playALSnd("plyALS", "Play TCD-alarm sound", settings.playALsnd, "title='Check to have the device play a sound then the TCD alarm sounds.'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_NMB("NMB", "Turn on blue light in night mode", settings.NMB, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX);

WiFiManagerParameter custom_sectstart_nw("Wireless communication (BTTF-Network)", WFM_SECTS|WFM_HL);
WiFiManagerParameter custom_tcdIP("tcdIP", "Hostname or IP address of TCD", settings.tcdIP, 31, "pattern='(^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}$)|([A-Za-z0-9\\-]+)' placeholder='Example: timecircuits' list='tcdh'");
WiFiManagerParameter custom_uNM("uNM", "Follow TCD night-mode", settings.useNM, "class='mb0'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
WiFiManagerParameter custom_ITT("ITT", "Ignore time travels", settings.ignTT, "class='mb0'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);

WiFiManagerParameter custom_haveSD(wmBuildHaveSD, WFM_SECTS);
WiFiManagerParameter custom_CfgOnSD("CfgSD", "Save secondary settings on SD<br><span>Check this to avoid flash wear</span>", settings.CfgOnSD, "class='mt5'", WFM_LABEL_AFTER|WFM_IS_CHKBOX);
//WiFiManagerParameter custom_sdFrq("sdFrq", "4MHz SD clock speed<br><span>Checking this might help in case of SD card problems</span>", settings.sdFreq, WFM_LABEL_AFTER|WFM_IS_CHKBOX);

WiFiManagerParameter custom_waitPwrOn("wpo", "Wait for fake-power-on upon boot", settings.waitForPower, "class='mt5'", WFM_LABEL_AFTER|WFM_IS_CHKBOX|WFM_SECTS|WFM_FOOT);

// Streaming settings

WiFiManagerParameter custom_streaming(wmBuildStreams);

// HA/MQTT Settings

WiFiManagerParameter custom_useMQTT("uMQTT", "Home Assistant support (MQTT)", settings.useMQTT, "class='mt5' style='margin-bottom:10px'", WFM_LABEL_AFTER|WFM_IS_CHKBOX|WFM_SECTS_HEAD);
WiFiManagerParameter custom_state(wmBuildMQTTstate);
WiFiManagerParameter custom_mqttServer("MQs", "Broker IP[:port] or domain[:port]", settings.mqttServer, 79, "pattern='[a-zA-Z0-9\\.:\\-]+' placeholder='Example: 192.168.1.5'");
WiFiManagerParameter custom_mqttVers(wmBuildMQTTprot);
WiFiManagerParameter custom_mqttUser("MQu", "User[:Password]", settings.mqttUser, 63, "placeholder='Example: ronald:mySecret'");

WiFiManagerParameter custom_pubMP("pMP", "Publish Music Player status to bttf/<i>hostname</i>/mpstatus", settings.pubMP, "class='mt5'", WFM_LABEL_AFTER|WFM_IS_CHKBOX|WFM_SECTS);

WiFiManagerParameter custom_mqttPwr("MQp", "HA controls Fake-Power at startup", settings.mqttPwr, "class='mt5' title='Check to have HA control Fake-Power and take precendence over Fake-Power switch at power-up'", WFM_LABEL_AFTER|WFM_IS_CHKBOX|WFM_SECTS);
WiFiManagerParameter custom_mqttPwrOn("MQpo", "Wait for POWER_ON at startup", settings.mqttPwrOn, "class='mt5 ml20'", WFM_LABEL_AFTER|WFM_IS_CHKBOX|WFM_FOOT);

WiFiManagerParameter custom_mqtttm(wmBuildMQTTTM);
WiFiManagerParameter custom_mqmpii("inclI", "Jukebox-style numbering includes letters 'I' and 'O'", settings.mqmpii, "", WFM_LABEL_AFTER|WFM_IS_CHKBOX|WFM_FOOT); // FOOT because topic leave section open

WiFiManagerParameter custom_sectstart_mqbc("Remote Player's backchannel", WFM_SECTS_HEAD|WFM_HL);
WiFiManagerParameter custom_mqttMPBC("mqmpBC", "Backchannel MQTT topic", settings.mqmpbackchannel, 63, "", WFM_LABEL_BEFORE);
WiFiManagerParameter custom_mqttMPBCSK("mqmpBCSK", "JSON Key for state", settings.mqmpbc_state_key, 15, "", WFM_LABEL_BEFORE);
WiFiManagerParameter custom_mqttMPBCSV("mqmpBCSV", "Value for state 'playing' (String)<br><span>Leave empty if 'playing' state is boolean</span>", settings.mqmpbc_state_val_play, 15, "", WFM_LABEL_BEFORE);
WiFiManagerParameter custom_mqttMPBCSO("mqmpBCSO", "Value for state 'off' (String)", settings.mqmpbc_state_val_off, 15, "", WFM_LABEL_BEFORE);
WiFiManagerParameter custom_mqttMPBCC("mqmpBCC", "JSON Key for current track", settings.mqmpbc_curTrack_key, 15, "", WFM_LABEL_BEFORE);
WiFiManagerParameter custom_mqttMPBCF("mqmpBCF", "JSON Key for first track number (which can be 0 or 1)<br><span>If unsupported, enter \"=<i>value</i>\" to set</span>", settings.mqmpbc_firstTrack_key, 15, "", WFM_LABEL_BEFORE);
WiFiManagerParameter custom_mqttMPBCL("mqmpBCL", "JSON Key for highest track number<br><span>If unsupported, enter \"=<i>value</i>\" to set</span>", settings.mqmpbc_lastTrack_key, 15, "", WFM_LABEL_BEFORE);
WiFiManagerParameter custom_mqttMPBCV("mqmpBCV", "JSON Key for volume", settings.mqmpbc_volume_key, 15, "", WFM_LABEL_BEFORE);
WiFiManagerParameter custom_mqttMPBCSHK("mqmpBCSHK", "JSON Key for shuffle mode", settings.mqmpbc_shuffle_key, 15, "", WFM_LABEL_BEFORE);
WiFiManagerParameter custom_mqttMPBCSHV("mqmpBCSHV", "Value for 'shuffle on' (String)<br><span>Leave empty if shuffle is boolean</span>", settings.mqmpbc_shuffle_val_on, 15, "", WFM_LABEL_BEFORE|WFM_FOOT);

static const int8_t wifiMenu[] = { 
    WM_MENU_WIFI,
    WM_MENU_PARAM,
    WM_MENU_PARAM2,
    WM_MENU_PARAM3,
    WM_MENU_SEP_F,
    WM_MENU_UPDATE,
    WM_MENU_SEP,
    WM_MENU_CUSTOM,
    WM_MENU_END
};
#define TC_MENUSIZE (sizeof(wifiMenu) / sizeof(wifiMenu[0]))

#define AA_TITLE "Jukebox"
#define AA_ICON "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQBAMAAADt3eJSAAAAGFBMVEVJSkrMy8ftWxv1AAEEAACXmZnt21zt///ki518AAAAUUlEQVQI12MQhAIGAQYwYAQymJSUFMAMJWNjJRCDydnY2EQBhVFe4l4OZrgAAZgRpmyUisZQUoIwQtPSQkEMllAGhlAHEMOBgYEFyIBZCncGAP0CDp1mouhlAAAAAElFTkSuQmCC"
#define AA_CONTAINER "JBAA"
#define UNI_VERSION JB_VERSION 
#define UNI_VERSION_EXTRA JB_VERSION_EXTRA
#define WEBHOME "jb"
#define CURRVERSION JB_VERSION
static const char apName[]  = "JB-AP";

static const char myTitle[] = AA_TITLE;
static const char myHead[]  = "<link rel='icon' type='image/png' href='data:image/png;base64," AA_ICON "'><script>window.onload=function(){xxx='" AA_TITLE "';yyy='?';wr=ge('wrap');if(wr){aa=ge('h3');if(aa){yyy=aa.innerHTML;aa.remove();dlel('h1')}dd=document.createElement('div');dd.classList.add('tpm0');dd.innerHTML='<div class=\"tpm\" onClick=\"shsp(1);window.location=\\'/\\'\"><div class=\"tpm2\"><img id=\"spi\" src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAMAAACdt4HsAAAAP1BMVEUAAACKkZGMk5N4fn75ucWIj4+CiYmEior1uMTttcDgsLq4oaaXlpigmZvFpqynm5/ZrbbSq7NyeHiunqJrcHAERiD/AAAAAXRSTlMAQObYZgAAAiVJREFUWMPtVtuuozAMbIxDnQuEy/n/b12PQ4Co0j707MM+MJUgNuPxxESirwcPHjx48D/hZxt+ge3ntQ3j+2uMw/YaRu++hh+H1/B29DXc+xE4BaYunfczboucryvSJXcCK890YhVmlppIvJBBWKUKS4bMAkacbgKBozswRa6I4C4sBBTmBDlcc2C2HlMvQM4QtTSVWUmzRipgDwpCtE7OzSKJ8sy8ngLucqAuI3k1Isy5CrhTQG+Td1GUTBQ5HgKjrwLkrCUXj0Vi2enmQNRQDtHDY8hOg7g3gbuDIMGTw5zR8+ZAZk97ROXMEuaJTNiPJkCXA2lSJLI4us0g1TR8sCLMRCbQO8jCqwcNuVAFEGIGWFQFvGjhUBw1gZsDqVJE8EL9DLAy5BSFJVC3BYCsrzEnlhUnjOkYaXIVNBU7L0F4cp8OFqSxTALTM1tIet+bQBCcQdP0/uMt7IzZESYl2RFYmHdkmZqA8Ir7jNQhQGo8AcVO4lJy0gqlURZVyHkWdGgmsR3atcbfhihsSLY3QGKufWqE40UQIGVojArdQRPQDvaThBNmy5VQgUcIw27lFSXWlKdzCy5PBjTVupKKltcSRGlqIUCWyx655kBXXiuRA7Cmxj1Cb6GBSOkEMl0C5BpaYZ/yPcG60yVwoVV2/aqrTq8x3pcD+x1LsuBvjlrcO6Dz8kk/Ez3HBDDAL+BQNm74On8PfJ1/tu13/w8ePPgX+APNpyE+pUyT9gAAAABJRU5ErkJggg==\" class=\"tpm3\"></div><H1 class=\"tpmh1\"><img style=\"\" src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAHAAAAAQCAMAAAD5ygsjAAAAM1BMVEUAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACjBUbJAAAAEHRSTlMAv0CA7xBgIM+vMN+fUI9wloiSiAAAAa1JREFUOMuNlduSrSAMRAkEuYr9/197HJKIZ6a2236hJKEXJFA6t9Gl6k6NcxAR2TAVC3l3y5/zlURld0vp6OdMqO5aqNGDunMBl/z0PwcRYMNUQXHulj/nPUzbhetQkToFcJwRQZjoGRjQoozBLXmQeerMzkA/vN8Hw1IzhlgX9xrogeo+AC1L80rSYDCin0fbwPE1MDGCewAeyNIvRl/RzWo4kH9Ch3sNzGr8F6jiKmDLs+Qi+2gIXULvgB0cPwCZTjUrXsbuboqArNvBQH0L1NI8ldRnmQb+mk4V2dI7YGUM9wDU5qUnYLCHk3z6CswApwWkIPIKFPVp2H7VjSELKwC2Z/UVCCbk1UNTuIDLaKDfeRuymXdCfw3cY0a+jLsXpf9POI2SVE5VWb8OcKyA1KRJMAFmaLsjBQ7nTmL/1sMq1jiuIKOI9wQHQWWUJNdsWDDEHx5jV6BudjzeUiMPoG0nO+4FoKhXlPQx/jg1IFODLBASiBhmDFzlEWAj1f0dribvDSoOOgO5Ol7GeBDAZZU+jQy0vlp5Lds+/i1ou1+V0gCmQ7YgJ7OT/gOFnRwwttXJvQAAAABJRU5ErkJggg==\"></H1>'+'<H3 class=\"tpmh3\">'+yyy+'</div></div>';wr.insertBefore(dd,wr.firstChild);wr.style.position='relative'}var lc=ge('lc');if(lc){lc.style.transform='rotate('+(358+[0,1,3,4,5][Math.floor(Math.random()*4)])+'deg)'}}</script><style>H1{font-family:Bahnschrift,-apple-system,'Segoe UI Semibold',Roboto,'Helvetica Neue',Arial,Verdana,sans-serif;margin:0;text-align:center;}H3{margin:0 0 5px 0;text-align:center;}input{border:thin inset}em > small{display:inline}form{margin-block-end:0;}.tpm{background-color:#7DABA1;cursor:pointer;border:6px double #f5f5f5;border-radius:15px;padding:0 0 0 0px;min-width:18em;}.tpm2{position:absolute;top:-1.7em;z-index:130;right:-0.7em;}.tpm3{width:4em;height:4em;}.tpmh1{font-variant-caps:all-small-caps;font-weight:normal;overflow:clip;}.tpmh3{font-weight:normal;font-size:0.7em;color:#000;margin-left:0.5em;margin-right:0.5em;overflow:hidden;white-space:nowrap}.tpm0{position:relative;width:20em;padding:5px 0px 5px 0px;margin:0 auto 0 auto;}.cmp0{margin:0;padding:0;}.sel0{font-size:90%;width:auto;margin-left:10px;vertical-align:baseline;}.mt5{margin-top:5px!important}.mb10{margin-bottom:10px!important}.mb0{margin-bottom:0px!important}.mb15{margin-bottom:15px!important}.ml20{margin-left:20px}.ss>label span{font-size:80%}</style>";
static const char *myCustMenu = "<div style='font-size:0.75em;line-height:1.2em;font-weight:bold;text-align:center;text-transform:uppercase'>" UNI_VERSION " (" UNI_VERSION_EXTRA ")<br>Powered by <a href='https://out-a-ti.me' target=_blank>A10001986</a> <a href='https://" WEBHOME ".out-a-ti.me' target=_blank>[Home/Updates]</a></div>";
static const char r_link[]  = WEBHOME "r.out-a-ti.me";

static char newversion[8];
static unsigned long lastUpdateCheck = 0;
static unsigned long lastUpdateLiveCheck = 0;

#define WLA_IP           1
#define WLA_DEL_IP       2
#define WLA_WIFI         4
#define WLA_SET1         8
#define WLA_SET1_B          3
#define WLA_SET2        16
#define WLA_SET2_B          4
#define WLA_SET3        32
#define WLA_SET3_B          5
#define WLA_SET_CM      64
#define WLA_SET_CM_ON  128
#define WLA_SET        (WLA_WIFI|WLA_SET1|WLA_SET2|WLA_SET3)
#define WLA_ANY        (WLA_IP|WLA_DEL_IP|WLA_SET)
static uint32_t     wifiLoopSaveAction = 0;

// Did user configure a WiFi network to connect to?
static bool wifiHaveSTAConf = false;
static bool connectedToTCDAP  = false;

bool carMode = false;

// WiFi power management in AP mode
bool          wifiInAPMode = false;
bool          wifiAPIsOff = false;
unsigned long wifiAPModeNow;
unsigned long wifiAPOffDelay = 0;     // default: never

// WiFi power management in STA mode
bool          wifiIsOff = false;

static File acFile;
static bool haveACFile = false;
static bool haveAC = false;
static int  numUploads = 0;
static int  *ACULerr = NULL;
static int  *opType = NULL;

#define MQTT_SHORT_INT (30*1000)
#define MQTT_LONG_INT  (5*60*1000)
static const char    emptyStr[1] = { 0 };
bool                 useMQTT = false;
static char          *mqttUser = (char *)emptyStr;
static char          *mqttPass = (char *)emptyStr;
static char          *mqttServer = (char *)emptyStr;
uint8_t              haveMQTTaudio = 0;
const char           *mqttAudioFile[3] = { "/ha-alert.mp3", "/ha-alert-p.mp3", "/ha-alert-l.mp3" };
static unsigned long mqttReconnectNow = 0;
static unsigned long mqttReconnectInt = MQTT_SHORT_INT;
static uint16_t      mqttReconnFails = 0;
static bool          mqttSubAttempted = false;
static bool          mqttOldState = true;
static bool          mqttDoPing = true;
static bool          mqttRestartPing = false;
static bool          mqttPingDone = false;
static unsigned long mqttPingNow = 0;
static unsigned long mqttPingInt = MQTT_SHORT_INT;
static uint16_t      mqttPingsExpired = 0;
bool                 pubMP = false;
char                 mqbackchannel[32+16];

static unsigned int wmLenBuf = 0;

static void wifiOff(bool force);
static void wifiConnect(bool deferConfigPortal = false);
static void wifi_ntp_setup(bool doUseNTP);
static void checkForUpdate();

static void saveParamsCallback(int);
static void saveWiFiCallback(const char *ssid, const char *pass, const char *bssid);
static void preUpdateCallback();
static void postUpdateCallback(bool);
static void wifiDelayReplacement(unsigned int mydel);
static void gpCallback(int);
static bool preWiFiScanCallback();
static void setCMCallback(bool enable);

static void updateConfigPortalValues();

static void setupWebServerCallback();
static void handleUploadDone();
static void handleUploading();
static void handleUploadDone();

static IPAddress stringToIp(char *str);
static void getServerParam(const char *name, char *destBuf, size_t length, int minval, int maxval, int defaultVal);
static bool myisspace(char mychar);
static char *strcpytrim(char* destination, const char* source, bool doFilter = false);
static char *strcpytrimMAC(char* destination, const char* source);
static char *strcpyfilter(char* destination, const char* source);
static void mystrcpy(char *sv, WiFiManagerParameter *el);
static void mystrcpyWiFiDelay(char *sv, WiFiManagerParameter *el);
static void evalCB(char *sv, WiFiManagerParameter *el);
static void setCBVal(WiFiManagerParameter *el, char *sv);
static void strcpyutf8(char *dst, const char *src, unsigned int len);
static void handleStream(int idx);

static void handleMQTTTopMsg(int idx);
static void mqttPing();
static bool mqttReconnect(bool force = false);
static void mqttLooper();
static void mqttCallback(char *topic, byte *payload, unsigned int length);
static void mqttSubscribe();

/*
 * wifi_setup()
 *
 */
void wifi_setup()
{
    int temp;

    WiFiManagerParameter *wifiParmArray[] = {

      #ifdef WM_CCM
      &custom_asel,
      
      &custom_sectstart_cm,
      &custom_cmhint,
      &custom_ssidcm,
      &custom_passcm,
      &custom_tcdssid,
      &custom_bssidcm,
      &custom_ecm,
      #endif
      
      &custom_hostName,

      &custom_sectstart_wifi,
      &custom_wifiConRetries,

      &custom_sectstart_ap,
      &custom_sysID,
      &custom_appw,
      &custom_apch,
      &custom_bapch,
      &custom_wifiAPOffDelay,
      &custom_wifihint,

      NULL
      
    };
    
    WiFiManagerParameter *parmArray[] = {

      &custom_playJBSnd,
      &custom_playALSnd,
      &custom_NMB,

      &custom_sectstart_nw,
      &custom_tcdIP,
      &custom_uNM,
      &custom_ITT,
      
      &custom_haveSD,
      &custom_CfgOnSD,
      //&custom_sdFrq,

      &custom_waitPwrOn,
      
      NULL
    };

    WiFiManagerParameter *parm2Array[] = {

      &custom_streaming,

      NULL
    };

    WiFiManagerParameter *parm3Array[] = {

      &custom_useMQTT,
      &custom_state,
      &custom_mqttServer, 
      &custom_mqttVers,
      &custom_mqttUser,

      &custom_pubMP,

      &custom_mqttPwr,
      &custom_mqttPwrOn,

      &custom_mqtttm,
      &custom_mqmpii,

      &custom_sectstart_mqbc,
      &custom_mqttMPBC,
      &custom_mqttMPBCSK,
      &custom_mqttMPBCSV,
      &custom_mqttMPBCSO,
      &custom_mqttMPBCC,
      &custom_mqttMPBCF,
      &custom_mqttMPBCL,
      &custom_mqttMPBCV,
      &custom_mqttMPBCSHK,
      &custom_mqttMPBCSHV,

      NULL
    };

    // No carMode if no CM SSID
    if(!*settings.cm_ssid) carMode = false;

    // Transition from NVS-saved data to own management:
    if(!settings.ssid[0] && settings.ssid[1] == 'X') {
        
        // Read NVS-stored WiFi data
        wm.getStoredCredentials(settings.ssid, sizeof(settings.ssid), settings.pass, sizeof(settings.pass));

        #ifdef JB_DBG_WIFI
        Serial.printf("WiFi Transition: ssid '%s' pass '%s'\n", settings.ssid, settings.pass);
        #endif

        write_settings();
    }

    wm.setHostname(settings.hostName);

    wm.showUploadContainer(haveSD, AA_CONTAINER, rspv, haveAudioFiles);

    wm.setSaveWiFiCallback(saveWiFiCallback);
    wm.setSaveParamsCallback(saveParamsCallback);
    wm.setPreOtaUpdateCallback(preUpdateCallback);
    wm.setPostOtaUpdateCallback(postUpdateCallback);
    wm.setWebServerCallback(setupWebServerCallback);
    wm.setDelayReplacement(wifiDelayReplacement);
    wm.setGPCallback(gpCallback);
    wm.setPreWiFiScanCallback(preWiFiScanCallback);
    
    // Our style-overrides, the page title
    wm.setCustomHeadElement(myHead);
    wm.setTitle(myTitle);

    // Hack some stuff into WiFiManager main page
    wm.setCustomMenuHTML(myCustMenu);

    #ifdef WM_CCM
    wm.setCCarMode(carMode);
    wm.setCCarModeCallback(setCMCallback);
    #endif

    temp = atoi(settings.apChnl);
    if(temp < 0) temp = 0;
    else if(temp > 11) temp = 11;
    if(!temp) temp = random(1, 11);
    wm.setWiFiAPChannel(temp);

    temp = atoi(settings.wifiConRetries);
    if(temp < 1) temp = 1;
    else if(temp > 10) temp = 10;
    wm.setConnectRetries(temp);

    wm.setMenu(wifiMenu, TC_MENUSIZE, false);

    // WiFi Settings
    wm.allocParms(WM_PARM_WIFI, (sizeof(wifiParmArray) / sizeof(WiFiManagerParameter *)) - 1);
    temp = 0;
    while(wifiParmArray[temp]) {
        wm.addParameter(WM_PARM_WIFI, wifiParmArray[temp]);
        temp++;
    }

    // Settings
    wm.allocParms(WM_PARM_SETTINGS, (sizeof(parmArray) / sizeof(WiFiManagerParameter *)) - 1);
    temp = 0;
    while(parmArray[temp]) {
        wm.addParameter(WM_PARM_SETTINGS, parmArray[temp]);
        temp++;
    }

    // Streaming settings
    wm.allocParms(WM_PARM_SETTINGS2, (sizeof(parm2Array) / sizeof(WiFiManagerParameter *)) - 1);
    temp = 0;
    while(parm2Array[temp]) {
        wm.addParameter(WM_PARM_SETTINGS2, parm2Array[temp]);
        temp++;
    }

    // HA/MQTT
    wm.allocParms(WM_PARM_SETTINGS3, (sizeof(parm3Array) / sizeof(WiFiManagerParameter *)) - 1);
    temp = 0;
    while(parm3Array[temp]) {
        wm.addParameter(WM_PARM_SETTINGS3, parm3Array[temp]);
        temp++;
    }

    updateConfigPortalValues();

    useMQTT = evalBool(settings.useMQTT);

    wifiHaveSTAConf = carMode ? true : (settings.ssid[0] != 0);

    // See if we have a configured WiFi network to connect to.
    // If we detect "TCD-AP" as the SSID, we make sure that we retry
    // at least 2 times so we have a chance to catch the TCD's AP if 
    // both are powered up at the same time.
    // Also, we disable MQTT if connected to the TCD-AP.
    if(wifiHaveSTAConf) {
        if(carMode || !strncmp("TCD-AP", settings.ssid, 6)) {
            if(wm.getConnectRetries() < 2) {
                wm.setConnectRetries(2);
            }
            // Delay to give the TCD some time
            // (differs accross the props)
            delay(1400);
            useMQTT = false;
            connectedToTCDAP = true;
        }      
    } else {
        // No point in retry when we have no WiFi config'd
        wm.setConnectRetries(1);
    }

    // Read settings for WiFi powersave countdown
    wifiAPOffDelay = (unsigned long)atoi(settings.wifiAPOffDelay);
    if(wifiAPOffDelay > 0 && wifiAPOffDelay < 10) wifiAPOffDelay = 10;
    wifiAPOffDelay *= (60 * 1000);

    // Configure static IP
    if(loadIpSettings()) {
        if(checkIPConfig() && !carMode) {
            IPAddress ip = stringToIp(ipsettings.ip);
            IPAddress gw = stringToIp(ipsettings.gateway);
            IPAddress sn = stringToIp(ipsettings.netmask);
            IPAddress dns = stringToIp(ipsettings.dns);
            wm.setSTAStaticIPConfig(ip, gw, sn, dns);
        }
    }
           
    // Connect
    wifiConnect(true);

    // MDNS. Needs to be called AFTER mode(STA) or softAP init
    #ifdef TC_MDNS
    if(MDNS.begin(settings.hostName)) {
        MDNS.addService("http", "tcp", 80);
    }
    #endif

    checkForUpdate();

    if((!settings.mqttServer[0]) || // No server -> no MQTT
       (wifiInAPMode))              // WiFi in AP mode -> no MQTT
        useMQTT = false;  
    
    if(useMQTT) {

        uint16_t mqttPort = 1883;
        char *t;
        int tt;

        if((t = strchr(settings.mqttServer, ':'))) {
            size_t ts = (t - settings.mqttServer) + 1;
            mqttServer = (char *)malloc(ts);
            memset(mqttServer, 0, ts);
            strncpy(mqttServer, settings.mqttServer, t - settings.mqttServer);
            tt = atoi(t + 1);
            if(tt > 0 && tt <= 65535) {
                mqttPort = tt;
            }
        } else {
            mqttServer = settings.mqttServer;
        }

        if(isIp(mqttServer)) {
            mqttClient.setServer(stringToIp(mqttServer), mqttPort);
        } else {
            IPAddress remote_addr;
            if(WiFi.hostByName(mqttServer, remote_addr)) {
                mqttClient.setServer(remote_addr, mqttPort);
            } else {
                /*
                mqttClient.setServer(mqttServer, mqttPort);
                // Disable PING if we can't resolve domain
                mqttDoPing = false;
                */
                useMQTT = false;
                mqttReconnFails = 1; // Abuse for "resolv error"
                Serial.printf("MQTT: Failed to resolve '%s'\n", mqttServer);
            }
        }

        #ifdef JB_DBG_MQTT
        Serial.printf("MQTT: server '%s' port %d\n", mqttServer, mqttPort);
        #endif

    }

    if(useMQTT) {

        char *t;

        pubMP = evalBool(settings.pubMP);
        strcpy(mqbackchannel, "bttf/");
        strcat(mqbackchannel, settings.hostName);
        strcat(mqbackchannel, "/mpstatus");

        evalBoolSetClear(settings.mqttPwr, csf, CSF_MQTTPM);
        if(csf & CSF_MQTTPM) {
            evalBoolSetClear(settings.mqttPwrOn, csf, CSF_MQTTPWROFF);
        }
        
        mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
        mqttClient.setVersion(atoi(settings.mqttVers) > 0 ? 5 : 3);
        mqttClient.setClientID(settings.hostName);

        mqttClient.setCallback(mqttCallback);
        mqttClient.setLooper(mqttLooper);

        if(*settings.mqttUser) {
            if((t = strchr(settings.mqttUser, ':'))) {
                size_t ts = strlen(settings.mqttUser) + 1;
                mqttUser = (char *)malloc(ts);
                strcpy(mqttUser, settings.mqttUser);
                mqttUser[t - settings.mqttUser] = 0;
                mqttPass = mqttUser + (t - settings.mqttUser + 1);
            } else {
                mqttUser = settings.mqttUser;
            }
        }

        #ifdef JB_DBG_MQTT
        Serial.printf("MQTT: user '%s' pass '%s'\n", mqttUser, mqttPass);
        #endif

        mqttReconnect(true);
        mqttInitialConnectNow = millisNonZero();
        
        // Rest done in loop
            
    } else {

        csf &= ~(CSF_MQTTPM|CSF_MQTTPWROFF);

        #ifdef JB_DBG_MQTT
        Serial.println("MQTT: Disabled");
        #endif

    }

    // Start the Config Portal
    if(WiFi.status() == WL_CONNECTED) {
        wifiStartCP();
    }
}

/*
 * wifi_loop()
 *
 */
void wifi_loop()
{
    char oldCfgOnSD = 0;

    if(useMQTT) {
        if(mqttClient.state() != MQTT_CONNECTING) {
            if(!mqttClient.connected()) {
                if(mqttOldState || mqttRestartPing) {
                    // Disconnection first detected:
                    mqttPingDone = mqttDoPing ? false : true;
                    mqttPingNow = mqttRestartPing ? millisNonZero() : 0;
                    mqttOldState = false;
                    mqttRestartPing = false;
                    mqttSubAttempted = false;
                }
                if(mqttDoPing && !mqttPingDone) {
                    audio_loop();
                    mqttPing();
                    audio_loop();
                    fade_loop();
                }
                if(mqttPingDone) {
                    audio_loop();
                    mqttReconnect();
                    audio_loop();
                    fade_loop();
                }
            } else {
                // Only call Subscribe() if connected
                mqttSubscribe();
                mqttOldState = true;
                mqttInitialConnectNow = 0;
            }
        }
        mqttClient.loop();

        // Time-out waiting for MQTT connection upon boot in case MQTT 
        // has control over fake-power and we wait for POWER_ON.
        if(mqttInitialConnectNow && (csf & CSF_MQTTPM) && (csf & CSF_MQTTPWROFF)) {
            if(millis() - mqttInitialConnectNow > 40*1000) {
                mqttInitialConnectNow = 0;
                csf &= ~(CSF_MQTTPM|CSF_MQTTPWROFF);
                mqttFakePowerOn();
            }
        }

    }

    if(millis() - lastUpdateCheck > 24*60*60*1000) {
        if(checkAudioDone()) {
            checkForUpdate();
        }
    }

    #ifdef WM_CCM
    if(wifiLoopSaveAction & WLA_SET_CM) {
        bool ocm = carMode;
        carMode = !!(wifiLoopSaveAction & WLA_SET_CM_ON);
        if(!*settings.cm_ssid) carMode = false;
        if(carMode != ocm) {
            csf |= CSF_BUSY;    // Force MP "off" state
            mp_stop(true, true);
            stopAudio();
            saveCarMode();
            if(!(wifiLoopSaveAction & WLA_SET)) {
                prepareReboot();
                delay(1000);
                esp_restart();
            }
        }
        wifiLoopSaveAction &= ~(WLA_SET_CM|WLA_SET_CM_ON);
    }
    #endif

    if(wifiLoopSaveAction & WLA_SET) {

        int temp;
        
        csf |= CSF_BUSY;    // Force MP "off" state
        mp_stop(true, true);
        st_stop(true);
        stopAudio();

        // Save settings and restart esp32

        #ifdef JB_DBG_WIFI
        Serial.println("Config Portal: Saving config");
        #endif

        if(wifiLoopSaveAction & WLA_WIFI) {

            // Parameters on WiFi Config page
            // Note: Parameters that need to be grabbed from the server directly
            // through getServerParam() must be handled in saveWiFiCallback().
            
            // Static IP
            if(wifiLoopSaveAction & WLA_IP) {
                #ifdef JB_DBG_WIFI
                Serial.println("WiFi: Saving IP config");
                #endif
                writeIpSettings();
            } else if(wifiLoopSaveAction & WLA_DEL_IP) {
                #ifdef JB_DBG_WIFI
                Serial.println("WiFi: Deleting IP config");
                #endif
                deleteIpSettings();
            }

            // ssid, pass, bssid copied to settings in saveWiFiCallback()

            #ifdef WM_CCM
            strcpytrim(settings.cm_ssid, custom_ssidcm.getValue(), true);
            strcpytrim(settings.cm_pass, custom_passcm.getValue(), true);
            strcpytrimMAC(settings.cm_bssid, custom_bssidcm.getValue());

            if(*settings.cm_ssid) {
                evalCB(settings.ecmKludge, &custom_ecm);
                carMode = evalBool(settings.ecmKludge);
                saveCarMode();
            }
            #endif
            
            strcpytrim(settings.hostName, custom_hostName.getValue(), true);
            if(!*settings.hostName) {
                strcpy(settings.hostName, DEF_HOSTNAME);
            } else {
                char *s = settings.hostName;
                for( ; *s; ++s) *s = tolower(*s);
            }
            mystrcpy(settings.wifiConRetries, &custom_wifiConRetries);
            
            strcpytrim(settings.systemID, custom_sysID.getValue(), true);
            strcpytrim(settings.appw, custom_appw.getValue(), true);
            if((temp = strlen(settings.appw)) > 0) {
                if(temp < 8) {
                    settings.appw[0] = 0;
                }
            }
            mystrcpyWiFiDelay(settings.wifiAPOffDelay, &custom_wifiAPOffDelay);
          
        } else if(wifiLoopSaveAction & WLA_SET1) {

            // Parameters on Settings page
            // Note: Parameters that need to be grabbed from the server directly
            // through getServerParam() must be handled in saveParamsCallback().

            evalCB(settings.playJB, &custom_playJBSnd);
            evalCB(settings.playALsnd, &custom_playALSnd);
            evalCB(settings.NMB, &custom_NMB);

            strcpytrim(settings.tcdIP, custom_tcdIP.getValue());
            if(*settings.tcdIP) {
                char *s = settings.tcdIP;
                for ( ; *s; ++s) *s = tolower(*s);
            }
            evalCB(settings.useNM, &custom_uNM);
            evalCB(settings.ignTT, &custom_ITT);
            
            oldCfgOnSD = settings.CfgOnSD[0];
            evalCB(settings.CfgOnSD, &custom_CfgOnSD);
            //evalCB(settings.sdFreq, &custom_sdFrq);
            
            evalCB(settings.waitForPower, &custom_waitPwrOn);

            // Copy SD-saved settings to other medium if
            // user changed respective option
            if(oldCfgOnSD != settings.CfgOnSD[0]) {
                moveSettings();
            }

        } else if(wifiLoopSaveAction & WLA_SET2) {

            // Streams all handled in saveParamsCallback() 

        } else if(wifiLoopSaveAction & WLA_SET3) {

            // Parameters on HA/MQTT Settings page
            // Note: Parameters that need to be grabbed from the server directly
            // through getServerParam() must be handled in saveParamsCallback().

            evalCB(settings.useMQTT, &custom_useMQTT);
            strcpytrim(settings.mqttServer, custom_mqttServer.getValue());
            strcpyutf8(settings.mqttUser, custom_mqttUser.getValue(), sizeof(settings.mqttUser));

            evalCB(settings.pubMP, &custom_pubMP);

            evalCB(settings.mqttPwr, &custom_mqttPwr);
            evalCB(settings.mqttPwrOn, &custom_mqttPwrOn);

            evalCB(settings.mqmpii, &custom_mqmpii);

            strcpyutf8(settings.mqmpbackchannel, custom_mqttMPBC.getValue(), sizeof(settings.mqmpbackchannel));

            strcpyutf8(settings.mqmpbc_state_key, custom_mqttMPBCSK.getValue(), sizeof(settings.mqmpbc_state_key));
            strcpyutf8(settings.mqmpbc_state_val_play, custom_mqttMPBCSV.getValue(), sizeof(settings.mqmpbc_state_val_play));
            strcpyutf8(settings.mqmpbc_state_val_off, custom_mqttMPBCSO.getValue(), sizeof(settings.mqmpbc_state_val_off));
            strcpyutf8(settings.mqmpbc_curTrack_key, custom_mqttMPBCC.getValue(), sizeof(settings.mqmpbc_curTrack_key));
            strcpyutf8(settings.mqmpbc_firstTrack_key, custom_mqttMPBCF.getValue(), sizeof(settings.mqmpbc_firstTrack_key));
            strcpyutf8(settings.mqmpbc_lastTrack_key, custom_mqttMPBCL.getValue(), sizeof(settings.mqmpbc_lastTrack_key));
            strcpyutf8(settings.mqmpbc_volume_key, custom_mqttMPBCV.getValue(), sizeof(settings.mqmpbc_volume_key));
            strcpyutf8(settings.mqmpbc_shuffle_key, custom_mqttMPBCSHK.getValue(), sizeof(settings.mqmpbc_shuffle_key));
            strcpyutf8(settings.mqmpbc_shuffle_val_on, custom_mqttMPBCSHV.getValue(), sizeof(settings.mqmpbc_shuffle_val_on));

        }

        write_settings();

        // Reset esp32 to load new settings

        #ifdef JB_DBG_WIFI
        Serial.println("Config Portal: Restarting ESP....");
        #endif
        Serial.flush();

        prepareReboot();
        delay(1000);
        esp_restart();
    }

    wm.process();

    // WiFi power management
    // If a delay > 0 is configured, WiFi is powered-down after timer has
    // run out. The timer starts when the device is powered-up/boots.
    // There are separate delays for AP mode and STA mode.
    // WiFi can be re-enabled for the configured time by holding '7'
    // on the keypad.
    // NTP requests will - under some conditions - re-enable WiFi for a 
    // short while automatically if the user configured a WiFi network 
    // to connect to.
    
    if(wifiInAPMode) {
        // Disable WiFi in AP mode after a configurable delay (if > 0)
        if(wifiAPOffDelay > 0) {
            if(!wifiAPIsOff && (millis() - wifiAPModeNow >= wifiAPOffDelay)) {
                if(!WiFi.softAPgetStationNum()) {
                    wifiOff(false);
                    wifiAPIsOff = true;
                    wifiIsOff = false;
                    #ifdef JB_DBG_WIFI
                    Serial.println("WiFi (AP-mode) switched off (power-save)");
                    #endif
                } else {
                    wifiAPModeNow += (wifiAPOffDelay / 10);   // Check again later
                    #ifdef JB_DBG_WIFI
                    Serial.println("WiFi (AP-mode) NOT switched off (power-save) - client connected to AP");
                    #endif
                }
            }
        }
    }
}

static void wifiConnect(bool deferConfigPortal)
{     
    char realAPName[16];
    char *mssid, *mpass, *mbssid;

    if(carMode) {
        mssid = settings.cm_ssid;
        mpass = settings.cm_pass;
        mbssid = settings.cm_bssid;
    } else {
        mssid = settings.ssid;
        mpass = settings.pass;
        mbssid = settings.bssid;  
    }

    strcpy(realAPName, apName);
    if(settings.systemID[0]) {
        strcat(realAPName, settings.systemID);
    }
    
    // Connect using saved credentials if they exist
    // If connection fails it starts an access point with the specified name
    if(wm.wifiConnect(mssid, mpass, mbssid, realAPName, settings.appw)) {
        #ifdef FC_DBG
        Serial.println("WiFi connected");
        #endif

        // During boot, we start the CP later
        if(!deferConfigPortal) {
            wm.startWebPortal();
        }

        // Allow modem sleep:
        // WIFI_PS_MIN_MODEM is the default, and activated when calling this
        // with "true". When this is enabled, received WiFi data can be
        // delayed for as long as the DTIM period.
        // Disable modem sleep, don't want delays accessing the CP or
        // with MQTT, let alone the Remote.
        WiFi.setSleep(false);

        // Set transmit power to max; we might be connecting as STA after
        // a previous period in AP mode.
        #ifdef JB_DBG_WIFI
        {
            wifi_power_t power = WiFi.getTxPower();
            Serial.printf("WiFi: Max TX power in STA mode %d\n", power);
        }
        #endif
        WiFi.setTxPower(WIFI_POWER_19_5dBm);

        wifiInAPMode = false;
        wifiIsOff = false;
        wifiAPIsOff = false;  // Sic! Allows checks like if(wifiAPIsOff || wifiIsOff)

    } else {

        #ifdef JB_DBG_WIFI
        Serial.println("Config portal running in AP-mode");
        #endif

        {
            #ifdef JB_DBG_WIFI
            int8_t power;
            esp_wifi_get_max_tx_power(&power);
            Serial.printf("WiFi: Max TX power in AP mode %d\n", power);
            #endif

            // Try to avoid "burning" the ESP when the WiFi mode
            // is "AP" and the vol knob is fully up by reducing
            // the max. transmit power.
            // This has been fixed with CB 1.4 but much more is
            // simply not needed. The props are close together,
            // and we don't want to burn power needlessly.
            // The choices are:
            // WIFI_POWER_19_5dBm    = 19.5dBm
            // WIFI_POWER_19dBm      = 19dBm
            // WIFI_POWER_18_5dBm    = 18.5dBm
            // WIFI_POWER_17dBm      = 17dBm
            // WIFI_POWER_15dBm      = 15dBm
            // WIFI_POWER_13dBm      = 13dBm
            // WIFI_POWER_11dBm      = 11dBm
            // WIFI_POWER_8_5dBm     = 8.5dBm
            // WIFI_POWER_7dBm       = 7dBm     <-- proven to avoid the issues on CB < 1.4
            // WIFI_POWER_5dBm       = 5dBm
            // WIFI_POWER_2dBm       = 2dBm
            // WIFI_POWER_MINUS_1dBm = -1dBm

            WiFi.setTxPower(WIFI_POWER_7dBm);

            #ifdef JB_DBG_WIFI
            esp_wifi_get_max_tx_power(&power);
            Serial.printf("WiFi: Max TX power set to %d\n", power);
            #endif
        }

        wifiInAPMode = true;
        wifiAPIsOff = false;
        wifiAPModeNow = millis();
        wifiIsOff = false;    // Sic!

    }
}

// This must not be called if no power-saving
// timers are configured.
static void wifiOff(bool force)
{
    if(!force) {
        if( (!wifiInAPMode && wifiIsOff) ||
            (wifiInAPMode && wifiAPIsOff) ) {
            return;
        }
    }

    // Parm for disableWiFi() is "waitForOFF"
    // which should be true if we stop in AP
    // mode and immediately re-connect, without
    // process()ing for a while after this call.
    // "force" is true if we want to try to
    // reconnect after disableWiFi(), false if 
    // we disconnect upon timer expiration, 
    // so it matches the purpose.
    // "false" also does not cause any delays,
    // while "true" may take up to 2 seconds.
    wm.disableWiFi(force);
}

void wifiOn(unsigned long newDelay, bool alsoInAPMode, bool deferCP)
{
    unsigned long desiredDelay;
    unsigned long Now = millis();
    
    // wifiON() is called when holding the middle button for 2 seconds while
    // fake-power is off (with alsoInAPMode TRUE)
    //
    // This serves two purposes: To re-enable WiFi if in power save
    // mode, and to re-connect to a configured WiFi network if we failed to 
    // connect to that network at the last connection attempt. In both cases,
    // the Config Portal is started.
    //
    // "wifiInAPMode" only tells us our latest mode; if the configured WiFi
    // network was - for whatever reason - was not available when we
    // tried to (re)connect, "wifiInAPMode" is true.

    // At this point, wifiInAPMode reflects the state after
    // the last connection attempt.

    if(alsoInAPMode) {    // User initiated
        
        if(wifiInAPMode) {  // We are in AP mode

            if(!wifiAPIsOff) {

                // If ON but no user-config'd WiFi network -> bail
                // Note: In carMode, wifiHaveSTAConf is false
                if(!wifiHaveSTAConf) {
                    // Best we can do is to restart the timer
                    wifiAPModeNow = Now;
                    return;
                }

                // Make sure we don't cause stutter
                stopAudio();

                // If ON and User has config's a NW, disable WiFi at this point
                // (in hope of successful connection below)
                wifiOff(true);

            }

        } else {            // We are in STA mode

            // If WiFi is not off, check if caller wanted
            // to start the CP, and do so, if not running
            if(!wifiIsOff && (WiFi.status() == WL_CONNECTED)) {
                if(!deferCP) {
                    if(!wm.getWebPortalActive()) {
                        wm.startWebPortal();
                    }
                }
                return;
            }

        }

    } else {
      
        return;

    }

    // (Re)connect
    wifiConnect(deferCP);

    // Restart timers
    // Note that wifiInAPMode now reflects the
    // result of our above wifiConnect() call

    if(wifiInAPMode) {

        #ifdef JB_DBG_WIFI
        Serial.println("wifiOn: in AP mode after connect");
        #endif
      
        wifiAPModeNow = Now;
        
        #ifdef JB_DBG_WIFI
        if(wifiAPOffDelay > 0) {
            Serial.printf("Restarting WiFi-off timer (AP mode); delay %d\n", wifiAPOffDelay);
        }
        #endif
        
    } else {

        #ifdef JB_DBG_WIFI
        Serial.println("wifiOn: in STA mode after connect");
        #endif

        if(!lastUpdateLiveCheck || (millis() - lastUpdateLiveCheck > 6*60*60*1000)) {
            checkForUpdate();
        }

    }
}

// Check if a longer interruption due to a re-connect is to
// be expected when calling wifiOn(true, xxx).
bool wifiOnWillBlock()
{
    if(wifiInAPMode) {  // We are in AP mode
        if(!wifiAPIsOff) {
            if(!wifiHaveSTAConf) {
                return false;
            }
        }
    } else {            // We are in STA mode
        if(!wifiIsOff && (WiFi.status() == WL_CONNECTED)) 
            return false;
    }

    return true;
}

void wifiRestartPSTimer()
{
    if(wifiInAPMode) {
        wifiAPModeNow = millis();
    }
}

void wifiStartCP()
{
    if(wifiInAPMode || wifiIsOff)
        return;

    #ifdef JB_DBG_WIFI
    Serial.println("Starting CP");
    #endif

    wm.startWebPortal();
}

static void checkForUpdate()
{
    int cver = 0, crev = 0, uver = 0, urev = 0;
    bool haveCVer = false;

    *newversion = 0;

    lastUpdateCheck = millis();

    if(connectedToTCDAP || sscanf(CURRVERSION, "V%d.%d", &cver, &crev) != 2) {
        lastUpdateLiveCheck = millisNonZero();
        return;
    }
    
    if(WiFi.status() == WL_CONNECTED) {
        IPAddress remote_addr;
        if(WiFi.hostByName(WEBHOME "v.out-a-ti.me", remote_addr)) {
            if(remote_addr[0] + remote_addr[1] == remote_addr[3]) {
                uver = remote_addr[0]; urev = remote_addr[1];
                if(uver) saveUpdVers(uver, urev);
            }
        }
        lastUpdateLiveCheck = millisNonZero();
    } else {
        loadUpdVers(uver, urev);
    }

    if(uver) {
        haveCVer = true;
        if(((uver << 8) | urev) > ((cver << 8) | crev)) {
            snprintf(newversion, sizeof(newversion), "%d.%d", uver, urev);
        }
    }

    wm.setDownloadLink(r_link, haveCVer, (*newversion) ? newversion : NULL);
}

bool updateAvailable()
{
    return (*newversion != 0);
}

bool checkIPConfig()
{
    return (*ipsettings.ip            &&
            isIp(ipsettings.ip)       &&
            isIp(ipsettings.gateway)  &&
            isIp(ipsettings.netmask)  &&
            isIp(ipsettings.dns));
}

// This is called when the WiFi config is to be saved. 
// SSID, password and BSSID are copied to settings here.
// Static IP and other parameters are taken from WiFiManager's server.
static void saveWiFiCallback(const char *ssid, const char *pass, const char *bssid)
{
    char ipBuf[20] = "";
    char gwBuf[20] = "";
    char snBuf[20] = "";
    char dnsBuf[20] = "";
    bool invalConf = false;

    #ifdef JB_DBG_WIFI
    Serial.println("saveWiFiCallback");
    #endif

    // clear as strncpy might leave us unterminated
    memset(ipBuf, 0, 20);
    memset(snBuf, 0, 20);
    memset(gwBuf, 0, 20);
    memset(dnsBuf, 0, 20);

    String temp;
    temp.reserve(24);
    if((temp = wm.server->arg(FPSTR(WMS_ip))) != "") {
        strncpy(ipBuf, temp.c_str(), 19);
    } else invalConf |= true;
    if((temp = wm.server->arg(FPSTR(WMS_sn))) != "") {
        strncpy(snBuf, temp.c_str(), 19);
    } else invalConf |= true;
    if((temp = wm.server->arg(FPSTR(WMS_gw))) != "") {
        strncpy(gwBuf, temp.c_str(), 19);
    } else invalConf |= true;
    if((temp = wm.server->arg(FPSTR(WMS_dns))) != "") {
        strncpy(dnsBuf, temp.c_str(), 19);
    } else invalConf |= true;

    #ifdef JB_DBG_WIFI
    if(*ipBuf) {
        Serial.printf("IP:%s / SN:%s / GW:%s / DNS:%s\n", ipBuf, snBuf, gwBuf, dnsBuf);
    } else {
        Serial.println("Static IP unset, using DHCP");
    }
    #endif

    if(!invalConf && isIp(ipBuf) && isIp(gwBuf) && isIp(snBuf) && isIp(dnsBuf)) {

        #ifdef JB_DBG_WIFI
        Serial.println("All IPs valid");
        #endif

        wifiLoopSaveAction |= WLA_IP;
          
        memset((void *)&ipsettings, 0, sizeof(ipsettings));
        strcpy(ipsettings.ip, ipBuf);
        strcpy(ipsettings.gateway, gwBuf);
        strcpy(ipsettings.netmask, snBuf);
        strcpy(ipsettings.dns, dnsBuf);

    } else {

        #ifdef JB_DBG_WIFI
        if(*ipBuf) {
            Serial.println("Invalid static IP setup");
        }
        #endif

        wifiLoopSaveAction |= WLA_DEL_IP;

    }
   
    // ssid is the (new?) ssid to connect to, pass the password.
    // (We don't need to compare to the old ones, the settings
    // hash will decide on save/not save.)
    // This is also used to "forget" a saved WiFi network, in
    // which case ssid and pass are empty strings.
    clearWiFiCredentials();
    if(*ssid) {
        strncpy(settings.ssid, ssid, sizeof(settings.ssid) - 1);
        strncpy(settings.pass, pass, sizeof(settings.pass) - 1);
        strncpy(settings.bssid, bssid, sizeof(settings.bssid) - 1);
    }

    #ifdef JB_DBG_WIFI
    Serial.printf("saveWiFiCallback: New ssid '%s'\n", settings.ssid);
    Serial.printf("saveWiFiCallback: New pass '%s'\n", settings.pass);
    Serial.printf("saveWiFiCallback: New bssid '%s'\n", settings.bssid);
    #endif

    // Other parameters on WiFi Config page that
    // need grabbing directly from the server

    getServerParam("apchnl", settings.apChnl, 2, 0, 11, DEF_AP_CHANNEL);
    
    wifiLoopSaveAction |= WLA_WIFI;
}

// This is the callback from the actual Params page. We set
// a flag for the loop to read out and save the new settings.
// paramspage is 1 (settings), 2 (HA/MQTT settings)
static void saveParamsCallback(int paramspage)
{
    wifiLoopSaveAction |= (1 << (paramspage - 1 + WLA_SET1_B));

    switch(paramspage) {
    case 1:
        break;
    case 2:
        for(int i = 0; i < NUM_STREAMS; i++) handleStream(i);
        break;
    case 3:
        getServerParam("mprot", settings.mqttVers, 1, 0, 1, 0);
        for(int i = 0; i < MQ_NUM; i++) handleMQTTTopMsg(i);
        break;
    }
}

// This is called before a firmware updated is initiated.
// Disables WiFi-off-timers and audio, flushes pending save operations
static void preUpdateCallback()
{
    wifiAPOffDelay = 0;

    csf |= CSF_BUSY;    // Force MP "off" state
    mp_stop(true, true);
    stopAudio();

    flushDelayedSave();

    showWaitSequence();
}

// This is called after a firmware updated has finished.
// parm = true of ok, false if error. WM reboots only 
// if the update worked, ie when res is true.
static void postUpdateCallback(bool res)
{
    Serial.flush();
    prepareReboot();

    // WM does not reboot on OTA update errors.
    // However, our preparation destroyed too 
    // much, so we reboot anyway.
    if(!res) {
        delay(1000);
        esp_restart();
    }
}

static bool preWiFiScanCallback()
{
    st_stop();
    return true;
}

static void wifiDelayReplacement(unsigned int mydel)
{
    // Called when WM needs to delay to wait for
    // stuff. Must call delay() inside to yield.
    // MUST NOT call wifi_loop() !!!
    if((mydel > 30) && audioInitDone) {
        unsigned long startNow = millis();
        while(millis() - startNow < mydel) {
            audio_loop();
            fade_loop();
            delay(20);
            audio_loop();
            fade_loop();
        }
    } else {
        delay(mydel);
    }
}

void gpCallback(int reason)
{
    // Called when WM does stuff that might
    // take some time, like before and after
    // HTTPSend().
    // MUST NOT call wifi_loop() !!!
    
    if(audioInitDone) {
        switch(reason) {
        case WM_LP_PREHTTPSEND:
        case WM_LP_NONE:
        case WM_LP_POSTHTTPSEND:
            audio_loop();
            fade_loop();
            break;
        }
    }
}

#ifdef WM_CCM
static void setCMCallback(bool enable)
{
    wifiLoopSaveAction |= WLA_SET_CM;
    if(enable) wifiLoopSaveAction |= WLA_SET_CM_ON;
    else       wifiLoopSaveAction &= ~WLA_SET_CM_ON;
}
#endif

static void updateConfigPortalValues()
{
    // Make sure the settings form has the correct values

    #ifdef WM_CCM
    custom_ssidcm.setValue(settings.cm_ssid);
    custom_passcm.setValue(settings.cm_pass);
    custom_bssidcm.setValue(settings.cm_bssid);
    #endif

    custom_hostName.setValue(settings.hostName);
    custom_wifiConRetries.setValue(settings.wifiConRetries);

    custom_sysID.setValue(settings.systemID);
    custom_appw.setValue(settings.appw);
    // ap channel done on-the-fly
    custom_wifiAPOffDelay.setValue(settings.wifiAPOffDelay);

    setCBVal(&custom_playJBSnd, settings.playJB);
    setCBVal(&custom_playALSnd, settings.playALsnd);
    setCBVal(&custom_NMB, settings.NMB);

    custom_tcdIP.setValue(settings.tcdIP);
    setCBVal(&custom_uNM, settings.useNM);
    setCBVal(&custom_ITT, settings.ignTT);

    setCBVal(&custom_CfgOnSD, settings.CfgOnSD);
    //setCBVal(&custom_sdFrq, settings.sdFreq);

    setCBVal(&custom_waitPwrOn, settings.waitForPower);

    setCBVal(&custom_useMQTT, settings.useMQTT);
    custom_mqttServer.setValue(settings.mqttServer);
    // protocol version done on-the-fly
    custom_mqttUser.setValue(settings.mqttUser);

    setCBVal(&custom_pubMP, settings.pubMP);
    
    setCBVal(&custom_mqttPwr, settings.mqttPwr);
    setCBVal(&custom_mqttPwrOn, settings.mqttPwrOn);

    setCBVal(&custom_mqmpii, settings.mqmpii);

    custom_mqttMPBC.setValue(settings.mqmpbackchannel);
    custom_mqttMPBCSK.setValue(settings.mqmpbc_state_key);
    custom_mqttMPBCSV.setValue(settings.mqmpbc_state_val_play);
    custom_mqttMPBCSO.setValue(settings.mqmpbc_state_val_off);
    custom_mqttMPBCC.setValue(settings.mqmpbc_curTrack_key);
    custom_mqttMPBCF.setValue(settings.mqmpbc_firstTrack_key);
    custom_mqttMPBCL.setValue(settings.mqmpbc_lastTrack_key);
    custom_mqttMPBCV.setValue(settings.mqmpbc_volume_key);
    custom_mqttMPBCSHK.setValue(settings.mqmpbc_shuffle_key);
    custom_mqttMPBCSHV.setValue(settings.mqmpbc_shuffle_val_on);
}

static unsigned int calcSelectMenu(const char **theHTML, int cnt, char *setting, bool indent = false)
{
    int sr = atoi(setting);

    unsigned int l = 0;

    l += STRLEN(custHTMLHdr1);
    if(indent) l += STRLEN(custHTMLHdrI);
    l += STRLEN(custHTMLHdr2);
    l += strlen(theHTML[0]);
    l += STRLEN(custHTMLSHdr);
    l += strlen(setting);
    l += (STRLEN(custHTMLSelFmt) - (2*2));
    l += (3*strlen(theHTML[1]));
    for(int i = 0; i < cnt - 2; i++) {
        if(sr == i) l += STRLEN(custHTMLSel);
        l += (strlen(theHTML[i+2]) - 2);
        l += strlen((i == cnt - 3) ? osde : ooe);
    }

    return l + 8;
}

static void buildSelectMenu(char *target, const char **theHTML, int cnt, char *setting, bool indent = false)
{
    int sr = atoi(setting);

    strcpy(target, custHTMLHdr1);
    if(indent) strcat(target, custHTMLHdrI);
    strcat(target, custHTMLHdr2);
    strcat(target, theHTML[1]);
    strcat(target, theHTML[0]);
    strcat(target, custHTMLSHdr);
    strcat(target, setting);
    sprintf(target + strlen(target), custHTMLSelFmt, theHTML[1], theHTML[1]);
    for(int i = 0; i < cnt - 2; i++) {
        if(sr == i) strcat(target, custHTMLSel);
        sprintf(target + strlen(target), 
            theHTML[i+2], (i == cnt - 3) ? osde : ooe);
    }
}

static const char *wmBuildSelect(const char *dest, int op, const char **src, int count, char *setting, bool indent = false)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }

    unsigned int l = calcSelectMenu(src, count, setting, indent);

    if(op == WM_CP_LEN) {
        wmLenBuf = l;
        return (const char *)&wmLenBuf;
    }
    
    char *str = (char *)malloc(l);

    buildSelectMenu(str, src, count, setting, indent);
    
    return str;
}

#ifdef WM_CCM
static const char *wmBuildTCDAPList(const char *dest, int op)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }

    unsigned int l = STRLEN(tcdList) + 4 + (bttfnHaveTCDSSID ? strlen(TCDSSID) : 0);

    if(op == WM_CP_LEN) {
        wmLenBuf = l;
        return (const char *)&wmLenBuf;
    }

    char *str = (char *)malloc(l);

    sprintf(str, tcdList, bttfnHaveTCDSSID ? TCDSSID : "");

    return str;
}

static const char *wmBuildTCDSSID(const char *dest, int op)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }

    if(!bttfnHaveTCDSSID)
        return NULL;

    unsigned int l = STRLEN(tcdSSIDp) + (TCDpwMarker ? STRLEN(tcdAPPW2) : STRLEN(tcdAPPW1)) + 4;
    l += strlen(TCDSSID);

    if(op == WM_CP_LEN) {
        wmLenBuf = l;
        return (const char *)&wmLenBuf;
    }

    char *str = (char *)malloc(l);

    sprintf(str, tcdSSIDp, TCDSSID, TCDpwMarker ? tcdAPPW2 : tcdAPPW1);

    return str;
}
#endif

static const char *wmBuildApChnl(const char *dest, int op)
{
    return wmBuildSelect(dest, op, apChannelCustHTMLSrc, 14, settings.apChnl, false);
}

static const char *wmBuildBestApChnl(const char *dest, int op)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }

    int32_t mychan = 0;
    int qual = 0;

    if(wm.getBestAPChannel(mychan, qual)) {
        unsigned int l = STRLEN(bestAP) - (5*2) + STRLEN(bannerStart) + 6 + STRLEN(bannerMid) + 4 + STRLEN(badWiFi) + 1 + 8;
        if(op == WM_CP_LEN) {
            wmLenBuf = l;
            return (const char *)&wmLenBuf;
        }
        char *str = (char *)malloc(l);
        sprintf(str, bestAP, bannerStart, qual < 0 ? col_r : (qual > 0 ? col_g : col_gr), bannerMid, mychan, qual < 0 ? badWiFi : "");
        return str;
    }

    return NULL;
}

static const char *buildBanner(const char *msg, const char *col, int op) 
{   // "%s%s%s<i>%s</i></div>"
    unsigned int l = STRLEN(bannerStart) + STRLEN(bannerGen) - (2*4) + STRLEN(bannerMid) + strlen(msg) + 6 + 4;

    if(op == WM_CP_LEN) {
        wmLenBuf = l;
        return (const char *)&wmLenBuf;
    }
    
    char *str = (char *)malloc(l);
    sprintf(str, bannerGen, bannerStart, col, bannerMid, msg);        

    return str;
}

static const char *wmBuildHaveSD(const char *dest, int op)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }
    
    if(haveSD)
        return NULL;

    return buildBanner(haveNoSD, col_r, op);
}

static const char *wmBuildStreams(const char *dest, int op)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }
    const char HTTP_SECT_HEAD[] = "<div class='ss'><div class='hl'>Streaming URLs<br><span style='font-size:80%'>Only http, content type \"audio/mpeg\" and bitrates up to 128kbps supported.</span></div>";
    const char HTTP_SECT_FOOT[] = "</div>";
    const char streamtm1[] = "<label for='str%d'>URL for %c %d</label><br><input id='str%d' name='str%d' maxlength='127' value='%s'%s><br>";
    const char streamtmp1[] = " placeholder='Example: http://abc/stream'";
    const char msgStrHTTPErr[] = "HTTP error";
    const char msgStrBad[] = "Unsupported bit rate or content type";

    unsigned int l = 0;

    l += STRLEN(HTTP_SECT_HEAD) + STRLEN(HTTP_SECT_FOOT) + STRLEN(streamtmp1);
    for(int i = 0; i < NUM_STREAMS; i++) {
        l += STRLEN(streamtm1) - (2*7);
        l += 3*2;  // 3 * id number
        l += 3;    // 'A' '10'
        if(*settings.streams[i]) l += strlen(settings.streams[i]);
        if(streamStatus[i] == STRS_HTTPERR || streamStatus[i] == STRS_BAD) {
            l += STRLEN(bannerStart) + 6 + STRLEN(bannerMid) + STRLEN(HTTP_SECT_FOOT);
            l += (streamStatus[i] == STRS_HTTPERR) ? STRLEN(msgStrHTTPErr) : STRLEN(msgStrBad);
        }
    }

    if(op == WM_CP_LEN) {
        wmLenBuf = l;
        return (const char *)&wmLenBuf;
    }

    char *str = (char *)malloc(l + 8);

    strcpy(str, HTTP_SECT_HEAD);
    for(int i = 0; i < NUM_STREAMS; i++) {
        sprintf(str + strlen(str), streamtm1,
               i, (i / 10) + 'A', (i % 10) + 1, i, i, settings.streams[i], !i ? streamtmp1 : "");
        if(streamStatus[i] == STRS_HTTPERR || streamStatus[i] == STRS_BAD) {
            sprintf(str + strlen(str), "%s%s%s%s%s", bannerStart, "f00", bannerMid, 
                          (streamStatus[i] == STRS_HTTPERR) ? msgStrHTTPErr : msgStrBad, HTTP_SECT_FOOT);
        }
    }
    strcat(str, HTTP_SECT_FOOT);

    return str;
}

static const char *wmBuildMQTTprot(const char *dest, int op)
{
    return wmBuildSelect(dest, op, mqttpCustHTMLSrc, 4, settings.mqttVers, false);
}

static const char *wmBuildMQTTstate(const char *dest, int op)
{
    // Check original setting, not "useMQTT" which
    // might be overruled.
    if(!evalBool(settings.useMQTT)) {
        return NULL;
    }
    
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }

    int s = 0;
    const char *msg = NULL;
    const char *cls = col_r;

    if(!useMQTT) {
        if(mqttReconnFails) {
            msg = msgResolvErr;
        } else {
            msg = mqttMsgDisabled;
            cls = col_gr;
        }
    } else {
        s = mqttClient.state();
        switch(s) {
        case MQTT_CONNECTED:
            msg = mqttMsgConnected;
            cls = col_g;
            break;
        case MQTT_CONNECTING:
            msg = mqttMsgConnecting;
            cls = col_gr;
            break;
        case MQTT_CONNECTION_TIMEOUT:
            msg = mqttMsgTimeout;
            break;
        case MQTT_CONNECTION_LOST:
        case MQTT_CONNECT_FAILED:
            msg = mqttMsgFailed;
            break;
        case MQTT_DISCONNECTED:
            msg = mqttMsgDisconnected;
            break;
        case MQTT_CONNECT_BAD_PROTOCOL:
        case MQTT_CONNECT_BAD_CLIENT_ID:
        case 129:
        case 130:
        case 132:
        case 133:
            msg = mqttMsgBadProtocol;
            break;
        case MQTT_CONNECT_UNAVAILABLE:
        case 136:
        case 137:
            msg = mqttMsgUnavailable;
            break;
        case MQTT_CONNECT_BAD_CREDENTIALS:
        case MQTT_CONNECT_UNAUTHORIZED:
        case 134:
        case 135:
        case 138:
        case 140:
        case 156:
        case 157:
            msg = mqttMsgBadCred;
            break;
        default:
            msg = mqttMsgGenError;
            break;
        }
    }

    // "%s%s%s%s%s (%d)</div>"
    unsigned int l = STRLEN(mqttStatus) - (6*2) + STRLEN(bannerStart) + strlen(cls) + 20 + STRLEN(bannerMid) + strlen(msg) + 6;

    if(op == WM_CP_LEN) {
        wmLenBuf = l;
        return (const char *)&wmLenBuf;
    }

    char *str = (char *)malloc(l);

    sprintf(str, mqttStatus, bannerStart, cls, ";margin-bottom:10px", bannerMid, msg, s);

    return str;
}

static const char *wmBuildMQTTTM(const char *dest, int op)
{
    if(op == WM_CP_DESTROY) {
        if(dest) free((void *)dest);
        return NULL;
    }

    const char *mpcmds[MQ_NUM] = { 
      "play", "stop", "next", "prev", "go to song #", 
      "volume up' or 'volume_set", "volume down", "request status", 
      "shuffle on", "shuffle off"
    };
    const char HTTP_SECT_HEAD[] = "<div class='ss'><div class='hl'>Remote Player's commands</div>";
    //const char HTTP_SECT_FOOT[] = "</div>";
    const char mqtttm1[] = "<label for='mqmpt%d'>Topic/Message for '%s' command</label><br><input id='mqmpt%d' name='mqmpt%d' maxlength='63' value='%s'%s><br><input id='mqmp%d' name='mqmp%d' maxlength='63' value='%s' class='mb15'%s><br>";
    const char mqtttmp1[] = " placeholder='Example: bttf/tcd/cmd'";
    const char mqtttmp2[] = " placeholder='Example: MP_PLAY'";

    unsigned int l = 0;

    l += STRLEN(HTTP_SECT_HEAD) + /*STRLEN(HTTP_SECT_FOOT) +*/ STRLEN(mqtttmp1) + STRLEN(mqtttmp2);
    for(int i = 0; i < MQ_NUM; i++) {
        l += STRLEN(mqtttm1) - (2*10);
        l += 5 * 3;   // number for id, name; 5 times
        l += strlen(mpcmds[i]);
        l += strlen(settings.mqmpt[i]);
        l += strlen(settings.mqmp[i]);
    }

    if(op == WM_CP_LEN) {
        wmLenBuf = l;
        return (const char *)&wmLenBuf;
    }

    char *str = (char *)malloc(l + 8);

    strcpy(str, HTTP_SECT_HEAD);
    for(int i = 0; i < MQ_NUM; i++) {
        sprintf(str + strlen(str), mqtttm1,
               i, mpcmds[i], 
               i, i, settings.mqmpt[i], !i ? mqtttmp1 : "",
               i, i, settings.mqmp[i], !i ? mqtttmp2 : ""
        );
    }
    //strcat(str, HTTP_SECT_FOOT);

    return str;
}

/*
 * Audio data uploader
 */

static void setupWebServerCallback()
{
    wm.server->on(R_updateacdone, HTTP_POST, &handleUploadDone, &handleUploading);
}

static void doReboot()
{
    delay(1000);
    prepareReboot();
    delay(1000);
    esp_restart();
}

static void allocUplArrays()
{
    if(opType) free((void *)opType);
    opType = (int *)malloc(MAX_SIM_UPLOADS * sizeof(int));
    if(ACULerr) free((void *)ACULerr);
    ACULerr = (int *)malloc(MAX_SIM_UPLOADS * sizeof(int));;
    memset(opType, 0, MAX_SIM_UPLOADS * sizeof(int));
    memset(ACULerr, 0, MAX_SIM_UPLOADS * sizeof(int));
}

static void doCloseACFile(int idx, bool doRemove)
{
    if(haveACFile) {
        closeACFile(acFile);
        haveACFile = false;
    }
    if(doRemove) removeACFile(idx);  
}

static void handleUploading()
{
    HTTPUpload& upload = wm.server->upload();

    if(upload.status == UPLOAD_FILE_START) {

          String c = upload.filename;
          const char *illChrs = "|~><:*?\" ";
          int temp;
          char tempc;

          if(numUploads >= MAX_SIM_UPLOADS) {
            
              haveACFile = false;

              #ifdef JB_DBG_WIFI
              Serial.printf("handleUploading: Too many files, ignoring %s\n", c.c_str());
              #endif

          } else {

              c.toLowerCase();
    
              #ifdef JB_DBG_WIFI
              Serial.printf("handleUploading: Filenames: %s %s\n", upload.filename.c_str(), c.c_str());
              #endif
    
              // Remove path and some illegal characters
              tempc = '/';
              for(int i = 0; i < 2; i++) {
                  if((temp = c.lastIndexOf(tempc)) >= 0) {
                      if(c.length() - 1 > temp) {
                          c = c.substring(temp);
                      } else {
                          c = "";
                      }
                      break;
                  }
                  tempc = '\\';
              }
              for(int i = 0; i < strlen(illChrs); i++) {
                  c.replace(illChrs[i], '_');
              }
              if(!c.indexOf("..")) {
                  c.replace("..", "");
              }
    
              #ifdef JB_DBG_WIFI
              Serial.printf("handleUploading: Filename after sanitation: %s\n", c.c_str());
              #endif
    
              if(!numUploads) {
                  allocUplArrays();
                  preUpdateCallback();
              }
    
              haveACFile = openUploadFile(c, acFile, numUploads, haveAC, opType[numUploads], ACULerr[numUploads]);

              if(haveACFile && opType[numUploads] == 1) {
                  haveAC = true;
              }

          }

    } else if(upload.status == UPLOAD_FILE_WRITE) {

          if(haveACFile) {
              if(writeACFile(acFile, upload.buf, upload.currentSize) != upload.currentSize) {
                  doCloseACFile(numUploads, true);
                  ACULerr[numUploads] = UPL_WRERR;
              }
          }

    } else if(upload.status == UPLOAD_FILE_END) {

        if(numUploads < MAX_SIM_UPLOADS) {

            doCloseACFile(numUploads, false);
    
            if(opType[numUploads] >= 0) {
                renameUploadFile(numUploads);
            }
    
            numUploads++;

        }
      
    } else if(upload.status == UPLOAD_FILE_ABORTED) {

        if(numUploads < MAX_SIM_UPLOADS) {
            doCloseACFile(numUploads, true);
        }

        #ifdef JB_DBG_WIFI
        Serial.printf("handleUploading: Aborted - rebooting\n");
        #endif

        doReboot();

    }

    delay(0);
}

static void handleUploadDone()
{
    const char *ebuf = "ERROR";
    const char *dbuf = "DONE";
    char *buf = NULL;
    bool haveErrs = false;
    bool haveAC = false;
    int titStart = -1;
    int buflen  = strlen(wm.getHTTPSTART(titStart)) + // includes </title>
                  STRLEN(myTitle)    +   
                  strlen(wm.getHTTPSCRIPT()) +
                  strlen(wm.getHTTPSTYLE()) +
                  STRLEN(acul_part1) +
                  STRLEN(myHead)     +
                  STRLEN(acul_part3) +
                  STRLEN(myTitle)    +
                  STRLEN(acul_part5) +
                  STRLEN(acul_part8) +
                  1;

    #ifdef JB_DBG_WIFI
    Serial.printf("handleUploadDone: %d files uploaded\n", numUploads);
    #endif

    for(int i = 0; i < numUploads; i++) {
        if(opType[i] > 0) {
            haveAC = true;
            if(!ACULerr[i]) {
                if(!check_if_default_audio_present()) {
                    haveAC = false;
                    ACULerr[i] = UPL_BADERR;
                    removeACFile(i);
                }
            }
            break;
        }
    }    

    if(!haveSD && numUploads) {
      
        buflen += (STRLEN(acul_part71) + strlen(acul_errs[1]));
        
    } else {

        for(int i = 0; i < numUploads; i++) {
            if(ACULerr[i]) haveErrs = true;
        }
        if(haveErrs) {
            buflen += STRLEN(acul_part71);
            for(int i = 0; i < numUploads; i++) {
                if(ACULerr[i]) {
                    buflen += getUploadFileNameLen(i);
                    buflen += 2; // :_
                    buflen += strlen(acul_errs[ACULerr[i]-1]);
                    buflen += 4; // <br>
                }
            }
        } else {
            buflen += strlen(wm.getHTTPSTYLEOK());
            buflen += STRLEN(acul_part7);
        }
        if(haveAC) {
            buflen += STRLEN(acul_part7a);
        }
    }

    buflen += 8;

    if(!(buf = (char *)malloc(buflen))) {
        buf = (char *)(haveErrs ? ebuf : dbuf);
    } else {
        strcpy(buf, wm.getHTTPSTART(titStart));
        if(titStart >= 0) {
            strcpy(buf + titStart, myTitle);
            strcat(buf, "</title>");
        }
        strcat(buf, wm.getHTTPSCRIPT());
        strcat(buf, wm.getHTTPSTYLE());
        if(!haveErrs) {
            strcat(buf, wm.getHTTPSTYLEOK());
        }
        strcat(buf, acul_part1);
        strcat(buf, myHead);
        strcat(buf, acul_part3);
        strcat(buf, myTitle);
        strcat(buf, acul_part5);

        if(!haveSD && numUploads) {

            strcat(buf, acul_part71);
            strcat(buf, acul_errs[1]);
            
        } else {
            
            if(haveErrs) {
                strcat(buf, acul_part71);
                for(int i = 0; i < numUploads; i++) {
                    if(ACULerr[i]) {
                        char *t = getUploadFileName(i);
                        if(t) {
                            strcat(buf, t);
                        }
                        strcat(buf, ": ");
                        strcat(buf, acul_errs[ACULerr[i]-1]);
                        strcat(buf, "<br>");
                    }
                }
            } else {
                strcat(buf, acul_part7);
            }
            if(haveAC) {
                strcat(buf, acul_part7a);
            }
        }

        strcat(buf, acul_part8);
    }

    freeUploadFileNames();
    
    String str(buf);
    wm.server->send(200, "text/html", str);

    // Reboot required even for mp3 upload, because for most files, we check
    // during boot if they exist (to avoid repeatedly failing open() calls)

    doReboot();
}

/*
 * Helpers
 */
int wifi_getStatus()
{
    switch(WiFi.getMode()) {
      case WIFI_MODE_STA:
          return (int)WiFi.status();
      case WIFI_MODE_AP:
          return 0x10000;     // AP MODE
      case WIFI_MODE_APSTA:
          return 0x10003;     // AP/STA MIXED
      case WIFI_MODE_NULL:
          return 0x10001;     // OFF
    }

    return 0x10002;           // UNKNOWN
}

bool wifi_getIP(uint8_t& a, uint8_t& b, uint8_t& c, uint8_t& d)
{
    IPAddress myip;

    switch(WiFi.getMode()) {
      case WIFI_MODE_STA:
          myip = WiFi.localIP();
          break;
      case WIFI_MODE_AP:
      case WIFI_MODE_APSTA:
          myip = WiFi.softAPIP();
          break;
      default:
          a = b = c = d = 0;
          return true;
    }

    a = myip[0];
    b = myip[1];
    c = myip[2];
    d = myip[3];

    return true;
}

void wifi_getMAC(char *buf, bool sta, bool s)
{
    byte myMac[6];
    
    if(sta) WiFi.macAddress(myMac);
    else    WiFi.softAPmacAddress(myMac);
    sprintf(buf, s ? "%02x%02x%02x%02x%02x%02x" : "%02x:%02x:%02x:%02x:%02x:%02x", myMac[0], myMac[1], myMac[2], myMac[3], myMac[4], myMac[5]); 
}

// Check if String is a valid IP address
bool isIp(char *str)
{
    int segs = 0;
    int digcnt = 0;
    int num = 0;

    while(*str) {

        if(*str == '.') {

            if(!digcnt || (++segs == 4))
                return false;

            num = digcnt = 0;
            str++;
            continue;

        } else if((*str < '0') || (*str > '9')) {

            return false;

        }

        if((num = (num * 10) + (*str - '0')) > 255)
            return false;

        digcnt++;
        str++;
    }

    if(segs == 3)
        return true;

    return false;
}

// String to IPAddress
static IPAddress stringToIp(char *str)
{
    int ip1, ip2, ip3, ip4;

    sscanf(str, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4);

    return IPAddress(ip1, ip2, ip3, ip4);
}

/*
 * Read parameter from server, for customhmtl input
 */

static bool isNumString(char *s)
{
    for( ; *s; ++s) {
        if(*s < '0' || *s > '9') return false;
    }
    return true;
}

static void getServerParam(const char *name, char *destBuf, size_t length, int minval, int maxval, int defaultVal)
{
    int i;
    memset(destBuf, 0, length + 1);
    strncpy(destBuf, wm.server->arg(name).c_str(), length);
    if(*destBuf) {
        if(isNumString(destBuf)) i = atoi(destBuf);
        else *destBuf = 0;
    }
    if(!*destBuf || i < minval || i > maxval) {
        sprintf(destBuf, "%d", defaultVal);
    }
}

static bool myisspace(char mychar)
{
    return (mychar == ' ' || mychar == '\n' || mychar == '\t' || mychar == '\v' || mychar == '\f' || mychar == '\r');
}

static bool myisgoodchar(char mychar)
{
    return ((mychar >= '0' && mychar <= '9') || (mychar >= 'a' && mychar <= 'z') || (mychar >= 'A' && mychar <= 'Z') || mychar == '-');
}

static bool myisgoodchar2(char mychar)
{
    return ((mychar == ' '));
}

static char* strcpytrim(char* destination, const char* source, bool doFilter)
{
    char *ret = destination;
    
    while(*source) {
        if(!myisspace(*source) && (!doFilter || myisgoodchar(*source))) *destination++ = *source;
        source++;
    }
    
    *destination = 0;
    
    return ret;
}

static char* strcpyfilter(char *destination, const char *source)
{
    char *ret = destination;
    
    while(*source) {
        if(myisgoodchar(*source) || myisgoodchar2(*source)) *destination++ = *source;
        source++;
    }
    
    *destination = 0;
    
    return ret;
}

#ifdef WM_CCM
static bool myisgoodcharMAC(char mychar)
{
    return ((mychar >= '0' && mychar <= '9') || (mychar >= 'a' && mychar <= 'f') || (mychar >= 'A' && mychar <= 'F') || mychar == ':');
}

static char* strcpytrimMAC(char* destination, const char* source)
{
    char *ret = destination;
    
    while(*source) {
        if(!myisspace(*source) && myisgoodcharMAC(*source)) *destination++ = *source;
        source++;
    }
    
    *destination = 0;
    
    return ret;
}
#endif

static void mystrcpy(char *sv, WiFiManagerParameter *el)
{
    strcpy(sv, el->getValue());
}

static void mystrcpyWiFiDelay(char *sv, WiFiManagerParameter *el)
{
    int a = atoi(el->getValue());
    if(a > 0 && a < 10) a = 10;
    else if(a > 99)     a = 99;
    else if(a < 0)      a = 0;
    sprintf(sv, "%d", a);
}

static void evalCB(char *sv, WiFiManagerParameter *el)
{
    *sv++ = (*(el->getValue()) == '0') ? '0' : '1';
    *sv = 0;
}

static void setCBVal(WiFiManagerParameter *el, char *sv)
{   
    el->setValue((*sv == '0') ? "0" : "1");
}

int16_t filterOutUTF8(char *src, char *dst, int srcLen = 0, int maxChars = 99999)
{
    int i, j, slen = srcLen ? srcLen : strlen(src);
    unsigned char c, e;

    for(i = 0, j = 0; i < slen && j < maxChars; i++) {
        c = (unsigned char)src[i];
        if(c >= 32 && c <= 126) {     // skip 127 = DEL
            if(c >= 'a' && c <= 'z') c &= ~0x20;
            else if(c == 126) c = '-';   // 126 = ~ but displayed as °, so make '-'
            dst[j++] = c; 
        } else {
            e = 0;
            if     (c >= 192 && c < 224)  e = 1;
            else if(c >= 224 && c < 240)  e = 2;
            else if(c >= 240 && c < 245)  e = 3;    // yes, 245 (otherwise bad UTF8)
            if(e) {
                if((i + e) < slen) {
                    /*
                    for(k = i + 1, d = 0; k <= i + 1 + e; k++) {
                        d |= (unsigned char)src[k];
                    }
                    if(d > 127 && d < 192) i += e;
                    */
                    i += e;   // Why check? Just skip.
                } else {
                    break;
                }
            }
        }
    }
    dst[j] = 0;

    return j;
}

static void truncateUTF8(char *src)
{
    int i, slen = strlen(src);
    unsigned char c, e;

    for(i = 0; i < slen; i++) {
        c = (unsigned char)src[i];
        e = 0;
        if     (c >= 192 && c < 224)  e = 1;
        else if(c >= 224 && c < 240)  e = 2;
        else if(c >= 240 && c < 248)  e = 3;  // Invalid UTF8 >= 245, but consider 4-byte char anyway
        if(e) {
            if((i + e) < slen) {
                i += e;
            } else {
                src[i] = 0;
                return;
            }
        }
    }
}

static void strcpyutf8(char *dst, const char *src, unsigned int len)
{
    char *dest = dst;
    len--;  // leave room for 0
    while(*src && len--) {
        if(*src != '\'') *dst++ = *src;
        src++;
    }
    *dst = 0;
    truncateUTF8(dest);
}

static void handleStream(int idx)
{
    int sz;
    char id[8];
    const char idt[] = "str%d";

    sprintf(id, idt, idx);

    // We don't allow the single quote (') since it messes
    // up our HTML. Maybe escape it.. in the future.

    *settings.streams[idx] = 0;

    String tt = wm.server->arg(id);
    if((sz = tt.length())) {
        sz++;
        if(sz > 128) sz = 128;
        strcpyutf8(settings.streams[idx], tt.c_str(), sz);
    }
}

static void handleMQTTTopMsg(int idx)
{
    int sz;
    char buf[10];

    // We don't allow the single quote (') since it messes
    // up our HTML. Maybe escape it.. in the future.

    sprintf(buf, "mqmpt%d", idx);
    String tt = wm.server->arg(buf);
    memset(settings.mqmpt[idx], 0, sizeof(settings.mqmpt[0]));
    strcpyutf8(settings.mqmpt[idx], tt.c_str(), sizeof(settings.mqmpt[0]));

    sprintf(buf, "mqmp%d", idx);
    String tm = wm.server->arg(buf);
    memset(settings.mqmp[idx], 0, sizeof(settings.mqmp[0]));
    strcpyutf8(settings.mqmp[idx], tm.c_str(), sizeof(settings.mqmp[0]));
}


static void mqttLooper()
{
    audio_loop();
    fade_loop();
}

static uint16_t a2i(char *p)
{
    unsigned int t = 0;
    t += (*p++ - '0') * 1000;
    t += (*p++ - '0') * 100;
    t += (*p++ - '0') * 10;
    t += (*p - '0');

    return (uint16_t)t;
}

static void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    int i = 0, j, ml = (length <= 255) ? length : 255;
    char tempBuf[256];
    static const char *cmdList[] = {
      "\x41" "SHUFFLE_ON",       // 0
      "\x41" "SHUFFLE_OFF",      // 1
      "\x01" "PLAY",             // 2
      "\x41" "STOP",             // 3
      "\x01" "NEXT",             // 4
      "\x01" "PREV",             // 5
      "\x81" "FOLDER_",          // 6  FOLDER_A..FOLDER_K, FOLDER_1..FOLDER_10
      "\x01" "GOTO_",            // 7  GOTO_X_Y (X=A-K, Y=1-10)
      "\x01" "PLAYKEY_",         // 8  PLAYKEY_1..PLAYKEY_9
      "\x41" "STOPKEY",          // 9
      "\x01" "VOLUME_SET_",      // 10 VOLUME_SET_0..VOLUME_SET_100
      "\x81" "POWER_ON",         // 11 also when CSF_OFF
      "\x81" "POWER_OFF",        // 12 also when CSF_OFF
      "\x81" "POWER_CONTROL_ON", // 13 also when CSF_OFF
      "\x81" "POWER_CONTROL_OFF",// 14 also when CSF_OFF
      "\x01" "MODE_MP",          // 15
      "\x01" "MODE_STREAM",      // 16
      "\x01" "MODE_REMOTE",      // 17
      "\x01" "NIGHTMODE_OFF",    // 18
      "\x01" "NIGHTMODE_ON",     // 19
      "\x01" "VOLUME_UP",        // 20
      "\x01" "VOLUME_DOWN",      // 21
      "\xc1" "REQSTATUS",        // 22 also while off or busy
      NULL
    };
    static const char *cmdList2[] = {
      "PREPARE",          // 0
      "TIMETRAVEL",       // 1
      "REENTRY",          // 2
      "ABORT_TT",         // 3
      "ALARM",            // 4
      "WAKEUP",           // 5
      NULL
    };

    // Note: This might be called while we are in a
    // wait-delay-loop. Best to just set flags here
    // that are evaluated synchronously (=later).
    // Do not stuff that messes with display, input,
    // etc.

    if(!length) return;

    memcpy(tempBuf, (const char *)payload, ml);
    tempBuf[ml] = 0;

    j = strlen(topic);
    
    if(!strcmp(topic, "bttf/tcd/pub")) {

        for(j = 0; j < ml; j++) {
            if(tempBuf[j] >= 'a' && tempBuf[j] <= 'z') tempBuf[j] &= ~0x20;
        }

        // Commands from TCD

        while(cmdList2[i]) {
            j = strlen(cmdList2[i]);
            if((length >= j) && !strncmp((const char *)tempBuf, cmdList2[i], j)) {
                break;
            }
            i++;          
        }

        if(!cmdList2[i]) return;

        // No commands from TCD while we are off
        if(csf & CSF_OFF) return;

        switch(i) {
        case 0:
            // Prepare for TT. Comes at some undefined point,
            // an undefined time before the actual tt, and may
            // now come at all.
            // We disable our Screen Saver and start the flux
            // sound (if to be played)
            // We don't ignore this if TCD is connected by wire,
            // because this signal does not come via wire.
            if(csf & CSF_OFF) return;
            doPrepareTT = true;
            break;
        case 1:
            // Trigger Time Travel (if not running already)
            // Ignore command if TCD is connected by wire
            if(!(csf & (CSF_OFF|CSF_TT|CSF_BUSY))) {
                networkTimeTravel = true;
                networkTCDTT = true;
                networkReentry = false;
                networkAbort = false;
                if(strlen(tempBuf) == 20) {
                    networkLead = a2i(&tempBuf[11]);
                    networkP1 = a2i(&tempBuf[16]);
                } else {
                    networkLead = ETTO_LEAD;
                    networkP1 = 6600;
                }
            }
            break;
        case 2:   // Re-entry
            // Start re-entry (if TT currently running)
            if((csf & CSF_TT) && networkTCDTT && (!(csf & CSF_OFF))) {
                networkReentry = true;
            }
            break;
        case 3:   // Abort TT (TCD fake-powered down during TT)
            if((csf & CSF_TT) && networkTCDTT && (!(csf & CSF_OFF))) {
                networkAbort = true;
            }
            break;
        case 4:
            networkAlarm = true;
            // Eval this at our convenience
            break;
        case 5:
            if(csf & CSF_OFF) return;
            doWakeup = true;
            break;
        }
       
    } else if(j > 9 && !strcmp(topic + j - 4, "/cmd")) {

        // User commands

        int tblen = 0;
        uint8_t k = 0;
        char *cmdBuf = tempBuf;

        for(j = 0; j < ml; j++) {
            if(tempBuf[j] >= 'a' && tempBuf[j] <= 'z') tempBuf[j] &= ~0x20;
        }

        // Allow "MP_" prefix for compatibility with other props
        if(!strncmp((const char *)tempBuf, "MP_", 3)) {
            cmdBuf = tempBuf + 3;
        }

        while(cmdList[i]) {
            k = (uint8_t)*cmdList[i];
            j = strlen(cmdList[i] + 1);
            if((length >= j) && !strncmp((const char *)cmdBuf, cmdList[i] + 1, j)) {
                break;
            }
            i++;          
        }

        if(!cmdList[i]) return;

        if((csf & CSF_OFF) && (!(k & 0x80)))
            return;

        if((csf & CSF_BUSY) && (!(k & 0x40)))
            return;

        tblen = strlen(cmdBuf);

        // What needs to be handled here:
        // - complete command parsing
        // CmdQueue execution checks for OFF. No need to do
        // this check before addCmdQueue.

        switch(i) {
        case 0:
        case 1:
            addCmdQueue(555 - (i*333));   // 555 = on, 222 = off
            break;
        case 6:
            if(tblen > j) {
                if(cmdBuf[j] >= 'A' && cmdBuf[j] <= 'K') {
                    if(cmdBuf[j] != 'I') {
                        int num = cmdBuf[j] - 'A';
                        if(num >= 9) num--; // I is skipped
                        addCmdQueue(50 + num);
                    }
                } else if(cmdBuf[j] >= '0' && cmdBuf[j] <= '9') {
                    int num = atoi(&cmdBuf[j]);
                    if(num >= 1 && num <= 10) {
                        addCmdQueue(60 + num - 1);
                    }
                }
            }
            break;
        case 7:
            if(tblen >= j+3) {
                if(cmdBuf[j] >= 'A' && cmdBuf[j] <= 'K' && cmdBuf[j] != 'I' &&
                   cmdBuf[j+2] >= '0' && cmdBuf[j+2] <= '9') {
                    int num = atoi(&cmdBuf[j+2]);
                    int ltr = cmdBuf[j] - 'A' + 1;
                    if(ltr >= 10) ltr--;    // I is skipped
                    if(num > 0 && num <= 10) {
                        num += (ltr * 100);
                        addCmdQueue(880000 + num);
                        #ifdef JB_DBG_MQTT
                        Serial.printf("MQTT: GOTO command %d\n", num + 880000);
                        #endif
                    }
                }
            }
            break;
        case 8:
            if(tblen > j && cmdBuf[j] >= '1' && cmdBuf[j] <= '9') {
                addCmdQueue(500 + (uint32_t)(cmdBuf[j] - '0'));
            }
            break;
        case 10:
            if(tblen > j && cmdBuf[j] >= '0' && cmdBuf[j] <= '9') {
                int p = atoi(cmdBuf+j);
                if(p >= 0 && p <= 100) {
                    switch(opMode) {
                    case 0:
                    case 1:
                        addCmdQueue(300 + ((VOL_LEVELS - 1) * p / 100));
                        break;
                    case 2:
                        addCmdQueue(9000 + p);
                        break;
                    }
                }
            }
            break;
        case 11:
            mqttFakePowerOn();
            break;
        case 12:
            mqttFakePowerOff();
            break;
        case 13:
        case 14:
            mqttFakePowerControl((i == 13));
            break;
        case 15:
            addCmdQueue(20);
            break;
        case 16:
            addCmdQueue(21);
            break;
        case 17:
            addCmdQueue(22);
            break;
        case 20:
            switch(opMode) {
            case 0:
            case 1:
                if(aud_state.curVolume < VOL_LEVELS - 1) {
                    addCmdQueue(300 + aud_state.curVolume + 1);
                }
                break;
            case 2:
                addCmdQueue(9101);
                break;
            }
            break;
        case 21:
            switch(opMode) {
            case 0:
            case 1:
                if(aud_state.curVolume > 0) {
                    addCmdQueue(300 + aud_state.curVolume - 1);
                }
                break;
            case 2:
                addCmdQueue(9102);
                break;
            }
            break;
        case 22:
            mp_sendStatus(1);
            break;
        default:
            addCmdQueue(1000 + i);
        }

    } else if(*settings.mqmpbackchannel && !strcmp(topic, settings.mqmpbackchannel)) {

        updateRemotePlayer((const char *)tempBuf);
      
    }
}

#ifdef JB_DBG_MQTT
#define MQTT_FAILCOUNT 6
#else
#define MQTT_FAILCOUNT 120
#endif

static void mqttPing()
{
    switch(mqttClient.pstate()) {
    case PING_IDLE:
        if(WiFi.status() == WL_CONNECTED) {
            if(!mqttPingNow || (millis() - mqttPingNow > mqttPingInt)) {
                mqttPingNow = millisNonZero();
                if(!mqttClient.sendPing()) {
                    // Mostly fails for internal reasons;
                    // skip ping test in that case
                    mqttDoPing = false;
                    mqttPingDone = true;  // allow mqtt-connect attempt
                }
            }
        }
        break;
    case PING_PINGING:
        if(mqttClient.pollPing()) {
            mqttPingDone = true;          // allow mqtt-connect attempt
            mqttPingNow = 0;
            mqttPingsExpired = 0;
            mqttPingInt = MQTT_SHORT_INT; // Overwritten on fail in reconnect
            // Delay re-connection for 5 seconds after first ping echo
            if(!(mqttReconnectNow = millis() - (mqttReconnectInt - 5000)))
                mqttReconnectNow--;
        } else if(millis() - mqttPingNow > 5000) {
            mqttClient.cancelPing();
            mqttPingNow = millisNonZero();
            mqttPingsExpired++;
            mqttPingInt = MQTT_SHORT_INT * (1 << (mqttPingsExpired / MQTT_FAILCOUNT));
            mqttReconnFails = 0;
        }
        break;
    } 
}

static bool mqttReconnect(bool force)
{
    bool success = false;

    if(useMQTT && (WiFi.status() == WL_CONNECTED)) {

        if(!mqttClient.connected()) {
    
            if(force || !mqttReconnectNow || (millis() - mqttReconnectNow > mqttReconnectInt)) {

                #ifdef JB_DBG_MQTT
                Serial.println("MQTT: Attempting to (re)connect");
                #endif
    
                if(strlen(mqttUser)) {
                    success = mqttClient.connect(mqttUser, strlen(mqttPass) ? mqttPass : NULL);
                } else {
                    success = mqttClient.connect();
                }
    
                mqttReconnectNow = millisNonZero();
                
                if(!success) {
                    mqttRestartPing = true;  // Force PING check before reconnection attempt
                    mqttReconnFails++;
                    if(mqttDoPing) {
                        mqttPingInt = MQTT_SHORT_INT * (1 << (mqttReconnFails / MQTT_FAILCOUNT));
                    } else {
                        mqttReconnectInt = MQTT_SHORT_INT * (1 << (mqttReconnFails / MQTT_FAILCOUNT));
                    }
                    #ifdef JB_DBG_MQTT
                    Serial.printf("MQTT: Failed to reconnect (%d)\n", mqttReconnFails);
                    #endif
                } else {
                    mqttReconnFails = 0;
                    mqttReconnectInt = MQTT_SHORT_INT;
                    #ifdef JB_DBG_MQTT
                    Serial.println("MQTT: Connected to broker, waiting for CONNACK");
                    #endif
                }
    
                return success;
            } 
        }
    }
      
    return true;
}

static void mqttSubscribe()
{
    // Meant only to be called when connected!

    char cmd[32+9];
    strcpy(cmd, "bttf/");
    strcat(cmd, settings.hostName);
    strcat(cmd, "/cmd");
    
    if(!mqttSubAttempted) {
        if(!mqttClient.subscribe(cmd, "bttf/tcd/pub")) {
            #ifdef JB_DBG_MQTT
            Serial.println("MQTT: Failed to subscribe to command topics");
            #endif
        }
        if(!*settings.mqmpbackchannel || !mqttClient.subscribe(settings.mqmpbackchannel)) {
            #ifdef JB_DBG_MQTT
            Serial.println("MQTT: Failed to subscribe to mp feedback topic");
            #endif
        }

        mqttConnectCallback();

        // Send out music player status as soon as possible
        mp_sendStatus(1);
        
        mqttSubAttempted = true;
    }
}

bool mqttConnected()
{
    return (useMQTT && (mqttClient.state() == MQTT_CONNECTED));
}

bool mqttPublish(const char *topic, const char *pl, unsigned int len)
{
    if(useMQTT) {
        if(*topic) {
            return mqttClient.publish(topic, (uint8_t *)pl, len, false);
        }
    }

    return true;
}

bool mqttPublishQuick(const char *topic, const char *pl)
{
    return mqttPublish(topic, pl, strlen(pl) + 1);
}
