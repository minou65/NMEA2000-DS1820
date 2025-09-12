// pin out https://www.usinainfo.com.br/blog/wp-content/uploads/2019/04/esp32_pinout-1.jpg
// NMEA http://www.interfacebus.com/NMEA-2000_Standard.html#:~:text=NMEA-2000%20Pin%20Out%20The%20pin%20out%20for%20the,as%20is%20the%20signal%20pair%20%5Bblue%20%2F%20white%5D.

// use the following Pins
#define ESP32_CAN_TX_PIN GPIO_NUM_5  // Set CAN TX port to D5 
#define ESP32_CAN_RX_PIN GPIO_NUM_4  // Set CAN RX port to D4
#define ONE_WIRE_BUS GPIO_NUM_22 // Set data wire

// #define DEBUG_NMEA_MSG // Uncomment to see, what device will send to bus. Use e.g. OpenSkipper or Actisense NMEA Reader  
// #define DEBUG_NMEA_MSG_ASCII // If you want to use simple ascii monitor like Arduino Serial Monitor, uncomment this line
// #define DEBUG_NMEA_Actisense // Uncomment this, so you can test code without CAN bus chips on Arduino Mega
// #define DEBUG_Temperatur_MSG

#include "common.h"

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <esp_mac.h>
#include <Preferences.h>

#include <N2kAlertTypes.h>
#include <NMEA2000_CAN.h>
#include <N2kMessages.h>

#include "webhandling.h"
#include "version.h"

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

typedef struct {
    unsigned long PGN;
    void (*Handler)(const tN2kMsg& N2kMsg);
} tNMEA2000Handler;

tNMEA2000Handler NMEA2000Handlers[] = {
    {126984L, ParseAlertResponse},
    {0,0}
};

const unsigned long ReciveMessages[] PROGMEM = {
    126984L,
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

void OnN2kOpen() {
    Sensor* sensor_ = &Sensor1;

    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {
            sensor_->AlarmScheduler.UpdateNextTime();
            sensor_->AlarmTextScheduler.UpdateNextTime();
            sensor_->TemperatureScheduler.UpdateNextTime();
        }
        sensor_ = (Sensor*)sensor_->getNext();
    }
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
    size_t deviceCount_ = sizeof(gN2KSource) / sizeof(gN2KSource[0]);
    for (size_t i_ = 0; i_ < deviceCount_; ++i_) {
        uint8_t currentSource_ = NMEA2000.GetN2kSource(i_);
        if (gN2KSource[i_] != currentSource_) {
            gN2KSource[i_] = currentSource_;
            changed_ = true;
        }
    }
    if (changed_) {
        Preferences prefs_;
        prefs_.begin("n2k", false); // Namespace "n2k"
        for (size_t i_ = 0; i_ < deviceCount_; ++i_) {
            char key_[16];
            snprintf(key_, sizeof(key_), "N2KSource%u", (unsigned)i_);
            prefs_.putUChar(key_, gN2KSource[i_]);
        }
        prefs_.end();
    }
}

