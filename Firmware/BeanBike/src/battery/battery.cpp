#include <Arduino.h>
#include "battery.h"
#include "globals.h"

float Battery::getBatteryVoltage() {
    const float rawVoltage = analogRead(Pins::BATT_LEVEL.pin);

    const float batteryVoltage = (rawVoltage / 4095.0f) * config.adcReferenceVoltage * config.batteryVoltageDividerRatio;
    voltage = batteryVoltage;
    return batteryVoltage;
}

float Battery::getBatteryLevel() {
    const float minVoltage = 39.0f;
    const float maxVoltage = 54.0f;

    const float batteryVoltage = getBatteryVoltage();
    float batteryLevel = (batteryVoltage - minVoltage) / (maxVoltage - minVoltage);
    batteryLevel = constrain(batteryLevel, 0.0f, 1.0f);
    const float finalLevel = batteryLevel * 100.0f;   // Return as percentage
    level = finalLevel;
    return finalLevel; 
}
    
void Battery::updateBatteryStatus() {
    voltage = getBatteryVoltage();
    level = getBatteryLevel();
}
