#pragma once
#include <Arduino.h>
#include "main.h"

class Sensor {
    public:
        Sensor(uint8_t id, uint8_t pinNumber, String model, bool analog = true);
        virtual ~Sensor() = default;

        bool begin();

        uint8_t getId() const { return _id; }
        int getPin() { return _pin; }
        bool isAnalog() const { return _analog; }
        bool isDigital() const { return !_analog; }

        String getModel() const { return _model; }
 
 
    protected:
        uint8_t _id = 0;
        uint8_t _pin = -1;
        bool _analog = false;
        String _model = "";
        bool _initialized = false;
};