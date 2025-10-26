#include <Arduino.h>
#include "pins.h"
#include "DRV8353.h"
#include "motor.h"
// Pin Definitions
const PinDef Pins::BATT_LEVEL = {"BATT_LEVEL", 16, INPUT};
const PinDef Pins::MOTOR_ENABLE = {"MOTOR_ENABLE", 13, OUTPUT};
const PinDef Pins::MOTOR_INHA = {"MOTOR_INHA", 12, OUTPUT};
const PinDef Pins::MOTOR_INLA = {"MOTOR_INLA", 14, OUTPUT};
const PinDef Pins::MOTOR_INHB = {"MOTOR_INHB", 27, OUTPUT};
const PinDef Pins::MOTOR_INLB = {"MOTOR_INLB", 26, OUTPUT};
const PinDef Pins::MOTOR_INHC = {"MOTOR_INHC", 25, OUTPUT};
const PinDef Pins::MOTOR_INLC = {"MOTOR_INLC", 33, OUTPUT};
const PinDef Pins::MOTOR_SOA = {"MOTOR_SOA", 21, INPUT};
const PinDef Pins::MOTOR_SOB = {"MOTOR_SOB", 22, INPUT};
const PinDef Pins::MOTOR_SOC = {"MOTOR_SOC", 17, INPUT};
const PinDef Pins::MOTOR_FAULT = {"MOTOR_FAULT", 15, INPUT};
const PinDef Pins::MOTOR_HALL_A = {"MOTOR_HALL_A", 9, INPUT};
const PinDef Pins::MOTOR_HALL_B = {"MOTOR_HALL_B", 10, INPUT};
const PinDef Pins::MOTOR_HALL_C = {"MOTOR_HALL_C", 11, INPUT};
const PinDef Pins::SENSOR_THROTTLE_DATA = {"SENSOR_THROTTLE_DATA", 34, INPUT};
const PinDef Pins::SENSOR_PAS_PULSE = {"SENSOR_PAS_PULSE", 35, INPUT};
const PinDef Pins::SENSOR_BRAKE_SIGNAL = {"SENSOR_BRAKE_SIGNAL", 32, INPUT_PULLUP};


void Pins::initPins() {
    // Initialize all pins modes
    pinMode(BATT_LEVEL.pin, BATT_LEVEL.mode);
    pinMode(MOTOR_ENABLE.pin, MOTOR_ENABLE.mode);
    pinMode(MOTOR_INHA.pin, MOTOR_INHA.mode);
    pinMode(MOTOR_INLA.pin, MOTOR_INLA.mode);
    pinMode(MOTOR_INHB.pin, MOTOR_INHB.mode);
    pinMode(MOTOR_INLB.pin, MOTOR_INLB.mode);
    pinMode(MOTOR_INHC.pin, MOTOR_INHC.mode);
    pinMode(MOTOR_INLC.pin, MOTOR_INLC.mode);
    ledcSetup(0, 20000, 16);
    ledcSetup(1, 20000, 16);
    ledcSetup(2, 20000, 16);
    ledcSetup(3, 20000, 16);
    ledcSetup(4, 20000, 16);
    ledcSetup(5, 20000, 16);
    ledcAttachPin(MOTOR_INHA.pin, 0);
    ledcAttachPin(MOTOR_INHB.pin, 1);
    ledcAttachPin(MOTOR_INHC.pin, 2);
    ledcAttachPin(MOTOR_INLA.pin, 3);
    ledcAttachPin(MOTOR_INLB.pin, 4);
    ledcAttachPin(MOTOR_INLC.pin, 5);
    pinMode(MOTOR_SOA.pin, MOTOR_SOA.mode);
    pinMode(MOTOR_SOB.pin, MOTOR_SOB.mode);
    pinMode(MOTOR_SOC.pin, MOTOR_SOC.mode);
    adcAttachPin(BATT_LEVEL.pin);
    adcAttachPin(MOTOR_SOA.pin);
    adcAttachPin(MOTOR_SOB.pin);
    adcAttachPin(MOTOR_SOC.pin);
    pinMode(MOTOR_HALL_A.pin, MOTOR_HALL_A.mode);
    pinMode(MOTOR_HALL_B.pin, MOTOR_HALL_B.mode);
    pinMode(MOTOR_HALL_C.pin, MOTOR_HALL_C.mode);
    attachInterrupt(digitalPinToInterrupt(MOTOR_HALL_A.pin), Motor::onHallChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(MOTOR_HALL_B.pin), Motor::onHallChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(MOTOR_HALL_C.pin), Motor::onHallChange, CHANGE);
    pinMode(MOTOR_FAULT.pin, MOTOR_FAULT.mode);
    attachInterrupt(digitalPinToInterrupt(MOTOR_FAULT.pin), DRV8353::checkFault, CHANGE);
    pinMode(SENSOR_THROTTLE_DATA.pin, SENSOR_THROTTLE_DATA.mode);
    pinMode(SENSOR_PAS_PULSE.pin, SENSOR_PAS_PULSE.mode);
    attachInterrupt(digitalPinToInterrupt(SENSOR_PAS_PULSE.pin), Motor::onPasPulse, RISING);
    pinMode(SENSOR_BRAKE_SIGNAL.pin, SENSOR_BRAKE_SIGNAL.mode);
    attachInterrupt(digitalPinToInterrupt(SENSOR_BRAKE_SIGNAL.pin), Motor::BRAKE, FALLING);
}