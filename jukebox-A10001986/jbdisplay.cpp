/*
 * -------------------------------------------------------------------
 * A10001986 Crosley CR-9 Jukebox
 * (C) 2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Jukebox
 * https://jukebox.out-a-ti.me
 *
 * LEDs handling
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
#include <Wire.h>

#include "jbdisplay.h"

/*
 * PWM LED class for white and blue top lights
 */

// Store basic config data
PWMLED::PWMLED(uint8_t pwm_pin)
{
    _pwm_pin = pwm_pin;
}

void PWMLED::begin(uint8_t ledChannel, uint32_t freq, uint8_t resolution, uint8_t pwm_pin)
{
    _chnl = ledChannel;
    _freq = freq;
    _res = resolution;
    if(pwm_pin != 255) {
        _pwm_pin = pwm_pin;
    }
    
    // Config PWM properties
    ledcSetup(_chnl, _freq, _res);

    // Attach channel to GPIO
    ledcAttachPin(_pwm_pin, _chnl);

    // For 3.x (chnl unused)
    //ledcAttach(_pwm_pin, _freq, _res);

    // Set DC to 0
    setDC(0);
}

void PWMLED::setDC(uint32_t dutyCycle)
{
    _curDutyCycle = dutyCycle;
    ledcWrite(_chnl, dutyCycle);
    //ledcWrite(_pwm_pin, dutyCycle); // For 3.x
}

uint32_t PWMLED::getDC()
{
    return _curDutyCycle;
}


/*
 * JB LED class
 */

// Timer interrupt
#define TMR_TIME      0.01    // 0.01s = 10ms
#define TMR_PRESCALE  80
#define TMR_TICKS     (uint64_t)(((double)TMR_TIME * 80000000.0) / (double)TMR_PRESCALE)
#define TME_TIMEUS    (TMR_TIME * 1000000)

static volatile int      _pin = 0;
static volatile int32_t  _ticks = 0;
static volatile bool     _critical = false;
static volatile bool     _blinkEnable = false;
static volatile bool     _blinkWasOff = false;
static volatile bool     _JBLED = false;
static volatile int16_t  _tick_interval = 400; // random, will be overwritten
static volatile uint16_t _blinkDelay = 0;

#define SS_ONESHOT 0xfe
#define SS_LOOP    0
#define SS_END     0xff
static volatile bool     _specialsig = false;
static volatile bool     _wasSpecial = false;
static volatile bool     _specialOS = false;
static volatile uint8_t  _specialsignum = 0;
static volatile uint8_t  _specialidx = 0;
static volatile int16_t  _specialticks = 0;

static const DRAM_ATTR uint8_t _specialArray[JBSEQ_MAX][64] = {
    {                                               // 1: Please update sound-pack ("SOS")
      SS_ONESHOT,
      1, 15, 0, 15, 1, 15, 0, 15, 1, 15, 0, 15,
      1, 40, 0, 40, 1, 40, 0, 40, 1, 40, 0, 40,
      1, 15, 0, 15, 1, 15, 0, 15, 1, 15, 0, 15,
      SS_END
    },
    {                                               // 2: Wait
      SS_LOOP,
      1, 50, 0, 50,
      SS_END
    },
    {                                               // 3: Alarm (BTTFN/MQTT)
      SS_ONESHOT,
      1, 100, 0, 50, 1, 100, 0, 50,
      1, 100, 0, 50, 1, 100, 0, 50,
      SS_END
    },
    {                                               // 4: Error (LOOP!)
      SS_LOOP,
      1, 20, 0, 20, 1, 20, 0, 100, 
      SS_END
    },
    {                                               // 5: "OK"
      SS_ONESHOT,
      1, 20, 0, 20,
      SS_END
    },
    {                                               // 6: Update available
      SS_ONESHOT,
      1, 10, 0, 10, 1, 10, 0, 10, 1, 10, 0, 10,
      1, 10, 0, 10, 1, 10, 0, 10, 1, 10, 0, 10,
      SS_END
    },
    {                                               // 7: No music (folder not found, folder empty, no folders...)
      SS_ONESHOT,                                   //    Stream unsupported or not found
      1, 100, 0, 10,
      SS_END
    },
    {                                               // 8: Shuffle on
      SS_ONESHOT,
      1, 100, 0, 20, 1, 10, 0, 10, 1, 10,
      SS_END
    },
    {                                               // 9: Shuffle off
      SS_ONESHOT,
      1, 100, 0, 20, 1, 20,
      SS_END
    },
    {                                               // 10: About to reset IP and AP-PW
      SS_LOOP,
      1, 10, 0, 10, 1, 10, 0, 10, 1, 10, 0, 10,
      SS_END
    },
    {                                               // 11: MP mode
      SS_ONESHOT,
      1, 30, 0, 10,
      SS_END
    },
    {                                               // 12: Streaming Mode
      SS_ONESHOT,
      1, 30, 0, 30, 1, 30, 0, 10,
      SS_END
    },
    {                                               // 13: Remote Mode
      SS_ONESHOT,
      1, 30, 0, 30, 1, 30, 0, 30, 1, 30, 0, 10,
      SS_END
    }
};

