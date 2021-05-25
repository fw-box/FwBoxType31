/*
 * switch.ino
 *
 *  Created on: 2020-05-15
 *      Author: Mixiaoxiao (Wang Bin)
 *
 * HAP section 8.38 Switch
 * An accessory contains a switch.
 *
 * This example shows how to:
 * 1. define a switch accessory and its characteristics (in my_accessory.c).
 * 2. get the switch-event sent from iOS Home APP.
 * 3. report the switch value to HomeKit.
 *
 * You should:
 * 1. read and use the Example01_TemperatureSensor with detailed comments
 *    to know the basic concept and usage of this library before other examples。
 * 2. erase the full flash or call homekit_storage_reset() in setup()
 *    to remove the previous HomeKit pairing storage and
 *    enable the pairing with the new accessory of this new HomeKit example.
 */

//
// Copyright (c) 2021 Fw-Box (https://fw-box.com)
// Author: Hartman Hsieh
//
// Description :
//   None
//
// Connections :
//
// Required Library :
//

#include <Arduino.h>
#include <arduino_homekit_server.h>
#include "FwBox.h"

#define DEVICE_TYPE 31
#define FIRMWARE_VERSION "1.0.2"

#define PIN_ON_OFF 12
#define PIN_LED 13
#define PIN_BUTTON 0

#define VAL_INDEX_ON_OFF 0

//
// Debug definitions
//
#define FW_BOX_DEBUG 0

#if FW_BOX_DEBUG == 1
  #define DBG_PRINT(VAL) Serial.print(VAL)
  #define DBG_PRINTLN(VAL) Serial.println(VAL)
  #define DBG_PRINTF(FORMAT, ARG) Serial.printf(FORMAT, ARG)
  #define DBG_PRINTF2(FORMAT, ARG1, ARG2) Serial.printf(FORMAT, ARG1, ARG2)
#else
  #define DBG_PRINT(VAL)
  #define DBG_PRINTLN(VAL)
  #define DBG_PRINTF(FORMAT, ARG)
  #define DBG_PRINTF2(FORMAT, ARG1, ARG2)
#endif

//
// Function definitions
//
void ICACHE_RAM_ATTR onButtonPressed();

//
// Global variable
//
bool FlagButtonPressed = false;

//==============================
// HomeKit setup and loop
//==============================

// access your HomeKit characteristics defined in my_accessory.c
extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t cha_switch_on;

static uint32_t next_heap_millis = 0;

void setup()
{
  Serial.begin(115200);

  //
  // Initialize the fw-box core (early stage)
  //
  fbEarlyBegin(DEVICE_TYPE, FIRMWARE_VERSION);
  FwBoxIns.setGpioStatusLed(PIN_LED);

  pinMode(PIN_ON_OFF, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT);

  //
  // Initialize the fw-box core
  //
  fbBegin(DEVICE_TYPE, FIRMWARE_VERSION);

  //
  // Setup MQTT subscribe callback
  //
  setRcvValueCallback(onReceiveValue);

  //
  // Attach interrupt for button
  //
  attachInterrupt(PIN_BUTTON, onButtonPressed, FALLING); //assign int0

  //homekit_storage_reset(); // to remove the previous HomeKit pairing storage when you first run this new HomeKit example
  my_homekit_setup();
  
} // void setup()

void loop()
{
  if(FlagButtonPressed == true) {
    //
    // Toggle the GPIO status of relay
    //
    if (digitalRead(PIN_ON_OFF) == 0) {
      actionOn();
    }
    else {
      actionOff();
    }
    DBG_PRINT("digitalRead(PIN_ON_OFF)=");
    DBG_PRINTLN(digitalRead(PIN_ON_OFF));

    //
    // Reset the flag
    //
    FlagButtonPressed = false;
  }

  FwBoxIns.setValue(VAL_INDEX_ON_OFF, digitalRead(PIN_ON_OFF));

  //
  // Run the handle
  //
  fbHandle();

  my_homekit_loop();

} // END OF "void loop()"

void onReceiveValue(int valueIndex, String* payload)
{
  DBG_PRINT("onReceiveValue valueIndex = ");
  DBG_PRINTLN(valueIndex);
  DBG_PRINT("onReceiveValue *payload = ");
  DBG_PRINTLN(*payload);

  if(valueIndex == 0) { // Relay
    payload->toUpperCase();
    if(payload->equals("ON") == true)
    {
      actionOn();
    }
    else
    {
      actionOff();
    }
  }
}

void ICACHE_RAM_ATTR onButtonPressed()
{
  static unsigned long previous_pressed_time = 0;
  if((millis() - previous_pressed_time) > 900) { // Debouncing
    FlagButtonPressed = true;
    previous_pressed_time = millis();
  }
}

//Called when the switch value is changed by iOS Home APP
void cha_switch_on_setter(const homekit_value_t value) {
  bool app_val = value.bool_value;
  cha_switch_on.value.bool_value = app_val;  //sync the value
  //LOG_D("Switch: %s", app_val ? "ON" : "OFF");
  if (app_val) {
    actionOn();
  }
  else {
    actionOff();
  }
}

void my_homekit_setup() {
  //pinMode(PIN_ON_OFF, OUTPUT);
  //digitalWrite(PIN_ON_OFF, HIGH);

  //Add the .setter function to get the switch-event sent from iOS Home APP.
  //The .setter should be added before arduino_homekit_setup.
  //HomeKit sever uses the .setter_ex internally, see homekit_accessories_init function.
  //Maybe this is a legacy design issue in the original esp-homekit library,
  //and I have no reason to modify this "feature".
  cha_switch_on.setter = cha_switch_on_setter;
  arduino_homekit_setup(&config);

  //report the switch value to HomeKit if it is changed (e.g. by a physical button)
  //bool switch_is_on = true/false;
  //cha_switch_on.value.bool_value = switch_is_on;
  //homekit_characteristic_notify(&cha_switch_on, cha_switch_on.value);
}

void my_homekit_loop() {
  arduino_homekit_loop();
  const uint32_t t = millis();
  if (t > next_heap_millis) {
    // show heap info every 5 seconds
    next_heap_millis = t + 5 * 1000;
    //LOG_D("Free heap: %d, HomeKit clients: %d",
    //    ESP.getFreeHeap(), arduino_homekit_connected_clients_count());

  }
}

void actionOn()
{
  digitalWrite(PIN_ON_OFF, HIGH);
  FwBoxIns.mqttPublish(0, "ON"); // Sync the status to MQTT server

  //
  // Sync the status to LED
  //
  digitalWrite(PIN_LED, LOW); // This value is reverse

  cha_switch_on.value.bool_value = HIGH;
  homekit_characteristic_notify(&cha_switch_on, cha_switch_on.value);
}

void actionOff()
{
  digitalWrite(PIN_ON_OFF, LOW);
  FwBoxIns.mqttPublish(0, "OFF"); // Sync the status to MQTT server

  //
  // Sync the status to LED
  //
  digitalWrite(PIN_LED, HIGH); // This value is reverse

  cha_switch_on.value.bool_value = LOW;
  homekit_characteristic_notify(&cha_switch_on, cha_switch_on.value);
}
