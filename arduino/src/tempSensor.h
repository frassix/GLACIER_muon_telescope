#pragma once
#include "main.h"

#include <Arduino.h>
#include "sensor.h"

const float AREF_VOLTAGE = 1.1; // Real reference voltage for analog readings

class tempSensor : public Sensor {
    protected:
        float _temperatureC = 0.0;

    public:
        tempSensor(uint8_t id, uint8_t pinNumber, String model, bool analog = true);

        float readTemperatureC();
};