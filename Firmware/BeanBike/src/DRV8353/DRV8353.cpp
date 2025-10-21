#include <Arduino.h>
#include <SPI.h>
#include "DRV8353.h"
#include "pins.h"
#include "UART.h"
#include "config.h"
Pins pins;
int csPin = 5;
String faultMessage = "";
UART uart;
Config config;

#pragma DRV8353 SPI
void beginTransaction() {
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
}

void endTransaction() {
    SPI.endTransaction();
}

uint16_t makeFrame(bool isRead, uint8_t addr, uint16_t data11)
{
    return (static_cast<uint16_t>(isRead) << 15) |
           ((addr & 0x0F) << 11) |
           (data11 & 0x07FF);
}

uint16_t transferFrame(uint16_t frame)
{
    uint8_t hi = frame >> 8;
    uint8_t lo = frame & 0xFF;

    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
    digitalWrite(csPin, LOW);
    uint8_t respHi = SPI.transfer(hi);
    uint8_t respLo = SPI.transfer(lo);
    digitalWrite(csPin, HIGH);
    SPI.endTransaction();

    return (static_cast<uint16_t>(respHi) << 8) | respLo;
}

uint16_t parseData(uint16_t rxWord)
{
    return rxWord & 0x07FF;  // lower 11 bits carry actual data
}

uint16_t readRegister(uint8_t addr)
{
    uint16_t rx = transferFrame(makeFrame(true, addr, 0x000));
    return parseData(rx);
}

void writeRegister(uint8_t addr, uint16_t data11)
{
    transferFrame(makeFrame(false, addr, data11));
}

#pragma endregion

namespace {
constexpr uint8_t DRIVER_CONTROL_ADDR = 0x02;
constexpr uint16_t DRIVER_CTRL_OCP_ACT   = 1u << 10;
constexpr uint16_t DRIVER_CTRL_DIS_GDUV  = 1u << 9;
constexpr uint16_t DRIVER_CTRL_DIS_GDF   = 1u << 8;
constexpr uint16_t DRIVER_CTRL_OTW_REP   = 1u << 7;
constexpr uint16_t DRIVER_CTRL_PWM_MODE  = 0b11u << 5;
constexpr uint16_t DRIVER_CTRL_1PWM_COM  = 1u << 4;
constexpr uint16_t DRIVER_CTRL_1PWM_DIR  = 1u << 3;
constexpr uint16_t DRIVER_CTRL_COAST     = 1u << 2;
constexpr uint16_t DRIVER_CTRL_BRAKE     = 1u << 1;
constexpr uint16_t DRIVER_CTRL_CLR_FLT   = 1u << 0;

constexpr uint8_t GATE_DRIVE_HS_ADDR = 0x03;
constexpr uint16_t GATE_HS_LOCK_MASK     = 0b111u << 8;
constexpr uint16_t GATE_HS_IDRIVEP_MASK  = 0x0Fu << 4;
constexpr uint16_t GATE_HS_IDRIVEN_MASK  = 0x0Fu;

constexpr uint8_t GATE_DRIVE_LS_ADDR = 0x04;
constexpr uint16_t GATE_LS_CBC_MASK      = 1u << 10;
constexpr uint16_t GATE_LS_TDRIVE_MASK   = 0b11u << 8;
constexpr uint16_t GATE_LS_IDRIVEP_MASK  = 0x0Fu << 4;
constexpr uint16_t GATE_LS_IDRIVEN_MASK  = 0x0Fu;

constexpr uint8_t OCP_CONTROL_ADDR = 0x05;
constexpr uint16_t OCP_TRETRY_MASK   = 1u << 10;
constexpr uint16_t OCP_DEADTIME_MASK = 0b11u << 8;
constexpr uint16_t OCP_MODE_MASK     = 0b11u << 6;
constexpr uint16_t OCP_DEG_MASK      = 0b11u << 4;
constexpr uint16_t OCP_VDS_MASK      = 0x0Fu;

constexpr uint8_t CSA_CONTROL_ADDR = 0x06;
constexpr uint16_t CSA_FET_MASK     = 1u << 10;
constexpr uint16_t CSA_VREF_DIV_MASK= 1u << 9;
constexpr uint16_t CSA_LS_REF_MASK  = 1u << 8;
constexpr uint16_t CSA_GAIN_MASK    = 0b11u << 6;
constexpr uint16_t CSA_DIS_SEN_MASK = 1u << 5;
constexpr uint16_t CSA_CAL_A_MASK   = 1u << 4;
constexpr uint16_t CSA_CAL_B_MASK   = 1u << 3;
constexpr uint16_t CSA_CAL_C_MASK   = 1u << 2;
constexpr uint16_t CSA_SEN_LVL_MASK = 0b11u;

constexpr uint8_t DRIVER_CONFIG_ADDR = 0x07;
constexpr uint16_t DRIVER_CFG_CAL_MODE_MASK = 1u << 0;

uint16_t readRegister11(uint8_t addr) {
    return readRegister(addr);
}

void writeRegister11(uint8_t addr, uint16_t value) {
    writeRegister(addr, value & 0x07FF);
}

void updateRegisterBit(uint8_t addr, uint16_t mask, bool set) {
    uint16_t reg = readRegister11(addr);
    if (set) {
        reg |= mask;
    } else {
        reg &= static_cast<uint16_t>(~mask);
    }
    writeRegister11(addr, reg);
}

void updateRegisterField(uint8_t addr, uint16_t mask, uint16_t value) {
    uint16_t reg = readRegister11(addr);
    reg &= static_cast<uint16_t>(~mask);
    reg |= static_cast<uint16_t>(value & mask);
    writeRegister11(addr, reg);
}

uint16_t readDriverControlRegister() {
    return readRegister11(DRIVER_CONTROL_ADDR);
}

void writeDriverControlRegister(uint16_t value) {
    writeRegister11(DRIVER_CONTROL_ADDR, value);
}

void updateDriverControlBit(uint16_t mask, bool set) {
    updateRegisterBit(DRIVER_CONTROL_ADDR, mask, set);
}

void updateDriverControlField(uint16_t mask, uint16_t value) {
    updateRegisterField(DRIVER_CONTROL_ADDR, mask, value);
}
} // namespace

