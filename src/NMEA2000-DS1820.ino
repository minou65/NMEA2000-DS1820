// NMEA http://www.interfacebus.com/NMEA-2000_Standard.html#:~:text=NMEA-2000%20Pin%20Out%20The%20pin%20out%20for%20the,as%20is%20the%20signal%20pair%20%5Bblue%20%2F%20white%5D.



// #define DEBUG_NMEA_MSG // Uncomment to see, what device will send to bus. Use e.g. OpenSkipper or Actisense NMEA Reader  
// #define DEBUG_NMEA_MSG_ASCII // If you want to use simple ascii monitor like Arduino Serial Monitor, uncomment this line
// #define DEBUG_NMEA_Actisense // Uncomment this, so you can test code without CAN bus chips on Arduino Mega
// #define DEBUG_Temperatur_MSG

#include "Config.h"
#include "common.h"

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <esp_mac.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>

#include <N2kAlertTypes.h>
#include <NMEA2000_CAN.h>
#include <N2kMessages.h>
#include <RebootManager.h>
#include <EventLogger.h>

#include "webhandling.h"
#include "version.h"

bool debugMode = false;

char Version[] = VERSION_STR; // Manufacturer's Software version code

uint8_t gN2KSource[] = { 22, 23, 24, 25 };
uint8_t gN2KInstance = 1;
uint8_t gN2KSID = 255;

// Setup a OneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Task handle for OneWire read (Core 0 on ESP32)
TaskHandle_t TaskHandle;

// Pass OneWire reference to Dallas Temperature
DallasTemperature sensors(&oneWire);

int8_t gDeviceCount = 1;

void ParseAlertResponse(const tN2kMsg& N2kMsg);
void ParseN2kSystemTime(const tN2kMsg& N2kMsg);
void ParseN2kGNSS(const tN2kMsg& N2kMsg);

typedef struct {
    unsigned long PGN;
    void (*Handler)(const tN2kMsg& N2kMsg);
} tNMEA2000Handler;

tNMEA2000Handler NMEA2000Handlers[] = {
    {126984L, ParseAlertResponse},  // Alert
    {126992L, ParseN2kSystemTime},  // System Time
    {129029L, ParseN2kGNSS},        // GNSS Position Data
    {0,0}
};

const unsigned long ReciveMessages[] PROGMEM = {
    126984L,    // Alert
    126992L,    // System Time
    129029L,    // GNSS Position Data
    0
};

// List here messages your device will transmit.
const unsigned long TemperaturDeviceMessages[] PROGMEM = {
    130310L, // Environmental Parameters - DEPRECATED
    130312L, // Temperature - DEPRECATED
    130316L, // Temperature, Extended Range
    126983L, // Alert
    126985L, // Alert text
    0
};

// ============================================================================
// TIME SYNCHRONIZATION FUNCTIONS
// ============================================================================
bool TimeSet = false;
unsigned long LastTimeUpdate = 0;
/**
 * Set ESP32 system time from NMEA2000 time
 */
void SetSystemTime(uint16_t DaysSince1970, double SecondsSinceMidnight) {
    if (DaysSince1970 == 0xFFFF || SecondsSinceMidnight > 86400) {
        return;
    }

    // Update only every 10 minutes (except first time)
    if (TimeSet && (millis() - LastTimeUpdate < 600000)) {
        return;
    }

    time_t timestamp = (DaysSince1970 * 86400UL) + (time_t)SecondsSinceMidnight;

    struct timeval tv;
    tv.tv_sec = timestamp;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    TimeSet = true;
    LastTimeUpdate = millis();

    char timeStr[64];
    struct tm* timeinfo = localtime(&timestamp);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
    eventLogger.logf("System time set from NMEA2000: %s UTC", timeStr);
}

/**
 * Parse PGN 126992 - System Time
 */
void ParseN2kSystemTime(const tN2kMsg& N2kMsg) {
    unsigned char SID;
    uint16_t SystemDate;
    double SystemTime;
    tN2kTimeSource TimeSource;

    if (ParseN2kPGN126992(N2kMsg, SID, SystemDate, SystemTime, TimeSource)) {
        if (TimeSource == N2ktimes_GPS || TimeSource == N2ktimes_GLONASS) {
            SetSystemTime(SystemDate, SystemTime);

            if (debugMode) {
                eventLogger.logf("Received System Time (PGN 126992): Date=%u, Time=%.1fs, Source=%d",
                    SystemDate, SystemTime, TimeSource);
            }
        }
    }
}

