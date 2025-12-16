#include "publisher_mqtt.h"

// WiFi credentials in main.h
char ssid[] = SECRET_SSID;  // your wifiClient SSID (name)
char pass[] = SECRET_PASS;  // your wifiClient password (use for WPA, or use as key for WEP)

// WiFiUDP ntpUDP;
// NTPClient timeClient(ntpUDP);
WiFiClient wifiClient;

// MQTT server details/credentials in main.h
MQTTClient mqtt = MQTTClient(256);

//DS1307 declaration
RTC_DS1307 rtc_ds1307;
Ds1307SqwPinMode modes[] = { DS1307_OFF, DS1307_ON, DS1307_SquareWave1HZ, DS1307_SquareWave4kHz, DS1307_SquareWave8kHz, DS1307_SquareWave32kHz};

// local functions declarations
bool wifiBegin();
bool waitForNTP(unsigned long timeoutMs = 60000UL); // default timeout 60 seconds

bool RTC1307begin();
void ds1307PeriodicCallback();
void set_mode(Ds1307SqwPinMode mode = DS1307_SquareWave1HZ);
void print_mode();

bool ds1307Sync();
bool syncRTCirq();

bool connectToMQTT();
void messageHandler(String& topic, String& payload);
bool executeIncomingMessage();

// RTC variables (IRQ)
volatile bool RTCirqFlag_muon = false;
volatile bool RTCirqFlag_temp = false;
volatile bool RTCirqFlag_daily = false;
volatile uint8_t RTCirqCounter_temp = 0;
volatile uint8_t seconds = 0;
volatile uint8_t minutes = 0;
volatile uint8_t hours = 0;

char publishTopic[128] = "";
char subscribeTopic[128];

bool newMessageReceived = false;
ArduinoJson::StaticJsonDocument<200> incomingMessage;


//////////////////////////////////
// Publisher_mqtt class section //
//////////////////////////////////

// creator/distructor functions definitions
Publisher_mqtt::Publisher_mqtt(char* publish, char* subscribe) {
    strncpy(publishTopic, publish, sizeof(publishTopic) - 1);
    publishTopic[sizeof(publishTopic)-1] = '\0'; // ensure null termination
    strncpy(subscribeTopic, subscribe, sizeof(subscribeTopic) - 1);
    subscribeTopic[sizeof(subscribeTopic)-1] = '\0'; // ensure null termination
};
Publisher_mqtt::~Publisher_mqtt() {};

// begin function definition
bool Publisher_mqtt::begin() {
    ArduinoJson::StaticJsonDocument<200> logMessage;
    bool flag = true;

    delay(100);

    if (wifiBegin()) {
        #if DEBUG == 1
        Serial.println("mqtt: WiFi connected successfully.");
        #endif
        delay(100);
    } else {
        #if DEBUG == 1
        Serial.println("mqtt: WiFi connection failed.");
        #endif
        flag = false;
    }

    delay(100);

    if(!waitForNTP()) { // wait for NTP to be available
        #if DEBUG == 1
        Serial.println("mqtt: NTP time not available, proceeding without NTP sync.");
        #endif
        flag = false;
    }

    delay(100);

    if(!RTC1307begin()) { // initialize RTC DS1307
        #if DEBUG == 1
        Serial.println("mqtt: RTC DS1307 initialization failed.");
        #endif
        flag = false;
    }
    
    delay(100);

    // RTCbegin(); // initialize internal RTC
    // delay(100);

    if(!connectToMQTT()) { // initialize MQTT connection
        #if DEBUG == 1
        Serial.println("mqtt: initial MQTT connection failed.");
        #endif
        flag = false;
    }

    delay(100);

    logMessage["type"] = "log";
    logMessage["mqtt"] = "Wifi and MQTT initialized";
    logMessage["NTP_timestamp"] = getNTPTimestamp();
    logMessage["RTC_timestamp"] = getRTCTimestamp();
    send(logMessage);
    logMessage.clear();

    if(!syncRTCirq()) { // sync RTC IRQ flags
        #if DEBUG == 1
        Serial.println("mqtt: RTC IRQ flags sync failed.");
        #endif
        flag = false;
    }

    return flag;
}

