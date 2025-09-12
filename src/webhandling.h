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
	"99",
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
	"None",
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
#define CONFIG_VERSION "B2"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN  GPIO_NUM_13

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN

#define ON_LEVEL HIGH





extern void wifiInit();
extern void wifiLoop();

extern IotWebConf iotWebConf;

class Sensor : public iotwebconf::ParameterGroup {
public:
    explicit Sensor(const char* id, const char* name)
        : ParameterGroup(id, name),
        _value(9.99),
        _next(nullptr),
        _LocationParam("Location", _locationId, _locationValue, STRING_LEN, "Location"),
        _SourceParam(
            "Source", 
            _sourceId, 
            _sourceValue, 
            sizeof(_sourceValue),
            (char*)TempSourceValues, 
            (char*)TempSourceNames,
            sizeof(TempSourceValues) / sizeof(TempSourceValues[0]),
            sizeof(TempSourceNames[0]), 
            "0"),
        _ThresholdParam("Threshold (&deg;C)", _thresholdId, _thresholdValue, NUMBER_LEN, "0", "0..200", "min='0' max='200' step='1'"),
        _MethodParam(
            "Method", 
            _methodId, 
            _methodValue, 
            sizeof(_methodValue),
            (char*)ThresholdMethodValues, 
            (char*)ThresholdMethodNames,
            sizeof(ThresholdMethodValues) / sizeof(ThresholdMethodValues[0]),
            sizeof(ThresholdMethodNames[0]), 
            "2"),
        _DescriptionParam("Alert Description", _descriptionId, _descriptionValue, STRING_LEN, "Alert"),
        _TemporarySilenceParam("Temporary silence time (minutes)", _silenceId, _silenceValue, NUMBER_LEN, "60", "0..300", "min='0' max='300' step='1'"),
        AlarmScheduler(false, 500, 100),
        AlarmTextScheduler(false, 10000, 2000),
        TemperatureScheduler(false, 2000, 500),
        Alert(
            N2kts_AlertTypeCaution,
            N2kts_AlertCategoryTechnical,
            100,
            N2kts_AlertTriggerAuto,
            100,
            N2kts_AlertYes,
            N2kts_AlertYes,
            N2kts_AlertNo,
            1
        ),
        FaultAlert(
            N2kts_AlertTypeAlarm,
            N2kts_AlertCategoryTechnical,
            101,
            N2kts_AlertTriggerAuto,
            100,
            N2kts_AlertYes,
            N2kts_AlertYes,
            N2kts_AlertNo,
            10
        )
    {
        snprintf(_sourceId, STRING_LEN, "%s-source", this->getId());
        snprintf(_thresholdId, STRING_LEN, "%s-threshold", this->getId());
        snprintf(_methodId, STRING_LEN, "%s-method", this->getId());
        snprintf(_descriptionId, STRING_LEN, "%s-description", this->getId());
        snprintf(_locationId, STRING_LEN, "%s-location", this->getId());
        snprintf(_silenceId, STRING_LEN, "%s-silence", this->getId());

        addItem(&_LocationParam);
        addItem(&_SourceParam);
        addItem(&_ThresholdParam);
        addItem(&_MethodParam);
        addItem(&_DescriptionParam);
        addItem(&_TemporarySilenceParam);
    }

    void setNext(Sensor* next) { _next = next; }
    Sensor* getNext() const { return _next; }

    void SetSensorValue(double v) {
        _value = v;
		Alert.TestAlertThreshold(_value);

        if (v == -127.0) {
            FaultAlert.SetAlertExceeded();
        }
        else {
            FaultAlert.ResetAlert();
            Alert.TestAlertThreshold(_value);
        }
    }

	bool isActive() const { return GetSourceId() != 99; }