#pragma DRV8353 Fault 
struct FaultBit {
    uint8_t bit;
    const char* label;
    const char* description;
};

static constexpr FaultBit fault1Map[] = {
    {10, "FAULT", "Fault detected"},
    {9,  "VDS_OCP", "VDS Monitor Overcurrent"},
    {8,  "GDF", "Gate Drive Fault"},
    {7,  "UVLO", "Under-voltage lockout"},
    {6,  "OTSD", "Over-temperature shutdown"},
    {5,  "VDS_HA", "VDS overcurrent high-side A"},
    {4,  "VDS_LA", "VDS overcurrent low-side A"},
    {3,  "VDS_HB", "VDS overcurrent high-side B"},
    {2,  "VDS_LB", "VDS overcurrent low-side B"},
    {1,  "VDS_HC", "VDS overcurrent high-side C"},
    {0,  "VDS_LC", "VDS overcurrent low-side C"},
};
static constexpr FaultBit VGSMap[] = {
    {10, "SA_OC", "Overcurrent On Phase A Amplifier"},
    {9,  "SB_OC", "Overcurrent On Phase B Amplifier"},
    {8,  "SC_OC", "Overcurrent On Phase C Amplifier"},
    {7,  "OTW", "Overtemperature Warning"},
    {6,  "GDUV", "VCP Charge Pump and/or VGLS Under-voltage"},
    {5,  "VGS_HA", "Gate Drive Fault A High-Side MOSFET"},
    {4,  "VGS_LA", "Gate Drive Fault A Low-Side MOSFET"},
    {3,  "VGS_HB", "Gate Drive Fault B High-Side MOSFET"},
    {2,  "VGS_LB", "Gate Drive Fault B Low-Side MOSFET"},
    {1,  "VGS_HC", "Gate Drive Fault C High-Side MOSFET"},
    {0,  "VGS_LC", "Gate Drive Fault C Low-Side MOSFET"},
};

