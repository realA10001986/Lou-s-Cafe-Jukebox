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


#ifndef _JBDISPLAYS_H
#define _JBDISPLAYS_H

/*
 * PWM LED class for white and blue LEDs
 */

class PWMLED {

    public:

        PWMLED(uint8_t pwm_pin);
        void begin(uint8_t ledChannel, uint32_t freq, uint8_t resolution, uint8_t pwm_pin = 255);

        void setDC(uint32_t dutyCycle);
        uint32_t getDC();
        
    private:
        uint8_t   _pwm_pin;
        uint8_t   _chnl;
        uint32_t  _freq;
        uint8_t   _res;

        uint32_t _curDutyCycle;
};

/*
 * Jukebox LED class
 */

class JBLED {

    public:

        JBLED(uint8_t timer_no);
        void begin(int pin);
        
        void specialSignal(uint8_t signum);
        bool specialDone();
        uint8_t curSignal();
        
    private:
        hw_timer_t *_JBTimer_Cfg = NULL;
        uint8_t _timer_no;
        
};

#define JBSEQ_NOAUDIO     1
#define JBSEQ_WAIT        2   // LOOP
#define JBSEQ_ALARM       3
#define JBSEQ_ERROR       4   // LOOP
#define JBSEQ_OK          5
#define JBSEQ_UPDAVAIL    6
#define JBSEQ_NOMUSIC     7
#define JBSEQ_SHUFFLE_ON  8
#define JBSEQ_SHUFFLE_OFF 9
#define JBSEQ_RESETIPPW   10
#define JBSEQ_MPMODE      11
#define JBSEQ_STMODE      12
#define JBSEQ_RMMODE      13
#define JBSEQ_MAX         JBSEQ_RMMODE

/* 
 *  PanelLEDs class
 */

// Panel board 1.0: 0x20, 0x21
// Panel board 1.1: 0x21, 0x23
#define MCP23017_1_ADDR  0x20 // MCP23017 16 bit port expander
#define MCP23017_2_ADDR  0x21
#define MCP23017_3_ADDR  0x23

class PanelLEDs {

    public:

        PanelLEDs();
        void begin();

        void onOff(int onOff);

        void setLEDs(int letter, int number, int direct = 0);
        void getCurrLit(int& ltr, int& num);

    private:
        void write16(int adr, uint16_t regno, uint16_t value);
        void write8(int adr, uint16_t regno, uint8_t value);

        int _adr1, _adr2;

        int _havePanelLEDs = 0;
        int _on = 0;

        int _lastLtr = 0;
        int  _lastNum = 0;
};

#endif
