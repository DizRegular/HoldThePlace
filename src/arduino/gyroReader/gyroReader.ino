/*
 *
 * This Arduino UNO R4 code is made available for public use without any restriction
 *
 */

#include <WiFiS3.h>
#include <MQTTClient.h>
#include <MPU6050.h>
const char WIFI_SSID[] = "Dursery_2.4G";     // CHANGE TO YOUR WIFI SSID
const char WIFI_PASSWORD[] = "durs12345";  // CHANGE TO YOUR WIFI PASSWORD

const char MQTT_BROKER_ADRRESS[] = "mqtt-dashboard.com";  // CHANGE TO MQTT BROKER'S ADDRESS
//const char MQTT_BROKER_ADRRESS[] = "192.168.0.11";  // CHANGE TO MQTT BROKER'S IP ADDRESS
const int MQTT_PORT = 1883;
const char MQTT_CLIENT_ID[] = "arduino-uno-r4-client";  // CHANGE IT AS YOU DESIRE
const char MQTT_USERNAME[] = "";              // CHANGE IT IF REQUIRED, empty if not required
const char MQTT_PASSWORD[] = "";              // CHANGE IT IF REQUIRED, empty if not required

// The MQTT topics that Arduino should publish/subscribe
const char PUBLISH_TOPIC[] = "HoldThePlaceIotDevice";       // CHANGE IT AS YOU DESIRE
const char SUBSCRIBE_TOPIC[] = "HoldThePlaceClient";  // CHANGE IT AS YOU DESIRE

const int PUBLISH_INTERVAL = 5;  // 5 seconds

WiFiClient network;
MQTTClient mqtt = MQTTClient(256);
MPU6050 gyro;

int16_t ax, ay, az;
int16_t gx, gy, gz;
unsigned long lastPublishTime = 0;

void setup() {
  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
  #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
  #endif
  Serial.begin(9600);
  // print your board's IP address:
  gyro.initialize();
  gyro.setDLPFMode(MPU6050_DLPF_BW_20); // or MPU6050_DLPF_BW_20
  connectToMQTT();
  Serial.println("Start collecting data");
  long gxN = 0, gyN = 0, gzN = 0;
  for (int i=0; i<1000; i++){
    gyro.getRotation(&gx, &gy, &gz);
    gxN += gx;
    gyN += gy;
    gzN += gz;
    delay(2);
  }
  gxN /= 1000; gyN /= 1000; gzN /= 1000;
  String val_str = String(gxN) + "," + String(gyN) + "," + String(gzN);
  char messageBuffer[21];
  val_str.toCharArray(messageBuffer, 21);
  Serial.println(messageBuffer);
  Serial.println("End collecting data");
}

void loop() {
  mqtt.loop();
  if (!mqtt.connected()){
    Serial.println("Arduino UNO R4 - MQTT broker Timeout!");
    connectToMQTT();
  }
  if (millis() - lastPublishTime > PUBLISH_INTERVAL) {
    sendToMQTT();
    lastPublishTime = millis();
  }
}

void connectToMQTT() {
  // Connect to the MQTT broker
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    Serial.print("Arduino UNO R4 - Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // wait 10 seconds for connection:
    delay(2000);
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  mqtt.begin(MQTT_BROKER_ADRRESS, MQTT_PORT, network);

  // Create a handler for incoming messages
  mqtt.onMessage(messageReceived);

  Serial.print("Arduino UNO R4 - Connecting to MQTT broker");

  while (!mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (!mqtt.connected()) {
    Serial.println("Arduino UNO R4 - MQTT broker Timeout!");
    return;
  }

  // Subscribe to a topic, the incoming messages are processed by messageHandler() function
  while (!mqtt.subscribe(SUBSCRIBE_TOPIC)) {
    Serial.print("Arduino UNO R4 - Failed to subscribe to the topic: ");
    Serial.println(SUBSCRIBE_TOPIC);
    int status = WL_IDLE_STATUS;
    while (status != WL_CONNECTED) {
      Serial.print("Arduino UNO R4 - Attempting to connect to SSID: ");
      Serial.println(WIFI_SSID);
      // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
      status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

      // wait 10 seconds for connection:
      delay(2000);
    }
    delay(1000);
  }
  Serial.print("Arduino UNO R4 - Subscribed to the topic: ");
  Serial.println(SUBSCRIBE_TOPIC);

  
  Serial.println("Arduino UNO R4 - MQTT broker Connected!");
}

void sendToMQTT() {

  int val = millis()/1000;
  //int val = analogRead(A0);
  gyro.getRotation(&gx, &gy, &gz);
  
  String val_str = String(gx) + "," + String(gy) + "," + String(gz);
  char messageBuffer[21];
  val_str.toCharArray(messageBuffer, 21);
  mqtt.publish(PUBLISH_TOPIC, messageBuffer);
  
  Serial.println(messageBuffer);
}

void messageReceived(String &topic, String &payload) {
  Serial.println("Arduino UNO R4 - received from MQTT:");
  Serial.println("- topic: " + topic);
  Serial.println("- payload:");
  Serial.println(payload);

         
  analogWrite(6, payload.toInt());
  // You can process the incoming data , then control something
}