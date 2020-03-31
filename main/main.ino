#include <EEPROM.h>

#include "IRremote.h"

// pinout
const int irLed = 7;
const int jacks[] = {2, 3, 4, 5};
const int rgb[] = {9, 10, 6};
const int usb5v = 8;
const int touchBtn = 12;

// required button on remote control - LG
unsigned long remoteControlBtn; // = 551519865;

// digital IN thresholds
const int usbVoltageThreshold = 0;
const int touchBtnThreshold = 0;

// color definitions
const uint8_t colorOrange[] = {255, 40, 0};
const uint8_t colorBlue[] = {0, 0, 255};
const uint8_t colorPurple[] = {255, 0, 40};
const uint8_t colorCyan[] = {0, 255, 20};
const uint8_t *colorSequence[] = {colorOrange, colorBlue, colorPurple, colorCyan};

const uint8_t colorRed[] = {255, 0, 0};
const uint8_t colorNone[] = {0, 0, 0};

// led delay definitions
const int initBrightnessDelayConst = 3000;
const int colorBrightnessDelayConst = 5;

// led state
bool ledActive = false;
bool colorMaxBrightness = true;
uint8_t colorBrightness = 255;
int colorBrightnessDelayLeft = 0;
int initBrightnessDelayLeft = 0;

// input state
bool usbConnectedActEvent = false;
bool usbConnected = false;
bool btnTouchedActEvent = false;
bool btnTouched = false;
int selectedInput = 0;

IRrecv irrecv(irLed);
decode_results results;

void createFancyRainbow() {
    int stepDelay = 3;

    for (int i = 0; i < 256; ++i) {
        analogWrite(rgb[0], i);

        delay(stepDelay);
    }

    for (int i = 0; i < 3; ++i) {
        for (int br = 0; br < 256; ++br)
        {
            analogWrite(rgb[i], 255 - br);
            analogWrite(rgb[(i+1) % 3], br);

            delay(stepDelay);
        }
    }

    for (int i = 0; i < 256; ++i) {
        analogWrite(rgb[0], 255 - i);

        delay(stepDelay);
    }
}

void setColor(const uint8_t *color, const uint8_t brightness) {
    for (int i = 0; i < 3; ++i) {
        int ledBrightnessRaw = (int)((double)color[i] * ((double)brightness / 255.0));
        int ledBrightness = (uint8_t)(ledBrightnessRaw >= 0 ? ledBrightnessRaw : 0);
        analogWrite(rgb[i], ledBrightness);
    }
}

void ledBlink(const uint8_t *color) {
    for (int i = 0; i < 2; ++i) {
        setColor(color, 255);
        delay(500);
        setColor(color, 0);
        delay(500);
    }
}

void setJack(const int jackNum) {
    for (int i = 0; i < sizeof(jacks) / sizeof(int); ++i) {
        digitalWrite(jacks[i], HIGH);
    }

    if (jackNum >= 0) {
        delay(100);
        digitalWrite(jacks[jackNum], LOW);
    }
}

void lightUpLed() {
    colorBrightnessDelayLeft = colorBrightnessDelayConst;
    initBrightnessDelayLeft = initBrightnessDelayConst;
    colorMaxBrightness = true;
    colorBrightness = 255;
    ledActive = true;
}

void resetLed() {
    colorBrightnessDelayLeft = 0;
    initBrightnessDelayLeft = 0;
    colorMaxBrightness = false;
    colorBrightness = 0;
    ledActive = false;

    ledBlink(colorSequence[selectedInput]);
}

void setInput() {
    setJack(selectedInput);
    lightUpLed();
    setColor(colorSequence[selectedInput], colorBrightness);
}

void resetInput() {
    setJack(-1);
    resetLed();
    setColor(colorNone, 0);
}

