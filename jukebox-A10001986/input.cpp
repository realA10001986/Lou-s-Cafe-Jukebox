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

#include "input.h"

/*
 * TCButton class
 */

#if 0
TCButton::TCButton()
{
}

/* pin: The pin to be used
 * activeLow: Set to true when the input level is LOW when the button is pressed, Default is true.
 * pullupActive: Activate the internal pullup when available. Default is true.
 */
void TCButton::begin(const int pin, const bool activeLow, const bool pullupActive, const bool pulldownActive)
{
    _pin = pin;
    _buttonPressed = activeLow ? LOW : HIGH;
    
    pinMode(_pin, pullupActive ? INPUT_PULLUP : (pulldownActive ? INPUT_PULLDOWN : INPUT));
}

// debounce: Number of millisec that have to pass by before a click is assumed stable
// press:    Number of millisec that have to pass by before a short press is detected
// lPress:   Number of millisec that have to pass by before a long press is detected
// elPressDur: Number of millisec to pass for an extra long press
void TCButton::setTiming(const int debounceDur, const int pressDur, const int lPressDur, const int elPressDur)
{
    _debounceDur = debounceDur;
    _pressDur = pressDur;
    _longPressDur = lPressDur;
    _elongPressDur = elPressDur;
}

// Check input of the pin and advance the state machine
void TCButton::scan(int idx)
{
    unsigned long now = millis();
    unsigned long waitTime = now - _startTime;
    bool active = (digitalRead(_pin) == _buttonPressed);
    
    switch(_state) {
    case TCBS_IDLE:
        if(active) {
            transitionTo(TCBS_PRESSED);
            _startTime = now;
        }
        break;

    case TCBS_PRESSED:
        if((!active) && (waitTime < _debounceDur)) {  // de-bounce
            transitionTo(_lastState);
        } else if(!active) {
            transitionTo(TCBS_RELEASED);
            _startTime = now;
        } else if(waitTime > _longPressDur) {
            if(_longPressStartFunc) _longPressStartFunc(idx, TCBS_LONGPRESS);
            transitionTo(TCBS_LONGPRESS);
        }
        break;

    case TCBS_RELEASED:
        if((active) && (waitTime < _debounceDur)) {  // de-bounce
            transitionTo(_lastState);
        } else if((!active) && (waitTime > _pressDur)) {
            if(_pressFunc) _pressFunc(idx, TCBS_PRESSED);
            reset();
        }
        break;
  
    case TCBS_LONGPRESS:
        if(!active) {
            transitionTo(TCBS_LONGPRESSEND);
            _startTime = now;
        } else if(_elongPressDur && (waitTime > _elongPressDur)) {
            if(_elongPressStartFunc) _elongPressStartFunc(idx, TCBS_ELONGPRESS); 
            transitionTo(TCBS_ELONGPRESS);
        }
        break;

    case TCBS_LONGPRESSEND:
        if((active) && (waitTime < _debounceDur)) { // de-bounce
            transitionTo(_lastState);
        } else if(waitTime >= _debounceDur) {
            if(_longPressStopFunc) _longPressStopFunc(idx, TCBS_LONGPRESSEND);
            reset();
        }
        break;

    case TCBS_ELONGPRESS:
        if(!active) {
            transitionTo(TCBS_ELONGPRESSEND);
            _startTime = now;
        }
        break;

    case TCBS_ELONGPRESSEND:
        if((active) && (waitTime < _debounceDur)) { // de-bounce
            transitionTo(_lastState);
        } else if(waitTime >= _debounceDur) {
            if(_elongPressStopFunc) _elongPressStopFunc(idx, TCBS_ELONGPRESSEND);
            reset();
        }
        break;

    default:
        transitionTo(TCBS_IDLE);
        break;
    }
}

/*
 * Private
 */

void TCButton::reset()
{
    _state = TCBS_IDLE;
    _lastState = TCBS_IDLE;
    _startTime = 0;
}

// Advance to new state
void TCButton::transitionTo(ButtonState nextState)
{
    _lastState = _state;
    _state = nextState;
}
#endif

/*
 * TCRotEnc class
 */

// General

#define HWUPD_DELAY_SMART   100   // ms between hw polls for smart RotEncs
#define FUPD_DELAY           10   // ms between currentVal updates
#define OFF_SLOTS             5

