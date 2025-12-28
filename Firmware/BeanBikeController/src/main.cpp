#include <Arduino.h>
#include <SPI.h>

// DRV8353S SPI + control pins
const int DRV_CS_PIN     = PA4;   // SPI CS
const int DRV_ENABLE_PIN = PC9;   // nSLEEP / ENABLE
const int DRV_FAULT_PIN  = PC8;   // nFAULT (active low)

// Motor driver pins (6-PWM)
const int PWM_PIN_AH = PA8;       // Phase A High
const int PWM_PIN_AL = PB13;      // Phase A Low
const int PWM_PIN_BH = PA9;       // Phase B High
const int PWM_PIN_BL = PB14;      // Phase B Low
const int PWM_PIN_CH = PA10;      // Phase C High
const int PWM_PIN_CL = PB15;      // Phase C Low

// Current sense inputs (not used here, keep as inputs)
const int SOA_PIN = PA0;
const int SOB_PIN = PA1;
const int SOC_PIN = PA2;

// Status LED
const int LED_PIN = PB5;

// SPI pins (STM32 default SPI1)
const int SPI_MOSI = PA7;  // SPI1_MOSI
const int SPI_MISO = PA6;  // SPI1_MISO
const int SPI_SCK  = PA5;  // SPI1_SCK

// DRV8353S register addresses
#define DRV8353_FAULT_STATUS_1 0x00
#define DRV8353_FAULT_STATUS_2 0x01
#define DRV8353_DRIVER_CONTROL 0x02
#define DRV8353_GATE_DRIVE_HS  0x03
#define DRV8353_GATE_DRIVE_LS  0x04
#define DRV8353_OCP_CONTROL    0x05
#define DRV8353_CSA_CONTROL    0x06

// SPI settings for DRV8353
// DRV8353 datasheet: Mode 1 (CPOL=0, CPHA=1), max 10 MHz
// Start VERY slow for debugging - 100kHz
SPISettings drvSPISettings(100000, MSBFIRST, SPI_MODE1);

// ========== DRV8353 SPI Functions ==========

/**
 * Manual bit-bang SPI test to verify hardware wiring
 * This bypasses the SPI hardware to test physical connections
 */
void manualSPITest() {
  Serial.println("\n========== Manual Bit-Bang SPI Test ==========");
  Serial.println("Testing hardware by manually toggling pins...");
  
  // Set pins to manual control
  pinMode(SPI_SCK, OUTPUT);
  pinMode(SPI_MOSI, OUTPUT);
  pinMode(SPI_MISO, INPUT);
  pinMode(DRV_CS_PIN, OUTPUT);
  
  digitalWrite(DRV_CS_PIN, HIGH);
  digitalWrite(SPI_SCK, LOW);  // CPOL=0
  digitalWrite(SPI_MOSI, LOW);
  delay(10);
  
  Serial.print("  Initial MISO state: ");
  Serial.println(digitalRead(SPI_MISO) ? "HIGH" : "LOW");
  
  // Send a byte manually (0xAA = 10101010)
  digitalWrite(DRV_CS_PIN, LOW);
  delayMicroseconds(10);
  
  uint8_t testByte = 0xAA;
  uint8_t received = 0;
  
  Serial.println("  Sending 0xAA (10101010) bit by bit:");
  
  for (int i = 7; i >= 0; i--) {
    // Set MOSI
    bool bitOut = (testByte >> i) & 0x01;
    digitalWrite(SPI_MOSI, bitOut);
    delayMicroseconds(10);
    
    // Rising edge (Mode 1: CPHA=1, data captured on rising edge)
    digitalWrite(SPI_SCK, HIGH);
    delayMicroseconds(10);
    
    // Read MISO
    bool bitIn = digitalRead(SPI_MISO);
    received = (received << 1) | bitIn;
    
    Serial.print("    Bit ");
    Serial.print(7 - i);
    Serial.print(": MOSI=");
    Serial.print(bitOut);
    Serial.print(", MISO=");
    Serial.println(bitIn);
    
    // Falling edge
    digitalWrite(SPI_SCK, LOW);
    delayMicroseconds(10);
  }
  
  digitalWrite(DRV_CS_PIN, HIGH);
  
  Serial.print("  Sent: 0x");
  Serial.print(testByte, HEX);
  Serial.print(", Received: 0x");
  Serial.println(received, HEX);
  
  if (received == 0xFF) {
    Serial.println("  ⚠️  MISO is stuck HIGH - not responding to clock!");
    Serial.println("  This confirms hardware issue:");
    Serial.println("    - DRV8353 SDO pin not connected to PA6, OR");
    Serial.println("    - DRV8353 PVDD not powered, OR");
    Serial.println("    - Wrong DRV8353 variant (M series has no SPI)");
  } else if (received == 0x00) {
    Serial.println("  ⚠️  MISO is stuck LOW - short to ground?");
  } else {
    Serial.println("  ✓ MISO is responding! SPI hardware should work.");
  }
  
  Serial.println("==============================================\n");
  
  // Re-initialize SPI hardware
  SPI.begin();
}