/**
 * Parse PGN 129029 - GNSS Position Data (contains time)
 */
void ParseN2kGNSS(const tN2kMsg& N2kMsg) {
    unsigned char SID;
    uint16_t DaysSince1970;
    double SecondsSinceMidnight;
    double Latitude;
    double Longitude;
    double Altitude;
    tN2kGNSStype GNSStype;
    tN2kGNSSmethod GNSSmethod;
    unsigned char nSatellites;
    double HDOP;
    double PDOP;
    double GeoidalSeparation;
    unsigned char nReferenceStations;
    tN2kGNSStype ReferenceStationType;
    uint16_t ReferenceSationID;
    double AgeOfCorrection;

    if (ParseN2kPGN129029(N2kMsg, SID, DaysSince1970, SecondsSinceMidnight,
        Latitude, Longitude, Altitude, GNSStype, GNSSmethod,
        nSatellites, HDOP, PDOP, GeoidalSeparation,
        nReferenceStations, ReferenceStationType,
        ReferenceSationID, AgeOfCorrection)) {

        SetSystemTime(DaysSince1970, SecondsSinceMidnight);

        if (debugMode) {
            eventLogger.logf("Received GNSS Time (PGN 129029): Date=%u, Time=%.1fs, Sats=%u",
                DaysSince1970, SecondsSinceMidnight, nSatellites);
        }
    }
}

// ============================================================================
// NMEA2000 CALLBACK
// ============================================================================
void OnN2kOpen() {
    //eventLogger.log("OnN2kOpen() called - Initializing schedulers...");
    //
    //Sensor* sensor_ = &Sensor1;
    //uint8_t idx_ = 0;

    //while (sensor_ != nullptr) {
    //    if (sensor_->isActive()) {
    //        sensor_->AlarmScheduler.UpdateNextTime();
    //        sensor_->AlarmTextScheduler.UpdateNextTime();
    //        sensor_->TemperatureScheduler.UpdateNextTime();
    //        
    //        eventLogger.logf("Sensor %u schedulers initialized", idx_);
    //    }
    //    sensor_ = (Sensor*)sensor_->getNext();
    //    idx_++;
    //}
    //
    //eventLogger.log("All schedulers initialized");
}

void handleN2kMessages(const tN2kMsg& N2kMsg_) {
    for (uint8_t i_ = 0; NMEA2000Handlers[i_].PGN != 0; i_++) {
        if (N2kMsg_.PGN == NMEA2000Handlers[i_].PGN) {
            NMEA2000Handlers[i_].Handler(N2kMsg_);
            break;
        }
    }
}

void CheckN2kSourceAddressChange() {
    bool changed_ = false;

    if (!NMEA2000.ReadResetAddressChanged()) {
        return;
    }

    // Count active sensors first
    Sensor* sensor_ = &Sensor1;
    uint8_t activeDeviceCount_ = 0;
    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {
            activeDeviceCount_++;
        }
        sensor_ = (Sensor*)sensor_->getNext();
    }

    // Check each device index for address changes
    for (size_t i_ = 0; i_ < activeDeviceCount_; ++i_) {
        uint8_t currentSource_ = NMEA2000.GetN2kSource(i_);
        if (gN2KSource[i_] != currentSource_) {
            eventLogger.logf("Device %u address changed from %u to %u",
                (unsigned)i_, gN2KSource[i_], currentSource_);
            gN2KSource[i_] = currentSource_;
            changed_ = true;
        }
    }

    // Save to Preferences only if something actually changed
    if (changed_) {
        Preferences prefs_;
        prefs_.begin("n2k", false);
        for (size_t i_ = 0; i_ < activeDeviceCount_; ++i_) {
            char key_[16];
            snprintf(key_, sizeof(key_), "N2KSource%u", (unsigned)i_);
            prefs_.putUChar(key_, gN2KSource[i_]);
            eventLogger.logf("Saved N2KSource%u = %u to Preferences",
                (unsigned)i_, gN2KSource[i_]);
        }
        prefs_.end();
    }
}

