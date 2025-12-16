#pragma once
#include "main.h"

#include <SPI.h>
#include <SD.h>

#include <publisher_mqtt.h>

class SDCard {
    public:
        SDCard();
        ~SDCard();
        bool begin(uint8_t csPin);
        bool write(String filename, String data);
        bool readSend(String filename, Publisher_mqtt& publisher);

    protected:

    // set up variables using the SD utility library functions:
    Sd2Card card;
    SdVolume volume;
    SdFile root;
};