/**
 * Test different SPI modes to find which works
 */
void testSPIModes() {
  Serial.println("\n========== Testing SPI Modes ==========");
  
  const uint8_t modes[] = {SPI_MODE0, SPI_MODE1, SPI_MODE2, SPI_MODE3};
  const char* modeNames[] = {"MODE0 (CPOL=0,CPHA=0)", "MODE1 (CPOL=0,CPHA=1)", 
                             "MODE2 (CPOL=1,CPHA=0)", "MODE3 (CPOL=1,CPHA=1)"};
  
  for (int i = 0; i < 4; i++) {
    Serial.print("\nTesting SPI ");
    Serial.println(modeNames[i]);
    
    SPISettings testSettings(100000, MSBFIRST, modes[i]);  // 100kHz for testing
    SPI.beginTransaction(testSettings);
    
    // Try to read Fault Status 1 register
    uint16_t command = (1 << 15) | ((0x00 & 0x0F) << 11);  // Read register 0x00
    
    digitalWrite(DRV_CS_PIN, LOW);
    delayMicroseconds(10);
    uint16_t response = SPI.transfer16(command);
    delayMicroseconds(10);
    digitalWrite(DRV_CS_PIN, HIGH);
    
    SPI.endTransaction();
    
    Serial.print("  Command: 0x");
    Serial.print(command, HEX);
    Serial.print(" | Response: 0x");
    Serial.print(response, HEX);
    
    if (response != 0xFFFF && response != 0x0000) {
      Serial.println(" ← VALID RESPONSE!");
    } else {
      Serial.println(" (no response)");
    }
    
    delay(10);
  }
  
  Serial.println("\n=======================================\n");
}

/**
 * Read a 16-bit register from DRV8353
 * Frame format: [R/W(1)][A3-A0(4)][D10-D0(11)]
 * R/W = 1 for read
 * 
 * DRV8353 SPI Timing (from datasheet):
 * - CS setup time: min 50ns
 * - CS hold time: min 50ns  
 * - Clock frequency: max 10MHz
 * - Data valid after falling edge of SCK
 */
uint16_t drv8353_readRegister(uint8_t address) {
  // Construct read command: R/W=1, address in bits [14:11]
  uint16_t command = (1 << 15) | ((address & 0x0F) << 11);
  
  // CS setup time (datasheet: min 50ns, using 2us to be safe)
  digitalWrite(DRV_CS_PIN, LOW);
  delayMicroseconds(2);
  
  // Send command and receive response
  // Try sending as two bytes to check byte order
  uint8_t cmdHigh = (command >> 8) & 0xFF;
  uint8_t cmdLow = command & 0xFF;
  
  uint8_t respHigh = SPI.transfer(cmdHigh);
  uint8_t respLow = SPI.transfer(cmdLow);
  
  uint16_t response = (respHigh << 8) | respLow;
  
  // CS hold time (datasheet: min 50ns, using 2us to be safe)
  delayMicroseconds(2);
  digitalWrite(DRV_CS_PIN, HIGH);
  delayMicroseconds(5);  // Inter-transaction delay
  
  // Extract 11-bit data (bits 10:0)
  uint16_t data = response & 0x07FF;
  
  Serial.print("  [SPI READ] Addr: 0x");
  Serial.print(address, HEX);
  Serial.print(" | Command: 0x");
  Serial.print(command, HEX);
  Serial.print(" | Response: 0x");
  Serial.print(response, HEX);
  Serial.print(" | Data: 0x");
  Serial.println(data, HEX);
  
  return data;
}

