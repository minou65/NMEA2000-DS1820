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
#include <esp_task_wdt.h>
#include <esp_mac.h>

#include <N2kAlertTypes.h>
#include <NMEA2000_CAN.h>
#include <N2kMessages.h>


#include "webhandling.h"
#include "version.h"
#include "neotimer.h"

char Version[] = VERSION_STR; // Manufacturer's Software version code

uint8_t gN2KSource[] = { 22, 23 };
uint8_t gN2KInstance = 1;
uint8_t gN2KSID = 255;

uint64_t DeviceId1 = 0;
uint64_t DeviceId2 = 0;

// Setup a OneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Task handle for OneWire read (Core 0 on ESP32)
TaskHandle_t TaskHandle;

// Configuration for the Watchdog Timer
esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 5000, // Timeout in milliseconds
    .trigger_panic = true // Trigger panic if the Watchdog Timer expires
};

// Pass OneWire reference to Dallas Temperature
DallasTemperature sensors(&oneWire);

int8_t gDeviceCount = 1;

// List here messages your device will transmit.
const unsigned long TemperaturDeviceMessages[] PROGMEM = { 
    130310L, // Environmental Parameters - DEPRECATED
    130312L, // Temperature - DEPRECATED
    130316L, // Temperature, Extended Range
    0 
};

const unsigned long AlarmDeviceDeviceMessages[] PROGMEM = {
    126983L, // Alert
    126984L, // Alert response
    126985L, // Alert text
    0
};

void OnN2kOpen() {
    Sensor* _sensor = &Sensor1;

    while (_sensor != nullptr) {
        if (_sensor->isActive()) {
            _sensor->AlarmScheduler.UpdateNextTime();
            _sensor->AlarmTextScheduler.UpdateNextTime();
            _sensor->TemperatureScheduler.UpdateNextTime();
        }
        _sensor = (Sensor*)_sensor->getNext();
    }
}

void CheckN2kSourceAddressChange() {

    if (NMEA2000.GetN2kSource(TemperaturDevice) != gN2KSource[TemperaturDevice]) {
        gN2KSource[TemperaturDevice] = NMEA2000.GetN2kSource(TemperaturDevice);
        gSaveParams = true;
    }

    if (NMEA2000.GetN2kSource(AlarmDevice) != gN2KSource[AlarmDevice]) {
        gN2KSource[AlarmDevice] = NMEA2000.GetN2kSource(AlarmDevice);
        gSaveParams = true;
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

    // init wifi
    wifiInit();
    InitDeviceId();

    Sensor1.setActive(true);

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

    // Enable multi device support for 3 devices
    NMEA2000.SetDeviceCount(2);

    NMEA2000.SetOnOpen(OnN2kOpen);



    // Set Product information for Temperatur Device
    NMEA2000.SetProductInformation(
        "1", // Manufacturer's Model serial code
        100, // Manufacturer's product code
        "TempMonitor",  // Manufacturer's Model ID
        Version,  // Manufacturer's Software version code
        Version, // Manufacturer's Model version
        1, // LEM
        0xffff,
        0xff,
        TemperaturDevice
    );

    // Set Product information for Alarm Device
    NMEA2000.SetProductInformation(
        "1", // Manufacturer's Model serial code
        100, // Manufacturer's product code
        "Alarm",  // Manufacturer's Model ID
        Version,  // Manufacturer's Software version code
        Version, // Manufacturer's Model version
        0, // LEM
        0xffff,
        0xff,
        AlarmDevice
    );

    // Set temperature device information
    NMEA2000.SetDeviceInformation(
        DeviceId1, // Unique number. Use e.g. Serial number.
        130, // Device function=Temperature. See DEVICE_FUNCTION (0 - 255) https://canboat.github.io/canboat/canboat.html#main
        75, // Device class=Sensor Communication Interface. DEVICE_CLASS (0 - 127) https://canboat.github.io/canboat/canboat.html#main
        2040, // Just choosen free from code list on MANUFACTURER_CODE (0 - 2047) https://canboat.github.io/canboat/canboat.html#main
        4, // Marine. INDUSTRY_CODE (0 - 7)
        TemperaturDevice
    );

    // Set temperature device information
    NMEA2000.SetDeviceInformation(
        DeviceId2, // Unique number. Use e.g. Serial number.
        120, // Device function=Alarm Enunciator. See DEVICE_FUNCTION (0 - 255) https://canboat.github.io/canboat/canboat.html#main
        20, // Device class=Safety systems. DEVICE_CLASS (0 - 127) https://canboat.github.io/canboat/canboat.html#main
        2040, // Just choosen free from code list on MANUFACTURER_CODE (0 - 2047) https://canboat.github.io/canboat/canboat.html#main
        4, // Marine. INDUSTRY_CODE (0 - 7)
        AlarmDevice
    );

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

    NMEA2000.SetN2kSource(gN2KSource[TemperaturDevice], TemperaturDevice);
    NMEA2000.SetN2kSource(gN2KSource[AlarmDevice], AlarmDevice);

    // Here we tell library, which PGNs we transmit
    NMEA2000.ExtendTransmitMessages(TemperaturDeviceMessages);
    NMEA2000.ExtendTransmitMessages(AlarmDeviceDeviceMessages);

    NMEA2000.Open();

    InitAlertsystem();

    // Initialize the Watchdog Timer
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL); //add current thread to WDT watch
}

