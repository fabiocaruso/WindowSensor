#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"
#include "ota.h"
#include "mqtt.h"

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

WiFiClient espClient;
PubSubClient client(espClient);

const char windowID[] = WSID;
const uint16_t delayTime = 5000;
const uint8_t closedPin = 4;
const uint8_t tiltedPin = 5;
uint8_t state = 0;

enum States {
  CLOSED_S = 0,
  OPENED_S = 1,
  TILTED_S = 2
};
struct language_t {
  std::string code;
  std::map<std::string,std::string> *directions;
  std::map<States,std::string> *states;
};
std::map<States,std::string> states_en = { {States::CLOSED_S,"closed"}, {States::OPENED_S,"opened"}, {States::TILTED_S,"tilted"} };
std::map<States,std::string> states_de = { {States::CLOSED_S,"geschlossen"}, {States::OPENED_S,"offen"}, {States::TILTED_S,"gekippt"} };
std::map<std::string,std::string> directions_en = { {"N", "North"}, {"E", "East"}, {"S", "South"}, {"W", "West"} };
std::map<std::string,std::string> directions_de = { {"N", "Norden"}, {"E", "Osten"}, {"S", "Süden"}, {"W", "Westen"} };
language_t lang_en = {"en", &directions_en, &states_en};
language_t lang_de = {"de", &directions_de, &states_de};
std::map<std::string,language_t> translations = { {"en",lang_en}, {"de",lang_de} };

void lightSleep() {
  // https://www.espressif.com/sites/default/files/documentation/2c-esp8266_non_os_sdk_api_reference_en.pdf
  Serial.println("Going to sleep NOW!");
  client.disconnect();
  wifi_station_disconnect();
  wifi_set_opmode_current(NULL_MODE);
  wifi_fpm_open(); // Enables force sleep
  wifi_fpm_do_sleep(0xFFFFFFF); // Sleep for longest possible time
  delay(1000); // Needed cuz there is a delay time until esp is in sleep mode
}

void lightSleepWakeup() {
  Serial.println("Woke up!");
  wifi_fpm_close();
  wifi_set_opmode_current(STATION_MODE);
  wifi_station_connect();
}

void publishStateUpdate(std::string topic, States fromState, States toState) {
  // Capacity determined by: https://arduinojson.org/v6/assistant/
  const size_t capacity = JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5);
  std::string topicRoot(MQTTTOPIC);
  DynamicJsonDocument doc(capacity);
  doc["timestamp"] = time(NULL);
  doc["windowID"] = WSID;
  doc["fromState"] = fromState;
  doc["toState"] = toState;
  JsonObject description = doc.createNestedObject("description");
  JsonObject description_en = description.createNestedObject("en");
  JsonObject description_en_properties = description_en.createNestedObject("properties");
  description_en_properties["fromState"] = (*translations["en"].states)[fromState].c_str();
  description_en_properties["toState"] = (*translations["en"].states)[toState].c_str();
  JsonObject description_en_position = description_en.createNestedObject("position");
  description_en_position["window"] = (*translations["en"].directions)[WSPOSD].c_str();
  description_en_position["floor"] = WSPOSFL;
  description_en_position["room"] = WSPOSRM;
  size_t mSize = measureJson(doc) + 1;
  char *message = (char *) malloc(mSize);
  if (message == NULL) {
    char m[50];
    snprintf(m, 50,"{\"timestamp\": \"%ld\",\"windowID\": \"%s\",\"error\": \"E001\"}", (long) time(NULL), WSID);
    client.publish("error", m, true);
  } else {
    serializeJson(doc, message, mSize);
    topicRoot.append(topic);
    client.beginPublish(topicRoot.c_str(), strlen(message), true);
    client.print(message);
    client.endPublish();
  }
  free(message);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  initOTA();
  initMQTT(&client);

  // ESP lightsleep
  gpio_init();
  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);

  // WindowSensor
  pinMode(tiltedPin, INPUT);
  pinMode(closedPin, INPUT);
}

void loop() {
  lightSleepWakeup();
  //ArduinoOTA.handle();
  Serial.println("Actual State: ");
  Serial.println(state);
  if (state == 0) {
    gpio_pin_wakeup_enable(GPIO_ID_PIN(closedPin), GPIO_PIN_INTR_LOLEVEL);
  }
  if (state == 0 && digitalRead(closedPin) == LOW) {
    Serial.println("Left Closed State!");
    delay(delayTime);
    if (digitalRead(tiltedPin) == HIGH) {
      Serial.println("Entered Tilted State!");
      gpio_pin_wakeup_enable(GPIO_ID_PIN(tiltedPin), GPIO_PIN_INTR_LOLEVEL);
      state = 2;
    } else {
      Serial.println("Entered Opened State!");
      gpio_pin_wakeup_enable(GPIO_ID_PIN(closedPin), GPIO_PIN_INTR_HILEVEL);
      state = 1;
    }
    reconnectMQTT(&client);
    //client.loop();
    //snprintf (msg, 50, "%s: s-%d", windowID, state);
    //client.publish("test", msg, true);
    publishStateUpdate("stateupdate", States::CLOSED_S, (state == 1) ? States::OPENED_S : States::TILTED_S);
  } else if (state == 2 && digitalRead(tiltedPin) == LOW) {
    Serial.println("Left Tilted State!");
    delay(delayTime);
    if (digitalRead(closedPin) == HIGH) {
      Serial.println("Entered Closed State from Tilted!");
      state = 0;
      reconnectMQTT(&client);
      snprintf (msg, 50, "%s: s-%d", windowID, state);
      client.publish("test", msg, true);
      gpio_pin_wakeup_enable(GPIO_ID_PIN(closedPin), GPIO_PIN_INTR_LOLEVEL);
    }
  } else if (state == 1 && digitalRead(closedPin) == HIGH) {
    Serial.println("Entered Closed State from Opened!");
    state = 0;
    reconnectMQTT(&client);
    snprintf (msg, 50, "%s: s-%d", windowID, state);
    client.publish("test", msg, true);
    gpio_pin_wakeup_enable(GPIO_ID_PIN(closedPin), GPIO_PIN_INTR_LOLEVEL);
  }
  lightSleep();
}
