#include <Arduino.h>
#include "globals.h"

Pins pins;
Motor motor;
UART uart;
DRV8353 drv8353;
Battery battery;
Config config;
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