// N2kAlertMessages.h

#ifndef _N2KALERTMESSAGES_h
#define _N2KALERTMESSAGES_h

#if defined(ARDUINO)  &&ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include <N2kMessages.h>
#include "N2kAlertTypes.h"

//*****************************************************************************
// Alert Notification

extern void SetN2kPGN126983(
    tN2kMsg &N2kMsg,
    tN2kAlertType AlertType,
    tN2kAlertCategory AlertCategory,
    unsigned char AlertSystem,
    unsigned char AlertSubSystem,
    unsigned int AlertID,
    uint64_t SourceNetworkID,
    unsigned char DataSourceInstance,
    unsigned char DataSourceIndex,
    unsigned char AlertOccurence,
    uint64_t AcknowledgeNetworkID,
    tN2kAlertTriggerCondition TriggerCondition,
    tN2kAlertThresholdStatus ThresholdStatus,
    unsigned char AlertPriority,
    tN2kAlertState AlertState,
    tN2kAlertYesNo TemporarySilenceStatus = N2kts_AlertNo,
    tN2kAlertYesNo AcknowledgeStatus = N2kts_AlertNo,
    tN2kAlertYesNo EscalationStatus = N2kts_AlertNo,
    tN2kAlertYesNo TemporarySilenceSupport = N2kts_AlertNo,
    tN2kAlertYesNo AcknowledgeSupport = N2kts_AlertNo,
    tN2kAlertYesNo EscalationSupport = N2kts_AlertNo
);

inline void SetN2kAlert(
    tN2kMsg &N2kMsg,
    tN2kAlertType AlertType,
    tN2kAlertCategory AlertCategory,
    unsigned char AlertSystem,
    unsigned char AlertSubSystem,
    unsigned int AlertID,
    uint64_t SourceNetworkID,
    unsigned char DataSourceInstance,
    unsigned char DataSourceIndex,
    unsigned char AlertOccurence,
    uint64_t AcknowledgeNetworkID,
    tN2kAlertTriggerCondition TriggerCondition,
    tN2kAlertThresholdStatus ThresholdStatus,
    unsigned char AlertPriority,
    tN2kAlertState AlertState,
    tN2kAlertYesNo TemporarySilenceStatus = N2kts_AlertNo,
    tN2kAlertYesNo AcknowledgeStatus = N2kts_AlertNo,
    tN2kAlertYesNo EscalationStatus = N2kts_AlertNo,
    tN2kAlertYesNo TemporarySilenceSupport = N2kts_AlertNo,
    tN2kAlertYesNo AcknowledgeSupport = N2kts_AlertNo,
    tN2kAlertYesNo EscalationSupport = N2kts_AlertNo
) {
    SetN2kPGN126983(N2kMsg, AlertType, AlertCategory, AlertSystem, AlertSubSystem, AlertID, SourceNetworkID, DataSourceInstance, DataSourceIndex, AlertOccurence,
        AcknowledgeNetworkID, TriggerCondition, ThresholdStatus, AlertPriority, AlertState, TemporarySilenceStatus, AcknowledgeStatus, EscalationStatus,
        TemporarySilenceSupport, AcknowledgeSupport, EscalationSupport);

};

extern bool ParseN2kPGN126983(
    const tN2kMsg &N2kMsg,
    tN2kAlertType &AlertType,
    tN2kAlertCategory &AlertCategory,
    unsigned char &AlertSystem,
    unsigned char &AlertSubSystem,
    unsigned int &AlertID,
    uint64_t &SourceNetworkID,
    unsigned char &DataSourceInstance,
    unsigned char &DataSourceIndex,
    unsigned char &AlertOccurence,
    uint64_t &AcknowledgeNetworkID,
    tN2kAlertTriggerCondition &TriggerCondition,
    tN2kAlertThresholdStatus &ThresholdStatus,
    unsigned char &AlertPriority,
    tN2kAlertState &AlertState,
    tN2kAlertYesNo &TemporarySilenceStatus,
    tN2kAlertYesNo &AcknowledgeStatus,
    tN2kAlertYesNo &EscalationStatus,
    tN2kAlertYesNo &TemporarySilenceSupport,
    tN2kAlertYesNo &AcknowledgeSupport,
    tN2kAlertYesNo &EscalationSupport
);

//*****************************************************************************
// Alert Response Notification

extern void SetN2kPGN126984(
    tN2kMsg &N2kMsg,
    tN2kAlertType AlertType,
    tN2kAlertCategory AlertCategory,
    unsigned char AlertSystem,
    unsigned char AlertSubSystem,
    unsigned int AlertID,
    uint64_t SourceNetworkID,
    unsigned char DataSourceInstance,
    unsigned char DataSourceIndex,
    unsigned char AlertOccurence,
    uint64_t AcknowledgeNetworkID,
    tN2kAlertResponseCommand ResponseCommand
);

