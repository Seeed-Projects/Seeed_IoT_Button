#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

/* Button Configuration */
#define BUTTON_PIN 9
#define DOUBLE_CLICK_TIME 400    // Maximum time between clicks for double click (ms)
#define LONG_PRESS_TIME 1000     // Minimum time for long press (ms)
#define DEBOUNCE_TIME 50         // Debounce time (ms)

/* Zigbee Configuration */
#define BUTTON_ENDPOINT_NUMBER 10

/* Button States */
typedef enum {
  BUTTON_IDLE,
  BUTTON_PRESSED,
  BUTTON_RELEASED,
  BUTTON_DOUBLE_CLICK_WAIT,
} ButtonState;

/* Button Events */
typedef enum {
  BUTTON_EVENT_SINGLE_CLICK = 1,
  BUTTON_EVENT_DOUBLE_CLICK = 2,
  BUTTON_EVENT_LONG_PRESS = 3,
} ButtonEvent;

/* Global Variables */
ZigbeeMultiButton zbIoTButton = ZigbeeMultiButton(BUTTON_ENDPOINT_NUMBER);
ButtonState buttonState = BUTTON_IDLE;
unsigned long pressStartTime = 0;
unsigned long lastReleaseTime = 0;
bool longPressReported = false;

/********************* Button Functions **************************/
void handleButtonEvent(ButtonEvent event) {
  // Report the button event via Zigbee
  switch (event) {
    case BUTTON_EVENT_SINGLE_CLICK:
      Serial.println("Single Click Detected");
      zbIoTButton.click1();
      break;
    case BUTTON_EVENT_DOUBLE_CLICK:
      Serial.println("Double Click Detected");
      zbIoTButton.click2();
      break;
    case BUTTON_EVENT_LONG_PRESS:
      Serial.println("Long Press Detected");
      Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
      delay(1000);
      Zigbee.factoryReset();
      break;
  }
}

void processButton() {
  static unsigned long lastDebounceTime = 0;
  bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);
  unsigned long currentTime = millis();

  // Debounce check
  if (currentTime - lastDebounceTime < DEBOUNCE_TIME) {
    return;
  }

  switch (buttonState) {
    case BUTTON_IDLE:
      if (buttonPressed) {
        pressStartTime = currentTime;
        buttonState = BUTTON_PRESSED;
        longPressReported = false;
      }
      break;

    case BUTTON_PRESSED:
      if (!buttonPressed) {
        // Button released
        buttonState = BUTTON_RELEASED;
        lastReleaseTime = currentTime;
      } else if (!longPressReported && (currentTime - pressStartTime >= LONG_PRESS_TIME)) {
        // Long press detected
        handleButtonEvent(BUTTON_EVENT_LONG_PRESS);
        longPressReported = true;
      }
      break;

    case BUTTON_RELEASED:
      if (currentTime - lastReleaseTime < DOUBLE_CLICK_TIME) {
        if (buttonPressed) {
          // Second press within double click time
          handleButtonEvent(BUTTON_EVENT_DOUBLE_CLICK);
          buttonState = BUTTON_IDLE;
          lastDebounceTime = currentTime;
        }
      } else {
        // Single click if no long press was reported
        if (!longPressReported) {
          handleButtonEvent(BUTTON_EVENT_SINGLE_CLICK);
        }
        buttonState = BUTTON_IDLE;
      }
      break;

    case BUTTON_DOUBLE_CLICK_WAIT:
      if (currentTime - lastReleaseTime >= DOUBLE_CLICK_TIME) {
        buttonState = BUTTON_IDLE;
        if (!longPressReported) {
          handleButtonEvent(BUTTON_EVENT_SINGLE_CLICK);
        }
      }
      break;
  }
}

/********************* Arduino Setup **************************/
void setup() {
  Serial.begin(115200);
  
  Serial.println("Zigbee IoT Button Starting...");

  // Initialize button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Set Zigbee device information
  zbIoTButton.setManufacturerAndModel("Seeed Studio", "IoT_Button");

  // Add endpoint to Zigbee Core
  Zigbee.addEndpoint(&zbIoTButton);

  Serial.println("Starting Zigbee...");
  // When all EPs are registered, start Zigbee in End Device mode
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  } else {
    Serial.println("Zigbee started successfully!");
  }
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
}

/********************* Arduino Loop **************************/
void loop() {
  // Process button state
  processButton();

  delay(100);
} 