void updateLed() {
    if (colorMaxBrightness) {
        if (initBrightnessDelayLeft > 0) {
            initBrightnessDelayLeft--;
        } else {
            colorMaxBrightness = false;
            setColor(colorNone, 0);
        }
    } else {
        if (colorBrightness > 0) {
            if (colorBrightnessDelayLeft > 0) {
                colorBrightnessDelayLeft--;
            } else {
                colorBrightness--;
                colorBrightnessDelayLeft = colorBrightnessDelayConst;
            }
            setColor(colorSequence[selectedInput], colorBrightness);
        } else {
            ledActive = false;
            setColor(colorNone, 0);
        }
    }
}

void setup()
{
    irrecv.enableIRIn();

    pinMode(usb5v, INPUT);
    pinMode(touchBtn, INPUT);
  
    // init relay
    for (int i = 0; i < sizeof(jacks) / sizeof(int); ++i) {
        pinMode(jacks[i], OUTPUT);
        digitalWrite(jacks[i], HIGH);
        delay(1);
    }
  
    // init LED
    for (int i = 0; i < sizeof(rgb) / sizeof(int); ++i) {
        pinMode(rgb[i], OUTPUT);
        analogWrite(rgb[i], 0);
    }

    // test for active button to change saved remote control button in EEPROM
    if (pollingBtn()) {
        ledBlink(colorRed);

        setColor(colorRed, 255);
        setRemoteControlBtn();
        setColor(colorNone, 0);
        
        delay(2000);
    }

    // load remote control button from EEPROM
    remoteControlBtn = getRemoteControlBtn();

    createFancyRainbow();
    delay(2000);
}

bool pollingIR() {
    if (irrecv.decode(&results)) {
        bool result = results.value == remoteControlBtn;
        irrecv.resume();
        return result;
    }

    return false;
}

bool pollingBtn() {
    btnTouchedActEvent = digitalRead(touchBtn) > touchBtnThreshold;

    // button deactivated
    if (btnTouched && !btnTouchedActEvent) {
        btnTouched = false;
        return false;
    }
    
    // button activated
    if (!btnTouched && btnTouchedActEvent) {
        btnTouched = true;
        return true;
    }

    return false;
}

bool pollingUSBConn() {
    usbConnectedActEvent = digitalRead(usb5v) > usbVoltageThreshold;

    // USB disconnected
    if (usbConnected && !usbConnectedActEvent) {
        usbConnected = false;
        resetInput();
        return false;
    }

    // USB connected
    if (!usbConnected && usbConnectedActEvent) {
        usbConnected = true;
        setInput();
        irrecv.resume();
        return true;
    }

    return false;
}

void loop()
{
    // update USB connection
    pollingUSBConn();

    // test IR or button and change audio input
    if (pollingIR() || pollingBtn()) {
        if (usbConnected) {
            selectedInput = (selectedInput + 1) % (sizeof(jacks) / sizeof(int));

            setInput();
        } else {
            ledBlink(colorSequence[selectedInput]);
        }
    }

    // decrease LED brightness
    if (usbConnected && ledActive) {
        updateLed();
    }

    delay(1);
}

unsigned long getRemoteControlBtn() {
    unsigned int byteArray[4];

    for (int i = 0; i < 4; i++) {
        byteArray[i] = EEPROM.read(i);
    }

    return ((unsigned long)(byteArray[0]) << 24)
         | ((unsigned long)(byteArray[1]) << 16)
         | ((unsigned long)(byteArray[2]) << 8)
         | (unsigned long)(byteArray[3]);
}

void setRemoteControlBtn() {
    while (!irrecv.decode(&results)) {
        delay(1);
    }

    unsigned long irBtn = results.value;
    irrecv.resume();

    unsigned int byteArray[4];

    byteArray[0] = (unsigned int)((irBtn & 0xFF000000) >> 24);
    byteArray[1] = (unsigned int)((irBtn & 0x00FF0000) >> 16);
    byteArray[2] = (unsigned int)((irBtn & 0x0000FF00) >> 8);
    byteArray[3] = (unsigned int)((irBtn & 0x000000FF));

    for (int i = 0; i < 4; i++) {
        EEPROM.write(i, byteArray[i]);
    }
}
