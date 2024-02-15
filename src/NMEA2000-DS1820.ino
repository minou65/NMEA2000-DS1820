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

#include <Arduino.h>
#include "common.h"
#include "webhandling.h"

#include <OneWire.h>
#include <DallasTemperature.h>

#include <N2kMessages.h>
#include <NMEA2000_CAN.h>
#include <NMEA2000.h>
#include "N2kAlerts.h"

uint8_t gN2KSource[] = { 22, 23 };
uint8_t gN2KInstance = 1;
uint8_t gN2KSID = 255;

uint64_t DeviceId1 = 0;
uint64_t DeviceId2 = 0;

char Version[] = "0.0.0.11 (2024-02-14)"; // Manufacturer's Software version code

// Setup a OneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Task handle for OneWire read (Core 0 on ESP32)
TaskHandle_t TaskHandle;

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
            _sensor->SchedulerAlarm.UpdateNextTime();
            _sensor->SchedulerAlarmText.UpdateNextTime();
            _sensor->SchedulerTemperatur.UpdateNextTime();
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

    // Init USB serial port
    Serial.begin(115200);
    delay(10);

    // init sensors
    sensors.begin(); 
    sensors.setResolution(12);

    gDeviceCount = sensors.getDeviceCount();

    // init wifi
    wifiInit();
    InitDeviceId();

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

    // Enable multi device support for 3 devices
    NMEA2000.SetDeviceCount(2);

    NMEA2000.SetOnOpen(OnN2kOpen);

    // Reserve enough buffer for sending all messages. This does not work on small memory devices like Uno or Mega
    NMEA2000.SetN2kCANMsgBufSize(8);
    NMEA2000.SetN2kCANReceiveFrameBufSize(150);
    NMEA2000.SetN2kCANSendFrameBufSize(150);

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

    // If you also want to see all traffic on the bus use N2km_ListenAndNode instead of N2km_NodeOnly below
    NMEA2000.SetN2kSource(gN2KSource[TemperaturDevice], TemperaturDevice);
    NMEA2000.SetN2kSource(gN2KSource[AlarmDevice], AlarmDevice);

    // Here we tell library, which PGNs we transmit
    NMEA2000.ExtendTransmitMessages(TemperaturDeviceMessages);
    NMEA2000.ExtendTransmitMessages(AlarmDeviceDeviceMessages);

    NMEA2000.Open();

    InitAlertsystem();

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

void InitAlertsystem() {
    // if (!NMEA2000.IsOpen()) { return; };

    Sensor* _sensor = &Sensor1;
    uint8_t _index = 0;
    while (_sensor != nullptr) {
        if (_sensor->isActive()) {

            _sensor->Alert.SetAlertSystem(_index, gN2KInstance + _index, NMEA2000.GetN2kSource(AlarmDevice), N2kts_AlertLanguageEnglishUS, _sensor->descriptionValue, TempSourceNames[_sensor->GetSourceId()]);

            _sensor->Alert.SetAlertDataSource(gN2KInstance + _index, 0, NMEA2000.GetN2kSource(TemperaturDevice));
            _sensor->Alert.SetAlertThreshold(t2kNAlertThresholdMethod(_sensor->GetThresholdMethod()), 0, _sensor->GetThresholdValue());

            _sensor->Alert.SetTemporarySilenceTime(_sensor->GetTemporarySilenceTime() * 60);
        }
        _index++;
        _sensor = (Sensor*)_sensor->getNext();
    }
}

void SendAlarm() {
    tN2kMsg N2kMsg;

    Sensor* _sensor = &Sensor1;
    uint8_t _index = 0;

    while (_sensor != nullptr) {
        if (_sensor->isActive() && _sensor->SchedulerAlarm.IsTime()) {
            _sensor->SchedulerAlarm.UpdateNextTime();

            _sensor->Alert.SetN2kAlert(N2kMsg);
            NMEA2000.SendMsg(N2kMsg, AlarmDevice);
        }
        _index++;
        _sensor = (Sensor*)_sensor->getNext();
    }
}

void SendAlarmText() {
    tN2kMsg N2kMsg;

    Sensor* _sensor = &Sensor1;
    uint8_t _index = 0;

    while (_sensor != nullptr) {
        if (_sensor->isActive() && _sensor->SchedulerAlarmText.IsTime()) {
            _sensor->SchedulerAlarmText.UpdateNextTime();

            _sensor->Alert.SetN2kAlertText(N2kMsg);
            NMEA2000.SendMsg(N2kMsg, AlarmDevice);
        }
        _index++;
        _sensor = (Sensor*)_sensor->getNext();
    }
}

void SendTemperatur(uint8_t SID_, uint8_t TempInstance_) {
    tN2kMsg N2kMsg;

    Sensor* _sensor = &Sensor1;
    uint8_t _index = TempInstance_;

    while (_sensor != nullptr) {
        if (_sensor->isActive() && _sensor->SchedulerTemperatur.IsTime()) {
            _sensor->SchedulerTemperatur.UpdateNextTime();

            tN2kTempSource _TempSource = tN2kTempSource(_sensor->GetSourceId());
            double _TempValue = _sensor->GetSensorValue();

            SetN2kPGN130312(N2kMsg, SID_, _index, _TempSource, _TempValue, N2kDoubleNA);
            NMEA2000.SendMsg(N2kMsg, TemperaturDevice);

            SetN2kPGN130316(N2kMsg, SID_, _index, _TempSource, _TempValue, N2kDoubleNA);
            NMEA2000.SendMsg(N2kMsg, TemperaturDevice);

            if (_TempSource == N2kts_OutsideTemperature) {
                SetN2kPGN130310(N2kMsg, SID_, N2kDoubleNA, _TempValue);
                NMEA2000.SendMsg(N2kMsg, TemperaturDevice);
            }

            if (_TempSource == N2kts_SeaTemperature) {
                SetN2kPGN130310(N2kMsg, SID_, _TempValue, N2kDoubleNA);
                NMEA2000.SendMsg(N2kMsg, TemperaturDevice);
            }
        }
        _index++;
        _sensor = (Sensor*)_sensor->getNext();
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
}

double GetTemperatur(int Index) {
    double T;
    T = sensors.getTempCByIndex(Index);
    if (T == -127) T = 0;
    return T;
}

void loop2(void* parameter) {
    
    for (;;) {   // Endless loop
        sensors.requestTemperatures(); // Send the command to get temperatures
        vTaskDelay(500);

        Sensor* _sensor = &Sensor1;
        uint8_t _index = 0;
        while (_sensor != nullptr) {
            if (_sensor->isActive()) {
                double d = GetTemperatur(_index);
                _sensor->SetSensorValue(d);
            }
            _index++;
            _sensor = (Sensor*)_sensor->getNext();
        }
    }
}


