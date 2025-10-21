#include <Arduino.h>
#include "DRV8353.h"
#include "motor.h"
#include "UART.h"
#include "config.h"
#include "pins.h"
#include <cmath>
#include "battery.h"

int hallEdgeCount = 0;
const int PULSES_PER_MECH_REV = 138; // Adjust as needed to make accurate
const int SAMPLE_MS = 100;

constexpr uint32_t PAS_ACTIVITY_TIMEOUT_US = 600000; // 0.6 s without pulses = not pedaling
constexpr float PAS_ASSIST_RATIOS[] = {0.0f, 0.25f, 0.4f, 0.6f, 0.8f, 1.0f};
constexpr int PAS_MAX_LEVEL = (sizeof(PAS_ASSIST_RATIOS) / sizeof(PAS_ASSIST_RATIOS[0])) - 1;

float rpm = 0;
float mph = 0;

bool isCruiseControl = false;
float targetMph = 0.0f;

bool isPASMode = false;
int pasLevel = 0;
float pasCadenceRpm = 0.0f;
volatile uint32_t pasPulseCount = 0;
volatile uint32_t lastPasPulseMicros = 0;

UART uart;
Config config;
DRV8353 drv8353;
Battery battery;

float lastBusVoltage = 0.0f;
float lastPhaseCurrent = 0.0f;
float lastElectricalPower = 0.0f;

float readAdcVoltage(const PinDef& pin) {
    if (config.adcResolutionBits <= 0 || config.adcReferenceVoltage <= 0.0f) {
        return 0.0f;
    }

    static bool resolutionConfigured = false;
    if (!resolutionConfigured) {
        analogReadResolution(config.adcResolutionBits);
        resolutionConfigured = true;
    }

    const int raw = analogRead(pin.pin);
    const float maxCount = static_cast<float>((1 << config.adcResolutionBits) - 1);
    if (maxCount <= 0.0f) {
        return 0.0f;
    }

    return (static_cast<float>(raw) / maxCount) * config.adcReferenceVoltage;
}

float readPhaseCurrentAmps(const PinDef& pin) {
    const float shuntOhms = config.shuntResistanceMilliOhm * 0.001f;
    if (shuntOhms <= 0.0f || config.currentSenseGain <= 0.0f) {
        return 0.0f;
    }

    const float voltage = readAdcVoltage(pin);
    const float senseVoltage = voltage - config.currentSenseOffsetVolt;
    const float current = senseVoltage / (config.currentSenseGain * shuntOhms);
    return current;
}

float readAveragePhaseCurrentMagnitude() {
    const float ia = fabsf(readPhaseCurrentAmps(Pins::MOTOR_SOA));
    const float ib = fabsf(readPhaseCurrentAmps(Pins::MOTOR_SOB));
    const float ic = fabsf(readPhaseCurrentAmps(Pins::MOTOR_SOC));
    return (ia + ib + ic) / 3.0f;
}

int applyPowerLimit(int requestedPwm) {
    if (requestedPwm <= 0 || config.maxMotorWattage <= 0) {
        lastBusVoltage = 0.0f;
        lastElectricalPower = 0.0f;
        lastPhaseCurrent = 0.0f;
        uart.sendData("MOTOR_POWER_LIMIT_ACTIVE", "FALSE");
        return requestedPwm;
    }

    lastBusVoltage = battery.getBatteryVoltage();
    lastPhaseCurrent = readAveragePhaseCurrentMagnitude();
    lastElectricalPower = lastBusVoltage * lastPhaseCurrent;

    uart.sendData("MOTOR_BUS_VOLT", String(lastBusVoltage, 2));
    uart.sendData("MOTOR_PHASE_CURRENT", String(lastPhaseCurrent, 2));
    uart.sendData("MOTOR_POWER_W", String(lastElectricalPower, 1));

    if (lastElectricalPower <= static_cast<float>(config.maxMotorWattage)) {
        uart.sendData("MOTOR_POWER_LIMIT_ACTIVE", "FALSE");
        return requestedPwm;
    }

    const float limitRatio = static_cast<float>(config.maxMotorWattage) / lastElectricalPower;
    const float clampedRatio = constrain(limitRatio, 0.0f, 1.0f);
    const int limitedPwm = static_cast<int>(roundf(requestedPwm * clampedRatio));
    uart.sendData("MOTOR_POWER_LIMIT_ACTIVE", "TRUE");
    uart.sendData("MOTOR_POWER_LIMIT_PWM", String(limitedPwm));
    return limitedPwm;
}



