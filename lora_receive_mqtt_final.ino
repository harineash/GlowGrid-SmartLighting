#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <LoRa.h>

// =====================================================
// WIFI
// =====================================================

const char* WIFI_SSID     = "esp";
const char* WIFI_PASSWORD = "12345678";

// =====================================================
// HIVEMQ
// =====================================================

const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

const char* MQTT_TOPIC  = "streetlights/data";
const char* STATUS_TOPIC = "streetlights/status";

// =====================================================
// MQTT
// =====================================================

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// =====================================================
// SX1276
// =====================================================

#define LORA_SCK   14
#define LORA_MISO  12
#define LORA_MOSI  13
#define LORA_SS    15
#define LORA_RST   27
#define LORA_DIO0   4

#define LORA_FREQ 866E6

// =====================================================
// REAL SL-01 DATA
// =====================================================

float sl01_voltage = 0.0;
float sl01_current = 0.0;
float sl01_power = 0.0;
float sl01_energy = 0.0;
float sl01_frequency = 0.0;
float sl01_pf = 0.0;

String sl01_status = "FAULT";

bool sl01_received = false;

// =====================================================
// MQTT TIMER
// =====================================================

unsigned long previousMillis = 0;

const unsigned long PUBLISH_INTERVAL = 3000;

// =====================================================
// WIFI
// =====================================================

