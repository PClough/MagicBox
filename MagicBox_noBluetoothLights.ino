#ifdef __AVR__
#include <avr/power.h>
#endif
#include <SoftwareSerial.h>
#include <ResponsiveAnalogRead.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>

#define PIN_S 7
#define N_Pixels_Strip 25
#define encoderPinA  2
#define encoderPinB  4
#define encoderButtonPin 3
#define commLinkPin 5
#define MAX9744_I2CADDR 0x4B
#define DC_Offset 0  // DC offset in mic signal - if unusure, leave 0
#define Noise 0 //100  // Noise/hum/interference in mic signal
#define Samples 60  // Length of buffer for dynamic level adjustment

//volatile unsigned int encoderPos = 0;
int8_t thevol = 50;
const int Top = 9;
byte volCount = 0;      // Frame counter for storing past volume data
int vol[Samples];       // Collection of prior volume samples
int lvl = 10;     // Current "dampened" audio level
int minLvlAvg = 0;      // For dynamic adjustment of graph low & high
int maxLvlAvg = 512;
const int MicPin = A1;
int midLEDs[] = {20, 21, 22, 23, 24, 1};
int RightLEDs[] = {2, 3, 4, 5, 6, 7, 8, 9, 10};
int LeftLEDs[] = {19, 18, 17, 16, 15, 14, 13, 12, 11};

ResponsiveAnalogRead analog(MicPin, false);
SoftwareSerial RN52(10, 11); // RX, TX
Adafruit_NeoPixel Strip = Adafruit_NeoPixel(N_Pixels_Strip, PIN_S, NEO_GRB + NEO_KHZ800);

void setup() {
  //Serial.begin(9600);

  RN52.begin(19200);
  Wire.begin();
  Strip.begin();
  Strip.setBrightness(200); // Set LED brightness 0-255
  Strip.show(); // Initialize all pixels to 'off'

  // Set up pin numbers
  pinMode(encoderPinA, INPUT);
  digitalWrite(encoderPinA, HIGH); // turn on pullup resistor
  pinMode(encoderPinB, INPUT);
  digitalWrite(encoderPinB, HIGH); // turn on pullup resistor
  pinMode(encoderButtonPin, INPUT);
  digitalWrite(encoderButtonPin, HIGH); // turn on pullup resistor
  digitalWrite(commLinkPin, LOW); //GPIO 9 on RN-52

  analog.setSnapMultiplier(0.1);
  memset(vol, 0, sizeof(vol));

  attachInterrupt(digitalPinToInterrupt(2), Vol_Control, CHANGE);
  attachInterrupt(digitalPinToInterrupt(3), PlayPause, FALLING);
}

void loop() {
  while (1) {
    for (int j = 0; j < 256; j++) {   // cycle all 256 colors in the wheel
      uint8_t  i;
      uint16_t minLvl, maxLvl;
      int n, height;
      analog.update();
      n = analog.getValue();
      //n   = analogRead(MicPin);                 // Raw reading from mic
      n   = abs(n - 512 - DC_Offset);            // Center on zero
      //n   = (n <= Noise) ? 0 : (n - Noise);      // Remove noise/hum
      lvl = n;
      //lvl = ((lvl * 7) + n + 100) >> 3;   // "Dampened" reading (else looks twitchy)
      height = Top * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);

      // Clip output
      if (height < 0) {
        height = 0;
      }
      else if (height > Top) {
        height = Top;
      }

      uint32_t c = Wheel(j & 255);
      // Middle LEDs at front
      for (i = 0; i < 6; i++) {
        int f = midLEDs[i];
        Strip.setPixelColor(f, c);
      }
      //R and L wing LEDs
      for (i = 0; i < Top; i++) {
        int R = RightLEDs[i];
        int L = LeftLEDs[i];
        if (i > height) {
          Strip.setPixelColor(R, 0, 0, 0);
          Strip.setPixelColor(L, 0, 0, 0);
        }
        else {
          Strip.setPixelColor(R, c);
          Strip.setPixelColor(L, c);
        }
      }
      Strip.show();

      vol[volCount] = n;                      // Save sample for dynamic leveling
      if (++volCount >= Samples) {
        volCount = 0; // Advance/rollover sample counter
      }

      // Get volume range of prior frames
      minLvl = maxLvl = vol[0];
      for (i = 1; i < Samples; i++) {
        if (vol[i] < minLvl) {
          minLvl = vol[i];
        }
        else if (vol[i] > maxLvl) {
          maxLvl = vol[i];
        }
      }

      if ((maxLvl - minLvl) < Top) {
        maxLvl = minLvl + Top;
        minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6; // Dampen min/max levels
        maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6; // (fake rolling average)
      }
      delay(50);
    }
  }
}

void Vol_Control() { //Volume control
  /* If pinA and pinB are both high or both low, it is spinning
     forward. If they're different, it's going backward. */
  if (digitalRead(encoderPinA) == digitalRead(encoderPinB)) {
    // Volume up
    thevol++;
    if (thevol > 63) {
      thevol = 63;
    }
    Wire.beginTransmission(MAX9744_I2CADDR);
    Wire.write(thevol);
    RN52.println("AV+");
  }
  else {
    //Volume down
    thevol--;
    if (thevol < 0) {
      thevol = 0;
    }
    Wire.beginTransmission(MAX9744_I2CADDR);
    Wire.write(thevol);
    RN52.println("AV-");
  }
}

void PlayPause() { //Play Pause Control
  RN52.println("AP");
  //  Serial.println("play/pause");
  //RN52.read();
  //delay(200);
}

