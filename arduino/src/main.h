#pragma once

#define DEBUG 0             // set to 1 to enable debug prints


// pin definitions
// VERIFY if pins labelet witn INTERRUP are interrupt-capable! 
// Only digital 2 and 3 are interrupt capable on Arduino Uno R4 boards
#define DHT22PIN 7                  // what pin we're connected to
#define TMP36PIN A0                 // TMP36 connected to analog pin A0
#define MUONCOINCIDENCEPIN 2        // muon detector connected to digital pin 2 INTERRUPT
#define TEMPMINUTES_INTERVAL 5      // interval in minutes for temp sensor readings
#define CSPIN 4                     // SD card chip select pin
#define RTC_INTERRUPTPIN 3          // RTC interrupt pin INTERRUPT
#define SIPM01PIN A1                // SiPM sensor 01 connected to analog pin A1
#define SIPM02PIN A2                // SiPM sensor 02 connected to analog pin A2
#define SIPM03PIN A3                // SiPM sensor 03 connected to analog pin A3

#define REALVOLTAGE 4.53             // real voltage for analog readings


#define MAX_ATTEMPTS_MQTT_CONNECTION 8                          // max attempts to connect to MQTT broker