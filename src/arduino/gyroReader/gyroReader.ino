#include <WiFiS3.h>
#include <Wire.h>
#include <MPU6050.h>
#include <EEPROM.h>

// Where to send sensor data (your PC / server running a UDP listener)
const uint16_t REMOTE_PORT  = 8080;
// Port the Arduino listens on for incoming commands
const uint16_t LOCAL_PORT   = 4211;

const unsigned long PUBLISH_INTERVAL = 5;

WiFiUDP udp;
MPU6050 gyro;

int16_t gx, gy, gz;
long gxOff = 0, gyOff = 0, gzOff = 0;
unsigned long lastPublishTime = 0;

struct NetworkConfig {
  char magic[4];   // e.g. "CFG" - tells us if valid data was ever saved
  char ssid[33];
  char pass[65];
  char ip[16];
};

NetworkConfig netConfig;
bool configLoaded = false;

// Non-blocking serial state
char rxBuffer[128];
size_t rxIndex = 0;

void setup() {
  Serial.begin(9600);
  Wire.begin();

  pinMode(8, INPUT_PULLUP);

  gyro.initialize();
  gyro.setDLPFMode(MPU6050_DLPF_BW_20);

  loadConfigFromEEPROM();
  handleWiFiConnection();
  udp.begin(LOCAL_PORT);

  // Gyro calibration (average of 1000 samples)
  Serial.println("Start collecting data");
  long sx = 0, sy = 0, sz = 0;
  for (int i = 0; i < 1000; i++) {
    gyro.getRotation(&gx, &gy, &gz);
    sx += gx; sy += gy; sz += gz;
    delay(2);
  }
  gxOff = sx / 1000; gyOff = sy / 1000; gzOff = sz / 1000;
  Serial.print(gxOff); Serial.print(",");
  Serial.print(gyOff); Serial.print(",");
  Serial.println(gzOff);
  Serial.println("End collecting data");
}

void loop() {
  readUSBNonBlocking();

  if (WiFi.status() != WL_CONNECTED) {
    handleWiFiConnection();
    if (WiFi.status() == WL_CONNECTED) {
      udp.begin(LOCAL_PORT);
    }
  }

  // Only attempt UDP if we have a valid configuration and active Wi-Fi
  if (configLoaded && WiFi.status() == WL_CONNECTED) {
    receiveUDP();

    if (millis() - lastPublishTime >= PUBLISH_INTERVAL) {
      sendUDP();
      lastPublishTime = millis();
    }
  }
}

void sendUDP() {
  gyro.getRotation(&gx, &gy, &gz);

  // Remove the next three lines' "- off" if you want raw values like before
  char msg[48];
  snprintf(msg, sizeof(msg), "%d,%d,%d,%d",
           (int)(gx - gxOff), (int)(gy - gyOff), (int)(gz - gzOff), digitalRead(8));

  udp.beginPacket(netConfig.ip, REMOTE_PORT);
  udp.print(msg);
  udp.endPacket();

  Serial.println(msg);
}

void receiveUDP() {
  int packetSize = udp.parsePacket();
  if (packetSize <= 0) return;

  char buf[64];
  int len = udp.read(buf, sizeof(buf) - 1);
  if (len <= 0) return;
  buf[len] = '\0';

  Serial.print("Received from ");
  Serial.print(udp.remoteIP());
  Serial.print(":");
  Serial.print(udp.remotePort());
  Serial.print(" -> ");
  Serial.println(buf);

  analogWrite(6, atoi(buf));
}

void handleWiFiConnection() {
  if (!configLoaded) return; // Do not attempt until credentials exist

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print(F("Connecting to "));
    Serial.println(netConfig.ssid);
    WiFi.begin(netConfig.ssid, netConfig.pass);
    
    // Non-blocking timeout or simple check
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 4000) {
      delay(200);
      Serial.print(F("."));
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(F("\nConnected!"));
      Serial.print(F("Device IP: "));
      Serial.println(WiFi.localIP());
    } else {
      Serial.println(F("\nConnection failed. Retrying later..."));
    }
  }
}

void loadConfigFromEEPROM() {
  EEPROM.get(0, netConfig);
  if (strncmp(netConfig.magic, "CFG", 3) == 0) {
    configLoaded = true;
    Serial.println(F("[EEPROM] Valid configuration loaded."));
    Serial.print(F("SSID: ")); Serial.println(netConfig.ssid);
    Serial.print(F("IP: "));   Serial.println(netConfig.ip);
  } else {
    configLoaded = false;
    Serial.println(F("[EEPROM] No credentials saved."));
    Serial.println(F("Send via Serial: SSID;PASSWORD;TARGET_IP<Enter>"));
  }
}

void saveConfigToEEPROM(const char* ssid, const char* pass, const char* ip) {
  strncpy(netConfig.magic, "CFG", sizeof(netConfig.magic));
  strncpy(netConfig.ssid, ssid, sizeof(netConfig.ssid) - 1);
  netConfig.ssid[sizeof(netConfig.ssid) - 1] = '\0';

  strncpy(netConfig.pass, pass, sizeof(netConfig.pass) - 1);
  netConfig.pass[sizeof(netConfig.pass) - 1] = '\0';

  strncpy(netConfig.ip, ip, sizeof(netConfig.ip) - 1);
  netConfig.ip[sizeof(netConfig.ip) - 1] = '\0';

  EEPROM.put(0, netConfig);
  configLoaded = true;
  Serial.println(F("[EEPROM] Credentials saved successfully."));
}

void parseSerialInput(char* input) {
  // Expected format: SSID;PASSWORD;TARGET_IP
  char* ssid = strtok(input, ";");
  char* pass = strtok(NULL, ";");
  char* ip   = strtok(NULL, ";");

  if (ssid != NULL && pass != NULL && ip != NULL) {
    saveConfigToEEPROM(ssid, pass, ip);
    // Force a fresh Wi-Fi connection
    WiFi.disconnect();
  } else {
    Serial.println(F("[ERROR] Format invalid. Use: SSID;PASSWORD;TARGET_IP"));
  }
}

void readUSBNonBlocking() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\r') {
      continue; // Skip carriage returns
    }

    if (c == '\n') {
      rxBuffer[rxIndex] = '\0';
      if (rxIndex > 0) {
        parseSerialInput(rxBuffer);
      }
      rxIndex = 0; // Reset buffer pointer
    } else {
      if (rxIndex < sizeof(rxBuffer) - 1) {
        rxBuffer[rxIndex++] = c;
      }
    }
  }
}