/**
 * Write a 16-bit register to DRV8353
 * Frame format: [R/W(1)][A3-A0(4)][D10-D0(11)]
 * R/W = 0 for write
 */
void drv8353_writeRegister(uint8_t address, uint16_t data) {
  // Construct write command: R/W=0, address in bits [14:11], data in bits [10:0]
  uint16_t command = (0 << 15) | ((address & 0x0F) << 11) | (data & 0x07FF);
  
  // CS setup time
  digitalWrite(DRV_CS_PIN, LOW);
  delayMicroseconds(2);
  
  // Send as two bytes
  uint8_t cmdHigh = (command >> 8) & 0xFF;
  uint8_t cmdLow = command & 0xFF;
  
  uint8_t respHigh = SPI.transfer(cmdHigh);
  uint8_t respLow = SPI.transfer(cmdLow);
  
  uint16_t response = (respHigh << 8) | respLow;
  
  // CS hold time
  delayMicroseconds(2);
  digitalWrite(DRV_CS_PIN, HIGH);
  delayMicroseconds(5);  // Inter-transaction delay
  
  Serial.print("  [SPI WRITE] Addr: 0x");
  Serial.print(address, HEX);
  Serial.print(" | Data: 0x");
  Serial.print(data, HEX);
  Serial.print(" | Command: 0x");
  Serial.print(command, HEX);
  Serial.print(" | Response: 0x");
  Serial.println(response, HEX);
}

/**
 * Initialize DRV8353
 */