#define HWUPD_DELAY_SIMPLE   80   // ms between hw polls for simple RotEncs

#define HWSWUPD_DELAY        10   // ms between switch polls

// Ada4991/5880

#define SEESAW_STATUS_BASE  0x00
#define SEESAW_GPIO_BASE    0x01
#define SEESAW_ENCODER_BASE 0x11

#define SEESAW_STATUS_HW_ID   0x01
#define SEESAW_STATUS_VERSION 0x02
#define SEESAW_STATUS_SWRST   0x7F

#define SEESAW_ENCODER_STATUS   0x00
#define SEESAW_ENCODER_INTENSET 0x10
#define SEESAW_ENCODER_INTENCLR 0x20
#define SEESAW_ENCODER_POSITION 0x30
#define SEESAW_ENCODER_DELTA    0x40

#define ADA4991_SWITCH_PIN  24

#define SEESAW_GPIO_DIRCLR_BULK 0x03
#define SEESAW_GPIO_BULK        0x04
#define SEESAW_GPIO_BULK_SET    0x05
#define SEESAW_GPIO_PULLENSET   0x0B


#define SEESAW_HW_ID_CODE_SAMD09   0x55
#define SEESAW_HW_ID_CODE_TINY806  0x84
#define SEESAW_HW_ID_CODE_TINY807  0x85
#define SEESAW_HW_ID_CODE_TINY816  0x86
#define SEESAW_HW_ID_CODE_TINY817  0x87
#define SEESAW_HW_ID_CODE_TINY1616 0x88
#define SEESAW_HW_ID_CODE_TINY1617 0x89

// Duppa V2.1

#define DUV2_BASE     0x100
#define DUV2_CONF     0x00
#define DUV2_ESTATUS  0x05
#define DUV2_CONF2    0x30
#define DUV2_CVALB4   0x08
#define DUV2_CMAXB4   0x0c
#define DUV2_CMINB4   0x10
#define DUV2_ISTEPB4  0x14
#define DUV2_DPPERIOD 0x1f
#define DUV2_IDCODE   0x70

enum DUV2_CONF_PARAM {
    DU2_FLOAT_DATA   = 0x01,
    DU2_INT_DATA     = 0x00,
    DU2_WRAP_ENABLE  = 0x02,
    DU2_WRAP_DISABLE = 0x00,
    DU2_DIRE_LEFT    = 0x04,
    DU2_DIRE_RIGHT   = 0x00,
    DU2_IPUP_DISABLE = 0x08,
    DU2_IPUP_ENABLE  = 0x00,
    DU2_RMOD_X2      = 0x10,
    DU2_RMOD_X1      = 0x00,
    DU2_RGB_ENCODER  = 0x20,
    DU2_STD_ENCODER  = 0x00,
    DU2_EEPROM_BANK2 = 0x40,
    DU2_EEPROM_BANK1 = 0x00,
    DU2_RESET        = 0x80
};
enum DUV2_CONF2_PARAM {
    DU2_CLK_STRECH_ENABLE  = 0x01,
    DU2_CLK_STRECH_DISABLE = 0x00,
    DU2_REL_MODE_ENABLE    = 0x02,
    DU2_REL_MODE_DISABLE   = 0x00,
};

// Duppa V1.2 Mini

#define DUV1M_BASE     0x100
#define DUV1M_CONF     0x00
#define DUV1M_ESTATUS  0x02
#define DUV1M_CONF2    0x30
#define DUV1M_CVALB4   0x03
#define DUV1M_CMAXB4   0x07
#define DUV1M_CMINB4   0x0b
#define DUV1M_ISTEPB4  0x0f
#define DUV1M_DPPERIOD 0x13
#define DUV1M_IDCODE   0x70
#define DUV1M_I2CADDR  0x72

enum DUV1M_CONF_PARAM {
    DU1M_WRAP_ENABLE  = 0x01,
    DU1M_WRAP_DISABLE = 0x00,
    DU1M_DIRE_LEFT    = 0x02,
    DU1M_DIRE_RIGHT   = 0x00,
    DU1M_IPUP_DISABLE = 0x04,
    DU1M_IPUP_ENABLE  = 0x00,
    DU1M_RMOD_X2      = 0x08,
    DU1M_RMOD_X1      = 0x00,
    DU1M_RESET        = 0x80
};

