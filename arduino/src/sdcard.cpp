#include "sdcard.h"

SDCard::SDCard() {};
SDCard::~SDCard() {};

bool SDCard::begin(uint8_t csPin) {

    if (!SD.begin(csPin)) {
        #if DEBUG == 1
        Serial.println("sdcard:  initialization failed!");
        #endif
        return false;
    }

    if (!SD.exists("syslog.txt")) {
        File file = SD.open("syslog.txt", FILE_WRITE);

        if (file) {
            file.println("sdcard: Log File Created.");
            file.close();
            #if DEBUG == 1
            Serial.println("sdcard: syslog.txt file created. New card initialized.");
            #endif
        } else {
            #if DEBUG == 1
            Serial.println("Error creating syslog.txt file.");
            #endif
            return false;
        }
    }

    #if DEBUG == 1
    Serial.println("sdcard: initialized.");
    #endif

    return true;
}

bool SDCard::write(String filename, String data) {
    File file = SD.open(filename.c_str(), FILE_WRITE); //open file in WRITE mode

    if(!file) {
        #if DEBUG == 1
        Serial.println("sdcard: error opening file for writing: " + filename);
        #endif
        return false;
    }

    file.seek(file.size());     // move to end to append

    file.println(data); //write data with newline
    file.close(); //close file
    
    #if DEBUG == 1
    Serial.println("sdcard: data written to file: " + filename);
    #endif

    return true;
}

bool SDCard::readSend(String filename, Publisher_mqtt& publisher) {
    File file = SD.open(filename.c_str(), FILE_READ); //open file in READ mode
    ArduinoJson::StaticJsonDocument<200> data;

    if (!file) {
        #if DEBUG == 1
        Serial.println("sdcard: error opening file for reading: " + filename);
        #endif
        return false;
    }

    while (file.available()) {
        DeserializationError error = deserializeJson(data, file.readStringUntil('\n')); //read line by line
        if (data.isNull()) {
            // avoid cases of empty lines
            #if DEBUG == 1
            Serial.println("sdcard: empty line encountered in file: " + filename + ", skipping.");
            #endif
            // continue; //skip to next line
        }
        publisher.send(data); //send data via MQTT
        #if DEBUG == 1
        Serial.println("sdcard: data sent from file: " + filename);
        Serial.println(data.as<String>());
        #endif
        data.clear(); //clear document for next read
    }

    file.close(); //close file

    #if DEBUG == 1
    Serial.println("sdcard: data read from file: " + filename);
    #endif

    return true;
} 