#include <Arduino.h>
#include "UART.h"

void UART::init() {
    Serial.begin(115200);
    Serial.println("UART initialized!");
}
void UART::sendMessage(String message) {
    Serial.println(message);
}
