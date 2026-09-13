#include "FaultManager.h"

FaultManager::FaultManager()
{
}

void FaultManager::begin()
{
    currentState = NORMAL;
    currentFault = NO_FAULT;
    stateTimestamp = millis();

    relayChangeTime = 0;
    relayPending = false;
    pendingRelayState = false;
   
    recoveryStartTime = 0;
    recoveryInProgress = false;

    commandedRelayState = true;
    actualRelayState = true;
    relayOutputState = false;

    relayMismatchStartTime = 0;

    lastCommunicationTime = millis();

    transitionReason = "System Start";

    faultCount = 0;
    lastLoggedFault = NO_FAULT;

    faultHistoryIndex = 0;
    faultHistoryCount = 0;
}

void FaultManager::update()
{
  if (millis() - lastCommunicationTime > 5000)
  {
      if (currentFault == NO_FAULT)
      {
          currentFault = COMMUNICATION_FAULT;
      }
  }
  else if (currentFault == COMMUNICATION_FAULT)
  {
      currentFault = NO_FAULT;
  }
    switch (currentState)
    {
        case NORMAL:

            if (currentFault != NO_FAULT)
            {
                setTransitionReason("Fault detected");
                setState(DEGRADED);
            }

            break;

        case DEGRADED:

    if (currentFault == NO_FAULT)
    {
        if (!recoveryInProgress)
        {
            recoveryInProgress = true;
            recoveryStartTime = millis();

            Serial.println("Recovery verification started...");
        }
        else if (millis() - recoveryStartTime >= 5000)
        {
            recoveryInProgress = false;
            setTransitionReason("Recovery verified for 5 seconds");
            setState(NORMAL);
        }
    }
    else
    {
        recoveryInProgress = false;

        if ((currentFault == BATTERY_FAULT ||
             currentFault == ADC_FAULT) &&
            millis() - stateTimestamp > 10000)
        {
            setTransitionReason("Fault persisted for 10 seconds");
            setState(FAILSAFE);
        }
    }

    break;
       case FAILSAFE:

    if (currentFault == NO_FAULT)
    {
        if (!recoveryInProgress)
        {
            recoveryInProgress = true;
            recoveryStartTime = millis();

            Serial.println("FAILSAFE recovery verification started...");
        }
        else if (millis() - recoveryStartTime >= 5000)
        {
            recoveryInProgress = false;
            setTransitionReason("Recovery verified for 5 seconds");
            setState(NORMAL);
        }
    }
    else
    {
        recoveryInProgress = false;

        if (currentFault == RELAY_FAULT)
        {
            setTransitionReason("Relay fault detected");
            setState(SHUTDOWN);
        }
    }

    break;
        case SHUTDOWN:

            // Stay here until recovery logic is implemented

            break;
    }
}

void FaultManager::setState(SystemState state)
{
    if (currentState != state)
    {
        logStateTransition(currentState, state, currentFault);

        // Count a new fault occurrence whenever we escalate into a
        // fault state with a fault ID we haven't already counted for
        // this episode (avoids double-counting the same ongoing fault).
        if ((state == DEGRADED || state == FAILSAFE) &&
            currentFault != NO_FAULT &&
            currentFault != lastLoggedFault)
        {
            faultCount++;
            lastLoggedFault = currentFault;
        }

        if (state == NORMAL)
        {
            lastLoggedFault = NO_FAULT;
        }

        currentState = state;
        stateTimestamp = millis();
    }
}

SystemState FaultManager::getState()
{
    return currentState;
}

void FaultManager::setFault(FaultID fault)
{
    currentFault = fault;
}

FaultID FaultManager::getFault()
{
    return currentFault;
}

void FaultManager::clearFault()
{
    currentFault = NO_FAULT;
}

void FaultManager::logStateTransition(SystemState previousState,
                                      SystemState newState,
                                      FaultID fault)
{
    faultHistory[faultHistoryIndex].timestamp = millis();
    faultHistory[faultHistoryIndex].previousState = previousState;
    faultHistory[faultHistoryIndex].newState = newState;
    faultHistory[faultHistoryIndex].fault = fault;

    faultHistoryIndex = (faultHistoryIndex + 1) % FAULT_HISTORY_SIZE;

if (faultHistoryCount < FAULT_HISTORY_SIZE)
{
    faultHistoryCount++;
}
    Serial.print("[");
    Serial.print(millis());
    Serial.print(" ms] ");

    Serial.print("State: ");

    switch (previousState)
    {
        case NORMAL:    Serial.print("NORMAL"); break;
        case DEGRADED:  Serial.print("DEGRADED"); break;
        case FAILSAFE:  Serial.print("FAILSAFE"); break;
        case SHUTDOWN:  Serial.print("SHUTDOWN"); break;
    }

    Serial.print(" -> ");

    switch (newState)
    {
        case NORMAL:    Serial.print("NORMAL"); break;
        case DEGRADED:  Serial.print("DEGRADED"); break;
        case FAILSAFE:  Serial.print("FAILSAFE"); break;
        case SHUTDOWN:  Serial.print("SHUTDOWN"); break;
    }
    Serial.print(" | Fault ID: ");
    Serial.print(fault);

    Serial.print(" | Reason: ");
    Serial.println(transitionReason);
    }
