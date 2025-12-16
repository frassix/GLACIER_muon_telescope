// INCLUDE FILES HERE:
#pragma once
#include "main.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include "sensor.h" 
#include "tempSensor.h"
#include "muonSensor.h"
#include "publisher_mqtt.h"
#include "sdcard.h"


// constants here:
#define DHTTYPE DHT22         // DHT 22 (AM2302)

// FUNCTION DECLARATIONS HERE:
int freeRam(); // DELETE 
void display_freeram(); // DELETE
static bool retryInit(const char* name, std::function<bool()> initFn, uint8_t maxRetries = 3, uint16_t retryDelayMs = 2000);

//
// variables/objects declarations here:
//
// TMP36 sensor object
tempSensor tmp36(1, TMP36PIN, "tmp36", true);
tempSensor sipm01(3, SIPM01PIN, "sipm01", true);
tempSensor sipm02(4, SIPM02PIN, "sipm02", true);
tempSensor sipm03(5, SIPM03PIN, "sipm03", true);
// DHT22 sensor object
DHT_Unified dht22(DHT22PIN, DHTTYPE);
sensors_event_t event;
// MQTT publisher object
Publisher_mqtt publisher("glacier-sasso/send","glacier-sasso/receive");
//muon sensor object
muonSensor muon(2, MUONCOINCIDENCEPIN, "muon-detector-v1", false);
// SD card object
SDCard sd;
// JSON document for messages
ArduinoJson::StaticJsonDocument<200> message;
ArduinoJson::StaticJsonDocument<200> logMessage; // larger doc for daily summary
//
String log_filename = "syslog.txt"; // MAX 8 CHARACTERS FOR FILENAME, MAX 4 chars for extension including the dot
String data_filename = ""; // MAX 8 CHARACTERS FOR FILENAME
String data_extension = ".txt"; // MAX 4 CHARACTERS FOR EXTENSION INCLUDING THE DOT
String previous_data_filename = ""; // to store previous day's data filename
bool previousDaySent = true; // flag to track if daily data has been sent

//
// SETUP AND LOOP FUNCTIONS HERE:
//

// ###########
// ## SETUP ##
// ###########
void setup() {

    bool status_tmp36 = false;
    bool status_sipm01 = false;
    bool status_sipm02 = false;
    bool status_sipm03 = false;
    bool status_dht = false;
    bool status_muon = false;
    bool status_publisher = false;
    bool status_sd = false;

    pinMode(LED_BUILTIN, OUTPUT); // initialize the built-in LED pin as an output
    digitalWrite(LED_BUILTIN, HIGH);  // LED on

    #if DEBUG == 1
    // initialize serial communication at 115200 bits per second
    Serial.begin(115200);
    delay(100); // wait for serial to initialize
    Serial.println("setup: debug mode is ON");
    Serial.println("setup: Serial initialized at 115200 bps");
    #endif
    logMessage["type"] = "log";
    logMessage["message"] = "Setup started";
    


    // hardware init with retries
    // Use object-level begin() checks but add a retry layer here
    status_tmp36 = retryInit("tmp36", [&]() { return tmp36.begin(); });
    logMessage["status_tmp36"] = status_tmp36;
    status_sipm01 = retryInit("sipm01", [&]() { return sipm01.begin(); });
    logMessage["status_sipm01"] = status_sipm01;
    status_sipm02 = retryInit("sipm02", [&]() { return sipm02.begin(); });
    logMessage["status_sipm02"] = status_sipm02;
    status_sipm03 = retryInit("sipm03", [&]() { return sipm03.begin(); });
    logMessage["status_sipm03"] = status_sipm03;

    // DHT needs a quick sanity read after begin()
    status_dht = retryInit("dht22", [&]() {
        dht22.begin();
        dht22.temperature().getEvent(&event);
        return !isnan(event.temperature);
    });
    logMessage["status_dht22"] = status_dht;

    status_muon = retryInit("muon", [&]() { return muon.begin(); });
    logMessage["status_muon"] = status_muon;

    status_sd = retryInit("sd", [&]() { return sd.begin(CSPIN); });
    logMessage["status_sd"] = status_sd;

    status_publisher = retryInit("publisher", [&]() { return publisher.begin(); });
    logMessage["status_publisher"] = status_publisher;

    interrupts();  // enable interrupts from sensors

    delay(100);
 
    data_filename = publisher.getDate() + data_extension;

    // check overall status
    if(!(status_tmp36 && status_dht && status_muon && status_publisher && status_sd)) {
        #if DEBUG == 1
        Serial.println("setup: One or more components failed to initialize.");
        #endif
        logMessage["setup_status"] = "failure";
    } else {
        #if DEBUG == 1
        Serial.println("setup: All components initialized successfully.");
        #endif
        logMessage["setup_status"] = "success";
    }
    
    delay(100);
    publisher.send(logMessage); // send initialization status message
    sd.write(log_filename, logMessage.as<String>()); // log initialization status to SD card
    logMessage.clear();
    message.clear();
    delay(100);

    // indicate done
    #if DEBUG == 1  
    Serial.println("setup: Setup done.");
    #endif

    sd.readSend(log_filename, publisher); // send log file at startup

    digitalWrite(LED_BUILTIN, LOW);  // LED off
    delay(100);
}

