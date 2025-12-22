#include "tempSensor.h"

tempSensor::tempSensor(uint8_t id, uint8_t pinNumber, String model, bool analog)
    : Sensor(id, pinNumber, model, analog) {}

float tempSensor::readTemperatureC() {
    // read temperature from sensor
    if(isAnalog()) {
        int analogValue = analogRead(getPin());                         // read raw value from sensor
        float voltage = analogValue * (REALVOLTAGE*1000 / 1024.0);      // Convert the reading into voltage:
        _temperatureC = (voltage - 500) / 10;                           // Convert the voltage into the temperature in Celsius:
    }

    return _temperatureC;
}