//////////////////
// WiFi section //
//////////////////

// connect to WiFi wifiClient
bool wifiBegin() {
    // attempt to connect to Wifi wifiClient:

    #if DEBUG == 1
    Serial.print("mqtt: attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    #endif

    const unsigned long WIFI_TIMEOUT_MS = 20000; // 20 seconds timeout
    unsigned long start = millis();

    WiFi.begin(ssid, pass);
    delay(100);

    while(millis() - start < WIFI_TIMEOUT_MS) {
        if (WiFi.status() == WL_CONNECTED) {
            delay(100);
            #if DEBUG == 1
            Serial.println("mqtt: connected to WiFi wifiClient!");
            Serial.print("mqtt: SSID: " + String(WiFi.SSID()));
            Serial.print("mqtt: GATEWAY IP address: ");
            Serial.println(WiFi.gatewayIP());
            Serial.print("mqtt: LOCAL IP address: ");
            Serial.println(WiFi.localIP());
            Serial.print("mqtt: Signal strength (RSSI): ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
            Serial.print("mqtt: gateway ping: ");
            Serial.println(WiFi.ping(WiFi.gatewayIP()));
            Serial.print("mqtt: ping mqtt server: ");
            Serial.println(WiFi.ping(MQTT_BROKER_ADRRESS));
            #endif

            #if DEBUG == 0
            WiFi.ping(WiFi.gatewayIP());
            #endif

            return true; // success
        }
        // failed, retry
        #if DEBUG == 1
        Serial.print(".");
        #endif
        delay(1000);
    }

    #if DEBUG == 1
    Serial.println("mqtt: could not connect to wifi (timeout)");
    #endif

    return false;
}

// get current timestamp as string FROM NTP
// formatted as hh:mm:ss
String Publisher_mqtt::getNTPTimestamp() {
    waitForNTP(50); // wait up to 5 seconds for NTP
    return DateTime(WiFi.getTime()).timestamp();
}

// Wait for NTP (WiFi.getTime()) to return a valid epoch time
bool waitForNTP(unsigned long timeoutMs) {
    const unsigned long MIN_VALID_EPOCH = 1763078400UL;
    const unsigned long MAX_VALID_EPOCH = 3340915200UL;
    unsigned long start = millis();
    unsigned long currentTime = 0UL;

    while (millis() - start < timeoutMs) {
        currentTime = WiFi.getTime();
        if (currentTime != 0UL && currentTime >= MIN_VALID_EPOCH && currentTime <= MAX_VALID_EPOCH) {  // check for extremes, errors
            #if DEBUG == 1
            Serial.println("mqtt: NTP time obtained: " + String(currentTime));
            #endif

            return true;
        }
        #if DEBUG == 1
        Serial.println("mqtt: waiting for NTP time... ");
        #endif
        delay(10);
    }

    return false;
}


////////////////////////
// RTC DS1307 section //
////////////////////////

// RTC DS1307 initialization and sync
bool RTC1307begin() {

    #if DEBUG == 1
    Serial.println("mqtt: Initializing RTC DS1307...");
    #endif

    #if DEBUG == 1
    Serial.println("mqtt: Initializing I2C bus...");
    #endif

    Wire.begin(); // initialize I2C
    Wire1.begin(); // initialize I2C1

    #if DEBUG == 1
    Serial.println("mqtt: I2C bus initialized.");
    #endif

    delay(500);

    if(!rtc_ds1307.begin()){
        #if DEBUG == 1
        Serial.println("mqtt: Couldn't find RTC DS1307. Trying again...");
        #endif
        delay(1000);
        if(!rtc_ds1307.begin()){
            #if DEBUG == 1
            Serial.println("mqtt: RTC DS1307 initialization failed twice.");
            #endif
            return false;
        }
    } else {
        #if DEBUG == 1
        Serial.println("mqtt: RTC DS1307 initialized successfully. Stored time is: " + String(rtc_ds1307.now().unixtime()));
        #endif

        #if DEBUG == 0
        rtc_ds1307.now();
        #endif
    }

    if(!ds1307Sync()) { // sync RTC DS1307 time
        #if DEBUG == 1
        Serial.println("mqtt: RTC DS1307 time sync failed.");
        #endif
        return false;
    }

    set_mode(DS1307_SquareWave1HZ); // set DS1307 SQW pin to 1Hz
    delay(100);

    #if DEBUG == 1
    print_mode();               // check DS1307 SQW pin mode
    if(rtc_ds1307.readSqwPinMode() != DS1307_SquareWave1HZ) {
        Serial.println("mqtt: RTC DS1307 SQW pin mode not set correctly!");
    } else {
        Serial.println("mqtt: RTC DS1307 SQW pin mode set correctly to 1Hz.");
    }
    #endif

    #if DEBUG == 0
    rtc_ds1307.readSqwPinMode();
    #endif

    // Ensure interrupt pin configured before attaching ISR
    pinMode(RTC_INTERRUPTPIN, INPUT);
    // DS1307 irq init
    attachInterrupt(digitalPinToInterrupt(RTC_INTERRUPTPIN), ds1307PeriodicCallback, RISING);
    
    #if DEBUG == 1
    Serial.println("mqtt: RTC DS1307 current date/time: " + (rtc_ds1307.now()).timestamp());
    #endif

    return true;
}

bool ds1307Sync() {
    // If the DS1307 isn't running, try to set it from NTP; otherwise fall back to
    // compile time. If it is running, we may still update it from NTP if NTP is
    // valid and differs by more than a small threshold.
    const unsigned long MIN_VALID_EPOCH = 1763078400UL;
    const unsigned long MAX_VALID_EPOCH = 3340915200UL;
    const unsigned long WRITE_THRESHOLD_SEC = 5UL; // only write DS1307 if drift > threshold

    DateTime ntp = DateTime(WiFi.getTime());
    uint32_t unixtime_ntp = ntp.unixtime();
    DateTime rtc = rtc_ds1307.now();
    uint32_t unixtime_rtc = rtc.unixtime(); // get current DS1307 epoch
    unsigned long drift = 0;

    delay(100);

    #if DEBUG == 1
    Serial.println("mqtt: Current RTC DS1307 epoch: " + String(unixtime_rtc));
    Serial.println("mqtt: Current NTP epoch: " + String(unixtime_ntp));
    #endif

    if(!rtc_ds1307.isrunning()) {

        #if DEBUG == 1
        Serial.println("mqtt: RTC DS1307 is NOT running, attempting to start and set time from NTP! Else, defaulting to compile time.");
        #endif

        rtc_ds1307.begin(); // try to restart DS1307

        unixtime_ntp = ntp.unixtime();
        unixtime_rtc = rtc.unixtime(); // get current DS1307 epoch

        if (unixtime_ntp != 0UL && unixtime_ntp >= MIN_VALID_EPOCH && unixtime_ntp <= MAX_VALID_EPOCH) {
            rtc_ds1307.adjust(DateTime(unixtime_ntp)); // set RTC DS1307 time from NTP
            
            #if DEBUG == 1
            Serial.println("mqtt: DS1307 set from NTP epoch: " + String(rtc.unixtime()));
            #endif

            #if DEBUG == 0
            rtc_ds1307.now();
            #endif
        } else {
            #if DEBUG == 1
            Serial.println("mqtt: NTP time is invalid, cannot set RTC DS1307 time from NTP. Setting from compile time.");
            #endif
            rtc_ds1307.adjust(DateTime(F(__DATE__), F(__TIME__))); // set RTC DS1307 time from compile time
        }
    } else {
        // DS12307 is running. Updating from NTP only if valid and drift > threshold

        unixtime_ntp = ntp.unixtime();
        unixtime_rtc = rtc.unixtime(); // get current DS1307 epoch

        #if DEBUG == 1
        Serial.println("mqtt: RTC DS1307 is running. Current DS1307 epoch: " + String(unixtime_rtc));
        Serial.println("mqtt: Current NTP epoch: " + String(unixtime_ntp));
        #endif

        if (ntp.unixtime() != 0UL && unixtime_ntp >= MIN_VALID_EPOCH && unixtime_ntp <= MAX_VALID_EPOCH) { // NTP time is valid
            drift = (unixtime_ntp > unixtime_rtc) ? (unixtime_ntp - unixtime_rtc) : (unixtime_rtc - unixtime_ntp); // calculate drift in seconds
            
            if (drift > WRITE_THRESHOLD_SEC) {
                rtc_ds1307.adjust(DateTime(unixtime_ntp)); // set RTC DS1307 time from NTP

                #if DEBUG == 1
                Serial.println("mqtt: DS1307 time updated from NTP epoch due to drift of " + String(drift) + " seconds.");
                #endif

            } else {
                #if DEBUG == 1
                Serial.println("mqtt: DS1307 time NOT updated from NTP epoch. Drift of " + String(drift) + " seconds is within threshold.");
                #endif
            }
        } else {
            #if DEBUG == 1
            Serial.println("mqtt: NTP time is invalid, not updating RTC DS1307 time from NTP.");
            #endif
        }
    }

    // check if RTC is running after sync
    if(!rtc_ds1307.isrunning()) {
        #if DEBUG == 1
        Serial.println("mqtt: RTC DS1307 not running after sync.");
        #endif
        return false;
    }

    return true;
}

bool Publisher_mqtt::sync() {
    #if DEBUG == 1
    Serial.println("mqtt: Syncing RTC DS1307...");
    #endif

    if (getRTCTimestamp() == String("") | rtc_ds1307.isrunning() == false) {
        #if DEBUG == 1
        Serial.println("mqtt: RTC crashed, try to reinitialize.");
        #endif
        RTC1307begin(); // try to reinitialize DS1307
    }

    if(ds1307Sync() && syncRTCirq()) {
        return true;
    }

    return false;
}

void set_mode(Ds1307SqwPinMode mode) {
    rtc_ds1307.writeSqwPinMode(mode);
}

// print RTC DS1307 SQW pin mode to Serial
void print_mode() {
  Ds1307SqwPinMode mode = rtc_ds1307.readSqwPinMode();

  Serial.print("mqtt: RTC DS1307 Sqw Pin Mode: ");
  switch(mode) {
  case DS1307_OFF:              Serial.println("OFF");       break;
  case DS1307_ON:               Serial.println("ON");        break;
  case DS1307_SquareWave1HZ:    Serial.println("1Hz");       break;
  case DS1307_SquareWave4kHz:   Serial.println("4.096kHz");  break;
  case DS1307_SquareWave8kHz:   Serial.println("8.192kHz");  break;
  case DS1307_SquareWave32kHz:  Serial.println("32.768kHz"); break;
  default:                      Serial.println("UNKNOWN");   break;
  }
}

// get current timestamp as string FROM RTC
String Publisher_mqtt::getRTCTimestamp() {

    ArduinoJson::StaticJsonDocument<200> logMessage;

    const unsigned long MIN_VALID_EPOCH = 1763078400UL;
    const unsigned long MAX_VALID_EPOCH = 3340915200UL;
    unsigned long currentTime = 0UL;

    DateTime dt = rtc_ds1307.now();

    if (!rtc_ds1307.isrunning()) {
        #if DEBUG == 1
        Serial.println("mqtt: RTC DS1307 not running, reinitializing.");
        #endif

        RTC1307begin(); // try to restart DS1307
        
        if (!rtc_ds1307.isrunning())
        {
            #if DEBUG == 1
            Serial.println("mqtt: RTC DS1307 still not running after reinitialization, cannot get timestamp.");
            #endif

            return String("");
        }

        sync(); // sync time and RTC IRQ values after reinitialization
        
        #if DEBUG == 1
        Serial.println("mqtt: RTC DS1307 running after reinitialization.");
        #endif
    }


    // Basic validation of components
    uint16_t y = dt.year();
    uint8_t mo = dt.month();
    uint8_t d = dt.day();
    uint8_t h = dt.hour();
    uint8_t mi = dt.minute();
    uint8_t s = dt.second();

    bool fields_ok = (y >= 2025 && y <= 2099) &&
                     (mo >= 1 && mo <= 12) &&
                     (d >= 1 && d <= 31) &&
                     (h <= 23) &&
                     (mi <= 59) &&
                     (s <= 59);

    currentTime = dt.unixtime();

    if (!fields_ok) {
        #if DEBUG == 1
        Serial.println("mqtt: RTC DS1307 returned invalid date/time fields - rejecting timestamp.");
        #endif

        return String("");
    }


    if (currentTime != 0UL && currentTime >= MIN_VALID_EPOCH && currentTime <= MAX_VALID_EPOCH) {  // check for extremes, errors
        #if DEBUG == 1
        Serial.println("mqtt: RTC time obtained: " + String(currentTime));
        #endif

        return dt.timestamp();

    } else {
        #if DEBUG == 1
        Serial.println("mqtt: RTC DS1307 time is invalid, cannot get timestamp. Unixtime: " + String(currentTime));
        #endif
    }
    
    return String("");
}

// get current date
String Publisher_mqtt::getDate() {

    String day = rtc_ds1307.now().day() >= 10 ? String(rtc_ds1307.now().day()) : "0" + String(rtc_ds1307.now().day());
    String month = rtc_ds1307.now().month() >= 10 ? String(rtc_ds1307.now().month()) : "0" + String(rtc_ds1307.now().month());

    if(!rtc_ds1307.isrunning()) {
        #if DEBUG == 1
        Serial.println("mqtt: RTC DS1307 not running, cannot get date.");
        #endif
        return String("");
    }

    String date = String(rtc_ds1307.now().year()) + month + day;
    return date;
}

// external RTC periodic callback(IRQ)
void ds1307PeriodicCallback()
{
    seconds++;
    if (seconds >= 60) { // every 60 seconds{
        RTCirqFlag_muon = true;
        minutes++;
        seconds = 0;
    }
    if(minutes >= 60) { // every 60 minutes
        minutes = 0;
        hours ++;
    }
    if (hours >= 24 && minutes == 0 && seconds == 0) { // every 24 hours
        hours = 0;
        RTCirqFlag_daily = true;
    }
    if (minutes%TEMPMINUTES_INTERVAL == 0 && seconds == 0) { // every TEMPMINUTES_INTERVAL minutes
        RTCirqFlag_temp = true;
    }
}

// check if RTC irq flag is set for muon readings
bool Publisher_mqtt::isRTCirqFlagSetMuon() {
    return RTCirqFlag_muon;
}
// check if RTC irq flag is set for temperature readings
bool Publisher_mqtt::isRTCirqFlagSetTemp() {
    return RTCirqFlag_temp;
}
// check if RTC irq flag is set for daily tasks
bool Publisher_mqtt::isRTCirqFlagSetDaily() {
    return RTCirqFlag_daily;
}

// clear RTC irq flag for muon tasks
bool Publisher_mqtt::clearRTCirqFlagMuon() {
    RTCirqFlag_muon = false;    // clear flag
    return true;
}
// clear RTC irq flag for temp tasks
bool Publisher_mqtt::clearRTCirqFlagTemp() {
    RTCirqFlag_temp = false;    // clear flag
    return true;
}
// clear RTC irq flag for daily tasks
bool Publisher_mqtt::clearRTCirqFlagDaily() {
    RTCirqFlag_daily = false;    // clear flag
    return true;
}

bool syncRTCirq() {

    DateTime dt = rtc_ds1307.now();

    if (!rtc_ds1307.isrunning() || dt.unixtime() == 0UL) {
        #if DEBUG == 1
        Serial.println("mqtt: RTC DS1307 not running, cannot sync IRQ flags.");
        #endif
        return false; // RTC not running
    }
    seconds = dt.second();
    minutes = dt.minute();
    hours = dt.hour();

    #if DEBUG == 1
    Serial.println("mqtt: RTC IRQ counters synced. Counters set to - Hours: " + String(hours) + " Minutes: " + String(minutes) + " Seconds: " + String(seconds)); 
    #endif

    return true;
}

////////////////////////////////////
// MQTT payload sending functions //
////////////////////////////////////

// connect to MQTT broker
bool connectToMQTT() {
    // Connect to the MQTT broker
    mqtt.begin(MQTT_BROKER_ADRRESS, MQTT_PORT, wifiClient);

    // Create a handler for incoming messages
    mqtt.onMessage(messageHandler);

    #if DEBUG == 1
    Serial.print("mqtt: connecting to MQTT broker");
    #endif

    // Avoid infinite blocking: try a limited number of attempts with backoff
    const int MAX_ATTEMPTS = MAX_ATTEMPTS_MQTT_CONNECTION;
    int attempt = 0;
    while (!mqtt.connected() && attempt < MAX_ATTEMPTS) {
        mqtt.loop(); // process keepalive and incoming messages
        if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) break;
        #if DEBUG == 1
        Serial.print(".");
        #endif
        attempt++;
        delay(250 * attempt); // small backoff
    }

    #if DEBUG == 1
    Serial.println();
    #endif

    if (!mqtt.connected()) {
        #if DEBUG == 1
        Serial.println("mqtt: connection timeout! No connection to MQTT broker.");
        #endif
        return false;
    }

    // Subscribe to a topic, the incoming messages are processed by messageHandler() function
    const char* SUBSCRIBE_TOPIC = subscribeTopic;
    if (mqtt.subscribe(SUBSCRIBE_TOPIC)) {
        #if DEBUG == 1
        Serial.println("mqtt: subscribed to the topic: " + String(subscribeTopic));
        #endif
    } else {
        #if DEBUG == 1
        Serial.println("mqtt: failed to subscribe to the topic: " + String(subscribeTopic));
        #endif
        return false;
    }

    #if DEBUG == 1
    Serial.println("mqtt: MQTT broker Connected!");
    #endif

    return true;
}

// send message to MQTT broker
// WITH checks for WiFi connection
bool Publisher_mqtt::send(StaticJsonDocument<200> message) {
    const size_t BUFFER_SIZE = 1024;
    char messageBuffer[BUFFER_SIZE];

    // syncRTC(); // sync internal RTC with DS1307 before sending timestamp

    // check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        #if DEBUG == 1
        Serial.println("mqtt: WiFi not connected, attempting to reconnect...");
        #endif

        if (!wifiBegin()) {
            #if DEBUG == 1
            Serial.println("mqtt: WiFi not connected, cannot send MQTT message");
            Serial.println("mqtt: Error code : " + String(WiFi.status()));
            #endif
            return false;
        } 
        #if DEBUG == 1
        Serial.println("mqtt: WiFi REconnected, starting MQTT send()");
        #endif
    } else {
        #if DEBUG == 1
        Serial.println("mqtt: WiFi already connected.");
        #endif
    }

    // check MQTT connection
    if (!mqtt.connected()) {
        #if DEBUG == 1
        Serial.println("mqtt: MQTT not connected, attempting to reconnect...");
        #endif
        if (!connectToMQTT()) {
            #if DEBUG == 1
            Serial.println("mqtt: MQTT not connected, cannot send MQTT message");
            #endif
            return false;
        } 
        #if DEBUG == 1
        else {
            Serial.println("mqtt: MQTT REconnected, starting MQTT send()");
        }
        #endif
    } else {
        #if DEBUG == 1
        Serial.println("mqtt: MQTT already connected.");
        #endif
    }

    
    if(serializeJson(message, messageBuffer, BUFFER_SIZE) == 0) {
        #if DEBUG == 1
        Serial.println("mqtt: Failed to serialize JSON message");
        #endif
        return false;
    }

    if(!mqtt.publish(publishTopic, messageBuffer)) {
        #if DEBUG == 1
        Serial.println("mqtt: Failed to publish message to MQTT broker");
        #endif
        return false;
    }

    #if DEBUG == 1
    Serial.println("mqtt: sent to MQTT - topic: " + String(publishTopic));
    #endif

    message.clear();
    return true;
}