float updatePasCadence() {
    static uint32_t lastSampleMicros = 0;
    static uint32_t lastPulseCount = 0;

    uint32_t pulseCountSnapshot;
    noInterrupts();
    pulseCountSnapshot = pasPulseCount;
    interrupts();

    const uint32_t nowMicros = micros();
    if (lastSampleMicros == 0) {
        lastPulseCount = pulseCountSnapshot;
        lastSampleMicros = nowMicros;
        pasCadenceRpm = 0.0f;
        return pasCadenceRpm;
    }

    if (pulseCountSnapshot < lastPulseCount) {
        lastPulseCount = pulseCountSnapshot;
        lastSampleMicros = nowMicros;
        pasCadenceRpm = 0.0f;
        return pasCadenceRpm;
    }

    const uint32_t deltaCount = pulseCountSnapshot - lastPulseCount;
    const uint32_t deltaMicros = nowMicros - lastSampleMicros;

    if (deltaCount == 0 || deltaMicros == 0 || config.pasPulsesPerRev <= 0) {
        return pasCadenceRpm;
    }

    const float pedalRevs = deltaCount / static_cast<float>(config.pasPulsesPerRev);
    pasCadenceRpm = (pedalRevs * 60000000.0f) / static_cast<float>(deltaMicros);

    lastPulseCount = pulseCountSnapshot;
    lastSampleMicros = nowMicros;
    return pasCadenceRpm;
}

void Motor::onHallChange() {
    hallEdgeCount++;
}

void Motor::onPasPulse() {
    pasPulseCount++;
    lastPasPulseMicros = micros();
}


#pragma region Calculations
// Caluclations
float wheelFactorMphPerRpm() {
    const float wheelDiameterInches = static_cast<float>(config.wheelDiameterInches);
    return (PI * wheelDiameterInches / 63360.0f) * 60.0f;
}
void CalculateSpeed(){
    static uint32_t lastCount = 0;
    static uint32_t lastSample = millis();

    uint32_t now = millis();
    if (now - lastSample < SAMPLE_MS) return;

    noInterrupts();
    uint32_t countSnapshot = hallEdgeCount;
    interrupts();

    uint32_t deltaCount = countSnapshot - lastCount;
    float intervalSec = (now - lastSample) / 1000.0f;
    float mechRevs = deltaCount / static_cast<float>(PULSES_PER_MECH_REV);
    rpm = (mechRevs / intervalSec) * 60.0f;

    uart.sendData("MOTOR_RPM", String(rpm));

    lastCount = countSnapshot;
    lastSample = now;
    RPMtoMPH();
}
int clampPasLevel(int level) {
    return constrain(level, 0, PAS_MAX_LEVEL);
}

