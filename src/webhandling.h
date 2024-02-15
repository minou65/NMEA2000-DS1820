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
#include <DNSServer.h>
#include <N2kTimer.h>
#include "N2kAlerts.h"


#define STRING_LEN 128
#define NUMBER_LEN 32

static char TThresholdMethodValues[][STRING_LEN] = {
    "0",
    "1",
    "2"
};

static char TThresholdMethodNames[][STRING_LEN] = {
    "equal",
    "lower than",
    "greater than"
};

static char TempSourceValues[][STRING_LEN] = {
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
//    "9",
//    "10",
//    "11",
//    "12",
    "13",
    "14",
    "15"
};

static char TempSourceNames[][STRING_LEN] = {
    "Sea water temperature",
    "Outside temperature",
    "Inside temperature",
    "Engine room temperature",
    "Main cabin temperature",
    "Live well temperature",
    "Bait well temperature",
    "Refrigeration temperature",
    "Heating system temperature",
//    "Dew point temperature",
//    "Apparent wind chill temperature",
//    "Theoretical wind chill temperature",
//    "Heat index temperature",
    "Freezer temperature",
    "Exhaust gas temperature",
    "Shaft seal temparature"
};


#define HTML_Start_Doc "<!DOCTYPE html>\
    <html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>\
    <title>{v}</title>\
";

#define HTML_Start_Body "</head><body>";
#define HTML_Start_Fieldset "<fieldset align=left style=\"border: 1px solid\">";
#define HTML_Start_Table "<table border=0 align=center>";
#define HTML_End_Table "</table>";
#define HTML_End_Fieldset "</fieldset>";
#define HTML_End_Body "</body>";
#define HTML_End_Doc "</html>";
#define HTML_Fieldset_Legend "<legend>{l}</legend>"
#define HTML_Table_Row "<tr><td>{n}</td><td>{v}</td></tr>";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "123456789";

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "A7"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN  GPIO_NUM_36

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

class Sensor : public iotwebconf::ChainedParameterGroup {
public:
    Sensor(const char* id) : ChainedParameterGroup(id, "Sensor") {
        // -- Update parameter Ids to have unique ID for all parameters within the application.
        snprintf(sourceId, STRING_LEN, "%s-source", this->getId());
        snprintf(thresholdId, STRING_LEN, "%s-threshold", this->getId());
        snprintf(methodId, STRING_LEN, "%s-method", this->getId());
        snprintf(descriptionId, STRING_LEN, "%s-description", this->getId());
        snprintf(descriptionId, STRING_LEN, "%s-silence", this->getId());

        // -- Add parameters to this group.
        this->addItem(&this->SourceParam);
        this->addItem(&this->ThresholdParam);
        this->addItem(&this->MethodParam);
        this->addItem(&this->DescriptionParam);
        this->addItem(&this->TemporarySilenceParam);

        value = 0.0;
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
        (char*)TThresholdMethodValues, (char*)TThresholdMethodNames, sizeof(TThresholdMethodValues) / STRING_LEN, STRING_LEN, "2");

    iotwebconf::TextParameter DescriptionParam = iotwebconf::TextParameter("Alert Description", descriptionId, descriptionValue, STRING_LEN, "Alert");

    iotwebconf::NumberParameter TemporarySilenceParam = iotwebconf::NumberParameter("Temporary silence time (minutes)", silenceId, silenceValue, NUMBER_LEN, "60", "0..300", "min='0' max='300' step='1'");

    void SetSensorValue(const double v) { value = v; };
    double GetSensorValue() { return value; };
    uint8_t GetSourceId() { return atoi(sourceValue); };
    uint8_t GetThresholdMethod() { return atoi(methodValue); };
    uint32_t GetThresholdValue() { return atoi(thresholdValue); };
    uint16_t GetTemporarySilenceTime() { return atoi(silenceValue); };

    tN2kSyncScheduler SchedulerAlarm = tN2kSyncScheduler(false, 500, 100);
    tN2kSyncScheduler SchedulerAlarmText = tN2kSyncScheduler(false, 10000, 2000);
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

extern Sensor Sensor1;
extern Sensor Sensor2;
extern Sensor Sensor3;
extern Sensor Sensor4;

#endif