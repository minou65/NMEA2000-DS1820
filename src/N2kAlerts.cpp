// 
// 
// 

#include "N2kAlerts.h"



tN2kAlert::tN2kAlert(tN2kAlertType _AlertType, tN2kAlertCategory _AlertCategory, uint16_t _AlertId, tN2kAlertTriggerCondition _TriggerCondition, uint8_t _AlertPriority,
	tN2kAlertYesNo _TemporarySilenceSupport, tN2kAlertYesNo _AcknowledgeSupport, tN2kAlertYesNo _EscalationSupport) :
	AlertType(_AlertType),
	Category(_AlertCategory),
	Id(_AlertId),
	Priority(_AlertPriority),
	TemporarySilenceSupport(_TemporarySilenceSupport),
	AcknowledgeSupport(_AcknowledgeSupport),
	EscalationSupport(_EscalationSupport),
	TemporarySilenceStatus(N2kts_AlertNo),
	AcknowledgeStatus(N2kts_AlertNo),
	EscalationStatus(N2kts_AlertNo),
	Occurence(0),
	TriggerCondition(_TriggerCondition) {

	AlertScheduler.start();
	DescriptionScheduler.start();
	ThresholdStatus = N2kts_AlertThresholdStatusNormal;

};

void tN2kAlert::SetAlertInformation(uint8_t _Alertsystem, uint8_t _AlertSubsystem, tN2kAlertLanguage _Language, char* _AlertDescription, char* _AlertLocation) {
	System = _Alertsystem;
	subSystem = _AlertSubsystem;
	Language = _Language;
	strlcpy(AlerttDescription, _AlertDescription, String_Len);
	strlcpy(AlertLocation, _AlertLocation, String_Len);
}

void tN2kAlert::SetAlertDataSource(uint64_t _NetworkId, uint8_t _Instance, uint8_t _Index, uint64_t _AcknowledgeNetworkId) {
	NetworkId = _NetworkId;
	Instance = _Instance;
	Index = _Index;
	AcknowledgeNetworkId = _AcknowledgeNetworkId;
}

void tN2kAlert::SetAlertThreshold(t2kNAlertThresholdMethod _Method, uint8_t _Format, uint64_t _Level){
	ThresholdMethod = _Method;
	ThresholdFormat = _Format;
	ThresholdLevel = _Level;
}

tN2kAlertType tN2kAlert::GetAlertType(){
	return tN2kAlertType(AlertType);
}

tN2kAlertCategory tN2kAlert::GetAlertCategory(){
	return tN2kAlertCategory(Category);
}

tN2kAlertThresholdStatus tN2kAlert::GetAlertThresholdStatus(){
	return tN2kAlertThresholdStatus(ThresholdStatus);
}

tN2kAlertState tN2kAlert::GetAlertState(){
	return tN2kAlertState(AlertState);
}

tN2kAlertYesNo tN2kAlert::GetTemporarySilenceSupport(){
	return tN2kAlertYesNo(TemporarySilenceSupport);
}

tN2kAlertYesNo tN2kAlert::GetAcknowledgeSupport(){
	return tN2kAlertYesNo(AcknowledgeSupport);
}

tN2kAlertYesNo tN2kAlert::GetEscalationSupport(){
	return tN2kAlertYesNo(EscalationSupport);
}

tN2kAlertYesNo tN2kAlert::GetTemporarySilenceStatus(){
	return tN2kAlertYesNo(TemporarySilenceStatus);
}

tN2kAlertYesNo tN2kAlert::GetAcknowledgeStatus(){
	return tN2kAlertYesNo(AcknowledgeStatus);
}

tN2kAlertYesNo tN2kAlert::GetEscalationStatus(){
	return tN2kAlertYesNo(EscalationStatus);
}

tN2kAlertThresholdStatus tN2kAlert::TestAlertThreshold(uint64_t v){

	if (ThresholdMethod == N2kts_AlertThresholddMethodGreater) {
		if (v > ThresholdLevel) {
			if (ThresholdStatus != N2kts_AlertThresholdStatusExceeded) {
				ThresholdStatus = N2kts_AlertThresholdStatusExceeded;
				Occurence++;
				if (Occurence > 250) {
					Occurence = 0;
					ThresholdStatus = N2kts_AlertThresholdStatusExtremeExceeded;
				}
				AlertState = N2kts_AlertStateActive;
			}
		}
		else {
			ThresholdStatus = N2kts_AlertThresholdStatusNormal;
			AlertState = N2kts_AlertStateNormal;
		}
	}

	if (ThresholdMethod == N2kts_AlertThresholdMethodLower) {
		if (v < ThresholdLevel) {
			if (ThresholdStatus != N2kts_AlertThresholdStatusExceeded) {
				ThresholdStatus = N2kts_AlertThresholdStatusExceeded;
				Occurence++;
				if (Occurence > 250) {
					Occurence = 0;
					ThresholdStatus = N2kts_AlertThresholdStatusExtremeExceeded;
				}
				AlertState = N2kts_AlertStateActive;
			}
		}
		else {
			ThresholdStatus = N2kts_AlertThresholdStatusNormal;
			AlertState = N2kts_AlertStateNormal;
		}
	}

	if (ThresholdMethod == N2kts_AlertThresholdMethodEqual) {
		if (v == ThresholdLevel) {
			if (ThresholdStatus != N2kts_AlertThresholdStatusExceeded) {
				ThresholdStatus = N2kts_AlertThresholdStatusExceeded;
				Occurence++;
				if (Occurence > 250) {
					Occurence = 0;
					ThresholdStatus = N2kts_AlertThresholdStatusExtremeExceeded;
				}
				AlertState = N2kts_AlertStateActive;
			}
		}
		else {
			ThresholdStatus = N2kts_AlertThresholdStatusNormal;
			AlertState = N2kts_AlertStateNormal;
		}
	}

	return tN2kAlertThresholdStatus(ThresholdStatus);
}

void tN2kAlert::SetN2kAlertText(tN2kMsg &N2kMsg){
	SetN2kPGN126985(N2kMsg, AlertType, Category, System, subSystem, Id, NetworkId, Instance, Index, Occurence, Language, AlerttDescription, AlertLocation);
}

void tN2kAlert::SetN2kAlert(tN2kMsg &N2kMsg){
	SetN2kPGN126983(N2kMsg, AlertType, Category, System, subSystem, Id, NetworkId, Instance, Index, Occurence, AcknowledgeNetworkId, TriggerCondition, ThresholdStatus, Priority, AlertState,
		TemporarySilenceStatus, AcknowledgeStatus, EscalationStatus, 
		TemporarySilenceSupport, AcknowledgeSupport, EscalationSupport);
}
;


