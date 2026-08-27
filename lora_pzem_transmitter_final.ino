/*
  ==========================================================
      SL-01 PZEM-004T + SX1276 LoRa TRANSMITTER
  ==========================================================

  PZEM CONNECTION:

    PZEM TX  -> ESP32 GPIO18 (RX)
    PZEM RX  -> ESP32 GPIO19 (TX)
    PZEM 5V  -> ESP32 5V
    PZEM GND -> ESP32 GND


  SX1276 CONNECTION:

    SCK  -> GPIO14
    MISO -> GPIO12
    MOSI -> GPIO13
    NSS  -> GPIO15
    RST  -> GPIO27
    DIO0 -> GPIO4


  LoRa SETTINGS:

    Frequency : 866 MHz
    BW        : 125 kHz
    SF        : 7
    CR        : 4/5
    SyncWord  : 0x12


  NORMAL PACKET:

    SL-01,V=231.5,I=0.524,P=119.8,E=0.012,F=50.0,PF=0.99,STATUS=ON


  PZEM ERROR PACKET:

    SL-01,V=0.0,I=0.000,P=0.0,E=0.000,F=0.0,PF=0.00,STATUS=FAULT
*/

// ==========================================================
// LIBRARIES
// ==========================================================

#include <PZEM004Tv30.h>
#include <SPI.h>
#include <LoRa.h>

// ==========================================================
// STREET LIGHT ID
// ==========================================================

#define STREET_LIGHT_ID "SL-01"

// ==========================================================
// PZEM UART
// ==========================================================

// PZEM TX -> ESP32 RX
#define PZEM_RX_PIN 18

// PZEM RX -> ESP32 TX
#define PZEM_TX_PIN 19

PZEM004Tv30 pzem(
  Serial2,
  PZEM_RX_PIN,
  PZEM_TX_PIN
);

// ==========================================================
// SX1276 LoRa PINS
// ==========================================================

#define LORA_SCK   14
#define LORA_MISO  12
#define LORA_MOSI  13
#define LORA_SS    15
#define LORA_RST   27
#define LORA_DIO0   4

// ==========================================================
// LoRa FREQUENCY
// ==========================================================

#define LORA_FREQ 866E6

// ==========================================================
// TRANSMISSION INTERVAL
// ==========================================================

unsigned long previousMillis = 0;

const unsigned long TRANSMIT_INTERVAL = 5000;

// ==========================================================
// SETUP
// ==========================================================

void setup()
{
  // --------------------------------------------------------
  // SERIAL MONITOR
  // --------------------------------------------------------

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println();
  Serial.println("==========================================");
  Serial.println("       SL-01 LoRa TRANSMITTER");
  Serial.println("==========================================");

  // --------------------------------------------------------
  // PZEM UART
  // --------------------------------------------------------

  Serial.println();
  Serial.println("Initializing PZEM-004T...");

  Serial2.begin(
    9600,
    SERIAL_8N1,
    PZEM_RX_PIN,
    PZEM_TX_PIN
  );

  delay(500);

  Serial.println("PZEM UART READY");

  Serial.print("PZEM RX : GPIO");
  Serial.println(PZEM_RX_PIN);

  Serial.print("PZEM TX : GPIO");
  Serial.println(PZEM_TX_PIN);

  // --------------------------------------------------------
  // LoRa SPI
  // --------------------------------------------------------

  Serial.println();
  Serial.println("Initializing SX1276...");

  SPI.begin(
    LORA_SCK,
    LORA_MISO,
    LORA_MOSI,
    LORA_SS
  );

  LoRa.setPins(
    LORA_SS,
    LORA_RST,
    LORA_DIO0
  );

  // --------------------------------------------------------
  // START LoRa
  // --------------------------------------------------------

  if (!LoRa.begin(LORA_FREQ))
  {
    Serial.println();
    Serial.println("ERROR: LoRa initialization failed!");

    while (1)
    {
      delay(1000);
    }
  }

  // --------------------------------------------------------
  // LoRa PARAMETERS
  // --------------------------------------------------------

  LoRa.setSpreadingFactor(7);

  LoRa.setSignalBandwidth(125E3);

  LoRa.setCodingRate4(5);

  LoRa.setSyncWord(0x12);

  // --------------------------------------------------------
  // READY
  // --------------------------------------------------------

  Serial.println();
  Serial.println("------------------------------------------");
  Serial.println("SX1276 TRANSMITTER READY");
  Serial.println("------------------------------------------");

  Serial.print("Node      : ");
  Serial.println(STREET_LIGHT_ID);

  Serial.println("Frequency : 866 MHz");
  Serial.println("BW        : 125 kHz");
  Serial.println("SF        : 7");
  Serial.println("CR        : 4/5");
  Serial.println("SyncWord  : 0x12");

  Serial.println("------------------------------------------");
  Serial.println("Waiting for PZEM data...");
}

