#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include "config.h"
#include "ota.h"
#include "mqtt.h"

const char windowID[] = WSID;
States state = States::CLOSED_S;

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

void setup() {
  EEPROM.begin(4);
  Serial.begin(115200);
  Serial.println("Booting..");
  initMQTT();

  // ESP lightsleep
  gpio_init();
  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);

  // WindowSensor
  pinMode(TILTEDPIN, INPUT);
  pinMode(CLOSEDPIN, INPUT);
}

void loop() {
  byte fwu = EEPROM.read(FWUFLAG);
  if (fwu == 218) {
    Serial.println("Update mode!");
    initOTA();
    EEPROM.write(FWUFLAG, 0);
    EEPROM.end();
    while (true) {
      ArduinoOTA.handle();
      delay(100);
    }
  } else {
    Serial.println("Normal mode!");
    lightSleepWakeup();
    Serial.println("Actual State: ");
    Serial.println(state);
    if (state == States::CLOSED_S && digitalRead(CLOSEDPIN) == LOW) {
      Serial.println("Left Closed State!");
      delay(DELAYTIME);
      if (digitalRead(TILTEDPIN) == HIGH) {
        Serial.println("Entered Tilted State!");
        gpio_pin_wakeup_enable(GPIO_ID_PIN(TILTEDPIN), GPIO_PIN_INTR_LOLEVEL);
        state = States::TILTED_S;
      } else {
        Serial.println("Entered Opened State!");
        gpio_pin_wakeup_enable(GPIO_ID_PIN(CLOSEDPIN), GPIO_PIN_INTR_HILEVEL);
        state = States::OPENED_S;
      }
      reconnectMQTT();
      //snprintf (msg, 50, "%s: s-%d", windowID, state);
      //client.publish("test", msg, true);
      //publishStateUpdate("stateupdate", States::CLOSED_S, state);
    } else if (state == States::TILTED_S && digitalRead(TILTEDPIN) == LOW) {
      Serial.println("Left Tilted State!");
      delay(DELAYTIME);
      if (digitalRead(CLOSEDPIN) == HIGH) {
        Serial.println("Entered Closed State from Tilted!");
        state = States::CLOSED_S;
        reconnectMQTT();
        publishStateUpdate("stateupdate", States::TILTED_S, state);
        gpio_pin_wakeup_enable(GPIO_ID_PIN(CLOSEDPIN), GPIO_PIN_INTR_LOLEVEL);
      }
    } else if (state == States::OPENED_S && digitalRead(CLOSEDPIN) == HIGH) {
      Serial.println("Entered Closed State from Opened!");
      state = States::CLOSED_S;
      reconnectMQTT();
      publishStateUpdate("stateupdate", States::OPENED_S, state);
      gpio_pin_wakeup_enable(GPIO_ID_PIN(CLOSEDPIN), GPIO_PIN_INTR_LOLEVEL);
    } else {
      gpio_pin_wakeup_enable(GPIO_ID_PIN(CLOSEDPIN), GPIO_PIN_INTR_LOLEVEL);
    }
    delay(1000);
    lightSleep();
  }
}
