#include <HomeSpan.h>

struct DEV_Light : Service::LightBulb {
  SpanCharacteristic *power;
  int relay_signal_pin;
  const char *power_map[3] = { "Off", "On" };
  enum HAP_LightPower { OFF, ON };
  
  DEV_Light(int relay_signal_pin) : Service::LightBulb() {
    this->power = new Characteristic::On();
    this->relay_signal_pin = relay_signal_pin;

    pinMode(this->relay_signal_pin, OUTPUT);

    this->power->setVal(OFF);
    digitalWrite(this->relay_signal_pin, LOW);
  }

  boolean update() {
    bool c_val = this->power->getVal();
    bool n_val = this->power->getNewVal();

    // Only do something if the state has actually changed
    if (c_val != n_val) {
      WEBLOG("HomeKit: Received state change for light. Current state: %s, New State: %s", power_map[c_val], power_map[n_val]);
      digitalWrite(this->relay_signal_pin, (n_val == ON ? HIGH : LOW));
    }
    return true;
  }
};
