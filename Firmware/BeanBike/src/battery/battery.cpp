#include "battery.h"
#include "pins.h"
#include "config.h"
#include <Arduino.h>
Pins pins;
Config config;

float voltage = 0.0f;
float level = 0.0f;

float Battery::getBatteryVoltage() {
    const float rawVoltage = analogRead(pins.BATT_LEVEL.pin);

    float batteryVoltage = (rawVoltage / 4095.0f) * config.adcReferenceVoltage * config.batteryVoltageDividerRatio;
    voltage = batteryVoltage;
    return batteryVoltage;
}

float Battery::getBatteryLevel() {
    const float minVoltage = 39.0f;
    const float maxVoltage = 54.0f;

    float batteryVoltage = getBatteryVoltage();
    float batteryLevel = (batteryVoltage - minVoltage) / (maxVoltage - minVoltage);
    batteryLevel = constrain(batteryLevel, 0.0f, 1.0f);
    float finalLevel = batteryLevel * 100.0f;   // Return as percentage
    level = finalLevel;
    return finalLevel; 
}
    
void Battery::updateBatteryStatus() {
    voltage = getBatteryVoltage();
    level = getBatteryLevel();
}