static String collectFaults(uint16_t value, const FaultBit* table, size_t count) {
    String out;
    for (size_t i = 0; i < count; ++i) {
        if (value & (1u << table[i].bit)) {
            out += table[i].label;
            out += "=";
            out += table[i].description;
            out += '\n';
        }
    }
    return out;
}
#pragma endregion

void DRV8353::init() {
    SPI.begin();
    digitalWrite(pins.MOTOR_ENABLE.pin, HIGH);
    uart.sendData("DRV8353_INITIALIZE", "TRUE");
    setAutoCalibrationMode(true);
    setHighSideSourceCurrentCode(0b1011);
    setHighSideSinkCurrentCode(0b0101);
    setLowSideSourceCurrentCode(0b1011);
    setLowSideSinkCurrentCode(0b0101);
    setPwmMode(PWMMode::ThreePWM);
    uint16_t csaControl = readRegister(0x06);
    config.currentSenseGain = 5.0f * (1 << (((csaControl >> 6) & 0x03) > 0 ? ((csaControl >> 6) & 0x03) - 1 : 0));
}
void DRV8353::checkFault() {
    uint16_t regFAULT_STATUS_1 = readRegister(drvRegisters[0].address);
    
    bool faultDetected = (regFAULT_STATUS_1 & (1u << 10)) != 0;

    if (faultDetected) {
        uint16_t regVGS_STATUS_2 = readRegister(drvRegisters[1].address);
        uart.sendData("FAULT_STATUS", "TRUE");
        uart.sendData("FAULT1", collectFaults(regFAULT_STATUS_1, fault1Map, sizeof(fault1Map)/sizeof(fault1Map[0])));
        uart.sendData("FAULT2", collectFaults(regVGS_STATUS_2, VGSMap, sizeof(VGSMap)/sizeof(VGSMap[0])));
    } else {
        uart.sendData("FAULT_STATUS", "FALSE");
    }
}
void DRV8353::send3PWMMotorSignal(uint16_t pwmA, uint16_t pwmB, uint16_t pwmC) {
    analogWrite(pins.MOTOR_INHA.pin, pwmA);
    analogWrite(pins.MOTOR_INHB.pin, pwmB);
    analogWrite(pins.MOTOR_INHC.pin, pwmC);
    uart.sendData("MOTOR_PWM", "A:" + String(pwmA) + " B:" + String(pwmB) + " C:" + String(pwmC));
}
#pragma region DRV8353ControlFunctions
void DRV8353::clearFault() {
    uint16_t driverControl = readDriverControlRegister();
    writeDriverControlRegister(driverControl | DRIVER_CTRL_CLR_FLT);
    uart.sendData("CLEAR_FAULT", "TRUE");
}
void DRV8353::setOcpActionAllBridges(bool enable) {
    updateDriverControlBit(DRIVER_CTRL_OCP_ACT, enable);
    uart.sendData("DRV_OCP_ACT", enable ? "ALL" : "HALF");
}
void DRV8353::enableChargePumpUvFault(bool enable) {
    updateDriverControlBit(DRIVER_CTRL_DIS_GDUV, !enable);
    uart.sendData("DRV_GDUV_FAULT", enable ? "ENABLED" : "DISABLED");
}
void DRV8353::enableGateDriveFault(bool enable) {
    updateDriverControlBit(DRIVER_CTRL_DIS_GDF, !enable);
    uart.sendData("DRV_GDF_FAULT", enable ? "ENABLED" : "DISABLED");
}
void DRV8353::enableOtwReporting(bool enable) {
    updateDriverControlBit(DRIVER_CTRL_OTW_REP, enable);
    uart.sendData("DRV_OTW_REPORT", enable ? "ON" : "OFF");
}
void DRV8353::setPwmMode(PWMMode mode) {
    uint16_t field = static_cast<uint16_t>(mode) << 5;
    updateDriverControlField(DRIVER_CTRL_PWM_MODE, field);
    switch (mode) {
        case PWMMode::SixPWM: uart.sendData("DRV_PWM_MODE", "6PWM"); break;
        case PWMMode::ThreePWM: uart.sendData("DRV_PWM_MODE", "3PWM"); break;
        case PWMMode::OnePWM: uart.sendData("DRV_PWM_MODE", "1PWM"); break;
        case PWMMode::IndependentPWM: uart.sendData("DRV_PWM_MODE", "INDEPENDENT"); break;
    }
}
void DRV8353::setOnePwmComAsync(bool enable) {
    updateDriverControlBit(DRIVER_CTRL_1PWM_COM, enable);
    uart.sendData("DRV_1PWM_COM", enable ? "ASYNC" : "SYNC");
}
void DRV8353::setOnePwmDirHigh(bool enable) {
    updateDriverControlBit(DRIVER_CTRL_1PWM_DIR, enable);
    uart.sendData("DRV_1PWM_DIR", enable ? "HIGH" : "FOLLOW");
}
void DRV8353::setCoast(bool enable) {
    updateDriverControlBit(DRIVER_CTRL_COAST, enable);
    uart.sendData("MOTOR_MODE", enable ? "COAST" : "RUN");
}
void DRV8353::setBrake(bool enable) {
    updateDriverControlBit(DRIVER_CTRL_BRAKE, enable);
    uart.sendData("MOTOR_MODE", enable ? "BRAKE" : "RUN");
}