// ==========================================================
// READ PZEM AND TRANSMIT
// ==========================================================

void readAndTransmit()
{
  // ========================================================
  // READ ALL PZEM PARAMETERS
  // ========================================================

  float voltage   = pzem.voltage();
  float current   = pzem.current();
  float power     = pzem.power();
  float energy    = pzem.energy();
  float frequency = pzem.frequency();
  float pf        = pzem.pf();

  // ========================================================
  // CHECK FOR PZEM ERROR
  // ========================================================

  bool pzemError =
    isnan(voltage) ||
    isnan(current) ||
    isnan(power) ||
    isnan(energy) ||
    isnan(frequency) ||
    isnan(pf);

  // ========================================================
  // PZEM ERROR
  // ========================================================

  if (pzemError)
  {
    Serial.println();
    Serial.println("==========================================");
    Serial.println("          PZEM READ ERROR!");
    Serial.println("==========================================");

    Serial.println("PZEM data unavailable.");
    Serial.println("Sending complete FAULT packet.");

    // ------------------------------------------------------
    // Set all parameters to zero
    // ------------------------------------------------------

    voltage   = 0.0;
    current   = 0.0;
    power     = 0.0;
    energy    = 0.0;
    frequency = 0.0;
    pf        = 0.0;
  }

  // ========================================================
  // DETERMINE STATUS
  // ========================================================

  const char* status;

  if (pzemError)
  {
    status = "FAULT";
  }
  else if (current <= 0.04)
  {
    status = "FAULT";
  }
  else if (current < 0.20)
  {
    status = "DIM";
  }
  else if (current <= 0.90)
  {
    status = "ON";
  }
  else
  {
    status = "OVERCURRENT";
  }

  // ========================================================
  // CREATE LoRa PACKET
  // ========================================================

  String packet;

  packet.reserve(180);

  packet += STREET_LIGHT_ID;

  packet += ",V=";
  packet += String(voltage, 1);

  packet += ",I=";
  packet += String(current, 3);

  packet += ",P=";
  packet += String(power, 1);

  packet += ",E=";
  packet += String(energy, 3);

  packet += ",F=";
  packet += String(frequency, 1);

  packet += ",PF=";
  packet += String(pf, 2);

  packet += ",STATUS=";
  packet += status;

  // ========================================================
  // SERIAL DISPLAY
  // ========================================================

  Serial.println();
  Serial.println("==========================================");
  Serial.println("              PZEM DATA");
  Serial.println("==========================================");

  Serial.print("Voltage   : ");
  Serial.print(voltage, 1);
  Serial.println(" V");

  Serial.print("Current   : ");
  Serial.print(current, 3);
  Serial.println(" A");

  Serial.print("Power     : ");
  Serial.print(power, 1);
  Serial.println(" W");

  Serial.print("Energy    : ");
  Serial.print(energy, 3);
  Serial.println(" kWh");

  Serial.print("Frequency : ");
  Serial.print(frequency, 1);
  Serial.println(" Hz");

  Serial.print("PF        : ");
  Serial.println(pf, 2);

  Serial.print("Status    : ");
  Serial.println(status);

  // ========================================================
  // LoRa PACKET
  // ========================================================

  Serial.println();
  Serial.println("==========================================");
  Serial.println("          TRANSMITTING PACKET");
  Serial.println("==========================================");

  Serial.println(packet);

  // ========================================================
  // TRANSMIT
  // ========================================================

  LoRa.beginPacket();

  LoRa.print(packet);

  int result = LoRa.endPacket();

  if (result == 1)
  {
    Serial.println();
    Serial.println(">>> PACKET SENT SUCCESSFULLY <<<");
  }
  else
  {
    Serial.println();
    Serial.println(">>> PACKET TRANSMISSION FAILED <<<");
  }

  Serial.println("==========================================");
}

// ==========================================================
// LOOP
// ==========================================================

void loop()
{
  unsigned long currentMillis = millis();

  // --------------------------------------------------------
  // TRANSMIT EVERY 5 SECONDS
  // --------------------------------------------------------

  if (
    currentMillis - previousMillis >=
    TRANSMIT_INTERVAL
  )
  {
    previousMillis = currentMillis;

    readAndTransmit();
  }
}