// ISR: Jukebox LED blinking

static void IRAM_ATTR _el_set(int mask)
{
    digitalWrite(_pin, !!(mask & 1));
}

static void IRAM_ATTR ISR_JB_Empty()
{       
    if(_critical)
        return;

    if(_specialsig) {
      
        // Special sequence for signalling
        _wasSpecial = true;
        if(_specialticks == 0) {
            if(_specialArray[_specialsignum][_specialidx] == SS_END) {
                 if(_specialOS) {
                    _specialsig = _wasSpecial = false; 
                    _el_set(0);
                 } else {
                    _specialidx = 1;
                 }
            }
            if(_specialsig) {
                _el_set(_specialArray[_specialsignum][_specialidx]);
            }
        }
        if(_specialsig) {
            _specialticks++;
            if(_specialticks >= _specialArray[_specialsignum][_specialidx + 1]) {
                _specialticks = 0;
                _specialidx += 2;
            }
        }

    } else if(_wasSpecial) {
      
        _el_set(0);
        _wasSpecial = false;
        
    }
}

JBLED::JBLED(uint8_t timer_no)
{
    
    _timer_no = timer_no;
}

void JBLED::begin(int pin)
{   
    _pin = pin;
    
    pinMode(_pin, OUTPUT);
    
    // Switch off
    _el_set(0);

    // Install & enable timer interrupt
    _JBTimer_Cfg = timerBegin(_timer_no, TMR_PRESCALE, true);
    timerAttachInterrupt(_JBTimer_Cfg, &ISR_JB_Empty, true);
    timerAlarmWrite(_JBTimer_Cfg, TMR_TICKS, true);
    timerAlarmEnable(_JBTimer_Cfg);
}

void JBLED::specialSignal(uint8_t signum) 
{
    _critical = true;
    _specialsig = false;
    if(signum) {
        _specialsignum = signum - 1;
        _specialOS = (_specialArray[_specialsignum][0] == SS_ONESHOT);
        _specialidx = 1;
        _specialticks = 0;
        _specialsig = true;
    }
    _critical = false;
}

bool JBLED::specialDone()
{
    return !_specialsig;
}

uint8_t JBLED::curSignal()
{
    if(!_specialsig) return 0;
    return _specialsignum + 1;
}

/*
 * Button panel illumination
 *
 */

 // MCP23017 registers
#define IOX_IODIRA  0x00
#define IOX_IODIRB  0x01
#define IOX_IOPOLA  0x02
#define IOX_IOPOLB  0x03
#define IOX_GPINTA  0x04
#define IOX_GPINTB  0x05
#define IOX_DEFVALA 0x06
#define IOX_DEFVALB 0x07
#define IOX_INTCONA 0x08
#define IOX_INTCONB 0x09
#define IOX_IOCON   0x0a
#define IOX_PUA     0x0c
#define IOX_PUB     0x0d
#define IOX_GPIOA   0x12
#define IOX_GPIOB   0x13
#define IOX_OLATA   0x14
#define IOX_OLATB   0x15

