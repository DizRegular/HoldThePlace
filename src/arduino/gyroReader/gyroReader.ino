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

const int PUBLISH_INTERVAL = 0.1;  // 5 seconds

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

  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    Serial.print("Arduino UNO R4 - Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // wait 10 seconds for connection:
    delay(10000);
  }
  // print your board's IP address:
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  gyro.initialize();
  connectToMQTT();
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
  
  Serial.println("/");
}

void messageReceived(String &topic, String &payload) {
  Serial.println("Arduino UNO R4 - received from MQTT:");
  Serial.println("- topic: " + topic);
  Serial.println("- payload:");
  Serial.println(payload);

         
  analogWrite(6, payload.toInt());
  // You can process the incoming data , then control something
}