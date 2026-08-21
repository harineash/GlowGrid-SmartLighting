/*
  PZEM-004T v3.0 (100A) - Final Test Sketch
  ------------------------------------------
  Load under test: 30W incandescent bulb (AC mains)

  Wiring (PZEM-004T 100A version, Modbus RTU, TTL @9600):
    PZEM TX  -> ESP32 GPIO16 (RX2)
    PZEM RX  -> ESP32 GPIO17 (TX2)
    PZEM 5V  -> Separate stable 5V supply (or ESP32 5V/VIN if powered by a proper wall adapter, not laptop USB)
    PZEM GND -> ESP32 GND (common ground required even with separate 5V supply)

  AC side (100A version has an INTERNAL SHUNT, no clamp CT):
    Mains LIVE   -> PZEM "L IN"
    PZEM "L OUT" -> Bulb -> back to Neutral
    Mains NEUTRAL passes straight through the module's N terminals

  !! MAINS AC IS DANGEROUS !!
  - Disconnect power before wiring.
  - Insulate all AC terminals; don't touch the module while powered.
  - If unsure, have someone experienced check your wiring first.

  Library required: "PZEM004Tv30" by Jakub Mandula
  Install via Arduino IDE: Sketch > Include Library > Manage Libraries > search "PZEM004Tv30"
*/

#include <PZEM004Tv30.h>

#define PZEM_RX_PIN 16   // ESP32 RX2 <- PZEM TX
#define PZEM_TX_PIN 17   // ESP32 TX2 -> PZEM RX

PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

// Reads a PZEM value, retrying once if the first read fails (returns NaN)
float readWithRetry(float (*readFunc)()) {
  float value = readFunc();
  if (isnan(value)) {
    delay(200);
    value = readFunc();
  }
  return value;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("PZEM-004T (100A) Test - 30W Incandescent Bulb Load");
  Serial.println("Expect roughly: mains voltage, ~0.13-0.25A, ~30W, PF ~1.0");
  Serial.println("----------------------------------------------------------");
}

void loop() {
  float voltage   = readWithRetry([]() -> float { return pzem.voltage(); });
  float current   = readWithRetry([]() -> float { return pzem.current(); });
  float power     = readWithRetry([]() -> float { return pzem.power(); });
  float energy    = readWithRetry([]() -> float { return pzem.energy(); });
  float frequency = readWithRetry([]() -> float { return pzem.frequency(); });
  float pf        = readWithRetry([]() -> float { return pzem.pf(); });

  if (isnan(voltage)) {
    Serial.println("Error reading PZEM data - check mains connection / wiring / power");
  } else {
    Serial.print("Voltage:   "); Serial.print(voltage);    Serial.println(" V");
    Serial.print("Current:   "); Serial.print(current, 3); Serial.println(" A");
    Serial.print("Power:     "); Serial.print(power);      Serial.println(" W");
    Serial.print("Energy:    "); Serial.print(energy, 3);  Serial.println(" kWh");
    Serial.print("Frequency: "); Serial.print(frequency);  Serial.println(" Hz");
    Serial.print("PF:        "); Serial.println(pf);
  }
  Serial.println("----------------------------------------------------------");

  delay(2000);
}
