/*
 *  WLED usermod for keypad and brightness-pot.
 *  3'2020 https://github.com/hobbyquaker
 */

#include <Keypad.h>
const byte keypad_rows = 4;
const byte keypad_cols = 4;
char keypad_keys[keypad_rows][keypad_cols] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
};

byte keypad_colPins[keypad_rows] = {D3, D2, D1, D0};
byte keypad_rowPins[keypad_cols] = {D7, D6, D5, D4};

Keypad myKeypad = Keypad(makeKeymap(keypad_keys), keypad_rowPins, keypad_colPins, keypad_rows, keypad_cols);

void userSetup()
{

}

void userConnected()
{

}

long lastTime = 0;
int delayMs = 20; //we want to do something every 2 seconds

void userLoop()
{
    if (millis()-lastTime > delayMs)
    {

        long analog = analogRead(0);
        int new_bri = 1;
        if (analog > 900) {
            new_bri = 255;
        } else if (analog > 30) {
            new_bri = dim8_video(map(analog, 31, 900, 16, 255));
        }
        if (bri != new_bri) {
            bri = new_bri;
            colorUpdated(1);

        }

        char myKey = myKeypad.getKey();
        if (myKey != NULL) {
            switch (myKey) {
                case '1':
                    applyPreset(1);
                    break;
                case '2':
                    applyPreset(2);
                    break;
                case '3':
                    applyPreset(3);
                    break;
                case '4':
                    applyPreset(4);
                    break;
                case '5':
                    applyPreset(5);
                    break;
                case '6':
                    applyPreset(6);
                    break;
                case 'A':
                    applyPreset(7);
                    break;
                case 'B':
                    applyPreset(8);
                    break;

                case '7':
                    effectCurrent += 1;
                    if (effectCurrent >= MODE_COUNT) effectCurrent = 0;
                    colorUpdated(CALL_MODE_FX_CHANGED);
                    break;
                case '*':
                    effectCurrent -= 1;
                    if (effectCurrent < 0) effectCurrent = (MODE_COUNT-1);
                    colorUpdated(CALL_MODE_FX_CHANGED);
                    break;

                case '8':
                    if (effectSpeed < 240) {
                        effectSpeed += 12;
                    } else if (effectSpeed < 255) {
                        effectSpeed += 1;
                    }
                    colorUpdated(CALL_MODE_FX_CHANGED);
                    break;
                case '0':
                    if (effectSpeed > 15) {
                        effectSpeed -= 12;
                    } else if (effectSpeed > 0) {
                        effectSpeed -= 1;
                    }
                    colorUpdated(CALL_MODE_FX_CHANGED);
                    break;

                case '9':
                    if (effectIntensity < 240) {
                        effectIntensity += 12;
                    } else if (effectIntensity < 255) {
                        effectIntensity += 1;
                    }
                    colorUpdated(CALL_MODE_FX_CHANGED);
                    break;
                case '#':
                    if (effectIntensity > 15) {
                        effectIntensity -= 12;
                    } else if (effectIntensity > 0) {
                        effectIntensity -= 1;
                    }
                    colorUpdated(CALL_MODE_FX_CHANGED);
                    break;

                case 'C':
                    effectPalette += 1;
                    if (effectPalette >= 50) effectPalette = 0;
                    colorUpdated(CALL_MODE_FX_CHANGED);
                    break;
                case 'D':
                    effectPalette -= 1;
                    if (effectPalette <= 0) effectPalette = 50;
                    colorUpdated(CALL_MODE_FX_CHANGED);
                    break;

            }

        }

        lastTime = millis();
    }

}