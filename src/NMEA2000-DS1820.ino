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

// Set time offsets
#define SlowDataUpdatePeriod 1000  // Time between CAN Messages sent
#define TempSendOffset 0

#define OutsideTemperature 0
#define CabinTemperature 1

double gOutsideTemperature = 0;
double gCabinTemperature = 0;
uint8_t gN2KSource = 22;

char Version[] = "0.0.0.7 (2023-07-02)"; // Manufacturer's Software version code

// Setup a OneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Task handle for OneWire read (Core 0 on ESP32)
TaskHandle_t TaskHandle;

// Pass OneWire reference to Dallas Temperature
DallasTemperature sensors(&oneWire);

// List here messages your device will transmit.
const unsigned long TransmitMessages[] PROGMEM = { 
                                                    130310L,
                                                    130311L,
                                                    130312L,
                                                   0 
                                                 };


void setup() {
    uint8_t chipid[6];
    uint32_t id = 0;
    int i = 0;

    // Generate unique number from chip id
    esp_efuse_mac_get_default(chipid);
    for (i = 0; i < 6; i++) id += (chipid[i] << (7 * i));

    // Init USB serial port
    Serial.begin(115200);
    delay(10);

    // init wifi
    wifiInit();

    // init sensors
    sensors.begin(); 
    sensors.setResolution(12);

    // Create GetTemperature task for core 0, loop() runs on core 1
    xTaskCreatePinnedToCore(
        GetTemperature, /* Function to implement the task */
        "TaskHandle", /* Name of the task */
        10000,  /* Stack size in words */
        NULL,  /* Task input parameter */
        0,  /* Priority of the task */
        &TaskHandle,  /* Task handle. */
        0 /* Core where the task should run */
    );

    // Reserve enough buffer for sending all messages. This does not work on small memory devices like Uno or Mega
    NMEA2000.SetN2kCANMsgBufSize(8);
    NMEA2000.SetN2kCANReceiveFrameBufSize(150);
    NMEA2000.SetN2kCANSendFrameBufSize(150);

    // Set Product information
    NMEA2000.SetProductInformation(
        "1", // Manufacturer's Model serial code
        100, // Manufacturer's product code
        "TempMonitor",  // Manufacturer's Model ID
        Version,  // Manufacturer's Software version code
        Version // Manufacturer's Model version
    );

    // Set device information
    NMEA2000.SetDeviceInformation(
        id, // Unique number. Use e.g. Serial number.
        130, // Device function=Temperature. See codes on https://web.archive.org/web/20190531120557/https://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
        75, // Device class=Sensor Communication Interface. See codes on https://web.archive.org/web/20190531120557/https://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
        2040 // Just choosen free from code list on https://web.archive.org/web/20190529161431/http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
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
    NMEA2000.SetMode(tNMEA2000::N2km_NodeOnly, gN2KSource);
    
    // Here we tell library, which PGNs we transmit
    NMEA2000.ExtendTransmitMessages(TransmitMessages);


    NMEA2000.Open();
}

void GetTemperature(void* parameter) {
    float tmp = 0;
    for (;;) {   // Endless loop
        sensors.requestTemperatures(); // Send the command to get temperatures
        vTaskDelay(100);
        gCabinTemperature = sensors.getTempCByIndex(CabinTemperature);
        gOutsideTemperature = sensors.getTempCByIndex(OutsideTemperature);
        if (gCabinTemperature == -127) gCabinTemperature = 0;
        if (gOutsideTemperature == -127) gOutsideTemperature = 0;
        vTaskDelay(100);
    }
}

bool IsTimeToUpdate(unsigned long NextUpdate) {
    return (NextUpdate < millis());
}

unsigned long InitNextUpdate(unsigned long Period, unsigned long Offset = 0) {
    return millis() + Period + Offset;
}

void SetNextUpdate(unsigned long& NextUpdate, unsigned long Period) {
    while (NextUpdate < millis()) NextUpdate += Period;
}

void SendN2kTemperature(void) {
    static unsigned long SlowDataUpdated = InitNextUpdate(SlowDataUpdatePeriod, TempSendOffset);
    tN2kMsg N2kMsg;

    if (IsTimeToUpdate(SlowDataUpdated)) {
        SetNextUpdate(SlowDataUpdated, SlowDataUpdatePeriod);

        Serial.printf("Cabin temperature  : %3.1f �C \n", gCabinTemperature);
        Serial.printf("Outside temperature: %3.1f �C \n", gOutsideTemperature);


        // Set N2K message
        SetN2kTemperature(N2kMsg, 255, 1, N2kts_MainCabinTemperature, CToKelvin(gCabinTemperature));

        // Send message
        NMEA2000.SendMsg(N2kMsg);

        // Set N2K message
        SetN2kTemperature(N2kMsg, 255, 2, N2kts_OutsideTemperature, CToKelvin(gOutsideTemperature));

        // Send message
        NMEA2000.SendMsg(N2kMsg);

        // Set N2K message
        SetN2kOutsideEnvironmentalParameters(N2kMsg, 255, N2kDoubleNA, CToKelvin(gOutsideTemperature));

        // Send message
        NMEA2000.SendMsg(N2kMsg);
    }
}


void loop() {
    wifiLoop();
    SendN2kTemperature();
    NMEA2000.ParseMessages();

    // Dummy to empty input buffer to avoid board to stuck with e.g. NMEA Reader
    if (Serial.available()) {
        Serial.read();
    }
}