void setup() {

    Serial.begin(115200);
    while (!Serial) {
        delay(1);
    }
    Serial.printf("Firmware version:%s\n", Version);

    // init sensors
    sensors.begin(); 
    sensors.setResolution(12);

    gDeviceCount = sensors.getDeviceCount();

    // Load N2K source addresses from Preferences if available
    Preferences prefs_;
    prefs_.begin("n2k", true); // read-only
    size_t deviceCount_ = sizeof(gN2KSource) / sizeof(gN2KSource[0]);
    for (size_t i_ = 0; i_ < deviceCount_; ++i_) {
        char key_[16];
        snprintf(key_, sizeof(key_), "N2KSource%u", (unsigned)i_);
        uint8_t val_ = prefs_.getUChar(key_, gN2KSource[i_]); // fallback: current default
        gN2KSource[i_] = val_;
    }
    prefs_.end();

	Serial.print("Found "); Serial.print(gDeviceCount); Serial.println(" devices.");

    // init wifi
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
    NMEA2000.SetForwardType(tNMEA2000::fwdt_Text)
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

    NMEA2000.Open();
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

void CreateDevicesForActiveSensors() {
    Sensor* sensor_ = &Sensor1;
    uint8_t deviceIndex_ = 0;

    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {
            // Set source address, SID and DeviceID for this sensor
            uint8_t source_ = gN2KSource[deviceIndex_];
            uint8_t sid_ = gN2KSID + deviceIndex_;
            uint64_t deviceId_ = getDeviceUID(deviceIndex_);

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

            // Optionally set product information per device
            NMEA2000.SetProductInformation(
                String(deviceId_).c_str(), // Model serial code
                100,
                "Multidevice temperatur sensor",
                Version,
                Version,
				1, // Load Equivalency
				0xffff, // NMEA version
				0xff,  // Certification level
				deviceIndex_ // Device instance
            );

            NMEA2000.SetConfigurationInformation(
                String(deviceId_).c_str(), // Unique number
                sensor_->GetLocationValue(),
                "",
				deviceIndex_ // Device instance
			);

			uint8_t instance_ = gN2KInstance + deviceIndex_;

			// each Alert-Device must have a unique instance number
            
            // --- Alert-Device für diesen Sensor anlegen (gleiche Adresse/Device wie Sensor) ---
			instance_++;
            sensor_->Alert.SetAlertSystem(
                instance_,
                gN2KInstance + instance_,
                source_, // gleiche Adresse wie Sensor
                N2kts_AlertLanguageEnglishUS,
                const_cast<char*>(sensor_->GetDescriptionValue()),
                const_cast<char*>(sensor_->GetSourceName())
            );
            sensor_->Alert.SetAlertDataSource(gN2KInstance + instance_, 0, source_);
            sensor_->Alert.SetAlertThreshold(
                t2kNAlertThresholdMethod(sensor_->GetThresholdMethod()), 0, sensor_->GetThresholdValue());
            sensor_->Alert.SetTemporarySilenceTime(sensor_->GetTemporarySilenceTime() * 60);

            // --- FaultAlert-Device für diesen Sensor anlegen (gleiche Adresse/Device wie Sensor) ---
			instance_++;
            String faultDescription_ = String(sensor_->GetDescriptionValue()) + " DS1820 faulty or not connected";
            sensor_->FaultAlert.SetAlertSystem(
                instance_,
                gN2KInstance + instance_,
                source_, // gleiche Adresse wie Sensor
                N2kts_AlertLanguageEnglishUS,
                const_cast<char*>(faultDescription_.c_str()),
                const_cast<char*>(sensor_->GetSourceName())
            );
            sensor_->FaultAlert.SetAlertDataSource(gN2KInstance + instance_, 0, source_);
            sensor_->FaultAlert.SetAlertThreshold(
                t2kNAlertThresholdMethod(sensor_->GetThresholdMethod()), 0, sensor_->GetThresholdValue());
            sensor_->FaultAlert.SetTemporarySilenceTime(sensor_->GetTemporarySilenceTime() * 60);

            deviceIndex_++;
        }
        sensor_ = (Sensor*)sensor_->getNext();
    }
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

        NMEA2000.SendProductInformation(deviceIndex);
        NMEA2000.SendConfigurationInformation(deviceIndex);
    }
}

void SendAlert(Sensor* sensor, uint8_t deviceIndex) {
    if (sensor->isActive() && sensor->AlarmScheduler.IsTime()) {
        sensor->AlarmScheduler.UpdateNextTime();

        tN2kMsg N2kMsg_;

        sensor->FaultAlert.SetN2kAlert(N2kMsg_);
        NMEA2000.SendMsg(N2kMsg_, deviceIndex);

        sensor->Alert.SetN2kAlert(N2kMsg_);
        NMEA2000.SendMsg(N2kMsg_, deviceIndex);
    }
}

void SendAlertText(Sensor* sensor, uint8_t deviceIndex) {
    if (sensor->isActive() && sensor->AlarmTextScheduler.IsTime()) {

        sensor->AlarmTextScheduler.UpdateNextTime();

        tN2kMsg N2kMsg_;

        sensor->FaultAlert.SetN2kAlertText(N2kMsg_);
        NMEA2000.SendMsg(N2kMsg_, deviceIndex);

        sensor->Alert.SetN2kAlertText(N2kMsg_);
        NMEA2000.SendMsg(N2kMsg_, deviceIndex);

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
        SendTemperatur(sensor_, deviceIndex_);
        SendAlert(sensor_, deviceIndex_);
		SendAlertText(sensor_, deviceIndex_);
        if (gParamsChanged) {
            SetInstallationDescription(sensor_, deviceIndex_);
        }

        deviceIndex_++;
        sensor_ = (Sensor*)sensor_->getNext();
    }

    NMEA2000.ParseMessages();

    CheckN2kSourceAddressChange();

    if (gParamsChanged) {
        gParamsChanged = false;
	}

    // Dummy to empty input buffer to avoid board to stuck with e.g. NMEA Reader
    if (Serial.available()) {
        Serial.read();
    }
}

double GetTemperatur(int Index) {
    double T_;
    T_ = sensors.getTempCByIndex(Index);
    //if (T == -127) T = 9.99;
    return T_;
}

void loop2(void* parameter) {
    
    for (;;) {   // Endless loop
        sensors.requestTemperatures(); // Send the command to get temperatures

        Sensor* sensor_ = &Sensor1;
        uint8_t index_ = 0;
        while (sensor_ != nullptr) {
            if (sensor_->isActive()) {
                double d = GetTemperatur(index_);
                sensor_->SetSensorValue(d);
            }
            index_++;
            sensor_ = (Sensor*)sensor_->getNext();
        }
        vTaskDelay(1000);
    }
}