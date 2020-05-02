#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <EEPROM.h>
#include "config.h"
#include "mqtt.h"

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
std::string topicRoot(MQTTTOPIC);

WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  // Delete retained message
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  // TODO: Check payload for windowID
  EEPROM.write(FWUFLAG, 218);
  if (EEPROM.commit()) {
    Serial.println("EEPROM successfully committed");
    std::string t = topicRoot;
    t.append("firmwareupdate");
    Serial.println(t.c_str());
    Serial.println(client.publish(t.c_str(), "", true));
    delay(20);
    ESP.restart();
  } else {
    Serial.println("ERROR! EEPROM commit failed");
  }
  Serial.println();
}

void initMQTT() {
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void reconnectMQTT() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
      std::string t = topicRoot;
      t.append("status");
      char m[120];
      snprintf(m, strlen(m),"{\"timestamp\": \"%ld\",\"windowID\": \"%s\",\"status\": \"S00\"}", (long) time(NULL), WSID);
      // TODO: This publishes an empty message
      client.beginPublish(t.c_str(), strlen(m), true);
      client.print(m);
      client.endPublish();
      // ... and resubscribe
      t = topicRoot;
      t.append("firmwareupdate");
      client.subscribe(t.c_str());
      for (int i = 0; i < 10; i++) {
        client.loop();
        delay(20);
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/*
 * - publish message Format:
 * 
 * {
 * "timestamp": "165341651",
 * "windowID": "window-0",
 * "fromState": "0",
 * "toState": "1",
 * "description": {
 *   "en": {
 *     "properties": {
 *       "fromState": "closed",
 *       "toState": "opened"
 *     },
 *     "position": {
 *       "window": "riverside",
 *       "floor": "1",
 *       "room": "3"
 *     }
 *   }
 * }
 * }
 * 
 * - Topic format:
 * /smarthome/{country}/{state}/{city}/{{street}_{housenumber}}/windowsensor/stateupdate
 * /smarthome/{country}/{state}/{city}/{{street}_{housenumber}}/windowsensor/status
 * /smarthome/{country}/{state}/{city}/{{street}_{housenumber}}/windowsensor/firmwareupdate
 * /smarthome/{country}/{state}/{city}/{{street}_{housenumber}}/windowsensor/error
 */

void publishStateUpdate(std::string topic, States fromState, States toState) {
  // Capacity determined by: https://arduinojson.org/v6/assistant/
  const size_t capacity = JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5);
  DynamicJsonDocument doc(capacity);
  doc["timestamp"] = time(NULL);
  doc["windowID"] = WSID;
  doc["fromState"] = fromState;
  doc["toState"] = toState;
  JsonObject description = doc.createNestedObject("description");
  JsonObject description_lang = description.createNestedObject(LANG);
  JsonObject description_lang_properties = description_lang.createNestedObject("properties");
  description_lang_properties["fromState"] = (*translations.at(LANG).states).at(fromState).c_str();
  description_lang_properties["toState"] = (*translations.at(LANG).states).at(toState).c_str();
  JsonObject description_lang_position = description_lang.createNestedObject("position");
  description_lang_position["window"] = (*translations.at(LANG).directions).at(WSPOSD).c_str();
  description_lang_position["floor"] = WSPOSFL;
  description_lang_position["room"] = WSPOSRM;
  size_t mSize = measureJson(doc) + 1;
  char *message = (char *) malloc(mSize);
  if (message == NULL) {
    char m[50];
    snprintf(m, 50,"{\"timestamp\": \"%ld\",\"windowID\": \"%s\",\"error\": \"E001\"}", (long) time(NULL), WSID);
    client.publish("error", m, true);
  } else {
    serializeJson(doc, message, mSize);
    std::string t = topicRoot;
    t.append(topic);
    client.beginPublish(t.c_str(), strlen(message), true);
    client.print(message);
    client.endPublish();
  }
  free(message);
}
