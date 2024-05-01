// 
// 
// 

#include "N2kAlerts.h"
#include "N2kAlertMessagesEnumToStr.h"



tN2kAlert::tN2kAlert(tN2kAlertType _AlertType, tN2kAlertCategory _AlertCategory, uint16_t _AlertId, tN2kAlertTriggerCondition _TriggerCondition, uint8_t _AlertPriority,
	tN2kAlertYesNo _TemporarySilenceSupport, tN2kAlertYesNo _AcknowledgeSupport, tN2kAlertYesNo _EscalationSupport) :
	AlertType(_AlertType),
	AlertCategory(_AlertCategory),
	AlertId(_AlertId),
	AlertPriority(_AlertPriority),
	TemporarySilenceSupport(_TemporarySilenceSupport),
	AcknowledgeSupport(_AcknowledgeSupport),
	EscalationSupport(_EscalationSupport),
	TemporarySilenceStatus(N2kts_AlertNo),
	AcknowledgeStatus(N2kts_AlertNo),
	EscalationStatus(N2kts_AlertNo),
	Occurence(0),
	TriggerCondition(_TriggerCondition) {

	ThresholdStatus = N2kts_AlertThresholdStatusNormal;
	SetTemporarySilenceTime(3600);
	TemporarySilenceTimer = tN2kScheduler(TemporarySilenceDelay);

};

// AlertSystem				: UID in the Network for this device
// AlertSubsystem			: ID for each alert on this device
// AcknowledgeNetworkId		; Network ID for this alert device
void tN2kAlert::SetAlertSystem(uint8_t _Alertsystem, uint8_t _AlertSubsystem, uint64_t _AcknowledgeNetworkId, tN2kAlertLanguage _AlertLanguage, char* _AlertDescription, char* _AlertLocation) {
	AlertSystem = _Alertsystem;
	AlertSubSystem = _AlertSubsystem;
	AcknowledgeNetworkId = _AcknowledgeNetworkId;
	AlertLanguage = _AlertLanguage;
	strlcpy(AlertDescription, _AlertDescription, String_Len);
	strlcpy(AlertLocation, _AlertLocation, String_Len);
}

// This settings are need for linking the alert and senosr together
// DataSourceInstance		: UID from the device (e.g. temperatur sensor) for wihich this alert will be send
// DatesourceIndexSource	: ID from the sensor on the device
// DataSourceNetworkId		: Network id for the sensor device	
void tN2kAlert::SetAlertDataSource(uint8_t _DataSourceInstance, uint8_t _DatesourceIndexSource, uint64_t _DataSourceNetworkId) {
	DataSourceNetworkId = _DataSourceNetworkId;
	DataSourceInstance = _DataSourceInstance;
	DataSourceIndexSource = _DatesourceIndexSource;
}

void tN2kAlert::SetAlertThreshold(t2kNAlertThresholdMethod _Method, uint8_t _Format, uint64_t _Level){
	ThresholdMethod = _Method;
	ThresholdFormat = _Format;
	ThresholdLevel = _Level;
}

uint16_t tN2kAlert::GetAlertID(){
	return AlertId;
}

tN2kAlertType tN2kAlert::GetAlertType(){
	return tN2kAlertType(AlertType);
}

