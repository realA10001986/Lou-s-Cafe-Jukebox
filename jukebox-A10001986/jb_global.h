/*
 * -------------------------------------------------------------------
 * A10001986 Crosley CR-9 Jukebox
 * (C) 2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Jukebox
 * https://jukebox.out-a-ti.me
 *
 * Global definitions
 */

#ifndef _JB_GLOBAL_H
#define _JB_GLOBAL_H

/*************************************************************************
 ***                     Basic Hardware Definition                     ***
 *************************************************************************/

/*************************************************************************
 ***                          Version Strings                          ***
 *************************************************************************/

// These must not contain any characters other than
// '0'-'9', 'A'-'Z', '(', ')', '.', '_', '-' or space
#define JB_VERSION       "V1.0"       // 7 chars max. Do NOT change format.
#define JB_VERSION_EXTRA "JUN292026"  // 13 chars max

/*************************************************************************
 ***                           Miscellaneous                           ***
 *************************************************************************/

// External time travel lead time, as defined by TCD firmware
// If FC is connected to TCD by wire, and the option "Signal Time Travel 
// without 5s lead" is set on the TCD, the FC option "TCD signals without 
// lead" must be set, too.
#define ETTO_LEAD 5000

// Maximum number of configurable streams.
#define NUM_STREAMS 20

/*************************************************************************
 ***                               Debug                               ***
 *************************************************************************/

// Debug output selection
#define JB_DBG_NONE

#if !defined(JB_DBG_NONE)
#define JB_DBG_BOOT           // Boot strap & settings
#define JB_DBG_WIFI           // WiFi-related
#define JB_DBG_MQTT           // MQTT-related
#define JB_DBG_AUDIO          // Audio/Jukebox-related
//#define JB_DBG_NET            // Prop network
#define JB_DBG_TT             // Time travel
#define JB_DBG_GEN            // Generic
#endif

/*************************************************************************
 ***                  esp32-arduino version detection                  ***
 *************************************************************************/

#if defined __has_include && __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#ifdef ESP_ARDUINO_VERSION_MAJOR
    #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2,0,8)
    #define HAVE_GETNEXTFILENAME
    #endif
#endif
#endif

/*************************************************************************
 ***                             GPIO pins                             ***
 *************************************************************************/

// I2S audio pins
#define I2S_BCLK_PIN      26
#define I2S_LRCLK_PIN     25
#define I2S_DIN_PIN       33

// SD Card pins
#define SD_CS_PIN          5
#define SPI_MOSI_PIN      23
#define SPI_MISO_PIN      19
#define SPI_SCK_PIN       18

#define BUTTON1_IO_PIN    15      // GPIO button 1 (used as output)       (has internal PU)
#define BUTTON2_IO_PIN    27      // GPIO button 2 (used as output)       (has internal PU)
#define BUTTON3_IO_PIN    32      // GPIO button 3 (unused)               (has no internal PU?; PU on CB)

#define LED_PIN_1         12      // "9"
#define LED_PIN_2         16      // "10"
#define LED_PIN_3         17      // "4C"

#define TOPLIGHT_PIN      LED_PIN_1       // Menu illumination (toplights)
#define TOPLIGHT_PWM_PIN  BUTTON1_IO_PIN
#define BLUELIGHT_PIN     LED_PIN_2       // Blue LED for menu illumination (bluelight)
#define BLUELIGHT_PWM_PIN BUTTON2_IO_PIN
#define SIGNALLIGHT_PIN   LED_PIN_3       // Jukebox LED for signals

#define TT_IN_PIN         13      // Time Travel button (or TCD tt trigger input) (has internal PU/PD) (PD on CB)
#define TT_OUT_PIN        14      // Time Travel output                           (has internal PU/PD)

#endif
