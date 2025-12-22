#pragma once
#include "main.h"

#include <Arduino.h>
#include <MQTTClient.h>
#include <WiFiS3.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <RTClib.h>

#include "RTC.h" // TODO: add RTC sync from example project

#include "arduino_secrets.h"

#define MQTT_LOG_ENABLED 1

class Publisher_mqtt{
    public:
        Publisher_mqtt(char* publish, char* subscribe);
        ~Publisher_mqtt();
        bool begin();

        String getNTPTimestamp();

        String getRTCTimestamp();
        bool isRTCirqFlagSetMuon();
        bool isRTCirqFlagSetTemp();
        bool isRTCirqFlagSetDaily();
        bool clearRTCirqFlagMuon();
        bool clearRTCirqFlagTemp();
        bool clearRTCirqFlagDaily();
        String getDate();
        bool sync();

        bool send(StaticJsonDocument<200> message);
        void mqttLoop();
        // bool isNewMessageReceived();
        // ArduinoJson::StaticJsonDocument<200> getIncomingMessage();
        // bool executeIncomingMessage();

    protected:
};

//TODO: remove logic for handling MQTT received messages
// void messageHandler(String& topic, String& payload); // DELETE THIS LATERw