void setup() {

    Serial.begin(115200);
    while (!Serial) {
        delay(1);
    }

    eventLogger.log("========================================");
    eventLogger.log("NMEA2000 DS1820 Sensor");
    eventLogger.log("========================================");
    eventLogger.logf("Firmware version: %s", Version);
    eventLogger.log("Sensor type: DS18B20 Temperature Sensor");
    eventLogger.logf("Sensor pin: GPIO_%d", ONE_WIRE_BUS);
    eventLogger.log("========================================\n");

#ifdef DISABLE_BROWNOUT_DETECTOR
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    eventLogger.log("Brownout detector disabled via build flag");
#endif

    RebootManager::begin();
    eventLogger.logf("Reboot count: %d", RebootManager::getRebootCount());
    eventLogger.logf("Last reboot reason: %s", RebootManager::getLastRebootReasonText().c_str());

    randomSeed(esp_random());

    // ===============================================================
    // MAC-basierte eindeutige Adressierung (Bereich 20-115)
    // Unterstützt bis zu 20 Geräte mit je 4 logischen Devices
    // ===============================================================
    uint8_t mac_[6];
    esp_efuse_mac_get_default(mac_);

    // Nutze MAC[3], MAC[4], MAC[5] für maximale Streuung
    uint16_t macSum_ = (uint16_t)mac_[3] + (uint16_t)mac_[4] + (uint16_t)mac_[5];
    uint8_t macBased_ = macSum_ % 20;  // 0-19 → 20 Slots
    uint8_t baseAddress_ = 20 + (macBased_ * 5);  // 20, 25, 30, 35, ..., 115

    eventLogger.logf("Device MAC: %02X:%02X:%02X:%02X:%02X:%02X",
        mac_[0], mac_[1], mac_[2], mac_[3], mac_[4], mac_[5]);
    eventLogger.logf("MAC sum: %u (0x%02X + 0x%02X + 0x%02X)",
        macSum_, mac_[3], mac_[4], mac_[5]);
    eventLogger.logf("Calculated slot: %u (out of 20 slots)", macBased_);
    eventLogger.logf("Base N2K address: %u (device addresses: %u-%u)",
        baseAddress_, baseAddress_, baseAddress_ + 3);

    // Startup-Delay: 1 Sekunde pro Slot (max 19 Sekunden)
    uint32_t randomDelay_ = macBased_ * 1000;
    eventLogger.logf("Startup delay: %lu ms (ensures collision-free boot)", randomDelay_);
    delay(randomDelay_);

    // init sensors
    eventLogger.log("\nInitializing sensor...");
    sensors.begin();
    sensors.setResolution(12);

    gDeviceCount = sensors.getDeviceCount();

    // Load N2K source addresses from Preferences
    Preferences prefs_;
    prefs_.begin("n2k", false);  // read-write

    bool prefsChanged_ = false;
    size_t deviceCount_ = sizeof(gN2KSource) / sizeof(gN2KSource[0]);

    for (size_t i_ = 0; i_ < deviceCount_; ++i_) {
        char key_[16];
        snprintf(key_, sizeof(key_), "N2KSource%u", (unsigned)i_);

        // Default basierend auf MAC + Index (mit Prüfung auf max 251)
        uint8_t defaultAddr_ = baseAddress_ + i_;
        if (defaultAddr_ > 251) {
            defaultAddr_ = 251;  // NMEA2000 max address
            eventLogger.logf("WARNING: Calculated address %u exceeds 251, capping at 251",
                baseAddress_ + i_);
        }

        uint8_t val_ = prefs_.getUChar(key_, 255);  // 255 = nicht gesetzt

        if (val_ == 255) {
            // Erste Inbetriebnahme: MAC-basierte Adresse speichern
            gN2KSource[i_] = defaultAddr_;
            prefs_.putUChar(key_, defaultAddr_);
            prefsChanged_ = true;
            eventLogger.logf("First boot: Set N2KSource%u = %u (MAC-based)",
                (unsigned)i_, defaultAddr_);
        }
        else {
            gN2KSource[i_] = val_;
        }
    }

    prefs_.end();

    eventLogger.logf("Using N2K addresses: %u, %u, %u, %u",
        gN2KSource[0], gN2KSource[1], gN2KSource[2], gN2KSource[3]);

    eventLogger.logf("Found %d devices.", gDeviceCount);

    // init wifi
    eventLogger.log("\nInitializing WiFi...");
    wifiInit();

    // Create GetTemperature task for core 0, loop() runs on core 1
    xTaskCreatePinnedToCore(
        loop2, /* Function to implement the task */
        "TaskHandle", /* Name of the task */
        10000,  /* Stack size in words */
        NULL,  /* Task input parameter */
        0,  /* Priority of the task */
        &TaskHandle,  /* Task handle. */
        0 /* Core where the task should run */
    );

    // Reserve enough buffer for sending all messages. This does not work on small memory devices like Uno or Mega
    eventLogger.log("\nSetting up NMEA2000...");
    NMEA2000.SetN2kCANMsgBufSize(10);
    NMEA2000.SetN2kCANReceiveFrameBufSize(250);
    NMEA2000.SetN2kCANSendFrameBufSize(250);

    // Set the device count for NMEA2000
    NMEA2000.SetDeviceCount(4);

    NMEA2000.SetOnOpen(OnN2kOpen);
    NMEA2000.SetMsgHandler(handleN2kMessages);

#ifdef DEBUG_NMEA_MSG
    Serial.begin(115200);
    NMEA2000.SetForwardStream(&Serial);

#ifdef DEBUG_NMEA_MSG_ASCII
    NMEA2000.SetForwardType(tNMEA2000::fwdt_Text);
#endif // DEBUG_NMEA_MSG_ASCII

#ifdef  DEBUG_NMEA_Actisense
    NMEA2000.SetDebugMode(tNMEA2000::dm_Actisense);
#endif //  DEBUG_NMEA_Actisense

#else
    NMEA2000.EnableForward(false); // Disable all msg forwarding to USB (=Serial)

#endif // DEBUG_NMEA_MSG

    // If you also want to see all traffic on the bus use N2km_ListenAndNode instead of N2km_NodeOnly below
    NMEA2000.SetMode(tNMEA2000::N2km_NodeOnly);

    CreateDevicesForActiveSensors();

    eventLogger.log("Opening NMEA2000...");
    NMEA2000.Open();

    esp_task_wdt_add(NULL);

    eventLogger.log("\n========================================");
    eventLogger.log("NMEA2000 started");
    eventLogger.logf("Base address: %u, Using addresses: %u, %u, %u, %u",
        baseAddress_, gN2KSource[0], gN2KSource[1], gN2KSource[2], gN2KSource[3]);
    eventLogger.log("Listening for GPS time on NMEA2000 bus");
    eventLogger.log("Setup complete");
    eventLogger.log("========================================\n");
}