/*
 * PanelLEDs class
 * 
 * Hardware config:
 * [0] LEDs A-K (K=9->A=0)
 * [1] LEDs 1-10 (10=9->1=0)
 *
 * (15-8 = Port B, 7-0: Port A)
 * LEDs are active HIGH
 *
 */
 
PanelLEDs::PanelLEDs()
{
}

// Initialize I2C
void PanelLEDs::begin()
{
    bool foundSt = false;

    // Auto-detect PanelLED board revision
    _adr1 = MCP23017_1_ADDR;
    _adr2 = MCP23017_2_ADDR;

    Wire.beginTransmission(MCP23017_3_ADDR);
    if(!Wire.endTransmission(true)) {
        _adr1 = MCP23017_2_ADDR;
        _adr2 = MCP23017_3_ADDR;
    }

    _havePanelLEDs = 0;
    _on = 0;

    for(int i = 0, a = _adr1; i < 2; i++) {
        Wire.beginTransmission(a);
        if(Wire.endTransmission(true))
            return;

        // Init MCP23017

        // Set BANK to 0, all others to default
        write8(a, IOX_IOCON, 0b00000000);
        write8(a, IOX_IOCON+1, 0b00000000);
    
        // Set A+B all to output  (1 = input, 0 = output)
        write16(a, IOX_IODIRA, 0);
    
        // Disable A+B pull-ups
        write16(a, IOX_PUA, 0);

        // Clear all A+B ports
        write16(a, IOX_GPIOA, 0);

        a = _adr2;
    }

    _havePanelLEDs = 1;
}

// Letter: 0 = a, 9 = k (i is skipped)
// Number: 0 = 1, 9 = 10
void PanelLEDs::setLEDs(int letter, int number, int direct)
{
    if(!direct) {
        _lastLtr = letter;
        _lastNum = number;
    }

    uint16_t u1 = (letter >= 0 && letter < 10) ? (1 << letter) : 0;
    uint16_t u2 = (number >= 0 && number < 10) ? (1 << number) : 0; 

    #ifdef JB_DBG_AUDIO
    {
        const char *off = "off";
        const char ltrs[] = "ABCDEFGHJK";
        char dltr[] = "A";
        char dnum[] = "10";
        const char *ddltr, *ddnum;
        if(letter < 0) ddltr = off;
        else {
            dltr[0] = ltrs[letter];
            ddltr = (const char *)dltr;
        }
        if(number < 0) ddnum = off;
        else {
            snprintf(dnum, 3, "%-2d", number + 1);
            ddnum = (const char *)dnum;
        }
        Serial.printf("setLEDs: %s %s %s %s\n", ddltr, ddnum, direct ? "direct" : "", _on ? "" : "(status off)");    
    }
    #endif
    
    if(!_havePanelLEDs || !_on)
        return;

    write16(_adr1, IOX_GPIOA, u1);
    write16(_adr2, IOX_GPIOA, u2);
}

void PanelLEDs::getCurrLit(int& ltr, int& num)
{
    ltr = _lastLtr;
    num = _lastNum;
}

void PanelLEDs::onOff(int onOff)
{
    if(onOff) {
        _on = 1;
        setLEDs(_lastLtr, _lastNum);
    } else {
        _on = 1;
        setLEDs(-1, -1, 1);
        _on = 0;
    }
}

/*
 * Private
 */

void PanelLEDs::write16(int adr, uint16_t regno, uint16_t value)
{
    Wire.beginTransmission(adr);
    Wire.write((uint8_t)(regno));
    Wire.write((uint8_t)(value & 0xff));
    Wire.write((uint8_t)(value >> 8));
    Wire.endTransmission();
}

void PanelLEDs::write8(int adr, uint16_t regno, uint8_t value)
{
    Wire.beginTransmission(adr);
    Wire.write((uint8_t)(regno));
    Wire.write((uint8_t)(value & 0xff));
    Wire.endTransmission();
}
