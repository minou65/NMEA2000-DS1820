// webhandling.h

#ifndef _WEBHANDLING_h
#define _WEBHANDLING_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include <IotWebConf.h>
#include <IotWebConfOptionalGroup.h>
#include <N2kTimer.h>
#include <N2kAlerts.h>
#include <WebSerial.h>


#define STRING_LEN 64
#define NUMBER_LEN 5

static char ThresholdMethodValues[][STRING_LEN] PROGMEM = {
    "0",
    "1",
    "2"
};

static char ThresholdMethodNames[][STRING_LEN] PROGMEM = {
    "equal",
    "lower than",
    "greater than"
};

static char TempSourceValues[][STRING_LEN] PROGMEM = {
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "13",
    "14",
    "15"
};

static char TempSourceNames[][STRING_LEN] PROGMEM = {
    "Sea water temperature",
    "Outside temperature",
    "Inside temperature",
    "Engine room temperature",
    "Main cabin temperature",
    "Live well temperature",
    "Bait well temperature",
    "Refrigeration temperature",
    "Heating system temperature",
    "Freezer temperature",
    "Exhaust gas temperature",
    "Shaft seal temparature"
};

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] PROGMEM = "123456789";

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "A9"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN  GPIO_NUM_13

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN
#if ESP32 
#define ON_LEVEL HIGH
#else
#define ON_LEVEL LOW
#endif


extern void wifiInit();
extern void wifiLoop();

extern IotWebConf iotWebConf;

class Sensor : public iotwebconf::ChainedParameterGroup {
public:
    Sensor(const char* id) : ChainedParameterGroup(id, "Sensor") {
        // -- Update parameter Ids to have unique ID for all parameters within the application.
        snprintf(sourceId, STRING_LEN, "%s-source", this->getId());
        snprintf(thresholdId, STRING_LEN, "%s-threshold", this->getId());
        snprintf(methodId, STRING_LEN, "%s-method", this->getId());
        snprintf(descriptionId, STRING_LEN, "%s-description", this->getId());
        snprintf(silenceId, STRING_LEN, "%s-silence", this->getId());

        // -- Add parameters to this group.
        this->addItem(&this->SourceParam);
        this->addItem(&this->ThresholdParam);
        this->addItem(&this->MethodParam);
        this->addItem(&this->DescriptionParam);
        this->addItem(&this->TemporarySilenceParam);

        value = 9.99;
    }

    char sourceValue[STRING_LEN];
    char thresholdValue[NUMBER_LEN];
    char methodValue[STRING_LEN];
    char descriptionValue[STRING_LEN];
    char silenceValue[STRING_LEN];
    iotwebconf::SelectParameter SourceParam = iotwebconf::SelectParameter("Source", sourceId, sourceValue, STRING_LEN, 
        (char*)TempSourceValues, (char*)TempSourceNames, sizeof(TempSourceValues) / STRING_LEN, STRING_LEN, "1");

    iotwebconf::NumberParameter ThresholdParam = iotwebconf::NumberParameter("Threshold (&deg;C)", thresholdId, thresholdValue, NUMBER_LEN, "0", "0..200", "min='0' max='200' step='1'");

    iotwebconf::SelectParameter MethodParam = iotwebconf::SelectParameter("Method", methodId, methodValue, STRING_LEN,
        (char*)ThresholdMethodValues, (char*)ThresholdMethodNames, sizeof(ThresholdMethodValues) / STRING_LEN, STRING_LEN, "2");

    iotwebconf::TextParameter DescriptionParam = iotwebconf::TextParameter("Alert Description", descriptionId, descriptionValue, STRING_LEN, "Alert");

    iotwebconf::NumberParameter TemporarySilenceParam = iotwebconf::NumberParameter("Temporary silence time (minutes)", silenceId, silenceValue, NUMBER_LEN, "60", "0..300", "min='0' max='300' step='1'");