uint64_t getDeviceUID(uint8_t devId) {
    uint8_t chipId_[6];
    esp_efuse_mac_get_default(chipId_);

    uint64_t uid_ = 0;
    for (int i_ = 0; i_ < 6; i_++) {
        uid_ += ((uint64_t)chipId_[i_] << (8 * i_));
    }
    // Add devId as the highest byte to make the UID unique per device
    uid_ |= ((uint64_t)devId << 48);
    return uid_;
}

// Hilfsfunktion für kurze, eindeutige Device-Namen (vor CreateDevicesForActiveSensors)
String GetDeviceModelID(uint8_t mac5_) {
    // Kurzer, eindeutiger Name: "DS1820-XX" wobei XX die letzten 2 MAC-Hex-Digits sind
    char modelId_[16];
    snprintf(modelId_, sizeof(modelId_), "DS1820-%02X", mac5_);
    return String(modelId_);
}

void CreateDevicesForActiveSensors() {
    Sensor* sensor_ = &Sensor1;
    uint8_t deviceIndex_ = 0;

    Preferences pref_;
    pref_.begin("ds1820", true);

    // MAC für eindeutige Device-Namen
    uint8_t mac_[6];
    esp_efuse_mac_get_default(mac_);
    String deviceModelId_ = GetDeviceModelID(mac_[5]);

    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {

            String maxKey_ = "max_" + String(deviceIndex_);
            String timeKey_ = "time_" + String(deviceIndex_);

            double savedMax_ = pref_.getDouble(maxKey_.c_str(), -273.15);
            time_t savedTime_ = pref_.getULong64(timeKey_.c_str(), 0);

            sensor_->ResetMaxTemperature();
            if (savedMax_ > -273.15) {
                if (savedTime_ > 0) {
                    sensor_->SetMaxTemperature(savedMax_, savedTime_);

                    char timeStr_[64];
                    struct tm* timeinfo_ = localtime(&savedTime_);
                    strftime(timeStr_, sizeof(timeStr_), "%Y-%m-%d %H:%M:%S", timeinfo_);
                    eventLogger.logf("Loaded max temp %.2f°C from %s for sensor %u",
                        savedMax_, timeStr_, deviceIndex_);
                }
                else {
                    sensor_->SetMaxTemperature(savedMax_, 0);
                    eventLogger.logf("Loaded max temp %.2f°C (no timestamp) for sensor %u",
                        savedMax_, deviceIndex_);
                }
            }

            // Set source address, SID and DeviceID for this sensor
            uint8_t source_ = gN2KSource[deviceIndex_];
            uint8_t sid_ = gN2KSID + deviceIndex_;
            uint64_t deviceId_ = getDeviceUID(deviceIndex_);
            uint64_t ackNetworkId_ = deviceId_;

            // Basisinstanz für dieses Sensor‑Device
            uint8_t deviceInstance_ = gN2KInstance + deviceIndex_ * 3;
            uint8_t alertInstance_ = deviceInstance_ + 1;
            uint8_t faultAlertInstance_ = deviceInstance_ + 2;

            // Register the device with NMEA2000
            NMEA2000.SetN2kSource(source_, deviceIndex_);

            // Here we tell library, which PGNs we transmit
            NMEA2000.ExtendTransmitMessages(TemperaturDeviceMessages, deviceIndex_);

            NMEA2000.SetDeviceInformation(
                deviceId_,
                130, // Device function=Temperature
                75,  // Device class=Sensor Communication Interface
                2040, // Manufacturer code
                4,     // Industry code
                deviceIndex_  // Device instance
            );

            // VERBESSERT: Kurzer, eindeutiger Product Name
            // Format: "DS1820-XX S1" wobei XX=MAC-ID, S1=Sensor-Nummer
            String productName_ = deviceModelId_ + " S" + String(deviceIndex_ + 1);

            // Optional: Verwende Sensor-Location als Teil des Namens
            String sensorLocation_ = String(sensor_->GetLocationValue());
            if (sensorLocation_.length() > 0 && sensorLocation_ != "undefined") {
                // Kürze Location falls zu lang (max 32 Zeichen für Model ID)
                if (sensorLocation_.length() > 20) {
                    sensorLocation_ = sensorLocation_.substring(0, 17) + "...";
                }
                productName_ = sensorLocation_;
            }

            NMEA2000.SetProductInformation(
                String(deviceId_).c_str(), // Model serial code
                100 + deviceIndex_,  // Product Code (eindeutig pro Sensor)
                productName_.c_str(),  // KURZER, eindeutiger Name
                Version,
                Version,
                1, // Load Equivalency
                0xffff, // NMEA version
                0xff,  // Certification level
                deviceIndex_ // Device instance
            );

            // Configuration Information mit Sensor-Location
            NMEA2000.SetConfigurationInformation(
                deviceModelId_.c_str(), // Unique device identifier
                sensor_->GetLocationValue(),
                "",
                deviceIndex_ // Device instance
            );

            eventLogger.logf("Created sensor device %u: '%s' with source %u, SID %u, instance %u",
                deviceIndex_, productName_.c_str(), source_, sid_, deviceInstance_);

            // Alert-Device
            sensor_->Alert.SetAlertSystem(
                alertInstance_,
                deviceInstance_,
                ackNetworkId_,
                N2kts_AlertLanguageEnglishUS,
                const_cast<char*>(sensor_->GetDescriptionValue()),
                const_cast<char*>(sensor_->GetSourceName())
            );

            sensor_->Alert.SetAlertDataSource(alertInstance_, 0, deviceId_);
            sensor_->Alert.SetAlertThreshold(t2kNAlertThresholdMethod(sensor_->GetThresholdMethod()), 0, sensor_->GetThresholdValue());
            sensor_->Alert.SetTemporarySilenceTime(sensor_->GetTemporarySilenceTime() * 60);

            eventLogger.logf("Alert system set for sensor %u with instance %u", deviceIndex_, alertInstance_);

            // Fault-Alert-Device
            String faultDescription_ = productName_ + " Fault";
            sensor_->FaultAlert.SetAlertSystem(
                faultAlertInstance_,
                deviceInstance_,
                ackNetworkId_,
                N2kts_AlertLanguageEnglishUS,
                const_cast<char*>(faultDescription_.c_str()),
                const_cast<char*>(sensor_->GetSourceName())
            );
            sensor_->FaultAlert.SetAlertDataSource(faultAlertInstance_, 0, ackNetworkId_);
            sensor_->FaultAlert.SetAlertThreshold(N2kts_AlertThresholdMethodEqual, 0, (uint64_t)(int64_t)-127);
            sensor_->FaultAlert.SetTemporarySilenceTime(sensor_->GetTemporarySilenceTime() * 60);

            eventLogger.logf("Fault alert system set for sensor %u with instance %u", deviceIndex_, faultAlertInstance_);

            sensor_->AlarmScheduler.UpdateNextTime();
            sensor_->AlarmTextScheduler.UpdateNextTime();
            sensor_->TemperatureScheduler.UpdateNextTime();
            eventLogger.logf("Initialized schedulers for sensor %u", deviceIndex_);

            deviceIndex_++;
        }
        sensor_ = (Sensor*)sensor_->getNext();
    }

    pref_.end();
}

