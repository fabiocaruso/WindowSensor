#ifndef MQTT_H
#define MQTT_H

//extern PubSubClient client;
extern char msg[50];

void initMQTT(PubSubClient *client);
void callback(char* topic, byte* payload, unsigned int length);
void reconnectMQTT(PubSubClient *client);

#endif