TCRotEnc::TCRotEnc(int numTypes, const uint8_t *addrArr)
{
    _numTypes = numTypes;
    _addrArr = addrArr;
}

bool TCRotEnc::begin(int encFunction, uint32_t maxVal)
{
    bool foundSt = false;
    uint32_t temp;
    union {
        uint8_t buf[4];
        int32_t ibuf;
    };

    _type = encFunction;
    _maxVal = maxVal;

    for(int i = 0; i < _numTypes * 2; i += 2) {

        _i2caddr = _addrArr[i];

        Wire.beginTransmission(_i2caddr);
        if(!Wire.endTransmission(true)) {

            switch(_addrArr[i+1]) {
            case TC_RE_TYPE_ADA4991:
                if(read(SEESAW_STATUS_BASE, SEESAW_STATUS_HW_ID, buf, 1) == 1) {
                    switch(buf[0]) {
                    case SEESAW_HW_ID_CODE_SAMD09:
                    case SEESAW_HW_ID_CODE_TINY806:
                    case SEESAW_HW_ID_CODE_TINY807:
                    case SEESAW_HW_ID_CODE_TINY816:
                    case SEESAW_HW_ID_CODE_TINY817:
                    case SEESAW_HW_ID_CODE_TINY1616:
                    case SEESAW_HW_ID_CODE_TINY1617:
                        //_hwtype = buf[0];
                        // Check for board type
                        read(SEESAW_STATUS_BASE, SEESAW_STATUS_VERSION, buf, 4);   
                        if((((uint16_t)buf[0] << 8) | buf[1]) == 4991) {
                            foundSt = true;
                        }
                    }
                }
                break;

            case TC_RE_TYPE_DUPPAV2:
                read(DUV2_BASE, DUV2_IDCODE, buf, 1);
                if(buf[0] == 0x53) {
                    foundSt = true;
                }
                break;

            case TC_RE_TYPE_DUPPAV1M_U:
                read(DUV1M_BASE, DUV1M_IDCODE, buf, 1);
                if(buf[0] == 0x39) {
                    Serial.println("RotEnc: Unconfigured DuPPA V1.2 Mini found. Re-configuring...");
                    buf[0] = 0x04;    // Set i2c address to 0x04
                    write(DUV1M_BASE, DUV1M_I2CADDR, &buf[0], 1);
                    write(DUV1M_BASE, DUV1M_I2CADDR, &buf[0], 1);
                    write(DUV1M_BASE, DUV1M_I2CADDR, &buf[0], 1);
                    delay(100);
                    buf[0] = DU1M_RESET;
                    write(DUV1M_BASE, DUV1M_CONF, &buf[0], 1);
                    delay(200);
                }
                break;

            case TC_RE_TYPE_DUPPAV1M:
                read(DUV1M_BASE, DUV1M_IDCODE, buf, 1);
                if(buf[0] == 0x39) {
                    foundSt = true;
                }
                break;

            }

            if(foundSt) {
                _st = _addrArr[i+1];
    
                #ifdef JB_DBG_BOOT
                const char *tpArr[6] = { "ADA4991/5880", "DuPPa V2.1", "", "DuPPa V1.2 Mini", "", "" };
                const char *purpose[2] = { "Simple", "Smart" };
                Serial.printf("Rotary encoder: Detected %s, used as %s\n", tpArr[_st], purpose[_type]);
                #endif
    
                break;
            }
        }
    }

    switch(_st) {
      
    case TC_RE_TYPE_ADA4991:
        // Reset
        buf[0] = 0xff;
        write(SEESAW_STATUS_BASE, SEESAW_STATUS_SWRST, &buf[0], 1);
        delay(10);
        // Default values suitable

        // Set pin mode of SWITCH pin to "INPUT PULLUP"
        temp = 1 << ADA4991_SWITCH_PIN;
        buf[0] = temp >> 24;
        buf[1] = temp >> 16;
        buf[2] = temp >> 8;
        buf[3] = temp & 0xff;
        write(SEESAW_GPIO_BASE, SEESAW_GPIO_DIRCLR_BULK, &buf[0], 4);
        write(SEESAW_GPIO_BASE, SEESAW_GPIO_PULLENSET, &buf[0], 4);
        write(SEESAW_GPIO_BASE, SEESAW_GPIO_BULK_SET, &buf[0], 4);
        _buttonPressed = false;
        haveSwitch = 1;
        break;

    case TC_RE_TYPE_DUPPAV2:
        // Reset
        buf[0] = DU2_RESET;
        write(DUV2_BASE, DUV2_CONF, &buf[0], 1);
        delay(10);
        // Init
        buf[0] = DU2_INT_DATA     | 
                 DU2_WRAP_DISABLE |
                 DU2_DIRE_RIGHT   |
                 DU2_RMOD_X1      |
                 DU2_STD_ENCODER  |
                 DU2_IPUP_DISABLE;
        buf[1] = DU2_CLK_STRECH_DISABLE |
                 DU2_REL_MODE_DISABLE;
        write(DUV2_BASE, DUV2_CONF, &buf[0], 1);
        write(DUV2_BASE, DUV2_CONF2, &buf[1], 1);
        // Reset counter to 0
        ibuf = 0;
        write(DUV2_BASE, DUV2_CVALB4, &buf[0], 4);
        // Step width 1
        buf[3] = 1;
        write(DUV2_BASE, DUV2_ISTEPB4, &buf[0], 4);
        // Set Min/Max
        buf[0] = 0x80; buf[3] = 0x00; // [1], [2] still zero from above
        write(DUV2_BASE, DUV2_CMINB4, &buf[0], 4);
        buf[0] = 0x7f; buf[1] = buf[2] = buf[3] = 0xff;
        write(DUV2_BASE, DUV2_CMAXB4, &buf[0], 4);
        // Disable double-press
        buf[0] = 0;
        write(DUV2_BASE, DUV2_DPPERIOD, &buf[0], 1);
        _buttonPressed = true;
        haveSwitch = 1;
        break;

    case TC_RE_TYPE_DUPPAV1M:
        buf[0] = DU1M_RESET;
        write(DUV1M_BASE, DUV1M_CONF, &buf[0], 1);
        delay(10);
        // Init
        buf[0] = DU1M_WRAP_DISABLE |
                 DU1M_DIRE_LEFT    |    // Should be RIGHT, but either FW or doc is wrong
                 DU1M_RMOD_X1      |
                 DU1M_IPUP_DISABLE;
        write(DUV1M_BASE, DUV1M_CONF, &buf[0], 1);
        // Reset counter to 0
        ibuf = 0;
        write(DUV1M_BASE, DUV1M_CVALB4, &buf[0], 4);
        // Step width 1
        buf[3] = 1;
        write(DUV1M_BASE, DUV1M_ISTEPB4, &buf[0], 4);
        // Set Min/Max
        buf[0] = 0x80; buf[3] = 0x00; // [1], [2] still zero from above
        write(DUV1M_BASE, DUV1M_CMINB4, &buf[0], 4);
        buf[0] = 0x7f; buf[1] = buf[2] = buf[3] = 0xff;
        write(DUV1M_BASE, DUV1M_CMAXB4, &buf[0], 4);
        // Disable double-press
        buf[0] = 0;
        write(DUV1M_BASE, DUV1M_DPPERIOD, &buf[0], 1);
        _buttonPressed = true;
        haveSwitch = 1;
        break;

    default:
        return false;
    }

    setZeroPos();

    return true;
}

