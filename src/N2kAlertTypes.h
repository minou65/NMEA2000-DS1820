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
    N2kts_AlertTypeEmergencyAlarm = 1,
    N2kts_AlertTypeAlarm = 2,
    N2kts_AlertTypeWarning = 5,
    N2kts_AlertTypeCaution = 8
};

// enum tN2kAlertCategory
enum tN2kAlertCategory {
    N2kts_AlertCategoryNavigational = 0,
    N2kts_AlertCategoryTechnical = 1
};

enum tN2kAlertTriggerCondition {
    N2kts_AlertTriggerManual = 0,
    N2kts_AlertTriggerAuto = 1,
    N2kts_AlertTriggerTest = 2,
    N2kts_AlertTriggerDisabled = 3
};

enum tN2kAlertThresholdStatus {
    N2kts_AlertThresholdStatusNormal = 0,
    N2kts_AlertThresholdStatusExceeded = 1,
    N2kts_AlertThresholdStatusExtremeExceeded = 2,
    N2kts_AlertThresholdStatusLowExceeded = 3,
    N2kts_AlertThresholdStatusAcknowledged = 4,
    N2kts_AlertThresholdStatusAwaitingAcknowledge = 5
};

enum tN2kAlertState {
    N2kts_AlertStateDisabled = 0,
    N2kts_AlertStateNormal = 1,
    N2kts_AlertStateActive = 2,
    N2kts_AlertStateSilenced = 3,
    N2kts_AlertStateAcknowledged = 4,
    N2kts_AlertStateAwaitingAcknowledge = 5
};

enum tN2kAlertLanguage {
    N2kts_AlertLanguageEnglishUS = 0,
    N2kts_AlertLanguageEnglishUK = 1,
    N2kts_AlertLanguageArabic = 2,
    N2kts_AlertLanguageChineseSimplified = 3,
    N2kts_AlertLanguageCroatian = 4,
    N2kts_AlertLanguageDanish = 5,
    N2kts_AlertLanguageDutch = 6,
    N2kts_AlertLanguageFinnish = 7,
    N2kts_AlertLanguageFrench = 8,
    N2kts_AlertLanguageGerman = 9,
    N2kts_AlertLanguageGreek = 10,
    N2kts_AlertLanguageItalian = 11,
    N2kts_AlertLanguageJapanese = 12,
    N2kts_AlertLanguageKorean = 13,
    N2kts_AlertLanguageNorwegian = 14,
    N2kts_AlertLanguagePolish = 15,
    N2kts_AlertLanguagePortuguese = 16,
    N2kts_AlertLanguageRussian = 17,
    N2kts_AlertLanguageSpanish = 18,
    N2kts_AlertLanguageSwedish = 19
};

enum tN2kAlertResponseCommand {
    N2kts_AlertResponseAcknowledge = 0,
    N2kts_AlertResponseTemporarySilence = 1,
    N2kts_AlertResponseTestCommandOff = 2,
    N2kts_AlertResponseTestCommandOn = 3
};

enum tN2kAlertYesNo {
    N2kts_AlertNo = 0,
    N2kts_AlertYes = 1
};


enum t2kNAlertThresholdMethod {
    N2kts_AlertThresholdMethodEqual = 0,
    N2kts_AlertThresholdMethodLower = 1,
    N2kts_AlertThresholddMethodGreater = 2
};

#endif

