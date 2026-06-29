/*
 * -------------------------------------------------------------------
 * A10001986 Crosley CR-9 Jukebox 
 * (C) 2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Jukebox
 * https://jukebox.out-a-ti.me
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

/*
 * Build instructions (for Arduino IDE)
 * 
 * - Install the Arduino IDE
 *   https://www.arduino.cc/en/software
 *    
 * - This firmware requires the "ESP32-Arduino" framework. To install this framework, 
 *   in the Arduino IDE, go to "File" > "Preferences" and add the URL   
 *   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
 *   - or (if the URL above does not work) -
 *   https://espressif.github.io/arduino-esp32/package_esp32_index.json
 *   to "Additional Boards Manager URLs". The list is comma-separated.
 *   
 * - Go to "Tools" > "Board" > "Boards Manager", then search for "esp32", and install 
 *   the latest 2.x version by Espressif Systems. Versions >=3.x are not supported.
 *   Detailed instructions for this step:
 *   https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html
 *   
 * - Go to "Tools" > "Board: ..." -> "ESP32 Arduino" and select your board model. For
 *   CircuitSetup original boards, select "NodeMCU-32S".
 *   
 * - If you want Arduino IDE to upload the firmware via USB (which is only required for
 *   fresh ESP32 boards; if a previous version of the firmware is installed on your
 *   board, you can update through the Config Portal and don't need an USB connection):
 *   
 *   Connect your ESP32 board using a suitable USB cable.
 *   
 *   Note that ESP32 boards come in two flavors that differ in which serial communications 
 *   chip is used: Either SiLabs CP210x or WCH CH340. CircuitSetup uses the CP210x.
 * 
 *   Mac:
 *   * CP210x: Since ca. 10.15.7, MacOS comes with a driver for the CP210x. For earlier 
 *     versions of MacOS, installing a driver is required:
 *     https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads
 *     The port ("Tools -> "Port" in Arduino IDE) is named 
 *     - /dev/cu.usbserial-xxxx when using the Apple driver, 
 *     - /dev/cu.SLAB_USBtoUART when using the SiLabs driver. 
 *     The maximum upload speed ("Tools" -> "Upload Speed" in Arduino IDE) can be used.
 *     Note: The SiLabs driver has a bug that affects TCD Control Boards V1.4 and later.
 *     Firmware uploads will fail ("No data received"). Use the Apple driver for those 
 *     boards.
 *   * CH340: This chip is supported out-of-the-box since Mojave. 
 *     The port ("Tools -> "Port" in Arduino IDE) is named /dev/cu.usbserial-XXXX, and 
 *     the maximum upload speed is 460800.
 * 
 *   Windows:
 *   * CP210x: Current Windows versions may come with a suitable driver. If the chip 
 *     is not recognized (ie no Port is created), a driver needs to be installed:
 *     https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads
 *   * CH340: Windows will install a driver when connecting the board; in case it 
 *     doesn't or it fails doing so, please install this driver:
 *     http://www.wch-ic.com/downloads/CH341SER_ZIP.html
 *     Note that the maximum upload speed for the CH340 is apparently 460800.
 *   After driver installation, connect your ESP32, start the Device Manager, expand 
 *   the "Ports (COM & LPT)" list and look for the port with the ESP32 name. Choose 
 *   this port under "Tools" -> "Port" in Arduino IDE.
 *
 * - Install required libraries. In the Arduino IDE, go to "Tools" -> "Manage Libraries" 
 *   and install the following library:
 *   - ArduinoJSON (>= 6.19): https://arduinojson.org/v6/doc/installation/
 *     (Versions 7 and on of this lib are much bigger, so it might happen that the 
 *     binary does not fit the ESP32's flash memory. Use v6 instead.)
 *
 * - Download the complete firmware source code:
 *   https://github.com/realA10001986/Jukebox/archive/refs/heads/main.zip
 *   Extract this file somewhere. Enter the "jukebox-A10001986" folder and 
 *   double-click on "jukebox-A10001986.ino". This opens the firmware in the
 *   Arduino IDE.
 *
 * - For USB connected ESP32 boards: 
 *   Go to "Sketch" -> "Upload" to compile and upload the firmware to your ESP32 board.
 * - For OTA updates via the Config Portal:
 *   Go to "Sketch" -> "Export compiled Binary" to compile the firmware; a ".bin" file 
 *   is created in the source directory, which can then be uploaded through the Config
 *   Portal.
 *   
 * - Install the sound-pack: 
 *   - Go to Config Portal, click "Update & Upload" and upload the sound-pack (JBA.bin,
 *     extracted from install/sound-pack-jbXX.zip) through the bottom file selector.
 *     A FAT32 (not ExFAT!) formatted SD card must be present in the slot during this 
 *     operation.
 *   Alternatively:
 *   - Copy JBA.bin to the top folder of a FAT32 (not ExFAT!) formatted SD card (max 
 *     32GB) and put this card into the slot while the Jukebox is powered down. 
 *   - Now power-up. The sound-pack will now be installed. When finished, the Jukebox 
 *     will reboot.
 */

/*  Changelog
 *  
 *  2026/xx/xx (A10001986)
 *  - Initial version
 *
 */

#include "jb_global.h"

#include <Arduino.h>
#include <Wire.h>

#include "jb_audio.h"
#include "jb_settings.h"
#include "jb_main.h"
#include "jb_wifi.h"

void setup()
{
    powerupMillis = millis();

    Serial.begin(115200);
    Serial.println();

    // I2C init
    // Make sure our i2c buf is 128 bytes
    Wire.setBufferSize(128);
    Wire.begin(-1, -1, 400000);

    main_boot();
    settings_setup();
    wifi_setup();
    audio_setup();
    main_setup();
}

void loop()
{
    audio_loop();
    fade_loop();
    main_loop();
    audio_loop();
    fade_loop();
    wifi_loop();
    audio_loop();
    fade_loop();
    bttfn_loop();
}