// handler for incoming MQTT messages
void messageHandler(String& topic, String& payload) {
    #if DEBUG == 1
    Serial.println("mqtt: received from MQTT:");
    Serial.println("- topic: " + topic);
    Serial.println("- payload:");
    Serial.println(payload);
    #endif

    // You can process the incoming data as json object, then control something
    incomingMessage.clear();
    deserializeJson(incomingMessage, payload);
    if (incomingMessage.isNull()) {
        #if DEBUG == 1
        Serial.println("mqtt: Failed to parse incoming message JSON.");
        #endif
        return;
    }
    newMessageReceived = true;
    if(!executeIncomingMessage()) {
        #if DEBUG == 1
        Serial.println("mqtt: Failed to execute incoming message command.");
        #endif
    }
}

// bool Publisher_mqtt::isNewMessageReceived() {
//     return newMessageReceived;
// }

// ArduinoJson::StaticJsonDocument<200> Publisher_mqtt::getIncomingMessage() {
//     if (!incomingMessage.isNull()) {
//         #if DEBUG == 1
//         Serial.println("mqtt: getIncomingMessage() called, returning message.");
//         #endif
//         return incomingMessage;
//     } else {
//         return ArduinoJson::StaticJsonDocument<200>(); // return empty document
//     }
// }

