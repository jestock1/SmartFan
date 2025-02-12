/*
 * Copyright (c) 2025 Joe Estock
 *
 * Firmware for converting a regular, non-smart, 3 speed ceiling fan with light
 * into a smart ceiling fan that is controllable via Apple HomeKit
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <HomeSpan.h>
#include "DEV_Identify.h"
#include "DEV_CeilingFan.h"
#include "DEV_Light.h"

// default HomeSpan setup code: 466-37-726
// default OTA password: homespan-ota

#define FIRMWARE_VERSION  "1.1.11"
#define SERIAL_NUMBER     "20250117"
#define PIN_LIGHT_RELAY   D5                      // Relay to control the fan light
#define PIN_FAN_RELAY_L   D10                     // Fan relay for low speed
#define PIN_FAN_RELAY_M   D9                      // Fan relay for medium speed
#define PIN_FAN_RELAY_H   D8                      // Fan relay for high speed
#define PIN_FAN_SIGNAL_L  D3                      // Signal pin for low speed
#define PIN_FAN_SIGNAL_M  D7                      // Signal pin for medium speed
#define PIN_FAN_SIGNAL_H  D2                      // Signal pin for high speed
#define RSWITCH_MADDR     "XX:XX:XX:XX:XX:XX"     // MAC Address of the remote ESP32 monitoring the physical light switch
                                                  // This needs to match the MAC Address of the MCU that monitors the physical
                                                  // light switch state

DEV_CeilingFan *ceilingFan;                       // Fan controller
DEV_Light *ceilingFanLight;                       // Light controller
unsigned long lastDebounceTime;                   // Prevent multiple events firing for the same action
SpanPoint *remoteLightSwitch;                     // Secondary MCU to handle the state of a physical switch
int lastSwitchState;                              // Last position of the physical switch (0 = off, 1 = on)
int lastFanSwitchState;                           // Last position of the fan pull chain (0 = off, 1 = low, 2 = medium, 3 = high)
int lastFanL;                                     // Last reading for the fan low speed (HIGH, LOW)
int lastFanM;                                     // Last reading for the fan medium speed (HIGH, LOW)
int lastFanH;                                     // Last reading for the fan high speed (HIGH, LOW)

void setup() {
  Serial.begin(115200);

  pinMode(PIN_FAN_RELAY_L, OUTPUT);
  pinMode(PIN_FAN_RELAY_M, OUTPUT);
  pinMode(PIN_FAN_RELAY_H, OUTPUT);
  pinMode(PIN_LIGHT_RELAY, OUTPUT);
  pinMode(PIN_FAN_SIGNAL_L, INPUT_PULLDOWN);
  pinMode(PIN_FAN_SIGNAL_M, INPUT_PULLDOWN);
  pinMode(PIN_FAN_SIGNAL_H, INPUT_PULLDOWN);

  /*
   * Set the initial state of the light and fan to off. The pull chain for the fan is a 4-position
   * latching switch (Off, Low, Medium and High). If power is removed, the fan will resume at whatever
   * speed it previously was. The pull chain will simply cycle through the available modes starting
   * with whatever the current mode is at the time (Off -> High -> Medium -> Low). In this scenario,
   * we are ignoring the current position of the switch and essentially treating it as a momentary
   * switch (sort of - a true momentary switch is non-latching, but you get the idea).
   */
  digitalWrite(PIN_LIGHT_RELAY, LOW);
  digitalWrite(PIN_FAN_RELAY_L, LOW);
  digitalWrite(PIN_FAN_RELAY_M, LOW);
  digitalWrite(PIN_FAN_RELAY_H, LOW);

  lastSwitchState = 0;
  lastFanSwitchState = 0;
  lastFanL = digitalRead(PIN_FAN_SIGNAL_L);
  lastFanM = digitalRead(PIN_FAN_SIGNAL_M);
  lastFanH = digitalRead(PIN_FAN_SIGNAL_H);
  lastDebounceTime = 0;
  remoteLightSwitch = new SpanPoint(RSWITCH_MADDR, 0, sizeof(int), 1, false);

  homeSpan.enableOTA();
  homeSpan.setSketchVersion(FIRMWARE_VERSION);
  homeSpan.setHostNameSuffix("");
  homeSpan.enableAutoStartAP();
  homeSpan.enableWebLog(200, "time.nist.gov", "EST+5EDT,M3.2.0/2,M11.1.0/2", "status");
  homeSpan.begin(Category::Fans, "Living Room Ceiling Fan", "LivingRoomFan");

  new SpanAccessory();
    new DEV_Identify("Living Room Ceiling Fan", "Joe Estock", SERIAL_NUMBER, "Living Room Ceiling Fan", FIRMWARE_VERSION, 0);
    new Service::HAPProtocolInformation();
      new Characteristic::Version(FIRMWARE_VERSION);
    ceilingFan = new DEV_CeilingFan(PIN_FAN_RELAY_L, PIN_FAN_RELAY_M, PIN_FAN_RELAY_H);
    ceilingFanLight = new DEV_Light(PIN_LIGHT_RELAY);

  WEBLOG("Info: MCU MAC Address: %s\n", Network.macAddress().c_str());
}

