#ifndef BATTERY_H
#define BATTERY_H


class Battery {
public:
    float voltage;
    float level;

    float getBatteryVoltage();
    float getBatteryLevel();
    void updateBatteryStatus();
};

#endif
