#include <Arduino.h>
#include <cmath>
#include "motor.h"
#include "globals.h"

int hallEdgeCount = 0;
const int PULSES_PER_MECH_REV = 138; // Adjust as needed to make accurate
const int SAMPLE_MS = 100;

constexpr uint32_t PAS_ACTIVITY_TIMEOUT_US = 600000; // 0.6 s without pulses = not pedaling
constexpr float PAS_ASSIST_RATIOS[] = {0.0f, 0.25f, 0.4f, 0.6f, 0.8f, 1.0f};
constexpr int PAS_MAX_LEVEL = (sizeof(PAS_ASSIST_RATIOS) / sizeof(PAS_ASSIST_RATIOS[0])) - 1;

volatile uint32_t pasPulseCount = 0;
volatile uint32_t lastPasPulseMicros = 0;

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

static int applyPowerLimit(Motor& m, int requestedPwm) {
    if (requestedPwm <= 0 || config.maxMotorWattage <= 0) {
        m.lastBusVoltage = 0.0f;
        m.lastElectricalPower = 0.0f;
        m.lastPhaseCurrent = 0.0f;
        uart.sendData("MOTOR_POWER_LIMIT_ACTIVE", "FALSE");
        return requestedPwm;
    }

    m.lastBusVoltage = battery.getBatteryVoltage();
    m.lastPhaseCurrent = readAveragePhaseCurrentMagnitude();
    m.lastElectricalPower = m.lastBusVoltage * m.lastPhaseCurrent;

    uart.sendData("MOTOR_BUS_VOLT", String(m.lastBusVoltage, 2));
    uart.sendData("MOTOR_PHASE_CURRENT", String(m.lastPhaseCurrent, 2));
    uart.sendData("MOTOR_POWER_W", String(m.lastElectricalPower, 1));

    if (m.lastElectricalPower <= static_cast<float>(config.maxMotorWattage)) {
        uart.sendData("MOTOR_POWER_LIMIT_ACTIVE", "FALSE");
        return requestedPwm;
    }

    const float limitRatio = static_cast<float>(config.maxMotorWattage) / m.lastElectricalPower;
    const float clampedRatio = constrain(limitRatio, 0.0f, 1.0f);
    const int limitedPwm = static_cast<int>(roundf(requestedPwm * clampedRatio));
    uart.sendData("MOTOR_POWER_LIMIT_ACTIVE", "TRUE");
    uart.sendData("MOTOR_POWER_LIMIT_PWM", String(limitedPwm));
    return limitedPwm;
}



static float updatePasCadence(Motor& m) {
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
        m.pasCadenceRpm = 0.0f;
        return m.pasCadenceRpm;
    }

    if (pulseCountSnapshot < lastPulseCount) {
        lastPulseCount = pulseCountSnapshot;
        lastSampleMicros = nowMicros;
        m.pasCadenceRpm = 0.0f;
        return m.pasCadenceRpm;
    }

    const uint32_t deltaCount = pulseCountSnapshot - lastPulseCount;
    const uint32_t deltaMicros = nowMicros - lastSampleMicros;

    if (deltaCount == 0 || deltaMicros == 0 || config.pasPulsesPerRev <= 0) {
        return m.pasCadenceRpm;
    }

    const float pedalRevs = deltaCount / static_cast<float>(config.pasPulsesPerRev);
    m.pasCadenceRpm = (pedalRevs * 60000000.0f) / static_cast<float>(deltaMicros);

    lastPulseCount = pulseCountSnapshot;
    lastSampleMicros = nowMicros;
    return m.pasCadenceRpm;
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

static float wheelFactorMphPerRpm() {
    const float wheelDiameterInches = static_cast<float>(config.wheelDiameterInches);
    return (PI * wheelDiameterInches / 63360.0f) * 60.0f;
}
void Motor::CalculateSpeed(){
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
    mph = rpm * wheelFactorMphPerRpm();
    uart.sendData("MOTOR_SPEED_MPH", String(mph, 2));
}
static int clampPasLevel(int level) {
    return constrain(level, 0, PAS_MAX_LEVEL);
}