    void SetSensorValue(const double v) { 
        value = v; 
		Alert.TestAlertThreshold(value);
    };
    double GetSensorValue() { return value; };
    uint8_t GetSourceId() { return atoi(sourceValue); };
    uint8_t GetThresholdMethod() { return atoi(methodValue); };
    uint32_t GetThresholdValue() { return atoi(thresholdValue); };
    uint16_t GetTemporarySilenceTime() { return atoi(silenceValue); };

    tN2kSyncScheduler AlarmScheduler = tN2kSyncScheduler(false, 500, 100);
    tN2kSyncScheduler TextAlarmScheduler = tN2kSyncScheduler(false, 10000, 2000);
    tN2kSyncScheduler SchedulerTemperatur = tN2kSyncScheduler(false, 2000, 500);

    tN2kAlert Alert = tN2kAlert(N2kts_AlertTypeCaution, N2kts_AlertCategoryTechnical, 100, N2kts_AlertTriggerAuto, 100, N2kts_AlertYes, N2kts_AlertYes);

private:
    char sourceId[STRING_LEN];
    char thresholdId[STRING_LEN];
    char methodId[STRING_LEN];
    char descriptionId[STRING_LEN];
    char silenceId[String_Len];
    double value;

};

class NMEAConfig : public iotwebconf::ParameterGroup {
public:
    NMEAConfig() : ParameterGroup("nmeaconfig", "NMEA configuration") {
        snprintf(instanceID, STRING_LEN, "%s-instance", this->getId());
        snprintf(sidID, STRING_LEN, "%s-sid", this->getId());
        snprintf(sourceID, STRING_LEN, "%s-n2ksource", this->getId());

        this->addItem(&this->InstanceParam);
        this->addItem(&this->SIDParam);

         //iotWebConf.addHiddenParameter(&this->SourceParam);
        this->addItem(&this->SourceParam);
        SourceParam.visible = false;

        // additional sources
        snprintf(sourceIDAlert, STRING_LEN, "%s-sourceAlert", this->getId());

        // iotWebConf.addHiddenParameter(&this->SourceAlertParam);
        this->addItem(&this->SourceAlertParam);
        SourceAlertParam.visible = false;

    }

    uint8_t Instance() { return atoi(InstanceValue); };
    uint8_t SID() { return atoi(SIDValue); };
    uint8_t Source() { return atoi(SourceValue); };

    void SetSource(uint8_t source_) {
        String s;
        s = (String)source_;
        strncpy(SourceParam.valueBuffer, s.c_str(), NUMBER_LEN);
    }

    // additional sources
    uint8_t SourceAlert() { return atoi(SourceAlertValue); };

    void SetSourceAlert(uint8_t source_) {
        String s;
        s = (String)source_;
        strncpy(SourceAlertParam.valueBuffer, s.c_str(), NUMBER_LEN);
    }

private:
    iotwebconf::NumberParameter InstanceParam = iotwebconf::NumberParameter("Instance", instanceID, InstanceValue, NUMBER_LEN, "255", "1..255", "min='1' max='254' step='1'");
    iotwebconf::NumberParameter SIDParam = iotwebconf::NumberParameter("SID", sidID, SIDValue, NUMBER_LEN, "255", "1..255", "min='1' max='255' step='1'");
    iotwebconf::NumberParameter SourceParam = iotwebconf::NumberParameter("N2kSource", sourceID, SourceValue, NUMBER_LEN, "22");

    char InstanceValue[NUMBER_LEN];
    char SIDValue[NUMBER_LEN];
    char SourceValue[NUMBER_LEN];


    char instanceID[STRING_LEN];
    char sidID[STRING_LEN];
    char sourceID[STRING_LEN];

    // additional sources
    iotwebconf::NumberParameter SourceAlertParam = iotwebconf::NumberParameter("N2kSourceAlert", sourceIDAlert, SourceAlertValue, NUMBER_LEN, "23");
    char SourceAlertValue[NUMBER_LEN];
    char sourceIDAlert[STRING_LEN];
};

extern Sensor Sensor1;
extern Sensor Sensor2;
extern Sensor Sensor3;
extern Sensor Sensor4;

#endif