/*
 * -------------------------------------------------------------------
 * A10001986 Crosley CR-9 Jukebox
 * (C) 2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Jukebox
 * https://jukebox.out-a-ti.me
 *
 * TCButton Class: Button handling
 * 
 * TCRotEnc Class: Rotary Encoder handling:
 * Supports Adafruit 4991/5880, DuPPA I2CEncoder 2.1, DuPPA 1.2 Mini.
 * For volume/fake power, the encoders must be set as follows:
 * - Ada4991/5880: Default (i2c address 0x36)
 * - DuPPA I2CEncoder 2.1: A0 closed (0x01).
 * For the two jog dials, the encoders must be configured as follows:
 * - Ada4991/5880: JD1: A0 closed (0x37); JD2: A1 closed (JD2:0x38)
 * - DuPPA I2CEncoder 2.1: JD1: A1 closed (0x02); JD2: A2 closed (0x04)
 * - DuPPA I2CEncoder 1.2 Mini: Only supported as JD2; i2c address
 *                              is set by the firmware to 0x04
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
 * 4. The source code and the binary form, and any modifications made 
 * to them may not be used for the purpose of training or improving 
 * machine learning algorithms, including but not limited to artificial
 * intelligence, natural language processing, or data mining. This 
 * condition applies to any derivatives, modifications, or updates 
 * based on the Software code. Any usage of the source code or the 
 * binary form in an AI-training dataset is considered a breach of 
 * this License.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _JBINPUT_H
#define _JBINPUT_H

#include <Wire.h>

// Used for TCButton and RotEnc switch
typedef enum {
    TCBS_IDLE,
    TCBS_PRESSED,
    TCBS_RELEASED,
    TCBS_LONGPRESS,
    TCBS_LONGPRESSEND,
    TCBS_ELONGPRESS,
    TCBS_ELONGPRESSEND
} ButtonState;

/*
 * TCButton class
 */

#if 0
class TCButton {
  
    public:
        TCButton();
        void begin(const int pin, const bool activeLow, const bool pullupActive, const bool pulldownActive = false);
      
        void setTiming(const int debounceDur, const int pressDur, const int lPressDur, const int elPressDur = 5000);

        void attachPress(void (*newFunction)(int, ButtonState))           { _pressFunc = newFunction; }
        void attachLongPressStart(void (*newFunction)(int, ButtonState))  { _longPressStartFunc = newFunction; }
        void attachLongPressStop(void (*newFunction)(int, ButtonState))   { _longPressStopFunc = newFunction; }
        void attachELongPressStart(void (*newFunction)(int, ButtonState)) { _elongPressStartFunc = newFunction; }
        void attachELongPressStop(void (*newFunction)(int, ButtonState))  { _elongPressStopFunc = newFunction; }

        void scan(int idx);
        void reset();

    private:

        void transitionTo(ButtonState nextState);

        void (*_pressFunc)(int, ButtonState) = NULL;
        void (*_longPressStartFunc)(int, ButtonState) = NULL;
        void (*_longPressStopFunc)(int, ButtonState) = NULL;
        void (*_elongPressStartFunc)(int, ButtonState) = NULL;
        void (*_elongPressStopFunc)(int, ButtonState) = NULL;

        int _pin;
        
        unsigned int _debounceDur = 50;
        unsigned int _pressDur = 400;
        unsigned int _longPressDur = 800;
        unsigned int _elongPressDur = 5000;
      
        int _buttonPressed;
      
        ButtonState _state     = TCBS_IDLE;
        ButtonState _lastState = TCBS_IDLE;
      
        unsigned long _startTime;
};
#endif

/*
 * TCRotEnc class
 */

#define RE_FUNC_SIMPLE 0      // Simple: Limit checks up to app, no "disable"
#define RE_FUNC_SMART  1      // Smart:  Configurable max val, with "disabled" if turned beyond 0.

#define TC_RE_TYPE_ADA4991    0     // Adafruit 4991/5880       https://www.adafruit.com/product/4991 https://www.adafruit.com/product/5880
#define TC_RE_TYPE_DUPPAV2    1     // DuPPA I2CEncoder V2.1    https://www.duppa.net/shop/i2cencoder-v2-1/
#define TC_RE_TYPE_DUPPAV1M_U 2     // DuPPA 1.2 Mini at default address
#define TC_RE_TYPE_DUPPAV1M   3     // DuPPA 1.2 Mini

class TCRotEnc {
  
    public:
        TCRotEnc(int numTypes, const uint8_t addrArr[]);
        bool    begin(int encFunction, uint32_t maxVal);
        void    setZeroPos(int offs = 0);
        void    setDisabledPos();
        void    setValuePos(int value);
        void    setMaxVal(uint32_t maxVal);
        int     pollSmart(bool force = false);
        int     pollSimple(bool force = false);

        bool    IsOff();

        // Button
        void    setTiming(const int debounceDur, const int pressDur, const int lPressDur);

        void    attachPressStart(void (*newFunction)(void))     { _pressStartFunc = newFunction; }
        void    attachPress(void (*newFunction)(void))          { _pressFunc = newFunction; }
        void    attachLongPressStart(void (*newFunction)(void)) { _longPressStartFunc = newFunction; }
        void    attachLongPressStop(void (*newFunction)(void))  { _longPressStopFunc = newFunction; }
        void    attachELongPressStart(void (*newFunction)(void)){ _elongPressStartFunc = newFunction; }
        void    attachELongPressStop(void (*newFunction)(void)) { _elongPressStopFunc = newFunction; }

        void    switch_setTiming(const int pressDur, const int lPressDur, const int elPressDur = 5000);
        void    switch_scan(bool force = false);
        void    switch_reset();

    private:
        int32_t getEncPos();
        int     read(uint16_t base, uint8_t reg, uint8_t *buf, uint8_t num);
        void    write(uint16_t base, uint8_t reg, uint8_t *buf, uint8_t num);

        bool    zeroEnc(int offs = 0);

        bool    readSwitch();
        void    switch_transitionTo(ButtonState nextState);

        int           _numTypes = 0;
        const uint8_t *_addrArr;
        int           _i2caddr;
        int8_t        _st = -1;
        
        int8_t        _type = 0;        // RE_FUNC_xxx
        uint32_t      _maxVal = 100;

        int           currentVal = 0;
        int           targetVal = 0;
        
        int32_t       rotEncPos = 0;
        unsigned long lastUpd = 0;
        unsigned long lastFUpd = 0;

        int     haveSwitch = 0;
        unsigned long lastSwUpd = 0;

        void    (*_pressStartFunc)(void) = NULL;
        void    (*_pressFunc)(void) = NULL;
        void    (*_longPressStartFunc)(void) = NULL;
        void    (*_longPressStopFunc)(void) = NULL;
        void    (*_elongPressStartFunc)(void) = NULL;
        void    (*_elongPressStopFunc)(void) = NULL;

        unsigned int _pressDur = 400;
        unsigned int _longPressDur = 800;
        unsigned int _elongPressDur = 5000;
      
        bool _buttonPressed;
      
        ButtonState _state     = TCBS_IDLE;
        ButtonState _lastState = TCBS_IDLE;
      
        unsigned long _startTime;

        bool _duvLastState = false;
};

#endif
