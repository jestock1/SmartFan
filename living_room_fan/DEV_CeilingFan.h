#include <HomeSpan.h>

struct DEV_CeilingFan : Service::Fan {
  enum HAP_FanActive { FAN_INACTIVE, FAN_ACTIVE };
  enum HAP_FanSpeed { FAN_OFF, FAN_HIGH, FAN_MEDIUM, FAN_LOW };
  const char *speed_map[7] = { "Off", "High", "Medium", "Low" };
  SpanCharacteristic *active;
  SpanCharacteristic *speed;
  int relay_pin_l;    // Relay for low speed
  int relay_pin_m;    // Relay for medium speed
  int relay_pin_h;    // Relay for high speed
  int currentFanSpeed;

  /**
   * Constructor for the ceiling fan controller
   *
   * @param   pin_l   Relay pin for low speed
   * @param   pin_m   Relay pin for medium speed
   * @param   pin_h   Relay pin for high speed
   */
  DEV_CeilingFan(int pin_l, int pin_m, int pin_h) : Service::Fan() {
    this->active = new Characteristic::Active();
    this->speed = new Characteristic::RotationSpeed(0);
    this->relay_pin_l = pin_l;
    this->relay_pin_m = pin_m;
    this->relay_pin_h = pin_h;

    this->speed->setRange(0, 100, 1);
    this->currentFanSpeed = FAN_OFF;

    pinMode(this->relay_pin_l, OUTPUT);
    pinMode(this->relay_pin_m, OUTPUT);
    pinMode(this->relay_pin_h, OUTPUT);

    this->setFanSpeed(FAN_OFF);
    this->speed->setVal(0);
    this->active->setVal(FAN_INACTIVE);
  }

  /**
   * Change the current speed of the fan, or completely shut it off
   *
   * All relays should first be turned off, then wait a brief amount of time to ensure they are off.
   * This is necessary before turning a relay on; If not, we run the (albeit very unlikely, but not impossible)
   * risk of overloading the motor windings, or some other undesired outcome
   *
   * @param   nfs   New fan speed
   */
  void setFanSpeed(int nfs) {
    digitalWrite(this->relay_pin_l, LOW);
    digitalWrite(this->relay_pin_m, LOW);
    digitalWrite(this->relay_pin_h, LOW);
    delay(100);

    switch (nfs) {
      case FAN_OFF:
        if (this->active->getVal() != FAN_INACTIVE)
          this->active->setVal(FAN_INACTIVE);
        break;
      case FAN_LOW:
        digitalWrite(this->relay_pin_l, HIGH);
        if (this->active->getVal() != FAN_ACTIVE)
          this->active->setVal(FAN_ACTIVE);
        break;
      case FAN_MEDIUM:
        digitalWrite(this->relay_pin_m, HIGH);
        if (this->active->getVal() != FAN_ACTIVE)
          this->active->setVal(FAN_ACTIVE);
        break;
      case FAN_HIGH:
        digitalWrite(this->relay_pin_h, HIGH);
        if (this->active->getVal() != FAN_ACTIVE)
          this->active->setVal(FAN_ACTIVE);
        break;
    }

    this->speed->setVal(this->fanStateToPercent(nfs));
    this->currentFanSpeed = nfs;
  }

  /**
   * Convert the fan percentage to the fan state
   *
   * @param   percent   Fan percentage (0 - 100)
   * @return  Fan state (Off, Low, Medium, High)
   */
  int fanPercentToState(int percent) {
    int state = FAN_OFF;

    if ((percent > 0) && (percent <= 33)) {
      state = FAN_LOW;
    }
    else if ((percent > 33) && (percent <= 66)) {
      state = FAN_MEDIUM;
    }
    else if (percent > 66) {
      state = FAN_HIGH;
    }

    return state;
  }

  /**
   * Convert the fan state to the fan percentage
   *
   * @param   state   Fan state (Off, Low, Medium, High)
   * @return  Fan percentage (0 - 100)
   */
  int fanStateToPercent(int state) {
    int speed = 0;

    switch (state) {
      case FAN_LOW:
        speed = 30;
        break;
      case FAN_MEDIUM:
        speed = 60;
        break;
      case FAN_HIGH:
        speed = 100;
        break;
      case FAN_OFF: default:
        speed = 0;
        break;
    }

    return speed;
  }
  
  boolean update() {
    bool c_val = this->active->getVal();
    bool n_val = this->active->getNewVal();
    int cs_val = this->speed->getVal();
    int ns_val = this->speed->getNewVal();
    int cfs = this->fanPercentToState(this->speed->getVal());
    int nfs = this->fanPercentToState(this->speed->getNewVal());

    // This should never happen, but just in case...
    if ((cfs < 0) || (cfs > 3) || (nfs < 0) || (nfs > 3))
      return false;

    if ((c_val != n_val) || (cs_val != ns_val)) {
      // This is needed because when using Siri to "turn the fan off", HomeKit leaves the speed at whatever it was prior
      if ((c_val == FAN_ACTIVE) && (n_val == FAN_INACTIVE))
        nfs = 0;

      this->setFanSpeed(nfs);

      WEBLOG("HomeKit: Received state change for fan. Current state: %s, Speed: %s, New state: %s, Speed: %s",
          (c_val == 1 ? "On" : "Off"),
          speed_map[cfs],
          (n_val == 1 ? "On" : "Off"),
          speed_map[nfs]);
    }

    return true;
  }

  /**
   * Returns the current fan speed as a human-readable string
   *
   * @return  String representing the current speed of the fan (Off, Low, Medium or High)
   */
  const char *getCurrentFanSpeed() {
    int cfs = this->fanPercentToState(this->speed->getVal());

    if ((cfs < 0) || (cfs > 3))
        return "Unknown";

    return speed_map[cfs];
  }

  /**
   * Translates the numerical fan speed to a human-readable string
   *
   * @param   speed   Numerical fan speed
   * @return  String representing the requested speed (Off, Low, Medium or High)
   */
  const char *mapFanSpeed(int speed) {
    if ((speed < 0) || (speed > 3))
      speed = FAN_OFF;

    return speed_map[speed];
  }
};