void DRV8353::lockGateDriveRegisters(bool lock) {
    uint16_t field = static_cast<uint16_t>((lock ? 0b110u : 0b011u) << 8);
    updateRegisterField(GATE_DRIVE_HS_ADDR, GATE_HS_LOCK_MASK, field);
    uart.sendData("DRV_LOCK", lock ? "LOCKED" : "UNLOCKED");
}

void DRV8353::setHighSideSourceCurrentCode(uint8_t code) {
    code &= 0x0F;
    updateRegisterField(GATE_DRIVE_HS_ADDR, GATE_HS_IDRIVEP_MASK, static_cast<uint16_t>(code) << 4);
    uart.sendData("DRV_HS_IDRIVEP", String(code));
}

void DRV8353::setHighSideSinkCurrentCode(uint8_t code) {
    code &= 0x0F;
    updateRegisterField(GATE_DRIVE_HS_ADDR, GATE_HS_IDRIVEN_MASK, static_cast<uint16_t>(code));
    uart.sendData("DRV_HS_IDRIVEN", String(code));
}

void DRV8353::setCbcClearingByPwm(bool enabled) {
    updateRegisterBit(GATE_DRIVE_LS_ADDR, GATE_LS_CBC_MASK, enabled);
    uart.sendData("DRV_LS_CBC", enabled ? "PWM" : "TIMEOUT");
}

void DRV8353::setLowSideTdriveCode(uint8_t code) {
    code &= 0x03;
    updateRegisterField(GATE_DRIVE_LS_ADDR, GATE_LS_TDRIVE_MASK, static_cast<uint16_t>(code) << 8);
    uart.sendData("DRV_LS_TDRIVE", String(code));
}

void DRV8353::setLowSideSourceCurrentCode(uint8_t code) {
    code &= 0x0F;
    updateRegisterField(GATE_DRIVE_LS_ADDR, GATE_LS_IDRIVEP_MASK, static_cast<uint16_t>(code) << 4);
    uart.sendData("DRV_LS_IDRIVEP", String(code));
}

void DRV8353::setLowSideSinkCurrentCode(uint8_t code) {
    code &= 0x0F;
    updateRegisterField(GATE_DRIVE_LS_ADDR, GATE_LS_IDRIVEN_MASK, static_cast<uint16_t>(code));
    uart.sendData("DRV_LS_IDRIVEN", String(code));
}

void DRV8353::setRetryTime(RetryTime retryTime) {
    updateRegisterBit(OCP_CONTROL_ADDR, OCP_TRETRY_MASK, retryTime == RetryTime::Retry50us);
    uart.sendData("DRV_OCP_RETRY", retryTime == RetryTime::Retry50us ? "50US" : "8MS");
}