void SendTemperatur(Sensor* sensor, uint8_t deviceIndex) {

    if (sensor->isActive() && sensor->TemperatureScheduler.IsTime()) {
        sensor->TemperatureScheduler.UpdateNextTime();

        tN2kMsg N2kMsg_;
        uint8_t sid_ = gN2KSID + deviceIndex;
        tN2kTempSource TempSource_ = tN2kTempSource(sensor->GetSourceId());
        double TempValue_ = CToKelvin(sensor->GetSensorValue());

        SetN2kPGN130312(N2kMsg_, sid_, deviceIndex, TempSource_, TempValue_, N2kDoubleNA);
        NMEA2000.SendMsg(N2kMsg_, deviceIndex);

        SetN2kPGN130316(N2kMsg_, sid_, deviceIndex, TempSource_, TempValue_, N2kDoubleNA);
        NMEA2000.SendMsg(N2kMsg_, deviceIndex);

        if (TempSource_ == N2kts_OutsideTemperature) {
            SetN2kPGN130310(N2kMsg_, sid_, N2kDoubleNA, TempValue_);
            NMEA2000.SendMsg(N2kMsg_, deviceIndex);
        }

        if (TempSource_ == N2kts_SeaTemperature) {
            SetN2kPGN130310(N2kMsg_, sid_, TempValue_, N2kDoubleNA);
            NMEA2000.SendMsg(N2kMsg_, deviceIndex);
        }

        static unsigned long lastInfoSent_[4] = { 0, 0, 0, 0 };
        if (millis() - lastInfoSent_[deviceIndex] > 60000) {
            NMEA2000.SendProductInformation(deviceIndex);
            NMEA2000.SendConfigurationInformation(deviceIndex);
            lastInfoSent_[deviceIndex] = millis();
        }

        eventLogger.logf("Sent temperature %.2f°C for sensor %u (source %u, sid %u)",
            sensor->GetSensorValue(), deviceIndex, TempSource_, sid_);
    }
}

