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
String completedString = "";
String BinaryList = "";
String BitValue = "";
int ConStatus = 0;
int iter = 1;
int8_t thevol = 50;
unsigned long previousMillis = 0;
const long interval = 3000;
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
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      ConStatus = check_connection();
      previousMillis = currentMillis;
    }
    //delay(250);
    //ConStatus = check_connection();
    //delay(250);
    if (ConStatus != 1) {
      //      Serial.println("Here");
      bufferingChase_S(Strip.Color(201 / 2, 59 / 2, 217 / 2)); // purple
      iter = 1;
    }

    if (iter == 1 && ConStatus == 1) {
      // blue flash
      for (int j = 0; j < 2; j++) {
        for (uint16_t i = 0; i < Strip.numPixels(); i++) {
          Strip.setPixelColor(i, Strip.Color(0, 60, 200));
        }
        Strip.show();
        delay(1000);
        for (uint16_t i = 0; i < Strip.numPixels(); i++) {
          Strip.setPixelColor(i, Strip.Color(0, 0, 0));
        }
        Strip.show();
        delay(500);
      }
      iter = 2;
    }

    if (iter == 2 && ConStatus == 1) {
      // Serial.println("I'm now here");
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
        //for (i = 1; i < 25; i++) {
          //Strip.setPixelColor(24, c);
        //}
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
        if (currentMillis - previousMillis >= interval) {
          ConStatus = check_connection();
          previousMillis = currentMillis;
        }
      }
      //      for (int j = 0; j < 256; j++) {   // cycle all 256 colors in the wheel
      //        for (int q = 0; q < 3; q++) {
      //          for (int i = 0; i < Strip.numPixels(); i = i + 3) {
      //            Strip.setPixelColor(i + q, Wheel((i + j) % 255)); //turn every third pixel on
      //          }
      //          Strip.show();
      //          delay(30);
      //          if (currentMillis - previousMillis >= interval) {
      //            ConStatus = check_connection();
      //            previousMillis = currentMillis;
      //          }
      //          for (int i = 0; i < Strip.numPixels(); i = i + 3) {
      //            Strip.setPixelColor(i + q, 0);      //turn every third pixel off
      //          }
      //        }
      //      }
    }
  }
  // Some example procedures showing how to display to the pixels:
  //colorWipe(Strip.Color(255, 0, 0), 50); // Red
  //colorWipe(Strip.Color(0, 255, 0), 50); // Green
  //colorWipe(Strip.Color(0, 0, 255), 50); // Blue
  // Send a theater pixel chase in...
  //theaterChase(Strip.Color(127, 127, 127), 50); // White
  //theaterChase(Strip.Color(127, 0, 0), 50); // Red
  //theaterChase(Strip.Color(0, 0, 127), 50); // Blue
  //rainbow(20);
  //rainbowCycle(20);
  //theaterChaseRainbow(50);
}