tN2kAlertCategory tN2kAlert::GetAlertCategory(){
	return tN2kAlertCategory(AlertCategory);
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

void tN2kAlert::SetAlertExceeded() {

	if (Occurence > 250) Occurence = 0;

	if (ThresholdStatus == N2kts_AlertThresholdStatusNormal) {
		ThresholdStatus = N2kts_AlertThresholdStatusExceeded;
		Occurence++;
	}

	if (ThresholdStatus == N2kts_AlertThresholdStatusExceeded) {
		AlertState = N2kts_AlertStateActive;
		if (TemporarySilenceStatus == N2kts_AlertYes) {
			AlertState == N2kts_AlertStateSilenced;
		}

		if (AcknowledgeStatus == N2kts_AlertYes) {
			AlertState == N2kts_AlertStateAcknowledged;
			ThresholdStatus = N2kts_AlertThresholdStatusAcknowledged;
		}
	}
}

void tN2kAlert::ResetAlert() {

	ThresholdStatus = N2kts_AlertThresholdStatusNormal;
	AlertState = N2kts_AlertStateNormal;

	AcknowledgeStatus = N2kts_AlertNo;
}

tN2kAlertThresholdStatus tN2kAlert::TestAlertThreshold(uint64_t v){

	//Serial.print("Threshold level : "); Serial.println(ThresholdLevel);
	//Serial.print("Value           : "); Serial.println(v);
	//Serial.print("Method          : "); Serial.println(N2kEnumAlertTypeToStr(ThresholdMethod));
	//Serial.print("Alert state     : "); Serial.println(N2kEnumAlertTypeToStr(AlertState));
	//Serial.print("Threshold status: "); Serial.println(N2kEnumAlertTypeToStr(ThresholdStatus));
	//Serial.println("*******************************");


	if (ThresholdMethod == N2kts_AlertThresholddMethodGreater) {
		if (v > ThresholdLevel) {
			SetAlertExceeded();
		}
		else {
			ResetAlert();
		}
	}

	if (ThresholdMethod == N2kts_AlertThresholdMethodLower) {
		if (v < ThresholdLevel) {
			SetAlertExceeded();
		}
		else {
			ResetAlert();
		}
	}

	if (ThresholdMethod == N2kts_AlertThresholdMethodEqual) {
		if (v == ThresholdLevel) {
			SetAlertExceeded();
		}
		else {
			ResetAlert();
		}
	}

	if (TemporarySilenceTimer.IsTime()) {
		TemporarySilenceStatus = N2kts_AlertNo;
	}

	return tN2kAlertThresholdStatus(ThresholdStatus);
}

void tN2kAlert::SetN2kAlertText(tN2kMsg &N2kMsg){
	SetN2kPGN126985(N2kMsg, AlertType, AlertCategory, AlertSystem, AlertSubSystem, AlertId, 
		DataSourceNetworkId, DataSourceInstance, DataSourceIndexSource, 
		Occurence, AlertLanguage, AlertDescription, AlertLocation);
}

void tN2kAlert::SetN2kAlert(tN2kMsg &N2kMsg){
	SetN2kPGN126983(N2kMsg, AlertType, AlertCategory, AlertSystem, AlertSubSystem, AlertId, 
		DataSourceNetworkId, DataSourceInstance, DataSourceIndexSource, 
		Occurence, AcknowledgeNetworkId, TriggerCondition, ThresholdStatus, AlertPriority, AlertState,
		TemporarySilenceStatus, AcknowledgeStatus, EscalationStatus, 
		TemporarySilenceSupport, AcknowledgeSupport, EscalationSupport);
}

void tN2kAlert::SetTemporarySilenceTime(uint16_t seconds){
	TemporarySilenceDelay = seconds * 1000;
}

bool tN2kAlert::ParseAlertResponse(const tN2kMsg &N2kMsg){
	tN2kAlertType _AlertType;
	tN2kAlertCategory _AlertCategory;
	unsigned char _AlertSystem;
	unsigned char _AlertSubSystem;
	unsigned int _AlertID;
	uint64_t _SourceNetworkID;
	unsigned char _DataSourceInstance;
	unsigned char _DataSourceIndex;
	unsigned char _AlertOccurence;
	uint64_t _AcknowledgeNetworkID;
	tN2kAlertResponseCommand _ResponseCommand;

	if (ParseN2kAlertResponse(N2kMsg, _AlertType, _AlertCategory, _AlertSystem, _AlertSubSystem, _AlertID, 
		_SourceNetworkID, _DataSourceInstance, _DataSourceIndex, _AlertOccurence, _AcknowledgeNetworkID, 
		_ResponseCommand)) {
		if ((AlertSystem == _AlertSystem) && (AlertSubSystem == _AlertSubSystem)) {
			switch (_ResponseCommand) {
				case N2kts_AlertResponseAcknowledge:
					AcknowledgeStatus = N2kts_AlertYes;
					break;

				case N2kts_AlertResponseTemporarySilence:
					TemporarySilenceStatus = N2kts_AlertYes;
					TemporarySilenceTimer.FromNow(TemporarySilenceDelay);
					break;

				case N2kts_AlertResponseTestCommandOff:
					break;

				case N2kts_AlertResponseTestCommandOn:
					break;
					
			}
		}
		return true;

	}

	return false;
}
bool tN2kAlert::isAlert(){
	return ThresholdStatus != N2kts_AlertThresholdStatusNormal;
}

bool tN2kAlert::isAcknowledged(){
	return AcknowledgeStatus = N2kts_AlertYes;
}

bool tN2kAlert::isSilent(){
	return TemporarySilenceStatus = N2kts_AlertYes;
}
;