// Init encoder into "zero" position (all functions)
void TCRotEnc::setZeroPos(int offs)
{
    zeroEnc(offs);
    rotEncPos = getEncPos();
    currentVal = targetVal = 0;
}

// Init encoder into "disabled" position (smart only)
void TCRotEnc::setDisabledPos()
{
    if(_type != RE_FUNC_SMART)
        return;

    zeroEnc(-OFF_SLOTS);
    rotEncPos = getEncPos();
    targetVal = -OFF_SLOTS;
    currentVal = -1;
}

// Init encoder to value position (smart only)
void TCRotEnc::setValuePos(int value)
{   
    if(_type != RE_FUNC_SMART)
        return;

    if(value < 0) {
        setDisabledPos();
        return;
    }

    zeroEnc(value);
    rotEncPos = getEncPos();
    
    currentVal = targetVal = value;
}

void TCRotEnc::setMaxVal(uint32_t maxVal)
{
    _maxVal = maxVal;
}

int TCRotEnc::pollSmart(bool force)
{
    bool timeout = (millis() - lastUpd > HWUPD_DELAY_SMART);
    int32_t updRotPos;

    if(_type != RE_FUNC_SMART)
        return 0;
    
    if(force || timeout || (currentVal != targetVal)) {

        if(force || timeout) {
            lastUpd = millis();
            
            int32_t t = rotEncPos;
            rotEncPos = updRotPos = getEncPos();

            //Serial.printf("Rot pos %x\n", rotEncPos);

            // If turned in + direction, don't start from <0, but 0: 
            // If we're at -3 (which is displayed as 0), and user 
            // turns +, we don't want -2, but 1.
            // If speed was off (or showing temperature), start 
            // with 0.
            if(rotEncPos > t && targetVal < 0) {
                int16_t resetTo = (targetVal == -OFF_SLOTS) ? 0 : 1;
                targetVal = resetTo;
                if(zeroEnc(resetTo)) {
                    updRotPos = resetTo;
                }
            } else {
                targetVal += (rotEncPos - t);
            }
            if(targetVal < -OFF_SLOTS)   targetVal = -OFF_SLOTS;
            else if(targetVal > _maxVal) targetVal = _maxVal;

            rotEncPos = updRotPos;
        }
        
        if(currentVal != targetVal) {
            if(targetVal < 0) {
                currentVal = targetVal;
            } else if(force || (millis() - lastFUpd > FUPD_DELAY)) {
                if(currentVal < targetVal) currentVal++;
                else currentVal--;
                lastFUpd = millis();
            }
        }

    }

    if(currentVal >= 0)                return currentVal;
    if(currentVal >= -(OFF_SLOTS - 1)) return 0;
    return -1;
}

