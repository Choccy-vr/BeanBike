#ifndef PINS_H
#define PINS_H

#include <stdint.h>

struct PinDef {
    const char* name;
    int pin;
    uint8_t mode;
};

class Pins {
public:
    // Pins
    static const PinDef BATT_LEVEL;
    static const PinDef MOTOR_ENABLE;
    static const PinDef MOTOR_INHA;
    static const PinDef MOTOR_INLA;
    static const PinDef MOTOR_INHB;  
    static const PinDef MOTOR_INLB;
    static const PinDef MOTOR_INHC;
    static const PinDef MOTOR_INLC;
    static const PinDef MOTOR_SOA;
    static const PinDef MOTOR_SOB;
    static const PinDef MOTOR_SOC;
    static const PinDef MOTOR_HALL_A;
    static const PinDef MOTOR_HALL_B;
    static const PinDef MOTOR_HALL_C;
    static const PinDef MOTOR_FAULT;
    static const PinDef SENSOR_THROTTLE_DATA;
    static const PinDef SENSOR_PAS_PULSE;
    static const PinDef SENSOR_BRAKE_SIGNAL;

    // Functions
    void initPins();
};

#endif