String FaultManager::getStateName()
{
    switch (currentState)
    {
        case NORMAL: return "NORMAL";
        case DEGRADED: return "DEGRADED";
        case FAILSAFE: return "FAILSAFE";
        case SHUTDOWN: return "SHUTDOWN";
    }

    return "UNKNOWN";
}
void FaultManager::checkBatteryFault(float imbalance, float threshold)
{
    Serial.print("Imbalance: ");
    Serial.print(imbalance, 3);

    Serial.print("  Threshold: ");
    Serial.print(threshold, 3);

    Serial.print("  Clear Below: ");
    Serial.println(threshold - 0.05, 3);

    static bool batteryFaultActive = false;

    if (!batteryFaultActive)
    {
        if (imbalance > threshold)
        {
            batteryFaultActive = true;
        }
    }
    else
    {
        if (imbalance < (threshold - 0.05))
        {
            batteryFaultActive = false;
        }
    }

    if (batteryFaultActive)
    {
        currentFault = BATTERY_FAULT;
    }
    else if (currentFault == BATTERY_FAULT)
    {
        currentFault = NO_FAULT;
    }
}
void FaultManager::checkADCFault(bool adcFault)
{
    if (adcFault)
    {
        currentFault = ADC_FAULT;
    }
    else if (currentFault == ADC_FAULT)
    {
        currentFault = NO_FAULT;
    }
}
void FaultManager::setCommandedRelayState(bool state)
{
    commandedRelayState = state;
}

void FaultManager::checkRelayMismatch(bool feedbackState)
{
    actualRelayState = feedbackState;

    if (commandedRelayState != actualRelayState)
    {
        if (relayMismatchStartTime == 0)
        {
            relayMismatchStartTime = millis();
        }
        else if (millis() - relayMismatchStartTime >= 1000)
        {
            currentFault = RELAY_FAULT;
        }
    }
    else
    {
        relayMismatchStartTime = 0;

        // Only clear if the relay was the active fault; don't stomp on
        // a battery/ADC/comm fault that might currently be active.
        if (currentFault == RELAY_FAULT)
        {
            currentFault = NO_FAULT;
        }
    }
}
void FaultManager::updateCommunication()
{
    lastCommunicationTime = millis();
}
void FaultManager::setTransitionReason(const char* reason)
{
    transitionReason = reason;
}
void FaultManager::updateRelay()
{
    if (relayPending)
    {
        if (millis() - relayChangeTime >= 200)
        {
            relayOutputState = pendingRelayState;
            relayPending = false;

            digitalWrite(RELAY_PIN, relayOutputState);
        }
    }
}
void FaultManager::requestRelayState(bool state)
{
    if (state != relayOutputState && !relayPending)
    {
        pendingRelayState = state;
        relayPending = true;
        relayChangeTime = millis();
    }
}

bool FaultManager::getRelayState()
{
    return relayOutputState;
}

uint32_t FaultManager::getFaultCount()
{
    return faultCount;
}

unsigned long FaultManager::getUptimeSeconds()
{
    return millis() / 1000;
}

String FaultManager::getFaultName()
{
    switch (currentFault)
    {
        case NO_FAULT:             return "None";
        case BATTERY_FAULT:        return "Battery Imbalance";
        case ADC_FAULT:            return "ADC/Sensor";
        case RELAY_FAULT:          return "Relay Mismatch";
        case COMMUNICATION_FAULT:  return "Communication";
    }
    return "Unknown";
}
int FaultManager::getFaultHistoryCount()
{
    return faultHistoryCount;
}

FaultManager::FaultHistoryEntry FaultManager::getFaultHistoryEntry(uint8_t index)
{
    if (index >= faultHistoryCount)
    {
        FaultHistoryEntry emptyEntry = {0, NORMAL, NORMAL, NO_FAULT};
        return emptyEntry;
    }

    uint8_t actualIndex;

    if (faultHistoryCount < FAULT_HISTORY_SIZE)
    {
        actualIndex = index;
    }
    else
    {
        actualIndex = (faultHistoryIndex + index) % FAULT_HISTORY_SIZE;
    }

    return faultHistory[actualIndex];
}