bool drv8353_init() {
  Serial.println("\n========== DRV8353 Initialization ==========");
  
  // Step 1: Enable the driver (bring out of sleep) - may already be enabled
  bool alreadyEnabled = digitalRead(DRV_ENABLE_PIN);
  if (!alreadyEnabled) {
    Serial.println("Step 1: Enabling DRV8353 (nSLEEP HIGH)...");
    digitalWrite(DRV_ENABLE_PIN, HIGH);
    delay(10);  // Wait for power-up (datasheet: typ 1ms)
  } else {
    Serial.println("Step 1: DRV8353 already enabled, skipping...");
  }
  
  // Step 2: Check fault pin
  bool faultState = digitalRead(DRV_FAULT_PIN);
  Serial.print("Step 2: Fault pin state: ");
  Serial.println(faultState ? "HIGH (No Fault)" : "LOW (FAULT DETECTED!)");
  
  // Step 3: Read fault status registers
  Serial.println("\nStep 3: Reading Fault Status Registers...");
  uint16_t fault1 = drv8353_readRegister(DRV8353_FAULT_STATUS_1);
  uint16_t fault2 = drv8353_readRegister(DRV8353_FAULT_STATUS_2);
  
  Serial.println("\n--- Fault Status 1 Breakdown (0x00) ---");
  Serial.print("  VDS_LC:  "); Serial.println((fault1 >> 0) & 0x01 ? "FAULT" : "OK");
  Serial.print("  VDS_HC:  "); Serial.println((fault1 >> 1) & 0x01 ? "FAULT" : "OK");
  Serial.print("  VDS_LB:  "); Serial.println((fault1 >> 2) & 0x01 ? "FAULT" : "OK");
  Serial.print("  VDS_HB:  "); Serial.println((fault1 >> 3) & 0x01 ? "FAULT" : "OK");
  Serial.print("  VDS_LA:  "); Serial.println((fault1 >> 4) & 0x01 ? "FAULT" : "OK");
  Serial.print("  VDS_HA:  "); Serial.println((fault1 >> 5) & 0x01 ? "FAULT" : "OK");
  Serial.print("  OTSD:    "); Serial.println((fault1 >> 6) & 0x01 ? "FAULT" : "OK");
  Serial.print("  UVLO:    "); Serial.println((fault1 >> 7) & 0x01 ? "FAULT" : "OK");
  Serial.print("  GDF:     "); Serial.println((fault1 >> 8) & 0x01 ? "FAULT" : "OK");
  Serial.print("  VDS_OCP: "); Serial.println((fault1 >> 9) & 0x01 ? "FAULT" : "OK");
  Serial.print("  FAULT:   "); Serial.println((fault1 >> 10) & 0x01 ? "YES" : "NO");
  
  Serial.println("\n--- Fault Status 2 Breakdown (0x01) ---");
  Serial.print("  VGS_LC:  "); Serial.println((fault2 >> 0) & 0x01 ? "FAULT" : "OK");
  Serial.print("  VGS_HC:  "); Serial.println((fault2 >> 1) & 0x01 ? "FAULT" : "OK");
  Serial.print("  VGS_LB:  "); Serial.println((fault2 >> 2) & 0x01 ? "FAULT" : "OK");
  Serial.print("  VGS_HB:  "); Serial.println((fault2 >> 3) & 0x01 ? "FAULT" : "OK");
  Serial.print("  VGS_LA:  "); Serial.println((fault2 >> 4) & 0x01 ? "FAULT" : "OK");
  Serial.print("  VGS_HA:  "); Serial.println((fault2 >> 5) & 0x01 ? "FAULT" : "OK");
  Serial.print("  CPUV:    "); Serial.println((fault2 >> 6) & 0x01 ? "FAULT" : "OK");
  Serial.print("  OTW:     "); Serial.println((fault2 >> 7) & 0x01 ? "WARNING" : "OK");
  Serial.print("  SC_OC:   "); Serial.println((fault2 >> 8) & 0x01 ? "FAULT" : "OK");
  Serial.print("  SB_OC:   "); Serial.println((fault2 >> 9) & 0x01 ? "FAULT" : "OK");
  Serial.print("  SA_OC:   "); Serial.println((fault2 >> 10) & 0x01 ? "FAULT" : "OK");
  
  // Step 4: Configure driver control register
  Serial.println("\nStep 4: Configuring Driver Control Register (0x02)...");
  // Default: 6-PWM mode, no override
  uint16_t driverCtrl = 0x0000;  // Start with defaults
  // Bit 8: CLR_FLT = 0 (don't clear faults yet)
  // Bit 7: BRAKE = 0 (no brake)
  // Bit 6: COAST = 0 (no coast)
  // Bit 5-4: PWM_MODE = 00 (6x PWM mode)
  // Bit 3: 1PWM_COM = 0
  // Bit 2: 1PWM_DIR = 0
  // Bit 1-0: OCP_ACT = 00 (shutdown on OCP)
  drv8353_writeRegister(DRV8353_DRIVER_CONTROL, driverCtrl);
  
  // Verify write
  uint16_t driverCtrlRead = drv8353_readRegister(DRV8353_DRIVER_CONTROL);
  Serial.print("  Driver Control readback: 0x");
  Serial.println(driverCtrlRead, HEX);
  
  // Step 5: Configure gate drive settings
  Serial.println("\nStep 5: Configuring Gate Drive High-Side (0x03)...");
  // IDRIVEP_HS = IDRIVEN_HS = 1000mA (bits [7:4] and [3:0] = 0101 = 5)
  // LOCK bits [10:8] = 110 to unlock
  uint16_t gateHS = (0x06 << 8) | (0x05 << 4) | (0x05 << 0);
  drv8353_writeRegister(DRV8353_GATE_DRIVE_HS, gateHS);
  uint16_t gateHSRead = drv8353_readRegister(DRV8353_GATE_DRIVE_HS);
  Serial.print("  Gate Drive HS readback: 0x");
  Serial.println(gateHSRead, HEX);
  
  Serial.println("\nStep 6: Configuring Gate Drive Low-Side (0x04)...");
  // Similar settings for low side
  uint16_t gateLS = (0x06 << 8) | (0x05 << 4) | (0x05 << 0);
  drv8353_writeRegister(DRV8353_GATE_DRIVE_LS, gateLS);
  uint16_t gateLSRead = drv8353_readRegister(DRV8353_GATE_DRIVE_LS);
  Serial.print("  Gate Drive LS readback: 0x");
  Serial.println(gateLSRead, HEX);
  
  // Step 6: Configure OCP (overcurrent protection)
  Serial.println("\nStep 7: Configuring OCP Control (0x05)...");
  // VDS_LVL = 0.06V (bits [3:0] = 0001), everything else default
  uint16_t ocpCtrl = 0x0001;
  drv8353_writeRegister(DRV8353_OCP_CONTROL, ocpCtrl);
  uint16_t ocpCtrlRead = drv8353_readRegister(DRV8353_OCP_CONTROL);
  Serial.print("  OCP Control readback: 0x");
  Serial.println(ocpCtrlRead, HEX);
  
  // Step 7: Configure CSA (current sense amplifier)
  Serial.println("\nStep 8: Configuring CSA Control (0x06)...");
  // CSA_GAIN = 10V/V (bits [7:6] = 01), everything else default
  uint16_t csaCtrl = (0x01 << 6);
  drv8353_writeRegister(DRV8353_CSA_CONTROL, csaCtrl);
  uint16_t csaCtrlRead = drv8353_readRegister(DRV8353_CSA_CONTROL);
  Serial.print("  CSA Control readback: 0x");
  Serial.println(csaCtrlRead, HEX);
  
  Serial.println("\n========== DRV8353 Initialization Complete ==========\n");
  
  // Check if any faults remain
  if ((fault1 & 0x400) || (fault2 & 0x700)) {
    Serial.println("WARNING: Faults detected during initialization!");
    return false;
  }
  
  return true;
}

