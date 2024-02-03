// N2kAlertTypes.h

#ifndef _N2KALERTTYPES_h
#define _N2KALERTTYPES_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif


// enum tN2kAlertType
enum tN2kAlertType {
    N2kts_EmergencyAlarm = 1,
    N2kts_Alarm = 2,
    N2kts_Warning = 5,
    N2kts_Caution = 8
};

// enum tN2kAlertCategory
enum tN2kAlertCategory {
    N2kts_Navigational = 0,
    N2kts_Technical = 1
};

enum tN2kAlertTriggerCondition {
    N2kts_Manual = 0,
    N2kts_Auto = 1,
    N2kts_Test = 2,
    N2kts_Disabled = 3
};

enum tN2kAlertThresholdStatus {
    N2kts_Normal = 0,
    N2kts_ThresholdExceeded = 1,
    N2kts_ExtremeThresholdExceeded = 2,
    N2kts_LowThresholdExceeded = 3,
    N2kts_Acknowledged = 4,
    N2kts_AwaitingAcknowledge = 5
};

enum tN2kAlertState {
    N2kts_Disabled = 0,
    N2kts_Normal = 1,
    N2kts_Active = 2,
    N2kts_Silenced = 3,
    N2kts_Acknowledged = 4,
    N2kts_AwaitingAcknowledge = 5
};

enum tN2kAlertLanguage {
    N2kts_EnglishUS = 0,
    N2kts_EnglishUK = 1,
    N2kts_Arabic = 2,
    N2kts_ChineseSimplified = 3,
    N2kts_Croatian = 4,
    N2kts_Danish = 5,
    N2kts_Dutch = 6,
    N2kts_Finnish = 7,
    N2kts_French = 8,
    N2kts_German = 9,
    N2kts_Greek = 10,
    N2kts_Italian = 11,
    N2kts_Japanese = 12,
    N2kts_Korean = 13,
    N2kts_Norwegian = 14,
    N2kts_Polish = 15,
    N2kts_Portuguese = 16,
    N2kts_Russian = 17,
    N2kts_Spanish = 18,
    N2kts_Swedish = 19
};

enum tN2kAlertResponseCommand {
    N2kts_Acknowledge = 0,
    N2kts_TemporarySilence = 1,
    N2kts_TestCommandOff = 2,
    N2kts_TestCommandOn = 3
};

enum tN2kAlertYesNo {
    N2kts_No = 0,
    N2kts_Yes = 1
};


enum t2kNAlertThresholdMethod {
    N2kts_equal = 0,
    N2kts_lower = 1,
    N2kts_greater = 2
};

#endif

