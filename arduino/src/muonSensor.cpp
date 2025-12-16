#include "muonSensor.h"

volatile uint32_t _muonCount = 0; 

void muonISR();

muonSensor::muonSensor(uint8_t id, uint8_t pinNumber, String model, bool analog)
    : Sensor(id, pinNumber, model, analog) {
    }

bool muonSensor::begin() {
    // muon sensor init 
    Sensor::begin();
    attachInterrupt(digitalPinToInterrupt(_pin), muonISR, RISING);
    return true;
}

// muon sensor ISR
void muonISR() {
    _muonCount++;
}

bool muonSensor::muonCountReset() {
    // Reset the muon sensor data
    _muonCount = 0;
    return true;
}

uint16_t muonSensor::getMuonCount() {
    return _muonCount;
}