bool TCRotEnc::IsOff()
{
    if(_type != RE_FUNC_SMART)
        return false;
        
    return (currentVal < -(OFF_SLOTS - 1));
}

int TCRotEnc::pollSimple(bool force)
{
    if(_type != RE_FUNC_SIMPLE)
        return 0;
        
    if(force || (millis() - lastUpd > HWUPD_DELAY_SIMPLE)) {

        lastUpd = millis();
        
        int32_t t = rotEncPos;
        rotEncPos = getEncPos();

        return (rotEncPos - t);
    }
    
    return 0;
}

int32_t TCRotEnc::getEncPos()
{
    uint8_t buf[4];

    switch(_st) {
      
    case TC_RE_TYPE_ADA4991:
        read(SEESAW_ENCODER_BASE, SEESAW_ENCODER_POSITION, buf, 4);
        // Ada4991 reports neg numbers when turned cw, so 
        // negate value
        return -(int32_t)(
                    ((uint32_t)buf[0] << 24) | 
                    ((uint32_t)buf[1] << 16) |
                    ((uint32_t)buf[2] <<  8) | 
                     (uint32_t)buf[3]
                );
    case TC_RE_TYPE_DUPPAV2:
        read(DUV2_BASE, DUV2_CVALB4, buf, 4);
        return (int32_t)(
                    ((uint32_t)buf[0] << 24) | 
                    ((uint32_t)buf[1] << 16) |
                    ((uint32_t)buf[2] <<  8) | 
                     (uint32_t)buf[3]
                );
    case TC_RE_TYPE_DUPPAV1M:
        read(DUV1M_BASE, DUV1M_CVALB4, buf, 4);
        return (int32_t)(
                    ((uint32_t)buf[0] << 24) | 
                    ((uint32_t)buf[1] << 16) |
                    ((uint32_t)buf[2] <<  8) | 
                     (uint32_t)buf[3]
                );
    }

    return 0;
}

bool TCRotEnc::zeroEnc(int offs)
{
    // Sets encoder value to "0"-pos+offs. 
    // We don't do this for encoders with a 32bit 
    // range, since turning it that far probably 
    // exceeds the encoder's life span anyway.   
    
    return false;
}

/*
 * Switch handling
 */

void TCRotEnc::switch_setTiming(const int pressDur, const int lPressDur, const int elPressDur)
{
    _pressDur = pressDur;
    _longPressDur = lPressDur;
    _elongPressDur = elPressDur;
}