void DRV8353::setDeadTime(DeadTime deadTime) {
    uint16_t field = static_cast<uint16_t>(deadTime) << 8;
    updateRegisterField(OCP_CONTROL_ADDR, OCP_DEADTIME_MASK, field);
    uart.sendData("DRV_DEADTIME", String(static_cast<uint8_t>(deadTime)));
}

void DRV8353::setOcpMode(OcpMode mode) {
    uint16_t field = static_cast<uint16_t>(mode) << 6;
    updateRegisterField(OCP_CONTROL_ADDR, OCP_MODE_MASK, field);
    uart.sendData("DRV_OCP_MODE", String(static_cast<uint8_t>(mode)));
}

void DRV8353::setOcpDeglitch(OcpDeglitch deglitch) {
    uint16_t field = static_cast<uint16_t>(deglitch) << 4;
    updateRegisterField(OCP_CONTROL_ADDR, OCP_DEG_MASK, field);
    uart.sendData("DRV_OCP_DEG", String(static_cast<uint8_t>(deglitch)));
}

void DRV8353::setVdsLevelCode(uint8_t code) {
    code &= 0x0F;
    updateRegisterField(OCP_CONTROL_ADDR, OCP_VDS_MASK, static_cast<uint16_t>(code));
    uart.sendData("DRV_VDS_LVL", String(code));
}

void DRV8353::selectCsaFetSense(bool useShx) {
    updateRegisterBit(CSA_CONTROL_ADDR, CSA_FET_MASK, useShx);
    uart.sendData("DRV_CSA_FET", useShx ? "SHX" : "SPX");
}

void DRV8353::setSenseReferenceDivideBy2(bool enable) {
    updateRegisterBit(CSA_CONTROL_ADDR, CSA_VREF_DIV_MASK, enable);
    uart.sendData("DRV_CSA_VREF", enable ? "DIV2" : "FULL");
}

void DRV8353::setLowSideReferenceToShx(bool enable) {
    updateRegisterBit(CSA_CONTROL_ADDR, CSA_LS_REF_MASK, enable);
    uart.sendData("DRV_CSA_LSREF", enable ? "SHX" : "SPX");
}

void DRV8353::setCsaGain(CsaGain gain) {
    uint16_t field = static_cast<uint16_t>(gain) << 6;
    updateRegisterField(CSA_CONTROL_ADDR, CSA_GAIN_MASK, field);
    uart.sendData("DRV_CSA_GAIN", String(static_cast<uint8_t>(gain)));
}

void DRV8353::enableSenseOcp(bool enable) {
    updateRegisterBit(CSA_CONTROL_ADDR, CSA_DIS_SEN_MASK, !enable);
    uart.sendData("DRV_CSA_OCP", enable ? "ENABLED" : "DISABLED");
}

void DRV8353::calibrateSenseAmpA(bool enable) {
    updateRegisterBit(CSA_CONTROL_ADDR, CSA_CAL_A_MASK, enable);
    uart.sendData("DRV_CSA_CAL_A", enable ? "ON" : "OFF");
}

void DRV8353::calibrateSenseAmpB(bool enable) {
    updateRegisterBit(CSA_CONTROL_ADDR, CSA_CAL_B_MASK, enable);
    uart.sendData("DRV_CSA_CAL_B", enable ? "ON" : "OFF");
}

void DRV8353::calibrateSenseAmpC(bool enable) {
    updateRegisterBit(CSA_CONTROL_ADDR, CSA_CAL_C_MASK, enable);
    uart.sendData("DRV_CSA_CAL_C", enable ? "ON" : "OFF");
}

void DRV8353::setSenseOcpThreshold(SenseOcpLevel level) {
    updateRegisterField(CSA_CONTROL_ADDR, CSA_SEN_LVL_MASK, static_cast<uint16_t>(level));
    uart.sendData("DRV_CSA_SENLVL", String(static_cast<uint8_t>(level)));
}

void DRV8353::setAutoCalibrationMode(bool enable) {
    updateRegisterBit(DRIVER_CONFIG_ADDR, DRIVER_CFG_CAL_MODE_MASK, enable);
    uart.sendData("DRV_CAL_MODE", enable ? "AUTO" : "MANUAL");
}
#pragma endregion