void SendAlert(Sensor* sensor, uint8_t deviceIndex) {
    if (sensor->isActive() && sensor->AlarmScheduler.IsTime()) {
        sensor->AlarmScheduler.UpdateNextTime();

        tN2kMsg N2kMsg_;

        sensor->FaultAlert.SetN2kAlert(N2kMsg_);
        if (!NMEA2000.SendMsg(N2kMsg_, deviceIndex)) {
            eventLogger.logf("Failed to send Fault Alert for device %u", deviceIndex);
        }

        sensor->Alert.SetN2kAlert(N2kMsg_);
        if (!NMEA2000.SendMsg(N2kMsg_, deviceIndex)) {
            eventLogger.logf("Failed to send Alert for device %u", deviceIndex);
        }
    }
}

void SendAlertText(Sensor* sensor, uint8_t deviceIndex) {
    if (sensor->isActive() && sensor->AlarmTextScheduler.IsTime()) {
        sensor->AlarmTextScheduler.UpdateNextTime();

        tN2kMsg N2kMsg_;

        sensor->FaultAlert.SetN2kAlertText(N2kMsg_);
        if (!NMEA2000.SendMsg(N2kMsg_, deviceIndex)) {
            eventLogger.logf("Failed to send Fault Alert Text for device %u", deviceIndex);
        }

        sensor->Alert.SetN2kAlertText(N2kMsg_);
        if (!NMEA2000.SendMsg(N2kMsg_, deviceIndex)) {
            eventLogger.logf("Failed to send Alert Text for device %u", deviceIndex);
        }
    }
}

