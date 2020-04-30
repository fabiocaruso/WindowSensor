#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include "config.h"

extern "C" {
  #include "gpio.h"
}

extern "C" {
  #include "user_interface.h"
}

const char windowID[] = WSID;

const char* mqtt_server = MQTTSERVER;
const char* mqtt_user = windowID;
const char* mqtt_password = MQTTPW;

char msg[50];

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void initMQTT(PubSubClient *client) {
  (*client).setServer(mqtt_server, 1883);
  (*client).setCallback(callback);
}

void reconnectMQTT(PubSubClient *client) {
  // Loop until we're reconnected
  while (!(*client).connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    strcpy(msg, windowID);
    strcat(msg, ": Connection lost!");
    if ((*client).connect(clientId.c_str(), mqtt_user, mqtt_password, "/windowSensor/status", 2, 1, msg)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      (*client).publish("outTopic", "hello world");
      // ... and resubscribe
      (*client).subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print((*client).state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
