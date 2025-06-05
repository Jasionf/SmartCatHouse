#include <Arduino.h>
#include <Wire.h>
#include "sensirion_common.h"
#include "sgp30.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "SHT31.h"

const char* ssid = "Maker_2.4G";
const char* password = "maker2025";
const char* mqtt_server = "192.168.1.52"; 
const char* mqtt_user = "cat";
const char* mqtt_password = "cat";

const char* temp_topic = "cathouse/temperature";
const char* hum_topic = "cathouse/humidity";
const char* co2_topic = "cathouse/co2";
const char* tvoc_topic = "cathouse/tvoc";
const char* nh3_topic = "cathouse/nh3";
const char* command_topic = "cathouse/command";


WiFiClient espClient;
PubSubClient client(espClient);

SHT31 sht31 = SHT31();


unsigned long lastMsgTime = 0;
long updateInterval = 5000;  
bool immediate_update = false;


void SHT31_Driver(){
  sht31.begin();  
};


// 连接WiFi
void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}


void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}


void publishSensorData() {
  float temp = round(sht31.getTemperature() * 100) / 100;
  float hum = round(sht31.getHumidity() * 100) / 100;

  int sensorPin = A0;
  float sensorValue = 0;
  sensorValue = analogRead(sensorPin);

  float nh3 = round(sensorValue * 100) / 100.0;

  s16 err = 0;
  u16 tvoc_ppb, co2_eq_ppm;
  err = sgp_measure_iaq_blocking_read(&tvoc_ppb, &co2_eq_ppm);

  float tvoc = round(tvoc_ppb * 100) / 100.0;
  float co2 = round(co2_eq_ppm * 100) / 100.0;

  // 创建JSON文档
  StaticJsonDocument<200> doc;
  doc["temperature"] = temp;
  doc["humidity"] = hum;
  doc["nh3"] = nh3;
  doc["tvoc"] = tvoc;
  doc["co2"] = co2;

  // 序列化JSON
  char output[500];
  serializeJson(doc, output);
  // 发布数据
  if (!isnan(temp)) {
    client.publish(temp_topic, output);  
    // client.publish(temp_topic, String(temp).c_str());
    // client.publish("cathouse/temperature", output);
  }
  if (!isnan(hum)) {
    client.publish(hum_topic, output);
    // client.publish(hum_topic, String(hum).c_str());
    // client.publish("cathouse/humidity", output);
  }
  if (!isnan(nh3)) {
    client.publish(nh3_topic, output);
    // client.publish(nh3_topic, String(nh3).c_str());
    // client.publish("cathouse/nh3", output); 
  }
  if (!isnan(co2)) {
    client.publish(co2_topic, output);
    // client.publish(co2_topic, String(co2).c_str());
    // client.publish("cathouse/co2", output);  
  }
  if (!isnan(tvoc)) {
    client.publish(tvoc_topic, output);
    // client.publish(tvoc_topic, String(tvoc).c_str());
    // client.publish("cathouse/tvoc", output); 
  }
  client.publish("cathouse/sensors", output);
  Serial.println("Published sensor data:");
  Serial.println(output);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  
  char message[length+1];
  for (int i=0; i<length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);
  if (String(topic) == command_topic) {
    if (String(message) == "update") {
      immediate_update = true;
      Serial.println("Immediate update triggered");
    }
    else if (String(message).startsWith("interval:")) {
      long new_interval = String(message).substring(9).toInt() * 1000;
      if (new_interval >= 1000) {
        updateInterval = new_interval;  // 修改这里使用updateInterval
        Serial.print("Update interval changed to: ");
        Serial.print(updateInterval/1000);
        Serial.println("s");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  SHT31_Driver();


  s16 err;
  u32 ah = 0;
  u16 scaled_ethanol_signal, scaled_h2_signal;
  Serial.begin(115200);
  Serial.println("serial start!!");
  while (sgp_probe() != STATUS_OK) {
      Serial.println("SGP failed");
      while (1);
  }
  /*Read H2 and Ethanol signal in the way of blocking*/
  err = sgp_measure_signals_blocking_read(&scaled_ethanol_signal,
                                          &scaled_h2_signal);
  if (err == STATUS_OK) {
      Serial.println("get ram signal!");
  } else {
      Serial.println("error reading signals");
  }

  sgp_set_absolute_humidity(13000);
  err = sgp_iaq_init();
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  unsigned long now = millis();
  if (now - lastMsgTime >= updateInterval || immediate_update) {  // 这里也改为updateInterval
    lastMsgTime = now;
    immediate_update = false;
    
    publishSensorData();
  }
}




