#pragma once
#include "main.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include "sensor.h"

class muonSensor : public Sensor {
    protected:
        struct muon_str {
            String timestamp;
            muon_str* next;
        };

        struct muonLast_str{
            muon_str* last;
        };

        muon_str* muonHead = nullptr;
        muon_str* muonLast = nullptr;

    public:
        muonSensor(uint8_t id, uint8_t pinNumber, String model, bool analog = true);

        bool begin();
        bool muonCountReset();
        uint16_t getMuonCount();

};