inline void SetN2kAlertReponse(
    tN2kMsg &N2kMsg,
    tN2kAlertType AlertType,
    tN2kAlertCategory AlertCategory,
    unsigned char AlertSystem,
    unsigned char AlertSubSystem,
    unsigned int AlertID,
    uint64_t SourceNetworkID,
    unsigned char DataSourceInstance,
    unsigned char DataSourceIndex,
    unsigned char AlertOccurence,
    uint64_t AcknowledgeNetworkID,
    tN2kAlertResponseCommand ResponseCommand
) {
    SetN2kPGN126984(N2kMsg, AlertType, AlertCategory, AlertSystem, AlertSubSystem, AlertID,
        SourceNetworkID, DataSourceInstance, DataSourceIndex, AlertOccurence, AcknowledgeNetworkID,
        ResponseCommand);
};

extern bool ParseN2kPGN126984(
    const tN2kMsg &N2kMsg,
    tN2kAlertType &AlertType,
    tN2kAlertCategory &AlertCategory,
    unsigned char &AlertSystem,
    unsigned char &AlertSubSystem,
    unsigned int &AlertID,
    uint64_t &SourceNetworkID,
    unsigned char &DataSourceInstance,
    unsigned char &DataSourceIndex,
    unsigned char &AlertOccurence,
    uint64_t &AcknowledgeNetworkID,
    tN2kAlertResponseCommand &ResponseCommand
);

inline bool ParseN2kAlertResponse(
    const tN2kMsg &N2kMsg,
    tN2kAlertType &AlertType,
    tN2kAlertCategory &AlertCategory,
    unsigned char &AlertSystem,
    unsigned char &AlertSubSystem,
    unsigned int &AlertID,
    uint64_t &SourceNetworkID,
    unsigned char &DataSourceInstance,
    unsigned char &DataSourceIndex,
    unsigned char &AlertOccurence,
    uint64_t &AcknowledgeNetworkID,
    tN2kAlertResponseCommand &ResponseCommand
) {
    ParseN2kPGN126984(N2kMsg, AlertType, AlertCategory, AlertSystem, AlertSubSystem, AlertID, 
        SourceNetworkID, DataSourceInstance, DataSourceIndex, AlertOccurence, AcknowledgeNetworkID, 
        ResponseCommand);
};

//*****************************************************************************
// Alert Text Notification

extern void SetN2kPGN126985(
    tN2kMsg &N2kMsg,
    tN2kAlertType AlertType,
    tN2kAlertCategory AlertCategory,
    unsigned char AlertSystem,
    unsigned char AlertSubSystem,
    unsigned int AlertID,
    uint64_t SourceNetworkID,
    unsigned char DataSourceInstance,
    unsigned char DataSourceIndex,
    unsigned char AlertOccurence,
    tN2kAlertLanguage AlertLanguage,
    char* AlertTextDescription,
    char* AlertLocationTextDescription
);

inline void SetN2kAlertText(
    tN2kMsg &N2kMsg,
    tN2kAlertType AlertType,
    tN2kAlertCategory AlertCategory,
    unsigned char AlertSystem,
    unsigned char AlertSubSystem,
    unsigned int AlertID,
    uint64_t SourceNetworkID,
    unsigned char DataSourceInstance,
    unsigned char DataSourceIndex,
    unsigned char AlertOccurence,
    tN2kAlertLanguage AlertLanguage,
    char* AlertTextDescription,
    char* AlertLocationTextDescription
) {
    SetN2kPGN126985(N2kMsg, AlertType, AlertCategory, AlertSystem, AlertSubSystem, AlertID,
        SourceNetworkID, DataSourceInstance, DataSourceIndex, AlertOccurence, AlertLanguage,
        AlertTextDescription, AlertLocationTextDescription);

};

extern bool ParseN2kPGN126985(
    const tN2kMsg &N2kMsg,
    tN2kAlertType &AlertType,
    tN2kAlertCategory &AlertCategory,
    unsigned char &AlertSystem,
    unsigned char &AlertSubSystem,
    unsigned int &AlertID,
    uint64_t &SourceNetworkID,
    unsigned char &DataSourceInstance,
    unsigned char &DataSourceIndex,
    unsigned char &AlertOccurence,
    tN2kAlertLanguage &AlertLanguage,
    char* &AlertTextDescription,
    char* &AlertLocationTextDescription
);

#endif