void connectWiFi()
{
  Serial.println();
  Serial.println("========================================");
  Serial.println("Connecting to WiFi...");
  Serial.println("========================================");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// =====================================================
// MQTT
// =====================================================

void connectMQTT()
{
  while (!mqttClient.connected())
  {
    Serial.println();
    Serial.println("Connecting to HiveMQ...");

    String clientID = "ESP32-LoRa-Gateway-";

    clientID += String(
      random(1000, 9999)
    );

    if (mqttClient.connect(clientID.c_str()))
    {
      Serial.println("MQTT Connected!");

      mqttClient.publish(
        STATUS_TOPIC,
        "ESP32 LoRa Gateway ONLINE"
      );
    }
    else
    {
      Serial.print("MQTT failed. State = ");
      Serial.println(mqttClient.state());

      delay(3000);
    }
  }
}

// =====================================================
// GET VALUE FROM PACKET
// =====================================================

float getValue(
  String packet,
  String key
)
{
  int start = packet.indexOf(key);

  if (start == -1)
  {
    return 0;
  }

  start += key.length();

  int end = packet.indexOf(
    ',',
    start
  );

  if (end == -1)
  {
    end = packet.length();
  }

  return packet.substring(
    start,
    end
  ).toFloat();
}

// =====================================================
// GET STATUS
// =====================================================

String getStatus(String packet)
{
  int start = packet.indexOf(
    "STATUS="
  );

  if (start == -1)
  {
    return "FAULT";
  }

  start += 7;

  int end = packet.indexOf(
    ',',
    start
  );

  if (end == -1)
  {
    end = packet.length();
  }

  String status =
    packet.substring(
      start,
      end
    );

  status.trim();

  return status;
}

// =====================================================
// PROCESS LoRa PACKET
// =====================================================

void processLoRaPacket(
  String packet
)
{
  Serial.println();
  Serial.println("========================================");
  Serial.println("LoRa PACKET RECEIVED");
  Serial.println("========================================");

  Serial.println(packet);

  // ---------------------------------------------------
  // Check SL-01
  // ---------------------------------------------------

  if (!packet.startsWith("SL-01"))
  {
    Serial.println("Packet ignored - not SL-01");
    return;
  }

  // ---------------------------------------------------
  // Extract real PZEM values
  // ---------------------------------------------------

  sl01_voltage =
    getValue(packet, "V=");

  sl01_current =
    getValue(packet, "I=");

  sl01_power =
    getValue(packet, "P=");

  sl01_energy =
    getValue(packet, "E=");

  sl01_frequency =
    getValue(packet, "F=");

  sl01_pf =
    getValue(packet, "PF=");

  sl01_status =
    getStatus(packet);

  sl01_received = true;

  // ---------------------------------------------------
  // Display
  // ---------------------------------------------------

  Serial.println();
  Serial.println("REAL SL-01 DATA");

  Serial.print("Voltage : ");
  Serial.print(sl01_voltage, 1);
  Serial.println(" V");

  Serial.print("Current : ");
  Serial.print(sl01_current, 3);
  Serial.println(" A");

  Serial.print("Power   : ");
  Serial.print(sl01_power, 1);
  Serial.println(" W");

  Serial.print("Energy  : ");
  Serial.print(sl01_energy, 3);
  Serial.println(" kWh");

  Serial.print("Freq    : ");
  Serial.print(sl01_frequency, 1);
  Serial.println(" Hz");

  Serial.print("PF      : ");
  Serial.println(sl01_pf, 2);

  Serial.print("Status  : ");
  Serial.println(sl01_status);

  Serial.println("========================================");
}

// =====================================================
// PUBLISH MQTT DATA
// =====================================================

void publishStreetlightData()
{
  char payload[2048];

  // ---------------------------------------------------
  // START JSON
  // ---------------------------------------------------

  strcpy(
    payload,
    "{"
  );

  // ===================================================
  // SL-01 REAL DATA
  // ===================================================

  char realData[300];

  sprintf(
    realData,

    "\"SL-01\":{"
    "\"voltage\":%.1f,"
    "\"current\":%.3f,"
    "\"power\":%.1f,"
    "\"pf\":%.2f,"
    "\"status\":\"%s\""
    "}",

    sl01_voltage,
    sl01_current,
    sl01_power,
    sl01_pf,
    sl01_status.c_str()
  );

  strcat(
    payload,
    realData
  );

  // ===================================================
  // SL-02 TO SL-10
  // DUMMY FAULT DATA
  // ===================================================

  for (int n = 2; n <= 10; n++)
  {
    char dummyData[200];

    // Slightly different voltage values
    float voltage =
      228.0 +
      random(0, 100) / 100.0;

    // Fault current: 0-0.04 A
    float current =
      random(0, 5) / 100.0;

    sprintf(
      dummyData,

      ",\"SL-%02d\":{"
      "\"voltage\":%.1f,"
      "\"current\":%.2f,"
      "\"power\":0.0,"
      "\"pf\":0.00,"
      "\"status\":\"FAULT\""
      "}",

      n,
      voltage,
      current
    );

    strcat(
      payload,
      dummyData
    );
  }

  // ---------------------------------------------------
  // END JSON
  // ---------------------------------------------------

  strcat(
    payload,
    "}"
  );

  // ===================================================
  // MQTT PUBLISH
  // ===================================================

  bool success =
    mqttClient.publish(
      MQTT_TOPIC,
      payload
    );

  // ===================================================
  // SERIAL
  // ===================================================

  Serial.println();
  Serial.println("========================================");

  if (success)
  {
    Serial.println("MQTT PUBLISH SUCCESS");
  }
  else
  {
    Serial.println("MQTT PUBLISH FAILED");
  }

  Serial.println("========================================");

  Serial.println("Topic:");
  Serial.println(MQTT_TOPIC);

  Serial.println();
  Serial.println("MQTT DATA:");

  Serial.println(payload);

  Serial.println("========================================");
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  SL-01 LoRa → MQTT GATEWAY");
  Serial.println("========================================");

  Serial.println("Real node : SL-01");
  Serial.println("Dummy     : SL-02 to SL-10");
  Serial.println("Interval  : 3 seconds");
  Serial.println("MQTT      : broker.hivemq.com");
  Serial.println("Topic     : streetlights/data");

  randomSeed(
    micros()
  );

  // ===================================================
  // WIFI
  // ===================================================

  connectWiFi();

  // ===================================================
  // MQTT
  // ===================================================

  mqttClient.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );

  mqttClient.setBufferSize(
    2048
  );

  connectMQTT();

  // ===================================================
  // LoRa
  // ===================================================

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

  if (!LoRa.begin(LORA_FREQ))
  {
    Serial.println(
      "LoRa initialization failed!"
    );

    while (1)
    {
      delay(1000);
    }
  }

  // Same settings as SL-01 transmitter
  LoRa.setSpreadingFactor(7);

  LoRa.setSignalBandwidth(
    125E3
  );

  LoRa.setCodingRate4(5);

  LoRa.setSyncWord(0x12);

  // Start receive mode
  LoRa.receive();

  Serial.println();
  Serial.println("========================================");
  Serial.println("GATEWAY READY");
  Serial.println("========================================");

  Serial.println("LoRa : 866 MHz");
  Serial.println("SF   : 7");
  Serial.println("BW   : 125 kHz");
  Serial.println("CR   : 4/5");
  Serial.println("Sync : 0x12");

  Serial.println();
  Serial.println("Waiting for SL-01...");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  // ===================================================
  // WIFI
  // ===================================================

  if (WiFi.status() != WL_CONNECTED)
  {
    connectWiFi();
  }

  // ===================================================
  // MQTT
  // ===================================================

  if (!mqttClient.connected())
  {
    connectMQTT();
  }

  mqttClient.loop();

  // ===================================================
  // CHECK LoRa
  // ===================================================

  int packetSize =
    LoRa.parsePacket();

  if (packetSize)
  {
    String receivedPacket = "";

    while (LoRa.available())
    {
      receivedPacket +=
        (char)LoRa.read();
    }

    processLoRaPacket(
      receivedPacket
    );

    Serial.print("RSSI: ");
    Serial.print(
      LoRa.packetRssi()
    );

    Serial.println(" dBm");

    Serial.print("SNR: ");
    Serial.print(
      LoRa.packetSnr()
    );

    Serial.println(" dB");

    LoRa.receive();
  }

  // ===================================================
  // MQTT PUBLISH EVERY 3 SECONDS
  // ===================================================

  unsigned long currentMillis =
    millis();

  if (
    currentMillis - previousMillis
    >= PUBLISH_INTERVAL
  )
  {
    previousMillis =
      currentMillis;

    /*
       Even if SL-01 hasn't transmitted yet,
       the gateway will publish dummy/initial
       values.

       After the first SL-01 packet arrives,
       its real values are used.
    */

    publishStreetlightData();
  }
}