/**
 * wm_local.h - Jukebox
 *
 */

#ifndef wm_local_h
#define wm_local_h

#define DEVNAME "Jukebox"
#define WEBFORMID "J"

#define WM_MDNS

#define WM_50S_STYLE

#define WM_PARAM2
#define WM_PARAM3

#ifdef WM_PARAM2
#define WM_PARAM2_CAPTION "Streaming Settings"
#define WM_PARAM2_TITLE "Streaming Settings"
#endif

#ifdef WM_PARAM3
#define WM_PARAM3_CAPTION "HA/MQTT Settings"
#define WM_PARAM3_TITLE "HA/MQTT Settings"
#endif

// Show sound upload form (or "SD required" message")
#define WM_UPLOAD

// Have CarMode - Not on Jukebox
//#define WM_CCM

#define WM_FWPROT "JUKEBOX"
#define WM_FWPROTSP "JBA"

//#define _A10001986_DBG
//#define _A10001986_V_DBG

// #define WM_AP_STATIC_IP
// #define WM_APCALLBACK
// #define WM_PRECONNECTCB
// #define WM_PRESAVECB
// #define WM_EVENTCB
// #define WM_ADDLGETTERS
// #define WM_ADDLSETTERS

#endif