int check_connection() {
  String completedString = "";
  String BinaryList = "";
  String BitValue = "";

  RN52.println("Q");
  //delay(20);
  while (RN52.available() > 0) {
    byte inChar = RN52.read();
    if (inChar == 13 || inChar == 10) {
      ; //carriage return and newline
    }
    else {
      //Serial.println(inChar);
      completedString += char(inChar);
      //Serial.println(char(inChar));
    }
    if (completedString.length() == 4) {
      //Serial.println(completedString);
      for (int i = 0; i < 4; i++) {
        if ((completedString.charAt(i) == 'Q') ||
            (completedString.charAt(i) == 'R') ||
            (completedString.charAt(i) == '?') ||
            (completedString.charAt(i) == 'Q') ||
            (completedString.charAt(i) == 'V') ||
            (completedString.charAt(i) == 'P')) {
          //'K', 'R' and '?' are not hexadecimal values therefore must be from
          //'AOK', 'ERR' or '?' phases so whole string can be ignored.
          return 0;
          //break;
        }
        switch (completedString.charAt(i)) {
          case '0':
            //Serial.println("0000");
            BitValue = "0000";
            BinaryList += BitValue;
            break;
          case '1':
            //Serial.println("0001");
            BitValue = "0001";
            BinaryList += BitValue;
            break;
          case '2':
            //Serial.println("0010");
            BitValue = "0010";
            BinaryList += BitValue;
            break;
          case '3':
            //Serial.println("0011");
            BitValue = "0011";
            BinaryList += BitValue;
            break;
          case '4':
            //Serial.println("0100");
            BitValue = "0100";
            BinaryList += BitValue;
            break;
          case '5':
            //Serial.println("0101");
            BitValue = "0101";
            BinaryList += BitValue;
            break;
          case '6':
            //Serial.println("0110");
            BitValue = "0110";
            BinaryList += BitValue;
            break;
          case '7':
            //Serial.println("0111");
            BitValue = "0111";
            BinaryList += BitValue;
            break;
          case '8':
            //Serial.println("1000");
            BitValue = "1000";
            BinaryList += BitValue;
            break;
          case '9':
            //Serial.println("1001");
            BitValue = "1001";
            BinaryList += BitValue;
            break;
          case 'A':
            //Serial.println("1010");
            BitValue = "1010";
            BinaryList += BitValue;
            break;
          case 'B':
            //Serial.println("1011");
            BitValue = "1011";
            BinaryList += BitValue;
            break;
          case 'C':
            //Serial.println("1100");
            BitValue = "1100";
            BinaryList += BitValue;
            break;
          case 'D':
            //Serial.println("1101");
            BitValue = "1101";
            BinaryList += BitValue;
            break;
          case 'E':
            //Serial.println("1110");
            BitValue = "1110";
            BinaryList += BitValue;
            break;
          case 'F':
            //Serial.println("1111");
            BitValue = "1111";
            BinaryList += BitValue;
            break;
          case 'a':
            //Serial.println("1010");
            BitValue = "1010";
            BinaryList += BitValue;
            break;
          case 'b':
            //Serial.println("1011");
            BitValue = "1011";
            BinaryList += BitValue;
            break;
          case 'c':
            //Serial.println("1100");
            BitValue = "1100";
            BinaryList += BitValue;
            break;
          case 'd':
            //Serial.println("1101");
            BitValue = "1101";
            BinaryList += BitValue;
            break;
          case 'e':
            //Serial.println("1110");
            BitValue = "1110";
            BinaryList += BitValue;
            break;
          case 'f':
            //Serial.println("1111");
            BitValue = "1111";
            BinaryList += BitValue;
            break;
          default:
            //BitValue = "";
            //BinaryList += BitValue;
            return 0;
            break;
        }
      }
      //Serial.println(BinaryList);
      if ((BinaryList.charAt(7) == '1') ||
          (BinaryList.charAt(6) == '1') ||
          (BinaryList.charAt(5) == '1') ||
          (BinaryList.charAt(4) == '1')) {
        //Serial.println("Connection");
        return 1; // Connection
      }
      else {
        //Serial.println("no connection");
        return 0; // no connection
      }
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
    if (thevol < 45) { //offset so that dont overdo the gain
      RN52.println("AV+");
    }
    Wire.beginTransmission(MAX9744_I2CADDR);
    Wire.write(thevol);
  }
  else {
    //Volume down
    thevol--;
    if (thevol < 0) {
      thevol = 0;
    }
    if (thevol > 45) { //offset so that dont overdo the gain
      RN52.println("AV-");
    }
    Wire.beginTransmission(MAX9744_I2CADDR);
    Wire.write(thevol);
  }
}

void PlayPause() { //Play Pause Control
  RN52.println("AP");
  //  Serial.println("play/pause");
  //RN52.read();
  //delay(200);
}

void bufferingChase_S(uint32_t c) {
  for (int q = 0; q < Strip.numPixels() + 3; q++) {
    for (int i = 0; i < 4; i++) {
      Strip.setPixelColor(q - i, c);
    }
    Strip.show();
    delay(20);
    //for (int i=0; i<3; i++) {
    Strip.setPixelColor(q - 3, 0);
    //}
    Strip.show();
    delay(20);
  }
}

// Fill the dots one after the other with a color
//void colorWipe(uint32_t c, uint8_t wait) {
//  for (uint16_t i = 0; i < Strip.numPixels(); i++) {
//    Strip.setPixelColor(i, c);
//    Strip.show();
//    delay(wait);
//  }
//}

//void rainbow(uint8_t wait) {
//  uint16_t i, j;
//  for (j = 0; j < 256; j++) {
//    for (i = 0; i < Strip.numPixels(); i++) {
//      Strip.setPixelColor(i, Wheel((i + j) & 255));
//    }
//    Strip.show();
//    delay(wait);
//  }
//}

// Slightly different, this makes the rainbow equally distributed throughout
//void rainbowCycle(uint8_t wait) {
//  uint16_t i, j;
//  for (j = 0; j < 256 * 5; j++) { // 5 cycles of all colors on wheel
//    for (i = 0; i < Strip.numPixels(); i++) {
//      Strip.setPixelColor(i, Wheel(((i * 256 / Strip.numPixels()) + j) & 255));
//    }
//    Strip.show();
//    delay(wait);
//  }
//}

//Theatre-style crawling lights.
//void theaterChase(uint32_t c, uint8_t wait) {
//  for (int j = 0; j < 10; j++) { //do 10 cycles of chasing
//    for (int q = 0; q < 3; q++) {
//      for (int i = 0; i < Strip.numPixels(); i = i + 3) {
//        Strip.setPixelColor(i + q, c);  //turn every third pixel on
//      }
//      Strip.show();
//      delay(wait);
//      for (int i = 0; i < Strip.numPixels(); i = i + 3) {
//        Strip.setPixelColor(i + q, 0);      //turn every third pixel off
//      }
//    }
//  }
//}

//Theatre-style crawling lights with rainbow effect
//void theaterChaseRainbow(uint8_t wait) {
//  for (int j = 0; j < 256; j++) {   // cycle all 256 colors in the wheel
//    for (int q = 0; q < 3; q++) {
//      for (int i = 0; i < Strip.numPixels(); i = i + 3) {
//        Strip.setPixelColor(i + q, Wheel( (i + j) % 255)); //turn every third pixel on
//      }
//      Strip.show();
//      delay(wait);
//      for (int i = 0; i < Strip.numPixels(); i = i + 3) {
//        Strip.setPixelColor(i + q, 0);      //turn every third pixel off
//      }
//    }
//  }
//}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return Strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return Strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return Strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}