void InitDeviceId() {
    uint8_t chipid[6];
    int i = 0;

    // Generate unique numbers from chip id
    esp_efuse_mac_get_default(chipid);

    for (int i = 0; i < 6; i++) {
        DeviceId1 += (chipid[i] << (7 * i));
        DeviceId2 += (chipid[i] << (8 * i)); // 8*i statt 7*i
    }
}

void UpdateAlertSystem() {
    if (NMEA2000.IsOpen()) InitAlertsystem();
}

void InitAlertsystem() {
    Sensor* sensor_ = &Sensor1;
    uint8_t index_ = 0;
    while (sensor_ != nullptr) {
        if (sensor_->isActive()) {

            sensor_->Alert.SetAlertSystem(
                index_,
                gN2KInstance + index_,
                NMEA2000.GetN2kSource(AlarmDevice),
                N2kts_AlertLanguageEnglishUS,
                const_cast<char*>(sensor_->GetDescriptionValue()),
                TempSourceNames[sensor_->GetSourceId()]
            );

            sensor_->FaultAlert.SetAlertSystem(
                index_,
                gN2KInstance + index_,
                NMEA2000.GetN2kSource(AlarmDevice),
                N2kts_AlertLanguageEnglishUS,
                (char*)"DS1820 faulty or not connected",
                TempSourceNames[sensor_->GetSourceId()]
            );


            sensor_->Alert.SetAlertDataSource(gN2KInstance + index_, 0, NMEA2000.GetN2kSource(TemperaturDevice));
            sensor_->Alert.SetAlertThreshold(t2kNAlertThresholdMethod(sensor_->GetThresholdMethod()), 0, sensor_->GetThresholdValue());
            sensor_->Alert.SetTemporarySilenceTime(sensor_->GetTemporarySilenceTime() * 60);

			sensor_->FaultAlert.SetAlertDataSource(gN2KInstance + index_, 0, NMEA2000.GetN2kSource(TemperaturDevice));
            sensor_->FaultAlert.SetAlertThreshold(t2kNAlertThresholdMethod(sensor_->GetThresholdMethod()), 0, sensor_->GetThresholdValue());
            sensor_->FaultAlert.SetTemporarySilenceTime(sensor_->GetTemporarySilenceTime() * 60);
        }
        index_++;
        sensor_ = (Sensor*)sensor_->getNext();
    }
}