    double GetSensorValue() const { return _value; }
    uint8_t GetSourceId() const { return static_cast<uint8_t>(atoi(_sourceValue)); }
    const char* GetSourceName() const {
        for (size_t i = 0; i < sizeof(TempSourceValues) / sizeof(TempSourceValues[0]); ++i) {
            if (strcmp(_sourceValue, TempSourceValues[i]) == 0) {
                return TempSourceNames[i];
            }
        }
        return TempSourceNames[0]; // Fallback: "None"
    }
    uint8_t GetThresholdMethod() const { return static_cast<uint8_t>(atoi(_methodValue)); }
    uint32_t GetThresholdValue() const { return static_cast<uint32_t>(atoi(_thresholdValue)); }
    uint16_t GetTemporarySilenceTime() const { return static_cast<uint16_t>(atoi(_silenceValue)); }
    const char* GetDescriptionValue() const { return _descriptionValue; }
    const char* GetLocationValue() const { return _locationValue; }
    void SetLocationValue(const char* location) { strncpy(_locationValue, location, STRING_LEN); _locationValue[STRING_LEN-1] = '\0'; }

    void resetToDefaults() {
        _value = 9.99;
        _SourceParam.applyDefaultValue();
        _ThresholdParam.applyDefaultValue();
        _MethodParam.applyDefaultValue();
        _DescriptionParam.applyDefaultValue();
        _LocationParam.applyDefaultValue();
        _TemporarySilenceParam.applyDefaultValue();
    }

    tN2kSyncScheduler AlarmScheduler;
    tN2kSyncScheduler AlarmTextScheduler;
    tN2kSyncScheduler TemperatureScheduler;

    tN2kAlert Alert;
    tN2kAlert FaultAlert;

private:
    Sensor* _next;

    char _sourceId[STRING_LEN];
    char _thresholdId[STRING_LEN];
    char _methodId[STRING_LEN];
    char _descriptionId[STRING_LEN];
    char _locationId[STRING_LEN];
    char _silenceId[STRING_LEN];

    char _sourceValue[3]{};
    char _thresholdValue[NUMBER_LEN]{};
    char _methodValue[3]{};
    char _descriptionValue[STRING_LEN]{};
    char _locationValue[STRING_LEN]{};
    char _silenceValue[STRING_LEN]{};

    iotwebconf::SelectParameter _SourceParam;
    iotwebconf::NumberParameter _ThresholdParam;
    iotwebconf::SelectParameter _MethodParam;
    iotwebconf::TextParameter _DescriptionParam;
    iotwebconf::TextParameter _LocationParam;
    iotwebconf::NumberParameter _TemporarySilenceParam;

    double _value;

};

class NMEAConfig : public iotwebconf::ParameterGroup {
public:
    NMEAConfig()
        : ParameterGroup("nmeaconfig", "NMEA configuration"),
        _InstanceParam("Instance", _instanceID, _InstanceValue, NUMBER_LEN, "255", "1..255", "min='1' max='254' step='1'"),
        _SIDParam("SID", _sidID, _SIDValue, NUMBER_LEN, "255", "1..255", "min='1' max='255' step='1'")
    {
        snprintf(_instanceID, STRING_LEN, "%s-instance", this->getId());
        snprintf(_sidID, STRING_LEN, "%s-sid", this->getId());

        this->addItem(&_InstanceParam);
        this->addItem(&_SIDParam);
    }

    uint8_t GetInstance() const { return static_cast<uint8_t>(atoi(_InstanceValue)); }
    uint8_t GetSID() const { return static_cast<uint8_t>(atoi(_SIDValue)); }

private:
    char _instanceID[STRING_LEN];
    char _sidID[STRING_LEN];

    char _InstanceValue[NUMBER_LEN]{};
    char _SIDValue[NUMBER_LEN]{};

    iotwebconf::NumberParameter _InstanceParam;
    iotwebconf::NumberParameter _SIDParam;
};

extern Sensor Sensor1;
extern Sensor Sensor2;
extern Sensor Sensor3;
extern Sensor Sensor4;

#endif