/**
 * Read and display all registers
 */
void drv8353_readAllRegisters() {
  Serial.println("\n========== DRV8353 Register Dump ==========");
  for (uint8_t addr = 0x00; addr <= 0x06; addr++) {
    uint16_t value = drv8353_readRegister(addr);
    Serial.print("Register 0x");
    Serial.print(addr, HEX);
    Serial.print(": 0x");
    Serial.print(value, HEX);
    Serial.print(" (");
    Serial.print(value, BIN);
    Serial.println(")");
  }
  Serial.println("===========================================\n");
}

// ========== SETUP ==========
void setup() {
  // Initialize USB Serial
  Serial.begin(115200);
  
  Serial.println("\n\n========================================");
  Serial.println("    STM32G473 + DRV8353 SPI Test");
  Serial.println("========================================\n");
  
  // Configure pins
  Serial.println("Configuring GPIO pins...");
  
  // SPI pins - set explicitly (though SPI.begin() should do this)
  pinMode(SPI_SCK, OUTPUT);
  pinMode(SPI_MOSI, OUTPUT);
  pinMode(SPI_MISO, INPUT);
  
  pinMode(DRV_CS_PIN, OUTPUT);
  pinMode(DRV_ENABLE_PIN, OUTPUT);
  pinMode(DRV_FAULT_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  
  // Initialize with driver disabled
  digitalWrite(DRV_CS_PIN, HIGH);
  digitalWrite(DRV_ENABLE_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  
  // Configure PWM pins as outputs (disabled for now)
  pinMode(PWM_PIN_AH, OUTPUT);
  pinMode(PWM_PIN_AL, OUTPUT);
  pinMode(PWM_PIN_BH, OUTPUT);
  pinMode(PWM_PIN_BL, OUTPUT);
  pinMode(PWM_PIN_CH, OUTPUT);
  pinMode(PWM_PIN_CL, OUTPUT);
  digitalWrite(PWM_PIN_AH, LOW);
  digitalWrite(PWM_PIN_AL, LOW);
  digitalWrite(PWM_PIN_BH, LOW);
  digitalWrite(PWM_PIN_BL, LOW);
  digitalWrite(PWM_PIN_CH, LOW);
  digitalWrite(PWM_PIN_CL, LOW);
  
  Serial.println("  CS Pin:     PA4 (HIGH)");
  Serial.println("  Enable Pin: PC9 (LOW - disabled)");
  Serial.println("  Fault Pin:  PC8 (INPUT)");
  Serial.println("  LED Pin:    PB5 (OUTPUT)");
  Serial.println("  SPI MOSI:   PA7");
  Serial.println("  SPI MISO:   PA6");
  Serial.println("  SPI SCK:    PA5");
  
  // Initialize SPI
  Serial.println("\nInitializing SPI...");
  Serial.println("  Default SPI1 Pins:");
  Serial.println("    MOSI: PA7 (SPI1_MOSI)");
  Serial.println("    MISO: PA6 (SPI1_MISO)");
  Serial.println("    SCK:  PA5 (SPI1_SCK)");
  Serial.println("    CS:   PA4 (Manual)");
  
  SPI.begin();
  Serial.println("  SPI Mode: 1 (CPOL=0, CPHA=1)");
  Serial.println("  Clock: 10 MHz");
  Serial.println("  Bit Order: MSB First");
  
  // Test SPI with loopback
  Serial.println("\n--- SPI Communication Test ---");
  Serial.println("Testing SPI by sending test patterns...");
  
  SPI.beginTransaction(drvSPISettings);
  digitalWrite(DRV_CS_PIN, LOW);
  delayMicroseconds(10);
  uint8_t test1 = SPI.transfer(0xAA);
  uint8_t test2 = SPI.transfer(0x55);
  delayMicroseconds(10);
  digitalWrite(DRV_CS_PIN, HIGH);
  SPI.endTransaction();
  
  Serial.print("  Sent: 0xAA, Received: 0x");
  Serial.println(test1, HEX);
  Serial.print("  Sent: 0x55, Received: 0x");
  Serial.println(test2, HEX);
  
  if (test1 == 0xFF && test2 == 0xFF) {
    Serial.println("  ⚠️  WARNING: MISO stuck HIGH - check wiring or DRV8353 power!");
    Serial.println("\n========== HARDWARE TROUBLESHOOTING ==========");
    Serial.println("MISO is not responding. Please verify:");
    Serial.println("  1. DRV8353 power supplies:");
    Serial.println("     - PVDD (3.3V or 5V logic supply) connected?");
    Serial.println("     - VM/VDRAIN (motor power supply) connected?");
    Serial.println("     - All GND pins connected?");
    Serial.println("  2. SPI connections:");
    Serial.println("     - STM32 PA6 (MISO) → DRV8353 SDO pin");
    Serial.println("     - STM32 PA7 (MOSI) → DRV8353 SDI pin");
    Serial.println("     - STM32 PA5 (SCK)  → DRV8353 SCLK pin");
    Serial.println("     - STM32 PA4 (CS)   → DRV8353 nSCS pin");
    Serial.println("  3. DRV8353 variant:");
    Serial.println("     - DRV8353S/RS/RH: Has SPI interface");
    Serial.println("     - DRV8353M: Hardware control only (no SPI!)");
    Serial.println("  4. Mode selection pins (if present):");
    Serial.println("     - Some DRV8353 variants need SPI_EN or MODE pins configured");
    Serial.println("  5. Enable pin:");
    Serial.println("     - nSLEEP/ENABLE (PC9) should be HIGH during SPI communication");
    Serial.println("================================================\n");
  } else if (test1 == 0x00 && test2 == 0x00) {
    Serial.println("  ⚠️  WARNING: MISO stuck LOW - check wiring!");
  } else {
    Serial.println("  ✓ SPI appears to be communicating (MISO is changing)");
  }
  
  delay(100);
  
  // Manual bit-bang test first
  manualSPITest();
  
  // Test all SPI modes
  testSPIModes();
  
  // Add pin state monitoring
  Serial.println("\n========== Pin State Monitoring ==========");
  Serial.print("  DRV_ENABLE (PC9): ");
  Serial.println(digitalRead(DRV_ENABLE_PIN) ? "HIGH" : "LOW");
  Serial.print("  DRV_FAULT (PC8):  ");
  Serial.println(digitalRead(DRV_FAULT_PIN) ? "HIGH (OK)" : "LOW (FAULT!)");
  Serial.print("  SPI_MISO (PA6):   ");
  Serial.println(digitalRead(SPI_MISO) ? "HIGH" : "LOW");
  Serial.println("==========================================\n");
  
  // Try enabling device before init
  Serial.println("Pre-enabling DRV8353 before SPI test...");
  digitalWrite(DRV_ENABLE_PIN, HIGH);
  delay(100);  // DRV8353 datasheet: typical wake-up time ~1ms, use 100ms to be safe
  Serial.print("  DRV_ENABLE now: ");
  Serial.println(digitalRead(DRV_ENABLE_PIN) ? "HIGH" : "LOW");
  Serial.print("  DRV_FAULT now:  ");
  Serial.println(digitalRead(DRV_FAULT_PIN) ? "HIGH (OK)" : "LOW (FAULT!)");
  Serial.print("  SPI_MISO now:   ");
  Serial.println(digitalRead(SPI_MISO) ? "HIGH" : "LOW");
  Serial.println();
  
  // Initialize DRV8353
  bool success = drv8353_init();
  
  if (success) {
    Serial.println("✓ DRV8353 initialized successfully!");
    digitalWrite(LED_PIN, HIGH);
  } else {
    Serial.println("✗ DRV8353 initialization failed!");
    digitalWrite(LED_PIN, LOW);
  }
  
  Serial.println("\nEntering main loop...\n");
}

// ========== LOOP ==========
void loop() {
  // CRITICAL: Stop immediately on driver fault and enter safe mode
  if (digitalRead(DRV_FAULT_PIN) == LOW) {
    // Turn OFF all motor outputs immediately
    digitalWrite(PWM_PIN_AH, LOW);
    digitalWrite(PWM_PIN_AL, LOW);
    digitalWrite(PWM_PIN_BH, LOW);
    digitalWrite(PWM_PIN_BL, LOW);
    digitalWrite(PWM_PIN_CH, LOW);
    digitalWrite(PWM_PIN_CL, LOW);
    
    Serial.println("\n!!! FAULT DETECTED - ENTERING SAFE MODE !!!");
    Serial.println("All motor outputs disabled.");
    Serial.println("Check DRV8353 fault registers for details.");
    
    // Blink LED twice repeatedly to indicate fault
    while (true) {
      // First blink
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);
      
      // Second blink
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(800); // Longer pause before repeating
      
      Serial.println("FAULT - System halted");
    }
  }

  static uint32_t lastStep = 0;
  static int step = 0;
  // Sweep dwell values between 5–50ms so you can spot a torque sweet spot while it runs
  static const uint8_t dwellTable[] = {5, 10, 15, 20, 25, 30, 35, 40, 45, 50};
  static uint8_t dwellIndex = 0;
  static uint32_t dwellMs = dwellTable[0];
  static uint32_t stepsAtCurrentDwell = 0;
  const uint32_t stepsPerDwell = 240;  // Hold each dwell for ~240 commutation steps (~40 electrical revs)

  if (millis() - lastStep >= dwellMs) {
    lastStep = millis();

    // Turn everything OFF first to avoid shoot-through
    digitalWrite(PWM_PIN_AH, LOW);
    digitalWrite(PWM_PIN_AL, LOW);
    digitalWrite(PWM_PIN_BH, LOW);
    digitalWrite(PWM_PIN_BL, LOW);
    digitalWrite(PWM_PIN_CH, LOW);
    digitalWrite(PWM_PIN_CL, LOW);

    switch (step) {
      case 0: // A+ B-
        digitalWrite(PWM_PIN_AH, HIGH);
        digitalWrite(PWM_PIN_BL, HIGH);
        break;
      case 1: // A+ C-
        digitalWrite(PWM_PIN_AH, HIGH);
        digitalWrite(PWM_PIN_CL, HIGH);
        break;
      case 2: // B+ C-
        digitalWrite(PWM_PIN_BH, HIGH);
        digitalWrite(PWM_PIN_CL, HIGH);
        break;
      case 3: // B+ A-
        digitalWrite(PWM_PIN_BH, HIGH);
        digitalWrite(PWM_PIN_AL, HIGH);
        break;
      case 4: // C+ A-
        digitalWrite(PWM_PIN_CH, HIGH);
        digitalWrite(PWM_PIN_AL, HIGH);
        break;
      case 5: // C+ B-
        digitalWrite(PWM_PIN_CH, HIGH);
        digitalWrite(PWM_PIN_BL, HIGH);
        break;
    }

    step = (step + 5) % 6; // advance commutation - REVERSED direction to test phase order

    // Advance dwell timing after a block of commutation steps
    stepsAtCurrentDwell++;
    if (stepsAtCurrentDwell >= stepsPerDwell) {
      stepsAtCurrentDwell = 0;
      dwellIndex = (dwellIndex + 1) % (sizeof(dwellTable) / sizeof(dwellTable[0]));
      dwellMs = dwellTable[dwellIndex];
      Serial.print("[COMM] Switching dwell to ");
      Serial.print(dwellMs);
      Serial.println(" ms");
    }

    // Blink LED each commutation step
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}