void SendAlarm() {
    Sensor* sensor_ = &Sensor1;
    while (sensor_ != nullptr) {
        if (sensor_->isActive() && sensor_->AlarmScheduler.IsTime()) {
            sensor_->AlarmScheduler.UpdateNextTime();

            tN2kMsg N2kMsg_;

            sensor_->FaultAlert.SetN2kAlert(N2kMsg_);
            NMEA2000.SendMsg(N2kMsg_, AlarmDevice);

            sensor_->Alert.SetN2kAlert(N2kMsg_);
            NMEA2000.SendMsg(N2kMsg_, AlarmDevice);
        }
        sensor_ = (Sensor*)sensor_->getNext();
    }
}

void SendAlarmText() {
    Sensor* sensor_ = &Sensor1;
    while (sensor_ != nullptr) {
        if (sensor_->isActive() && sensor_->AlarmTextScheduler.IsTime()) {

            sensor_->AlarmTextScheduler.UpdateNextTime();

            tN2kMsg N2kMsg_;

            sensor_->FaultAlert.SetN2kAlertText(N2kMsg_);
            NMEA2000.SendMsg(N2kMsg_, AlarmDevice);

            sensor_->Alert.SetN2kAlertText(N2kMsg_);
            NMEA2000.SendMsg(N2kMsg_, AlarmDevice);

        }
        sensor_ = (Sensor*)sensor_->getNext();
    }
}


void SendTemperatur(uint8_t SID, uint8_t TempInstance) {
    tN2kMsg N2kMsg_;

    Sensor* sensor_ = &Sensor1;
    uint8_t index_ = TempInstance;

    while (sensor_ != nullptr) {
        if (sensor_->isActive() && sensor_->TemperatureScheduler.IsTime()) {
            sensor_->TemperatureScheduler.UpdateNextTime();

            tN2kTempSource TempSource_ = tN2kTempSource(sensor_->GetSourceId());
            double TempValue_ = CToKelvin(sensor_->GetSensorValue());

            SetN2kPGN130312(N2kMsg_, SID, index_, TempSource_, TempValue_, N2kDoubleNA);
            NMEA2000.SendMsg(N2kMsg_, TemperaturDevice);

            SetN2kPGN130316(N2kMsg_, SID, index_, TempSource_, TempValue_, N2kDoubleNA);
            NMEA2000.SendMsg(N2kMsg_, TemperaturDevice);

            if (TempSource_ == N2kts_OutsideTemperature) {
                SetN2kPGN130310(N2kMsg_, SID, N2kDoubleNA, TempValue_);
                NMEA2000.SendMsg(N2kMsg_, TemperaturDevice);
            }

            if (TempSource_ == N2kts_SeaTemperature) {
                SetN2kPGN130310(N2kMsg_, SID, TempValue_, N2kDoubleNA);
                NMEA2000.SendMsg(N2kMsg_, TemperaturDevice);
            }

        }

        index_++;
        sensor_ = (Sensor*)sensor_->getNext();
    }

}



void loop() {
    wifiLoop();

    SendTemperatur(gN2KSID, gN2KInstance);
    SendAlarm();
    SendAlarmText();

    NMEA2000.ParseMessages();

    CheckN2kSourceAddressChange();

    // Dummy to empty input buffer to avoid board to stuck with e.g. NMEA Reader
    if (Serial.available()) {
        Serial.read();
    }

    esp_task_wdt_reset();
}

double GetTemperatur(int Index) {
    double T;
    T = sensors.getTempCByIndex(Index);
    //if (T == -127) T = 9.99;
    return T;
}

void loop2(void* parameter) {
    esp_task_wdt_add(NULL); //add current thread to WDT watch (Core 0)

    
    for (;;) {   // Endless loop
        sensors.requestTemperatures(); // Send the command to get temperatures
       

        Sensor* _sensor = &Sensor1;
        uint8_t _index = 0;
        while (_sensor != nullptr) {
            if (_sensor->isActive()) {
                double d = GetTemperatur(_index);
                _sensor->SetSensorValue(d);
                _sensor->Alert.TestAlertThreshold(d);
            }
            _index++;
            _sensor = (Sensor*)_sensor->getNext();
        }

        esp_task_wdt_reset();

        vTaskDelay(1000);
    }
}