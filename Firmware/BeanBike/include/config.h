#ifndef CONFIG_H
#define CONFIG_H


class Config {
public:
    //Bike
    int wheelDiameterInches = 26;

    //PAS
    int pasPulsesPerRev = 12; // Change when we know the actual number of PAS magnets

    //Motor
    int maxMotorWattage = 1000;
    float maxMotorRPM = 600.0f;

    // ADC configuration
    int adcResolutionBits = 12;
    float adcReferenceVoltage = 3.3f;

    // Shunt amplifier configuration
    float shuntResistanceMilliOhm = 1.0f; // mΩ
    float currentSenseGain = 0.0f;       // CSA_GAIN; Set when initializing DRV8353
    float currentSenseOffsetVolt = 1.65f;

    // Battery
    float batteryVoltageDividerRatio = 19.0f;
};

#endif