#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include <Arduino.h>
#include "Config.h"

enum SystemState
{
    NORMAL,
    DEGRADED,
    FAILSAFE,
    SHUTDOWN
};

enum FaultID
{
    NO_FAULT,
    BATTERY_FAULT,
    ADC_FAULT,
    RELAY_FAULT,
    COMMUNICATION_FAULT
};

class FaultManager
{
private:
    SystemState currentState;
    FaultID currentFault;

    unsigned long stateTimestamp;
    unsigned long relayChangeTime;
    unsigned long recoveryStartTime;
    unsigned long relayMismatchStartTime;
    unsigned long lastCommunicationTime;
    bool relayPending;
    bool pendingRelayState;
    bool recoveryInProgress;
    bool commandedRelayState;
    bool actualRelayState;
    bool relayOutputState;

    const char* transitionReason;

    uint32_t faultCount;
    FaultID lastLoggedFault;

    // Fixed-size fault history for analytics
static const uint8_t FAULT_HISTORY_SIZE = 20;

struct FaultHistoryEntry
{
    unsigned long timestamp;
    SystemState previousState;
    SystemState newState;
    FaultID fault;
};

FaultHistoryEntry faultHistory[FAULT_HISTORY_SIZE];
uint8_t faultHistoryIndex;
uint8_t faultHistoryCount;
    
public:
    FaultManager();

    void begin();

    void update();

    void setState(SystemState state);

    SystemState getState();

    void setFault(FaultID fault);

    FaultID getFault();

    void logStateTransition(SystemState previousState,
                            SystemState newState,
                            FaultID fault);

    void clearFault();

    String getStateName();
    void checkBatteryFault(float imbalance, float threshold);

    void checkADCFault(bool adcFault);

    void setCommandedRelayState(bool state);
    void checkRelayMismatch(bool feedbackState);
  
    void updateCommunication();
    void setTransitionReason(const char* reason);

    void requestRelayState(bool state);
    bool getRelayState();

    void updateRelay();

    uint32_t getFaultCount();
    unsigned long getUptimeSeconds();
    String getFaultName();

    int getFaultHistoryCount();
    FaultHistoryEntry getFaultHistoryEntry(uint8_t index);

};

#endif