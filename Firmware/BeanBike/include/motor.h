#ifndef MOTOR_H
#define MOTOR_H


class Motor {
public:
    float rpm;
    float mph;
    bool isCruiseControl;
    float targetMph;
    bool isPASMode;
    int pasLevel;
    float lastBusVoltage;
    float lastPhaseCurrent;
    float lastElectricalPower;
    float pasCadenceRpm;
    float throttleFilteredRatio;

    static void onHallChange();
    static void onPasPulse();
    void setPASMode(int mode);
    void CalculateSpeed();
    static void COAST();
    static void BRAKE();
    void updateCruiseControl();
    void updatePASControl();
    void updateThrottleControl();
    
    
};

#endif