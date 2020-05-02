#ifndef MQTT_H
#define MQTT_H

extern WiFiClient espClient;
extern PubSubClient client;

void initMQTT();
void callback(char* topic, byte* payload, unsigned int length);
void reconnectMQTT();
void publishStateUpdate(std::string topic, States fromState, States toState);

#endif
