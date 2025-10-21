#ifndef DRV8353_H
#define DRV8353_H

#include <Arduino.h>

struct RegisterInfo {
    const char* name;
    uint8_t address;
};

class DRV8353 {
public:
    String faultMessage;
    enum class PWMMode : uint8_t {
        SixPWM        = 0b00,
        ThreePWM      = 0b01,
        OnePWM        = 0b10,
        IndependentPWM = 0b11
    };
    enum class RetryTime : uint8_t {
        Retry8ms  = 0b0,
        Retry50us = 0b1
    };
    enum class DeadTime : uint8_t {
        DeadTime50ns  = 0b00,
        DeadTime100ns = 0b01,
        DeadTime200ns = 0b10,
        DeadTime400ns = 0b11
    };
    enum class OcpMode : uint8_t {
        Latched     = 0b00,
        AutoRetry   = 0b01,
        ReportOnly  = 0b10,
        Disabled    = 0b11
    };
    enum class OcpDeglitch : uint8_t {
        Deglitch1us = 0b00,
        Deglitch2us = 0b01,
        Deglitch4us = 0b10,
        Deglitch8us = 0b11
    };
    enum class CsaGain : uint8_t {
        Gain5VPerV  = 0b00,
        Gain10VPerV = 0b01,
        Gain20VPerV = 0b10,
        Gain40VPerV = 0b11
    };
    enum class SenseOcpLevel : uint8_t {
        Level025V = 0b00,
        Level050V = 0b01,
        Level075V = 0b10,
        Level100V = 0b11
    };
    static constexpr RegisterInfo drvRegisters[] = {
    {"FAULT_STATUS_1", 0x00},
    {"VGS_STATUS_2", 0x01},
    {"DRIVER_CONTROL", 0x02},
    {"GATE_DRIVE_HS", 0x03},
    {"GATE_DRIVE_LS", 0x04},
    {"OCP_CONTROL",   0x05},
    {"CSA_CONTROL",   0x06},
    {"DRIVER_CONFIG", 0x07},
    };
    void init();
    static void checkFault();
    void send3PWMMotorSignal(uint16_t pwmA, uint16_t pwmB, uint16_t pwmC);
    #pragma region DRV8353ControlFunctions
    // Control Functions
    void clearFault();
    /** Configure whether an OCP shuts down all bridges (true) or only the affected half-bridge (false). */
    void setOcpActionAllBridges(bool enable);
    /** Enable or disable VCP/VGLS undervoltage fault reporting (DIS_GDUV bit). */
    void enableChargePumpUvFault(bool enable);
    /** Enable or disable gate drive fault reporting (DIS_GDF bit). */
    void enableGateDriveFault(bool enable);
    /** Choose whether OTW is mirrored to nFAULT/FAULT bit (OTW_REP bit). */
    void enableOtwReporting(bool enable);
    /** Select the desired PWM input topology using the PWM_MODE bits. */
    void setPwmMode(PWMMode mode);
    /** Control synchronous/asynchronous rectification in 1x PWM mode (1PWM_COM bit). */
    void setOnePwmComAsync(bool enable);
    /** Override DIR polarity in 1x PWM mode (1PWM_DIR bit). */
    void setOnePwmDirHigh(bool enable);
    /** Place all MOSFETs in Hi-Z when true (COAST bit). */
    void setCoast(bool enable);
    /** Turn on all low-side MOSFETs when true (BRAKE bit). Emergency Stop / Regen Braking */
    void setBrake(bool enable);
    // Gate Drive HS
    /** Write the LOCK field (110 to lock, 011 to unlock). */
    void lockGateDriveRegisters(bool lock);
    /** Program IDRIVEP_HS (source) current code per Table 15 (0-15). */
    void setHighSideSourceCurrentCode(uint8_t code);
    /** Program IDRIVEN_HS (sink) current code per Table 15 (0-15). */
    void setHighSideSinkCurrentCode(uint8_t code);
    // Gate Drive LS
    /** Configure CBC behavior (true clears on next PWM edge). */
    void setCbcClearingByPwm(bool enabled);
    /** Program TDRIVE code (0-3) to select peak gate-current drive time. */
    void setLowSideTdriveCode(uint8_t code);
    /** Program IDRIVEP_LS (source) current code per Table 16 (0-15). */
    void setLowSideSourceCurrentCode(uint8_t code);
    /** Program IDRIVEN_LS (sink) current code per Table 16 (0-15). */
    void setLowSideSinkCurrentCode(uint8_t code);
    // OCP Control
    /** Select retry timing for VDS_OCP and SEN_OCP faults. */
    void setRetryTime(RetryTime retryTime);
    /** Select gate-driver dead time. */
    void setDeadTime(DeadTime deadTime);
    /** Choose the OCP response mode. */
    void setOcpMode(OcpMode mode);
    /** Configure the OCP deglitch time. */
    void setOcpDeglitch(OcpDeglitch deglitch);
    /** Program VDS overcurrent threshold code (0-15). */
    void setVdsLevelCode(uint8_t code);
    // CSA Control (DRV8353 variants)
    /** Choose SHx (true) or SPx (false) as the CSA positive input. */
    void selectCsaFetSense(bool useShx);
    /** Divide VREF by 2 for the CSA reference when true. */
    void setSenseReferenceDivideBy2(bool enable);
    /** Measure LS VDS across SHx-SNx when true; SHx-SPx when false. */
    void setLowSideReferenceToShx(bool enable);
    /** Set the shunt amplifier gain. */
    void setCsaGain(CsaGain gain);
    /** Enable or disable the CSA overcurrent fault (DIS_SEN bit). */
    void enableSenseOcp(bool enable);
    /** Short sense amplifier A inputs for offset calibration. */
    void calibrateSenseAmpA(bool enable);
    /** Short sense amplifier B inputs for offset calibration. */
    void calibrateSenseAmpB(bool enable);
    /** Short sense amplifier C inputs for offset calibration. */
    void calibrateSenseAmpC(bool enable);
    /** Set the CSA sense overcurrent threshold. */
    void setSenseOcpThreshold(SenseOcpLevel level);
    // Driver Configuration
    /** Enable automatic amplifier calibration (CAL_MODE). */
    void setAutoCalibrationMode(bool enable);
    #pragma endregion
};

#endif
