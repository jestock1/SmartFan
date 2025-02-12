# SmartFan
This project contains the steps I took to convert my "dumb" ceiling fan into a "smart" ceiling fan that can be controlled via Apple HomeKit as well as via the physical pull chain.

# DISCLAIMER
This project involves disassembly of the ceiling fan and working with AC voltage that can be LETHAL. If you are not comfortable working with household current, or do not have experience working with household current, it is strongly advised that you do not attempt this project. No project in the world is worth risking your life.

While I have did my best to ensure there are no errors or omissions, I'm not infallible. Please exercise common sense and caution; Double-check your work before connecting your fan and if something does not make sense, research first to ensure that you are 100% correct.

## Parts Used
[ESP32-C3 SuperMini](https://www.amazon.com/dp/B0CW62PPZ1?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1)

[5V 4 Channel Solid State Relay](https://www.amazon.com/dp/B0B4NWKWK2?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1)

[Mini AC/DC Converter](https://www.amazon.com/dp/B0CLCWPG3R?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1)

18 gauge stranded wire

24 gauge stranded wire

## Steps Taken
Shut off the breaker that controls the fan and ensure that the power is off before proceeding. Remove the ceiling fan blades, then remove the ceiling fan (optional, but much easier to work on). The next steps depend on your particular ceiling fan.

Locate the switch for the fan and remove whatever is needed to gain access to it. Examine the back of the switch and take note of which wire colors go where; There will be a number on the back of the switch above each wire from 1 to 3, except for the black wire which is the line voltage. Cut each of the wires at the switch leaving enough left attached to the switch so that you can attach your 24 gauge wire to. These will be your signal wires that we will use to determine when the pull chain has been pulled.

Remove the line voltage wire (usually black) by pulling it all the way through as we will not need it and we will need the extra room to route the new wiring. Attach your 24 gauge wires to each wire on the switch, except for the line voltage (black) wire, cover the splice with heat shrink tubing or wire nuts, then route them through the top of the ceiling fan. Using your 18 gauge wire, do the same thing for the non-switch side of the wires you cut. These will be the line voltage that powers the Low, Medium and High speeds on the fan. Take note of the wire color for your light then replace the cover you removed to gain access to the pull chain switch as we have nothing more to do inside.

At the top of the fan, connect your three 18 gauge wires, plus the wire for your light, to each output of the relay. Connect the DC +5v of the relay to the +5v of the ESP32-C3 and the DC ground of the relay to the ground on the ESP32-C3. Connect the signal wires from the relay to the pins on the ESP32-C3 that corresponds to the `PIN_LIGHT_RELAY`, `PIN_FAN_RELAY_L`, `PIN_FAN_RELAY_M` and `PIN_FAN_RELAY_H` in the code. Connect your signal wires to the pins on the ESP32-C3 that correspond to `PIN_FAN_SIGNAL_L`, `PIN_FAN_SIGNAL_M` and `PIN_FAN_SIGNAL_H` in the code.

Verify that all connections are correct and that there are no shorts, etc. Compile and flash the firmware onto the first ESP32-C3, then re-install your fan as well as fan blades.

You can optionally use a second ESP32-C3 to monitor the state of a physical switch and use ESPNow to send the state of the switch to your fan. If you want to do the same, then create another sketch with the following code. Replace `XX:XX:XX:XX:XX:XX` with the MAC Address from the ESP32-C3 controlling your fan and add the MAC Address from your second ESP32-C3 to the `RSWITCH_MADDR` in the main firmware code. Connect one wire from the +3.3v pin on the ESP32-C3 to one pole of the light switch then connect another wire from the second pole on the light switch to the pin defined in `SWITCH_PIN` in the code below.

```
#include <HomeSpan.h>

#define FIRMWARE_VERSION  "1.1.5"
#define SERIAL_NUMBER     "20250210"
#define REMOTE_MAC_ADDR   "18:8B:0E:1E:B0:64"
#define SWITCH_PIN        D4

SpanPoint *mainController;
int lastSwitchState = 0;
unsigned long lastDebounceTime;

void setup() {
  Serial.begin(115200);

  pinMode(SWITCH_PIN, INPUT_PULLDOWN);

  lastSwitchState = 0;
  lastDebounceTime = 0;

  mainController = new SpanPoint(REMOTE_MAC_ADDR, sizeof(int), 0);

  homeSpan.enableOTA();
  homeSpan.setSketchVersion(FIRMWARE_VERSION);
  homeSpan.setHostNameSuffix("");
  homeSpan.enableAutoStartAP();
  homeSpan.enableWebLog(200, "time.nist.gov", "EST+5EDT,M3.2.0/2,M11.1.0/2", "status");
  homeSpan.begin(Category::Switches, "Living Room Ceiling Fan Light Switch", "LivingRoomLightSwitch");

  WEBLOG("Info: MCU MAC Address: %s\n", Network.macAddress().c_str());
}

void loop() {
  int currentSwitchState = digitalRead(SWITCH_PIN);
  int ss = 0;

  /*
   * Make sure we have a continuous state change to prevent false-hits
   */
  for (int i = 0; i < 5; i++) {
    ss = digitalRead(SWITCH_PIN);
    if (ss != currentSwitchState) {
      currentSwitchState = lastSwitchState;
      break;
    }
    delay(20);
  }

  if ((currentSwitchState != lastSwitchState) && ((millis() - lastDebounceTime > 200))) {
    bool status;

    status = mainController->send(&currentSwitchState);
    if (!status)
      Serial.printf("Error: Command not received by remote MCU\n");

    lastSwitchState = currentSwitchState;
    lastDebounceTime = millis();
  }

  delay(100);
}
```

## Pairing with HomeKit
This project uses the excellent [HomeSpan Library](https://github.com/HomeSpan/HomeSpan) written by Gregg Berman. Please refer to the [Device Configuration Mode](https://github.com/HomeSpan/HomeSpan/blob/master/docs/UserGuide.md#device-configuration-mode) and the [Pairing to HomeKit](https://github.com/HomeSpan/HomeSpan/blob/master/docs/UserGuide.md#pairing-to-homekit) sections in the [HomeSpan UserGuide.md](https://github.com/HomeSpan/HomeSpan/blob/master/docs/UserGuide.md) to configure and pair your device.
