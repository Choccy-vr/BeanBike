#ifndef MOTOR_H
#define MOTOR_H


class Motor {
public:
    float rpm;

    static void onHallChange();
    static void onPasPulse();
    void setPASMode(int mode);
    void CalculateSpeed();
    void Brake();
    void updateCruiseControl();
    void updatePASControl();
    
};

#endif