#include <WiFiS3.h>
#include <Wire.h>
#include <MPU6050.h>

const char WIFI_SSID[]     = "vivoY29";
const char WIFI_PASSWORD[] = "WtfBro555";

// Where to send sensor data (your PC / server running a UDP listener)
const char   REMOTE_HOST[]  = "10.90.143.49";  // IP or hostname of receiver
const uint16_t REMOTE_PORT  = 8080;
// Port the Arduino listens on for incoming commands
const uint16_t LOCAL_PORT   = 4211;

const unsigned long PUBLISH_INTERVAL = 5;

WiFiUDP udp;
MPU6050 gyro;

int16_t gx, gy, gz;
long gxOff = 0, gyOff = 0, gzOff = 0;
unsigned long lastPublishTime = 0;

void connectToWiFi() {
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(2000);
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  gyro.initialize();
  gyro.setDLPFMode(MPU6050_DLPF_BW_20);

  connectToWiFi();
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
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
    udp.begin(LOCAL_PORT);
  }

  receiveUDP();

  if (millis() - lastPublishTime >= PUBLISH_INTERVAL) {
    sendUDP();
    lastPublishTime = millis();
  }
}

void sendUDP() {
  gyro.getRotation(&gx, &gy, &gz);

  // Remove the next three lines' "- off" if you want raw values like before
  char msg[48];
  snprintf(msg, sizeof(msg), "%d,%d,%d",
           (int)(gx - gxOff), (int)(gy - gyOff), (int)(gz - gzOff));

  udp.beginPacket(REMOTE_HOST, REMOTE_PORT);
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