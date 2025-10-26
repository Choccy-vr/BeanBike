#ifndef GLOBALS_H
#define GLOBALS_H

#include "pins.h"
#include "motor.h"
#include "UART.h"
#include "DRV8353.h"
#include "battery.h"
#include "config.h"

extern Pins pins;
extern Motor motor;
extern UART uart;
extern DRV8353 drv8353;
extern Battery battery;
extern Config config;

#endif
