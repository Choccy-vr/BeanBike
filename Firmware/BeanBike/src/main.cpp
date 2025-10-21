#include <Arduino.h>
#include "pins.h"
#include "motor.h"
#include "UART.h"
#include "DRV8353.h"
#include "battery.h"
Pins pins;
Motor motor;
UART uart;
DRV8353 drv8353;
Battery battery;
void uartReceiveCommandTask(void *pvParameters) {
  while (true) {
    uart.receiveCommand();  // Poll for commands
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  pins.initPins();
  uart.init();
  drv8353.init();
  battery.updateBatteryStatus();
  xTaskCreatePinnedToCore(
    uartReceiveCommandTask, 
    "UARTReceive",    
    2048,         
    NULL,           
    1,        
    NULL,            
    0   
  );
}



void loop() {
  motor.CalculateSpeed();
  motor.updateCruiseControl();
  motor.updatePASControl();
  motor.updateThrottleControl();
  battery.updateBatteryStatus();
}