bool pedalsAreMoving() {
    noInterrupts();
    const uint32_t lastPulseMicrosSnapshot = lastPasPulseMicros;
    const uint32_t pulseCountSnapshot = pasPulseCount;
    interrupts();

    if (pulseCountSnapshot == 0 || lastPulseMicrosSnapshot == 0) {
        return false;
    }

    const uint32_t nowMicros = micros();
    const uint32_t elapsed = nowMicros - lastPulseMicrosSnapshot;
    return elapsed <= PAS_ACTIVITY_TIMEOUT_US;
}
void RPMtoMPH(){
    const float mphPerRpm = wheelFactorMphPerRpm();
    mph = rpm * mphPerRpm;
}
int CalculateMotorPowerSpeed(float targetMph) {
    const float mphPerRpm = wheelFactorMphPerRpm();
    if (mphPerRpm <= 0.0f || config.maxMotorRPM <= 0.0f) {
        return 0;
    }

    const float targetRPM = targetMph / mphPerRpm;
    const float pwmRatio = constrain(targetRPM / config.maxMotorRPM, 0.0f, 1.0f);
    const int pwmValue = static_cast<int>(roundf(pwmRatio * ((1 << 16) - 1)));

    return pwmValue;
}
int CalculateMotorPowerPAS() {
    const float mphPerRpm = wheelFactorMphPerRpm();
    if (!isPASMode || mphPerRpm <= 0.0f || config.maxMotorRPM <= 0.0f) {
        drv8353.send3PWMMotorSignal(0, 0, 0);
        drv8353.setCoast(true);
        uart.sendData("PAS_PEDAL_ACTIVE", "FALSE");
        uart.sendData("PAS_PWM", "0");
        uart.sendData("PAS_CADENCE_RPM", "0");
        pasCadenceRpm = 0.0f;
        return 0;
    }

    const bool pedaling = pedalsAreMoving();
    uart.sendData("PAS_PEDAL_ACTIVE", pedaling ? "TRUE" : "FALSE");

    if (!pedaling) {
        drv8353.send3PWMMotorSignal(0, 0, 0);
        drv8353.setCoast(true);
        pasCadenceRpm = 0.0f;
        uart.sendData("PAS_PWM", "0");
        uart.sendData("PAS_CADENCE_RPM", "0");
        return 0;
    }

    const float assistRatio = PAS_ASSIST_RATIOS[pasLevel];
    const float targetRPM = config.maxMotorRPM * assistRatio;
    const float targetMph = targetRPM * mphPerRpm;

    const int requestedPwm = CalculateMotorPowerSpeed(targetMph);
    int pwmValue = applyPowerLimit(requestedPwm);
    drv8353.setCoast(false);
    drv8353.send3PWMMotorSignal(pwmValue, pwmValue, pwmValue);

    const float cadence = updatePasCadence();
    uart.sendData("PAS_LEVEL", String(pasLevel));
    uart.sendData("PAS_ASSIST_RATIO", String(assistRatio, 2));
    uart.sendData("PAS_CADENCE_RPM", String(cadence, 1));
    uart.sendData("PAS_TARGET_RPM", String(targetRPM, 2));
    uart.sendData("PAS_PWM_REQUEST", String(requestedPwm));
    uart.sendData("PAS_PWM", String(pwmValue));

    return pwmValue;
}
#pragma endregion

void Motor::Brake() {
    drv8353.send3PWMMotorSignal(0, 0, 0);
    drv8353.setCoast(true);
}

void Motor::updateCruiseControl() {
    if (isCruiseControl) {
        drv8353.setCoast(false);
        int requestedPwm = CalculateMotorPowerSpeed(targetMph);
        int pwmValue = applyPowerLimit(requestedPwm);
        drv8353.send3PWMMotorSignal(pwmValue, pwmValue, pwmValue);
        uart.sendData("CRUISE_CONTROL_TARGET", String(targetMph));
        uart.sendData("CRUISE_PWM_REQUEST", String(requestedPwm));
        uart.sendData("CRUISE_PWM", String(pwmValue));
    }
}

void Motor::setPASMode(int mode) {
    pasLevel = clampPasLevel(mode);
    isPASMode = pasLevel > 0;

    uart.sendData("PAS_MODE_ENABLED", isPASMode ? "TRUE" : "FALSE");
    uart.sendData("PAS_LEVEL", String(pasLevel));
    uart.sendData("PAS_ASSIST_RATIO", String(PAS_ASSIST_RATIOS[pasLevel], 2));

    if (!isPASMode) {
        noInterrupts();
        pasPulseCount = 0;
        lastPasPulseMicros = 0;
        interrupts();
        pasCadenceRpm = 0.0f;
        drv8353.send3PWMMotorSignal(0, 0, 0);
        drv8353.setCoast(true);
        uart.sendData("PAS_PEDAL_ACTIVE", "FALSE");
        uart.sendData("PAS_PWM", "0");
        uart.sendData("PAS_CADENCE_RPM", "0");
    }
}

void Motor::updatePASControl() {
    if (isPASMode) {
        CalculateMotorPowerPAS();
    }
}