// execute command from incoming MQTT message
// expects format: {"type":"command","command":<int>}
// command 1: sync RTC
// command 2: RTC begin
// returns true if command executed successfully
bool executeIncomingMessage() {
    
    if (incomingMessage.isNull()) {
        #if DEBUG == 1
        Serial.println("mqtt: executeIncomingMessage() called, but no message to process.");
        #endif
        return false; // no message to process
    }

    switch (incomingMessage["command"].as<int>()) {
        case 1: // sync RTC command
            #if DEBUG == 1
            Serial.println("mqtt: Executing command 1 - SYNC RTC - from incoming message.");
            #endif

            break;
        case 2: // RTC begin command
            #if DEBUG == 1
            Serial.println("mqtt: Executing command 2 - RTC BEGIN - from incoming message.");
            #endif

            if(!waitForNTP()) { // wait for NTP to be available
                #if DEBUG == 1
                Serial.println("mqtt: NTP time not available, proceeding without NTP sync.");
                #endif
            }
            delay(100);
            if(!RTC1307begin()) { // initialize RTC DS1307
                #if DEBUG == 1
                Serial.println("mqtt: RTC DS1307 initialization failed.");
                #endif
            }
            delay(100);

            break;
        default:
            #if DEBUG == 1
            Serial.println("mqtt: Unknown command in incoming message.");
            #endif
            return false; // unknown command
    }

    return true; // return true if processed successfully
}

// MQTT loop function, to handle keepalive and incoming messages
void Publisher_mqtt::mqttLoop() {
    // ensure the MQTT client processes keepalive and incoming messages
    #if DEBUG == 1
    Serial.println("mqtt: mqttLoop() called.");
    #endif
    mqtt.loop();
}