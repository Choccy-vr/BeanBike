#include <Arduino.h>
#include "UART.h"
#include "config.h"
#include "motor.h"

Config config;
Motor motor;

void UART::init() {
    Serial.begin(115200);
    Serial.println("UART initialized!");
}
void UART::sendMessage(String message) {
    Serial.println(message);
}
void UART::sendData(String name, String data) {
    Serial.print(name + ":" + data);

}
void UART::receiveCommand() {
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        ParseCommand(command);
        Serial.println("RECEIVED");
    }
}
static bool tokenize(const String& line, String& cmd, String& item, String& arg) {
    int firstSpace = line.indexOf(' ');
    if (firstSpace < 0) return false;
    cmd = line.substring(0, firstSpace);

    int secondSpace = line.indexOf(' ', firstSpace + 1);
    if (secondSpace < 0) {
        item = line.substring(firstSpace + 1);
        arg = "";
    } else {
        item = line.substring(firstSpace + 1, secondSpace);
        arg = line.substring(secondSpace + 1);
    }
    cmd.trim();
    item.trim();
    arg.trim();
    return true;
}
void ParseCommand(String command) {
    String cmd, item, arg;
    if (!tokenize(command, cmd, item, arg)) {
        Serial.println("ERR BAD FORMAT");
        return;
    }

    if (cmd == "SET") {
        if (item == "CONFIG_WHEEL_DIAMETER_INCHES" && arg.length()) {
            config.wheelDiameterInches = arg.toInt();
            Serial.println("OK SET");
        } 
        else if (item == "CONFIG_PAS_PULSES_PER_REV" && arg.length()) {
            config.pasPulsesPerRev = arg.toInt();
            Serial.println("OK SET");
        }
        else if (item == "CONFIG_MAX_MOTOR_WATTAGE" && arg.length()) {
            config.maxMotorWattage = arg.toInt();
            Serial.println("OK SET");
        } 
        else if (item == "CONFIG_MAX_MOTOR_RPM" && arg.length()) {
            config.maxMotorRPM = arg.toFloat();
            Serial.println("OK SET");
        } 
        else if (item == "CONFIG_ADC_RESOLUTION_BITS" && arg.length()) {
            config.adcResolutionBits = arg.toInt();
            Serial.println("OK SET");
        } 
        else if (item == "CONFIG_ADC_REFERENCE_VOLTAGE" && arg.length()) {
            config.adcReferenceVoltage = arg.toFloat();
            Serial.println("OK SET");
        } 
        else if (item == "CONFIG_CURRENT_SENSE_OFFSET_VOLT" && arg.length()) {
            config.currentSenseOffsetVolt = arg.toFloat();
            Serial.println("OK SET");
        } 
        else if (item == "CONFIG_BATTERY_VOLTAGE_DIVIDER_RATIO" && arg.length()) {
            config.batteryVoltageDividerRatio = arg.toFloat();
            Serial.println("OK SET");
        } 
        else if (item == "CONFIG_THROTTLE_MIN_VOLTAGE" && arg.length()) {
            config.throttleMinVoltage = arg.toFloat();
            Serial.println("OK SET");
        } 
        else if (item == "CONFIG_THROTTLE_MAX_VOLTAGE" && arg.length()) {
            config.throttleMaxVoltage = arg.toFloat();
            Serial.println("OK SET");
        } 
        else if (item == "CONFIG_THROTTLE_DEADBAND" && arg.length()) {
            config.throttleDeadband = arg.toFloat();
            Serial.println("OK SET");
        } 
        else if (item == "CONFIG_THROTTLE_FILTER_ALPHA" && arg.length()) {
            config.throttleFilterAlpha = arg.toFloat();
            Serial.println("OK SET");
        } 
        else if (item == "CONFIG_THROTTLE_FILTER_ALPHA" && arg.length()) {
            config.throttleFilterAlpha = arg.toFloat();
            Serial.println("OK SET");
        }
        else if (item == "MOTOR_IS_CRUISE_CONTROL" && arg.length()) {
            motor.isCruiseControl = (arg == "TRUE");
            Serial.println("OK SET");
        }
        else if (item == "MOTOR_CRUISE_TARGET_MPH" && arg.length()) {
            motor.targetMph = arg.toFloat();
            Serial.println("OK SET");
        }
        else if (item == "MOTOR_IS_PAS" && arg.length()) {
            motor.isPASMode = (arg == "TRUE");
            Serial.println("OK SET");
        }
        else if (item == "MOTOR_PAS_LEVEL" && arg.length()) {
            motor.pasLevel = arg.toInt();
            Serial.println("OK SET");
        }
        else {
            Serial.println("ERR SET");
        }
    } else if (cmd == "READ") {
        if(item == "CONFIG_WHEEL_DIAMETER_INCHES") {
            Serial.println(String("VALUE ") + config.wheelDiameterInches);
        } 
        else if(item == "CONFIG_PAS_PULSES_PER_REV") {
            Serial.println(String("VALUE ") + config.pasPulsesPerRev);
        }
        else if (item == "CONFIG_MAX_MOTOR_WATTAGE") {
            Serial.println(String("VALUE ") + config.maxMotorWattage);
        } 
        else if (item == "CONFIG_MAX_MOTOR_RPM") {
            Serial.println(String("VALUE ") + config.maxMotorRPM);
        } 
        else if (item == "CONFIG_ADC_RESOLUTION_BITS") {
            Serial.println(String("VALUE ") + config.adcResolutionBits);
        } 
        else if (item == "CONFIG_ADC_REFERENCE_VOLTAGE") {
            Serial.println(String("VALUE ") + config.adcReferenceVoltage);
        } 
        else if (item == "CONFIG_SHUNT_RESISTANCE_MOHM") {
            Serial.println(String("VALUE ") + config.shuntResistanceMilliOhm);
        } 
        else if (item == "CONFIG_CURRENT_SENSE_GAIN") {
            Serial.println(String("VALUE ") + config.currentSenseGain);
        }
        else if (item == "CONFIG_CURRENT_SENSE_OFFSET_VOLT") {
            Serial.println(String("VALUE ") + config.currentSenseOffsetVolt);
        } 
        else if (item == "CONFIG_BATTERY_VOLTAGE_DIVIDER_RATIO") {
            Serial.println(String("VALUE ") + config.batteryVoltageDividerRatio);
        } 
        else if (item == "CONFIG_THROTTLE_MIN_VOLTAGE") {
            Serial.println(String("VALUE ") + config.throttleMinVoltage);
        } 
        else if (item == "CONFIG_THROTTLE_MAX_VOLTAGE") {
            Serial.println(String("VALUE ") + config.throttleMaxVoltage);
        } 
        else if (item == "CONFIG_THROTTLE_DEADBAND") {
            Serial.println(String("VALUE ") + config.throttleDeadband);
        } 
        else if (item == "CONFIG_THROTTLE_FILTER_ALPHA") {
            Serial.println(String("VALUE ") + config.throttleFilterAlpha);
        }
        else if (item == "MOTOR_RPM") {
            Serial.println(String("VALUE ") + motor.rpm);
        }
        else if (item == "MOTOR_MPH") {
            Serial.println(String("VALUE ") + motor.mph);
        }
        else if (item == "MOTOR_POWER_WATTS") {
            Serial.println(String("VALUE ") + motor.lastElectricalPower);
        }
        else if (item == "MOTOR_BUS_VOLTAGE") {
            Serial.println(String("VALUE ") + motor.lastBusVoltage);
        }
        else if (item == "MOTOR_PHASE_CURRENT") {
            Serial.println(String("VALUE ") + motor.lastPhaseCurrent);
        }
        else if (item == "MOTOR_IS_CRUISE_CONTROL") {
            Serial.println(String("VALUE ") + (motor.isCruiseControl ? "TRUE" : "FALSE"));
        }
        else if (item == "MOTOR_CRUISE_TARGET_MPH") {
            Serial.println(String("VALUE ") + motor.targetMph);
        }
        else if (item == "MOTOR_IS_PAS") {
            Serial.println(String("VALUE ") + (motor.isPASMode ? "TRUE" : "FALSE"));
        }
        else if (item == "MOTOR_PAS_LEVEL") {
            Serial.println(String("VALUE ") + motor.pasLevel);
        }
        else {
            Serial.println("ERR READ");
        }
    } else if (cmd == "MOTOR") {
        if(item == "COAST") {
            motor.COAST();
        }
        else if(item == "BRAKE") {
            motor.BRAKE();
        }
        else {
            Serial.println("ERR MOTOR");
        }

    } else {
        Serial.println("ERR UNKNOWN CMD");
    }
}