// ##########
// ## LOOP ##
// ##########
void loop() {
    delay(100);  // small delay to avoid busy wait

    // sync RTC DS1307 with NTP if needed
    // if RTC crashed, try reinitialize and sync
    if (publisher.getRTCTimestamp() == "") {
        #if DEBUG == 1 
        Serial.println("loop: RTC DS1307 crashed, trying to reinitialize and sync with NTP.");
        #endif
        if(publisher.sync()) logMessage["rtc_reinitialized"] = true;
        else logMessage["rtc_reinitialized"] = false;
        sd.write(log_filename, logMessage.as<String>()); // log RTC reinit status to SD card
        logMessage.clear();
    }
    
    if(publisher.isRTCirqFlagSetMuon()) {
        publisher.mqttLoop(); // ensure MQTT client is running

        #if DEBUG == 1 
        Serial.println("loop: RTC IRQ flag is set, sending muon sensor payload");
        #endif

        String timestamp = publisher.getRTCTimestamp();

        message["type"] = "muon";

        noInterrupts();  // disable interrupts to read muon count safely
        //CRITICAL SECTION START
            message["timestamp"] = timestamp;
            message["eventCount"] = muon.getMuonCount(); // get muon count
            muon.muonCountReset(); // reset muon sensor data
        //CRITICAL SECTION END
        interrupts();  // re-enable interrupts

        publisher.clearRTCirqFlagMuon(); // clear RTC IRQ flag

        publisher.send(message);
        sd.write(data_filename, message.as<String>()); // log muon data to SD card
        message.clear();
    }

    if(publisher.isRTCirqFlagSetTemp()) {
        
        #if DEBUG == 1 
        Serial.println("loop: RTC IRQ flag is set, sending temperature sensor payload");
        #endif

        String timestamp = publisher.getRTCTimestamp();

        // read temperature from sensors
        dht22.temperature().getEvent(&event);
        float dht22TempC = event.temperature;
        dht22.humidity().getEvent(&event);
        float dht22HumidityPct = event.relative_humidity;

        // send temperature sensor payload
        message["type"] = "temp-hum";
        message["timestamp"] = timestamp;
        message["tmp36TempC"] = tmp36.readTemperatureC();
        message["sipm01TempC"] = sipm01.readTemperatureC();
        message["sipm02TempC"] = sipm02.readTemperatureC();
        message["sipm03TempC"] = sipm03.readTemperatureC();
        message["dht22TempC"] = dht22TempC;
        message["dht22HumidityPct"] = dht22HumidityPct;

        publisher.clearRTCirqFlagTemp(); // clear RTC IRQ flag

        publisher.send(message);
        sd.write(data_filename, message.as<String>()); // log temp data to SD card
        message.clear();
    }

    if(publisher.isRTCirqFlagSetDaily()) {
        String sd_data = "";

        logMessage["type"] = "log";
        logMessage["message"] = "Daily summary";
        
        // sync RTC DS1307 with NTP if needed
        // if RTC crashed, try reinitialize and sync
        if (publisher.sync()) {
            #if DEBUG == 1 
            Serial.println("loop: RTC DS1307 synced with NTP successfully.");
            #endif
            logMessage["rtc_resynced"] = true;

        } else {
            #if DEBUG == 1 
            Serial.println("loop: RTC DS1307 failed to sync with NTP.");
            #endif
            logMessage["rtc_resynced"] = false;
        }

        logMessage["timestamp"] = publisher.getRTCTimestamp();

        #if DEBUG == 1 
        Serial.println("loop: Daily RTC IRQ flag is set, performing daily tasks");
        #endif    

        #if DEBUG == 1 
        Serial.println("loop: Muon log data:");
        Serial.println(message.as<String>());
        #endif
        

        //check if previous day data was sent and retry if not
        logMessage["previous_day_data_sent"] = true;
        if(!previousDaySent) {
            #if DEBUG == 1 
            Serial.println("loop: data from yesterday was not sent. Retrying.");
            #endif

            if(!sd.readSend(previous_data_filename, publisher)) { // retry sending previous day's data
                #if DEBUG == 1 
                Serial.println("loop: Failed to send previous day's data on retry.");
                #endif

                logMessage["previous_day_data_sent"] = false;
            }
        }

        if(sd.readSend(data_filename, publisher)) { // send muon and temp log data
            #if DEBUG == 1 
            Serial.println("loop: Daily data sent successfully.");
            #endif

            logMessage["daily_data_sent"] = true;
            previousDaySent = true;
        } else {
            #if DEBUG == 1 
            Serial.println("loop: Failed to send daily data.");
            #endif

            logMessage["daily_data_sent"] = false;
            previousDaySent = false;
        }

        previous_data_filename = data_filename; // store previous day's data filename
        data_filename = publisher.getDate() + data_extension; // update data filename for new day
        sd.write(data_filename, data_filename); // clear data file for new day

        publisher.clearRTCirqFlagDaily(); // clear daily RTC IRQ flag

        #if DEBUG == 1 
        Serial.println("loop: Daily tasks done. time: " + publisher.getRTCTimestamp());
        display_freeram();
        #endif

        sd.write(log_filename, logMessage.as<String>()); // log daily summary to SD card
        publisher.send(logMessage); // send daily summary
        logMessage.clear();
    }
}

// small retry wrapper that calls the object's begin or any initialization function
static bool retryInit(const char* name, std::function<bool()> initFn, uint8_t maxRetries, uint16_t retryDelayMs) {
    for (uint8_t i = 0; i < maxRetries; ++i) {
        if (initFn()) {
            #if DEBUG == 1
            Serial.print(name); Serial.println(": initialized");
            #endif
            return true;
        }
        #if DEBUG == 1
        Serial.print(name); Serial.println(": init failed, retrying...");
        #endif
        delay(retryDelayMs);
    }
    #if DEBUG == 1
    Serial.print(name); Serial.println(": failed to initialize");
    #endif
    return false;
}

extern "C" char* sbrk(int incr);
void display_freeram(){
  Serial.print(F("- SRAM left: "));
  Serial.println(freeRam());
}
int freeRam() {
  char top;
  return &top - reinterpret_cast<char*>(sbrk(0));
}