static bool pedalsAreMoving() {
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

static int CalculateMotorPowerSpeed(float targetMph) {
    const float mphPerRpm = wheelFactorMphPerRpm();
    if (mphPerRpm <= 0.0f || config.maxMotorRPM <= 0.0f) {
        return 0;
    }

    const float targetRPM = targetMph / mphPerRpm;
    const float pwmRatio = constrain(targetRPM / config.maxMotorRPM, 0.0f, 1.0f);
    const int pwmValue = static_cast<int>(roundf(pwmRatio * ((1 << 16) - 1)));

    return pwmValue;
}
static int CalculateMotorPowerPAS(Motor& m) {
    const float mphPerRpm = wheelFactorMphPerRpm();
    if (!m.isPASMode || mphPerRpm <= 0.0f || config.maxMotorRPM <= 0.0f) {
        drv8353.send3PWMMotorSignal(0, 0, 0);
        drv8353.setCoast(true);
        uart.sendData("PAS_PEDAL_ACTIVE", "FALSE");
        uart.sendData("PAS_PWM", "0");
        uart.sendData("PAS_CADENCE_RPM", "0");
        m.pasCadenceRpm = 0.0f;
        return 0;
    }

    const bool pedaling = pedalsAreMoving();
    uart.sendData("PAS_PEDAL_ACTIVE", pedaling ? "TRUE" : "FALSE");

    if (!pedaling) {
        drv8353.send3PWMMotorSignal(0, 0, 0);
        drv8353.setCoast(true);
        m.pasCadenceRpm = 0.0f;
        uart.sendData("PAS_PWM", "0");
        uart.sendData("PAS_CADENCE_RPM", "0");
        return 0;
    }

    const float assistRatio = PAS_ASSIST_RATIOS[m.pasLevel];
    const float targetRPM = config.maxMotorRPM * assistRatio;
    const float targetMph = targetRPM * mphPerRpm;

    const int requestedPwm = CalculateMotorPowerSpeed(targetMph);
    int pwmValue = applyPowerLimit(m, requestedPwm);
    drv8353.setCoast(false);
    drv8353.send3PWMMotorSignal(pwmValue, pwmValue, pwmValue);

    const float cadence = updatePasCadence(m);
    uart.sendData("PAS_LEVEL", String(m.pasLevel));
    uart.sendData("PAS_ASSIST_RATIO", String(assistRatio, 2));
    uart.sendData("PAS_CADENCE_RPM", String(cadence, 1));
    uart.sendData("PAS_TARGET_RPM", String(targetRPM, 2));
    uart.sendData("PAS_PWM_REQUEST", String(requestedPwm));
    uart.sendData("PAS_PWM", String(pwmValue));

    return pwmValue;
}
#pragma endregion

void Motor::COAST() {
    drv8353.send3PWMMotorSignal(0, 0, 0);
    drv8353.setCoast(true);
}
void Motor::BRAKE() {
    drv8353.send3PWMMotorSignal(0, 0, 0);
    drv8353.setBrake(true);
}
void Motor::updateCruiseControl() {
    if (isCruiseControl) {
        drv8353.setCoast(false);
        int requestedPwm = CalculateMotorPowerSpeed(targetMph);
        int pwmValue = applyPowerLimit(*this, requestedPwm);
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
        CalculateMotorPowerPAS(*this);
    }
}

void Motor::updateThrottleControl() {
    const bool brakeActive = digitalRead(Pins::SENSOR_BRAKE_SIGNAL.pin) == LOW;
    uart.sendData("BRAKE_ACTIVE", brakeActive ? "TRUE" : "FALSE");

    if (brakeActive) {
        isCruiseControl = false;
        throttleFilteredRatio = 0.0f;
        drv8353.send3PWMMotorSignal(0, 0, 0);
        drv8353.setCoast(true);
        uart.sendData("THROTTLE_RATIO", "0");
        uart.sendData("THROTTLE_PWM", "0");
        return;
    }

    const float throttleVoltage = readAdcVoltage(Pins::SENSOR_THROTTLE_DATA);
    uart.sendData("THROTTLE_VOLT", String(throttleVoltage, 2));

    const float vMin = config.throttleMinVoltage;
    const float vMax = config.throttleMaxVoltage;
    float rawRatio = 0.0f;
    if (vMax > vMin) {
        rawRatio = (throttleVoltage - vMin) / (vMax - vMin);
    }
    rawRatio = constrain(rawRatio, 0.0f, 1.0f);

    if (rawRatio < config.throttleDeadband) {
        rawRatio = 0.0f;
    }

    const float alpha = constrain(config.throttleFilterAlpha, 0.0f, 1.0f);
    throttleFilteredRatio += alpha * (rawRatio - throttleFilteredRatio);

    if (throttleFilteredRatio < 0.001f) {
        throttleFilteredRatio = 0.0f;
        uart.sendData("THROTTLE_RATIO", "0");
        uart.sendData("THROTTLE_PWM", "0");
        return;
    }

    uart.sendData("THROTTLE_RATIO", String(throttleFilteredRatio, 3));

    isCruiseControl = false;

    const int requestedPwm = static_cast<int>(roundf(throttleFilteredRatio * ((1 << 16) - 1)));
    int pwmValue = applyPowerLimit(*this, requestedPwm);

    drv8353.setCoast(false);
    drv8353.send3PWMMotorSignal(pwmValue, pwmValue, pwmValue);

    uart.sendData("THROTTLE_PWM_REQUEST", String(requestedPwm));
    uart.sendData("THROTTLE_PWM", String(pwmValue));
}