void loop() {
  int currentSwitchState;
  int newSwitchState = (ceilingFanLight->power->getVal() == 1 ? 0 : 1);

  if ((millis() - lastDebounceTime > 50)) {
    /*
     * Check for manual activation of the light via the switch. If so, we need to notify our service so that
     * the appropriate status is shown in the HomeKit app
     */
    if (!remoteLightSwitch->get(&currentSwitchState))
      currentSwitchState = lastSwitchState;

    if (currentSwitchState != lastSwitchState) {
      WEBLOG("Manual: Received state change for light. Current state: %s, New State: %s", (ceilingFanLight->power->getVal() == 1 ? "On" : "Off"),
          (newSwitchState == ceilingFanLight->ON ? "On" : "Off"));

      ceilingFanLight->power->setVal(newSwitchState);
      digitalWrite(PIN_LIGHT_RELAY, (newSwitchState == ceilingFanLight->ON ? HIGH : LOW));

      lastSwitchState = currentSwitchState;
    }

    /*
     * The pull chain switch is normally a quad state latching switch and the default is to
     * let it govern what speed the fan should be. This is really only used if the power is
     * interrupted - when power is restored, whatever position the switch is in determines
     * what speed the fan should be. Instead, treat it as a momentary switch. In this mode,
     * the default state will always be off and each activation of the pull chain will toggle
     * the fan speed from Off -> High -> Medium -> Low.
     */
    int currentFanSwitchState = lastFanSwitchState;
    int currentFanL = digitalRead(PIN_FAN_SIGNAL_L);
    int currentFanM = digitalRead(PIN_FAN_SIGNAL_M);
    int currentFanH = digitalRead(PIN_FAN_SIGNAL_H);
    if ((currentFanL != lastFanL) || (currentFanM != lastFanM) || (currentFanH != lastFanH)) {
      currentFanSwitchState++;
      if (currentFanSwitchState > ceilingFan->FAN_LOW)
        currentFanSwitchState = ceilingFan->FAN_OFF;

      lastFanL = currentFanL;
      lastFanM = currentFanM;
      lastFanH = currentFanH;
    }

    if (currentFanSwitchState != lastFanSwitchState) {
      WEBLOG("Manual: Set fan speed from %s to %s", ceilingFan->mapFanSpeed(lastFanSwitchState), ceilingFan->mapFanSpeed(currentFanSwitchState));
      ceilingFan->setFanSpeed(currentFanSwitchState);

      lastFanSwitchState = currentFanSwitchState;
    }

    lastDebounceTime = millis();
  }

  homeSpan.poll();
}
