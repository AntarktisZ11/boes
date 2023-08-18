#include <Adafruit_Protomatter.h>
#include <Fonts/FreeSerifBold9pt7b.h>
#include <Thread.h>
#include <StaticThreadController.h>
#include "timer.h"
#include "pico/stdlib.h"
#include "logo.h"

// Color definitions used for setting text and background color
#define BLACK    0x0000
#define RED      0xF800
#define GREEN    0x07E0
#define YELLOW   0xFF00
#define WHITE    0xFFFF
#define BLUE     0x00FF

// Display controller pins
uint8_t rgbPins[]  = {2, 3, 4, 5, 8, 9};
uint8_t addrPins[] = {10, 16, 18, 20, 22};
uint8_t clockPin   = 11;
uint8_t latchPin   = 12;
uint8_t oePin      = 13;

// Button/input pins
uint8_t startBtnPin = 15;           // Button that triggers a timer.start() call if triggered
uint8_t stopBtnPin = 14;            // Button that triggers a timer.stop() call if triggered
uint8_t resetBtnPin = 17;           // Button that triggers a timer.reset() call if triggered
uint8_t sensPot = 26;               // Potentiometer that controls the sensitivity of the vibration sensor
uint8_t judgeVibSensor = 27;        // Vibration sensor that triggers a timer.stop() call if triggered
uint8_t participantVibSensor = 28;  // Vibration sensor that triggers a timer.start() call if triggered
uint8_t buzzer = 6;

Adafruit_Protomatter matrix(
  64,          // Matrix width in pixels
  6,           // Bit depth -- 6 here provides maximum color options
  1, rgbPins,  // # of matrix chains, array of 6 RGB pins for each
  4, addrPins, // # of address pins (height is inferred), array of pins
  clockPin, latchPin, oePin, // Other matrix control pins
  true);       // HERE IS THE MAGIC FOR DOUBLE-BUFFERING!

int16_t  textX = matrix.width(), // Current text position (X)
         textY,                  // Current text position (Y)
         textMin,                // Text pos. (X) when scrolled off left edge
         hue = 0;
char     str[8];                // Buffer to hold timer text "XXX:YYY\0"

Timer timer;
long buzzer_timestamp = 0;
int start_grace_period_us = 1500000; // 1.5 sec start grace period


String httpRequest;

Thread timerThread = Thread();
Thread displayThread = Thread();
Thread buzzerThread = Thread();
StaticThreadController<4> controller (&timerThread, &displayThread, &buzzerThread);

void setup(void) {
  Serial.begin(115200);

  // Initialize matrix...
  ProtomatterStatus status = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)status);
  
  if(status != PROTOMATTER_OK) {
    // DO NOT CONTINUE if matrix setup encountered an error.
    Serial.println("Matrix error occurred.. cannot continue");
    while(true);
  }

  // TODO Maybe change font as adjacent zeroes touch each other
  matrix.setFont(&FreeSerifBold9pt7b); // Use nice bitmap font
  matrix.setTextColor(0xFFFF);         // White
  matrix.setRotation(2);
  int16_t  x1, y1;
  uint16_t w, h;
  sprintf(str, "%s:%s", timer.sec_counter, timer.ms_counter); // Get initial text to get bounds
  matrix.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  textX = matrix.width() / 2 - (x1 + w / 2); // Center text horizontally
  textY = matrix.height() / 2 - (y1 + h / 2); // Center text vertically

  pinMode(startBtnPin, INPUT_PULLUP);
  pinMode(stopBtnPin, INPUT_PULLUP);
  pinMode(resetBtnPin, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);

  timerThread.onRun(updateTimer);
  displayThread.onRun(updateDisplay);
  buzzerThread.onRun(updateBuzzer);

  timerThread.setInterval(1); // update every 1ms
  displayThread.setInterval(1); // update every 1ms
  buzzerThread.setInterval(100); // update every 100ms
  

  // Startup screen
  matrix.fillScreen(BLACK);
  matrix.drawRGBBitmap(0, 0, logo, 64, 32);
  matrix.show();
}


void loop(void) {
  controller.run();
}

void updateBuzzer() {
  unsigned long buzzer_time_diff = (time_us_64() - timer.timeout_timestamp) / _1_S_IN_US;

  // TODO Create constant for buzzer sound
  if (buzzer_time_diff >= 3) {
    noTone(buzzer);

    // Handles a faulty case where the time_diff is larger than 3 but state is still timeout_1
    return; 
  }

  // Handle buzzer timeout case(s)
  switch (timer.state) {
    case Timeout_1:
      tone(buzzer, 1000);
      break;
    
    case Timeout_2:
      tone(buzzer, 1000);
      break;

    default:
      noTone(buzzer);
  }

}

// Update timer timestamps and input handling
void updateTimer() {
  // Scale sens pot value
  // TODO Add two of these, one for judge and one for participant 
  // make them changeable from the web server
  uint16_t sensPotValue = (1024 * 0.01) + (0.33 * analogRead(sensPot));

  //! Is it worth only updating the relevant sensor instead of both?
  uint16_t judgeVibSensorValue = analogRead(judgeVibSensor);
  uint16_t participantVibSensorValue = analogRead(participantVibSensor);

  timer.run();
  
  // Handle button input handling for timer. (It should always be allowed to control from buttons)
  if (digitalRead(startBtnPin) == LOW) {
    timer.start();
  } else if ((digitalRead(stopBtnPin) == LOW)) {
    timer.stop();
  } else if (digitalRead(resetBtnPin) == LOW) {
    timer.reset();
  }

  switch (timer.state) {
    case Timeout_1:
    case Timeout_2:
    case Pause:
      // Timer should not be started/stopped from VibSensors. Buttons shall be used here instead.
      break;

    default:
      //! This should be fully ignored if it is already started.
      if (judgeVibSensorValue >= sensPotValue) {
        timer.start();
      } else if (participantVibSensorValue >= sensPotValue) { //! This should be checked only if it is currently running.
        // Participant only allowed to stop after 1.5 seconds after judge has started
        if (time_us_64() - timer.start_timestamp >= start_grace_period_us) {
          timer.stop();
        } else {
          Serial.println("Participant not allowed to stop yet");
        }
      }
      break;
  }
}

// Update display text based on input from timer
void updateDisplay() {
  timer.run();
  sprintf(str, "%s:%s", timer.sec_counter, timer.ms_counter);

  matrix.fillScreen(0);
  matrix.setCursor(textX, textY);

  // Timer text default white (init and reset).
  uint16_t textColor;

  switch (timer.state) {
    case Run:
      textColor = GREEN;
      break;

    case Pause:
      textColor = RED;
      break;

    case Timeout_1:
      textColor = YELLOW;
      break;
    
    case Timeout_2:
      textColor = BLUE;
      break;

    default:
      textColor = WHITE;
      break;
  }

  matrix.setTextColor(textColor);
  matrix.print(str);
  matrix.show();
}
