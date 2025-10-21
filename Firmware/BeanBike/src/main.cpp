#include <Arduino.h>
#include "pins.h"
#include "motor.h"
#include "UART.h"
#include "DRV8353.h"
#include "battery.h"
Pins pins;
Motor motor;
UART uart;
DRV8353 drv8353;
Battery battery;
void setup() {
  pins.initPins();
  uart.init();
  drv8353.init();
  battery.updateBatteryStatus();
}



void loop() {
  motor.CalculateSpeed();
  motor.updateCruiseControl();
  motor.updatePASControl();
}
