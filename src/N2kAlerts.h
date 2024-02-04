// N2kAlerts.h

#ifndef _N2KALERTS_h
#define _N2KALERTS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "N2kAlertMessages.h"

#define String_Len 50

class tN2kAlert {
public:
	tN2kAlert(tN2kAlertType _AlertType, tN2kAlertCategory _AlertCategory, uint16_t _AlertId, tN2kAlertTriggerCondition _TriggerCondition = N2kts_AlertTriggerAuto, uint8_t _AlertPriority = 100,
		tN2kAlertYesNo _TemporarySilenceSupport = N2kts_AlertNo, tN2kAlertYesNo _AcknowledgeSupport = N2kts_AlertNo, tN2kAlertYesNo _EscalationSupport = N2kts_AlertNo);
	void SetAlertInformation(uint8_t _Alertsystem, uint8_t _AlertSubsystem, tN2kAlertLanguage _Language, char* _AlertDescription, char* _AlertLocation);
	void SetAlertDataSource(uint64_t _NetworkId, uint8_t _Instance, uint8_t _Index, uint64_t _AcknowledgeNetworkId);
	void SetAlertThreshold(t2kNAlertThresholdMethod _Method, uint8_t _Format, uint64_t _Level);

	tN2kAlertType GetAlertType();
	tN2kAlertCategory GetAlertCategory();
	tN2kAlertThresholdStatus GetAlertThresholdStatus();
	tN2kAlertState GetAlertState();

	tN2kAlertYesNo GetTemporarySilenceSupport();
	tN2kAlertYesNo GetAcknowledgeSupport();
	tN2kAlertYesNo GetEscalationSupport();

	tN2kAlertYesNo GetTemporarySilenceStatus();
	tN2kAlertYesNo GetAcknowledgeStatus();
	tN2kAlertYesNo GetEscalationStatus();

	tN2kAlertThresholdStatus TestAlertThreshold(uint64_t v);

	void SetN2kAlertText(tN2kMsg &N2kMsg);
	void SetN2kAlert(tN2kMsg &N2kMsg);

private:
	uint16_t Id;
	uint8_t Priority;
	tN2kAlertType AlertType;
	tN2kAlertCategory AlertCategory;
	tN2kAlertState AlertState;
	uint8_t Occurence;

	uint8_t System;
	uint8_t subSystem;

	tN2kAlertLanguage AlertLanguage;
	char AlertDescription[String_Len + 1];
	char AlertLocation[String_Len + 1];

	uint64_t NetworkId;
	uint64_t AcknowledgeNetworkId;

	uint8_t Instance;
	uint8_t Index;

	tN2kAlertYesNo TemporarySilenceSupport;
	tN2kAlertYesNo AcknowledgeSupport;
	tN2kAlertYesNo EscalationSupport;

	tN2kAlertYesNo TemporarySilenceStatus;
	tN2kAlertYesNo AcknowledgeStatus;
	tN2kAlertYesNo EscalationStatus;

	tN2kAlertTriggerCondition TriggerCondition;
	tN2kAlertThresholdStatus ThresholdStatus;

	t2kNAlertThresholdMethod ThresholdMethod;
	uint8_t ThresholdFormat;
	uint64_t ThresholdLevel;
};

#endif

