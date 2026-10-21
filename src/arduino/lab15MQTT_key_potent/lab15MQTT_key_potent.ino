// C++ code
//
#include<MQTTClient.h>
#include<WiFiS3.h>
#include<string>

const char WIFI_SSID[] = "vivoY29"; 
const char WIFI_PASSWORD[] = "WtfBro555";

const char MQTT_BROKER_ADRRESS[] = "mqtt-dashboard.com";
const int MQTT_PORT = 1883;
const char MQTT_CLIENT_ID[] = "arduino-uno-r4-client"; 
const char MQTT_USERNAME[] = "";             
const char MQTT_PASSWORD[] = "";            

const char PUBLISH_TOPIC[] = "arduino-uno-r4/keypad";       
const char SUBSCRIBE_TOPIC[] = "aabbcc";  

WiFiClient network;
MQTTClient mqtt = MQTTClient(256);

unsigned long lastPublishTime = 0;
const int PUBLISH_INTERVAL = 10; 

void setup()
{
  Serial.begin(9600);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    Serial.print("Arduino UNO R4 - Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // wait 10 seconds for connection:
    delay(1000);
  }
  // print your board's IP address:
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  connectToMQTT();
  
}

void loop()
{
  mqtt.loop();
  if (!mqtt.connected()){
    Serial.println("Arduino UNO R4 - MQTT broker Timeout!");
    connectToMQTT();
  }
  if (millis() - lastPublishTime > PUBLISH_INTERVAL) {
    checkKeyPad();
    lastPublishTime = millis();
  }

  
  
}

void checkKeyPad(){
  digitalWrite(6, HIGH);
  digitalWrite(7, HIGH);
  digitalWrite(8, HIGH);
  digitalWrite(9, LOW);
  int column1 = (digitalRead(5)!=1)*1;
  int column2 = (digitalRead(4)!=1)*2;
  int column3 = (digitalRead(3)!=1)*3;
  int column4 = (digitalRead(2)!=1)*97;
  if (column4){
  	sendToMQTT("A      ");
  } else if (column1+column2+column3) {
    int res = column1+column2+column3;
  	String str = String(res) + "  ";
    sendToMQTT(str.c_str());
  }
  
  digitalWrite(6, HIGH);
  digitalWrite(7, HIGH);
  digitalWrite(8, LOW);
  digitalWrite(9, HIGH);
   column1 = (digitalRead(5)!=1)*4;
   column2 = (digitalRead(4)!=1)*5;
   column3 = (digitalRead(3)!=1)*6;
   column4 = (digitalRead(2)!=1)*98;
  if (column4){
  	sendToMQTT("B      ");
  } else if (column1+column2+column3) {
    int res = column1+column2+column3;
  	String str = String(res) + "  ";
    sendToMQTT(str.c_str());
  }
  
  digitalWrite(6, HIGH);
  digitalWrite(7, LOW);
  digitalWrite(8, HIGH);
  digitalWrite(9, HIGH);
   column1 = (digitalRead(5)!=1)*7;
   column2 = (digitalRead(4)!=1)*8;
   column3 = (digitalRead(3)!=1)*9;
   column4 = (digitalRead(2)!=1)*99;
  if (column4){
  	sendToMQTT("C      ");
  } else if (column1+column2+column3) {
    int res = column1+column2+column3;
  	String str = String(res) + "  ";
    sendToMQTT(str.c_str());
  }
  
  digitalWrite(6, LOW);
  digitalWrite(7, HIGH);
  digitalWrite(8, HIGH);
  digitalWrite(9, HIGH);
   column1 = (digitalRead(5)!=1)*1;
   column2 = (digitalRead(4)!=1)*2;
   column3 = (digitalRead(3)!=1)*3;
   column4 = (digitalRead(2)!=1)*4;
  switch (column1+column2+column3+column4){
    case (1):
      sendToMQTT("*      ");
    	break;
    case (2):
      sendToMQTT("0      ");
    	break;
    case (3):
      sendToMQTT("#      ");
    	break;
    case (4):
      sendToMQTT("D      ");
    	break;
  }
}

void checkPotentioMeter() {
  long LEDBrightness =  map(analogRead(A0), 0, 1023, 0, 255);
  Serial.print(LEDBrightness);
  sendToMQTT(String(LEDBrightness).c_str());
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

void sendToMQTT(const char key[30]) {
  mqtt.publish(PUBLISH_TOPIC, key);
  Serial.println(key);
}

void messageReceived(String &topic, String &payload) {
  Serial.println("Arduino UNO R4 - received from MQTT:");
  Serial.println("- topic: " + topic);
  Serial.println("- payload:");
  Serial.println(payload);

         
  analogWrite(6, payload.toInt());
  // You can process the incoming data , then control something
}