// webhandling.h

#ifndef _WEBHANDLING_h
#define _WEBHANDLING_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "common.h"

#include <IotWebConf.h>
#include <IotWebConfOptionalGroup.h>
#include <N2kTimer.h>
#include <N2kAlerts.h>
#include <WebSerial.h>


#define STRING_LEN 64
#define NUMBER_LEN 5

static char ThresholdMethodValues[][3] PROGMEM = {
    "0",
    "1",
    "2"
};

static char ThresholdMethodNames[][14] PROGMEM = {
    "equal",
    "lower than",
    "greater than"
};

static char TempSourceValues[][3] PROGMEM = {
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

static char TempSourceNames[][27] PROGMEM = {
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
#define CONFIG_VERSION "B1"

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
        snprintf(_sourceId, STRING_LEN, "%s-source", this->getId());
        snprintf(_thresholdId, STRING_LEN, "%s-threshold", this->getId());
        snprintf(_methodId, STRING_LEN, "%s-method", this->getId());
        snprintf(_descriptionId, STRING_LEN, "%s-description", this->getId());
        snprintf(_silenceId, STRING_LEN, "%s-silence", this->getId());

        this->addItem(&_SourceParam);
        this->addItem(&_ThresholdParam);
        this->addItem(&_MethodParam);
        this->addItem(&_DescriptionParam);
        this->addItem(&_TemporarySilenceParam);

        _value = 9.99;
    }

    char _sourceValue[STRING_LEN];
    char _thresholdValue[NUMBER_LEN];
    char _methodValue[STRING_LEN];
    char _descriptionValue[STRING_LEN];
    char _silenceValue[STRING_LEN];

    iotwebconf::SelectParameter _SourceParam = iotwebconf::SelectParameter(
        "Source",
        _sourceId,
        _sourceValue,
        sizeof(TempSourceNames[0]),
        (char*)TempSourceValues,
        (char*)TempSourceNames,
        sizeof(TempSourceValues) / sizeof(TempSourceValues[0]),
        sizeof(TempSourceNames[0]),
        "1"
    );

    iotwebconf::NumberParameter _ThresholdParam = iotwebconf::NumberParameter(
        "Threshold (&deg;C)", _thresholdId, _thresholdValue, NUMBER_LEN, "0", "0..200", "min='0' max='200' step='1'");

    iotwebconf::SelectParameter _MethodParam = iotwebconf::SelectParameter(
        "Method",
        _methodId,
        _methodValue,
        sizeof(ThresholdMethodNames[0]),
        (char*)ThresholdMethodValues,
        (char*)ThresholdMethodNames,
        sizeof(ThresholdMethodValues) / sizeof(ThresholdMethodNames[0]),
        sizeof(ThresholdMethodNames[0]),
        "2"
    );

    iotwebconf::TextParameter _DescriptionParam = iotwebconf::TextParameter(
        "Alert Description", _descriptionId, _descriptionValue, STRING_LEN, "Alert");

    iotwebconf::NumberParameter _TemporarySilenceParam = iotwebconf::NumberParameter(
        "Temporary silence time (minutes)", _silenceId, _silenceValue, NUMBER_LEN, "60", "0..300", "min='0' max='300' step='1'");

    void SetSensorValue(const double v) {
        _value = v;
        if (v == -127.0) {
            _FaultAlert.SetAlertExceeded();
        }
        else {
            _FaultAlert.ResetAlert();
            _Alert.TestAlertThreshold(_value);
        }
    }

    double GetSensorValue() { return _value; }
    uint8_t GetSourceId() { return atoi(_sourceValue); }
    uint8_t GetThresholdMethod() { return atoi(_methodValue); }
    uint32_t GetThresholdValue() { return atoi(_thresholdValue); }
    uint16_t GetTemporarySilenceTime() { return atoi(_silenceValue); }

    tN2kSyncScheduler _AlarmScheduler = tN2kSyncScheduler(false, 500, 100);
    tN2kSyncScheduler _TextAlarmScheduler = tN2kSyncScheduler(false, 10000, 2000);
    tN2kSyncScheduler _SchedulerTemperatur = tN2kSyncScheduler(false, 2000, 500);

    tN2kAlert _Alert = tN2kAlert(
        N2kts_AlertTypeCaution,
        N2kts_AlertCategoryTechnical,
        100,
        N2kts_AlertTriggerAuto,
        100,
        N2kts_AlertYes,
        N2kts_AlertYes,
        N2kts_AlertNo,
        1
    );

    tN2kAlert _FaultAlert = tN2kAlert(
        N2kts_AlertTypeAlarm,
        N2kts_AlertCategoryTechnical,
        101,
        N2kts_AlertTriggerAuto,
        100,
        N2kts_AlertYes,
        N2kts_AlertYes,
        N2kts_AlertNo,
        10
    );

private:
    char _sourceId[STRING_LEN];
    char _thresholdId[STRING_LEN];
    char _methodId[STRING_LEN];
    char _descriptionId[STRING_LEN];
    char _silenceId[STRING_LEN];
    double _value;
};

extern Sensor Sensor1;
extern Sensor Sensor2;
extern Sensor Sensor3;
extern Sensor Sensor4;

#endif