bool TCRotEnc::readSwitch()
{
    uint8_t buf[4];
    uint32_t temp;

    switch(_st) {
      
    case TC_RE_TYPE_ADA4991:
        read(SEESAW_GPIO_BASE, SEESAW_GPIO_BULK, &buf[0], 4);
        temp = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
        return !!(temp & (1 << ADA4991_SWITCH_PIN));
    case TC_RE_TYPE_DUPPAV2:
        read(DUV2_BASE, DUV2_ESTATUS, &buf[0], 1);      // What if we miss 0x02 and 0x01? We get 0x03, and we miss
        switch(buf[0] & 0x03) {                         // an entire press event. Register is cleared on read!!!
        case 1: _duvLastState = false; break;           // Answer: Under normal operation, this would mean a bounce.
        case 2: _duvLastState = true;  break;
        default: break;
        }
        return _duvLastState;
    case TC_RE_TYPE_DUPPAV1M:
        read(DUV1M_BASE, DUV1M_ESTATUS, &buf[0], 1);
        switch(buf[0] & 0x03) {
        case 1: _duvLastState = false; break;
        case 2: _duvLastState = true;  break;
        default: break;
        }
        return _duvLastState;
    default:
        return false;
    }
}

void TCRotEnc::switch_scan(bool force)
{
    unsigned long now = millis();
    unsigned long waitTime = now - _startTime;

    if(!haveSwitch) return;

    if(!force && (now - lastSwUpd < HWSWUPD_DELAY))
        return;

    lastSwUpd = now;
    
    bool active = (readSwitch() == _buttonPressed);
    
    switch(_state) {
    case TCBS_IDLE:
        if(active) {
            switch_transitionTo(TCBS_PRESSED);
            _startTime = now;
            if(_pressStartFunc) _pressStartFunc();
        }
        break;

    case TCBS_PRESSED:
        if(!active) {
            switch_transitionTo(TCBS_RELEASED);
            _startTime = now;
        } else if(waitTime > _longPressDur) {
            if(_longPressStartFunc) _longPressStartFunc();
            switch_transitionTo(TCBS_LONGPRESS);
        }
        break;

    case TCBS_RELEASED:
        if((!active) && (waitTime > _pressDur)) {
            if(_pressFunc) _pressFunc();
            switch_reset();
        }
        break;
  
    case TCBS_LONGPRESS:
        if(!active) {
            switch_transitionTo(TCBS_LONGPRESSEND);
            _startTime = now;
        } else if(_elongPressDur && (waitTime > _elongPressDur)) {
            if(_elongPressStartFunc) _elongPressStartFunc(); 
            switch_transitionTo(TCBS_ELONGPRESS);
        }
        break;

    case TCBS_LONGPRESSEND:
        if(_longPressStopFunc) _longPressStopFunc();
        switch_reset();
        break;

    case TCBS_ELONGPRESS:
        if(!active) {
            switch_transitionTo(TCBS_ELONGPRESSEND);
            _startTime = now;
        }
        break;

    case TCBS_ELONGPRESSEND:
        if(_elongPressStopFunc) _elongPressStopFunc();
        switch_reset();
        break;

    default:
        switch_transitionTo(TCBS_IDLE);
        break;
    }
}

void TCRotEnc::switch_reset()
{
    _state = TCBS_IDLE;
    _lastState = TCBS_IDLE;
    _startTime = 0;
}

// Advance to new state
void TCRotEnc::switch_transitionTo(ButtonState nextState)
{
    _lastState = _state;
    _state = nextState;
}

int TCRotEnc::read(uint16_t base, uint8_t reg, uint8_t *buf, uint8_t num)
{
    int i2clen;
    
    Wire.beginTransmission(_i2caddr);
    if(base <= 0xff) Wire.write((uint8_t)base);
    Wire.write(reg);
    Wire.endTransmission(true);
    delay(1);
    i2clen = Wire.requestFrom(_i2caddr, (int)num);
    for(int i = 0; i < i2clen; i++) {
        buf[i] = Wire.read();
    }
    return i2clen;
}

void TCRotEnc::write(uint16_t base, uint8_t reg, uint8_t *buf, uint8_t num)
{
    Wire.beginTransmission(_i2caddr);
    if(base <= 0xff) Wire.write((uint8_t)base);
    Wire.write(reg);
    for(int i = 0; i < num; i++) {
        Wire.write(buf[i]);
    }
    Wire.endTransmission();
}
