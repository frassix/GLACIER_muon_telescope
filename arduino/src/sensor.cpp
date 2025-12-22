#include "sensor.h"

Sensor::Sensor(uint8_t id, uint8_t pinNumber, String model, bool analog)
    : _id(id), _pin(pinNumber), _model(model), _analog(analog), _initialized(false) {}

bool Sensor::begin() {
    pinMode(_pin, INPUT); // initialize the pin as input
    delay(10); // small delay for pin to stabilize

    int value = 0;

    if (_analog) {
        value = analogRead(_pin); // test read analog pin
        #if DEBUG == 1
        Serial.println("sensor: begin() analog read value on pin " + String(_pin) + ": " + String(value));
        #endif
        if (value < 0 || value > 1023) {
            #if DEBUG == 1
            Serial.println("sensor: begin() analog read failed on pin " + String(_pin));
            #endif
            return false; // invalid analog read
        }
    } else {
        value = digitalRead(_pin); // test read digital pin
        #if DEBUG == 1
        Serial.println("sensor: begin() digital read value on pin " + String(_pin) + ": " + String(value));
        #endif
        if (value != LOW && value != HIGH) {
            #if DEBUG == 1
            Serial.println("sensor: begin() digital read failed on pin " + String(_pin));
            #endif
            return false; // invalid digital read
        }
    }
    _initialized = true;
    #if DEBUG == 1
    Serial.println("sensor: begin() successful on pin " + String(_pin));
    #endif
    return true;
}