void SetInstallationDescription(Sensor* sensor, uint8_t deviceIndex) {
    if (sensor->isActive()) {
        NMEA2000.SetInstallationDescription1(sensor->GetLocationValue(), deviceIndex);
    }
}

void ParseAlertResponse(const tN2kMsg& N2kMsg) {
    Sensor* sensor_ = &Sensor1;
    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {
            sensor_->Alert.ParseAlertResponse(N2kMsg);
            sensor_->FaultAlert.ParseAlertResponse(N2kMsg);
        }
        sensor_ = (Sensor*)sensor_->getNext();
    }
}

void loop() {
    wifiLoop();

    Sensor* sensor_ = &Sensor1;
    uint8_t deviceIndex_ = 0;
    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {
            SendTemperatur(sensor_, deviceIndex_);
            if (gParamsChanged) {
                SetInstallationDescription(sensor_, deviceIndex_);
            }

            if ((sensor_->GetThresholdMethod() > 0) && (sensor_->GetSourceId() > 0)) {
                SendAlert(sensor_, deviceIndex_);
                SendAlertText(sensor_, deviceIndex_);
            }

            deviceIndex_++;
        }
        sensor_ = (Sensor*)sensor_->getNext();
    }

    NMEA2000.ParseMessages();
    CheckN2kSourceAddressChange();

    if (gParamsChanged) {
        gParamsChanged = false;
    }

    if (TimeSet && (millis() - LastTimeUpdate > 3600000)) {
        eventLogger.log("Warning: No time update received for 1 hour");
        TimeSet = false;
    }

    if (ShouldReboot) {
        eventLogger.log("Rebooting due to request...");
        delay(100);
        ESP.restart();
	}

    if (Serial.available()) {
        Serial.read();
    }
    esp_task_wdt_reset();
}

double GetTemperatur(int Index) {
    double T_;
    T_ = sensors.getTempCByIndex(Index);
    //if (T == -127) T = 9.99;
    return T_;
}

void loop2(void* parameter) {
    esp_task_wdt_add(NULL);
    for (;;) {   // Endless loop
        sensors.requestTemperatures(); // Send the command to get temperatures

        // Wait for conversion to complete (750ms for 12-bit resolution + safety margin)
        vTaskDelay(1000);

        Sensor* sensor_ = &Sensor1;
        uint8_t index_ = 0;
        while (sensor_ != nullptr) {
            if (sensor_->isActive()) {
                double d_ = GetTemperatur(index_);
                double previousMax_ = sensor_->GetMaxTemperature();
                sensor_->SetSensorValue(d_);

                if (d_ != -127 && d_ > previousMax_) {
                    sensor_->UpdateMaxTemperature(d_);

                    Preferences pref_;
                    pref_.begin("ds1820", false);
                    String maxKey_ = "max_" + String(index_);
                    String timeKey_ = "time_" + String(index_);
                    pref_.putDouble(maxKey_.c_str(), sensor_->GetMaxTemperature());

                    time_t maxTime_ = sensor_->GetMaxTemperatureTime();
                    if (maxTime_ > 0) {
                        pref_.putULong64(timeKey_.c_str(), maxTime_);

                        char timeStr_[64];
                        struct tm* timeinfo_ = localtime(&maxTime_);
                        strftime(timeStr_, sizeof(timeStr_), "%Y-%m-%d %H:%M:%S", timeinfo_);
                        eventLogger.logf("New max temp %.2f°C saved for sensor %u at %s",
                            d_, index_, timeStr_);
                    }
                    else {
                        eventLogger.logf("New max temp %.2f°C saved for sensor %u (no valid time yet)",
                            d_, index_);
                    }

                    pref_.end();
                }
            }
            index_++;
            sensor_ = (Sensor*)sensor_->getNext();
        }
        esp_task_wdt_reset();
    }
}
