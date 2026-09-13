#include "TelemetryManager.h"

TelemetryManager::TelemetryManager()
{
}

void TelemetryManager::begin()
{
    queueHead = 0;
    queueTail = 0;
    queueCount = 0;

    lastSentPack = -1.0;
    lastSentImbalance = -1.0;
    lastSentState = NORMAL;
    lastSentRelay = false;

    lastHeartbeat = 0;
    lastRSSICheck = 0;
    lastQueueFlush = 0;
    lastRSSI = 0;

    wasConnected = false;

    pendingReady = false;
    pendingFromQueue = false;

    historyIndex = 0;
    historyCount = 0;
    lastHistorySample = 0;
}

bool TelemetryManager::hasSignificantChange(BatteryManager &battery, FaultManager &faultManager)
{
    if (fabs(battery.getPackVoltage() - lastSentPack) > TELEMETRY_MIN_PACK_DELTA) return true;
    if (fabs(battery.getCurrentImbalance() - lastSentImbalance) > TELEMETRY_MIN_IMBALANCE_DELTA) return true;
    if (faultManager.getState() != lastSentState) return true;
    if (faultManager.getRelayState() != lastSentRelay) return true;

    return false;
}

void TelemetryManager::captureEvent(TelemetryEvent &e, BatteryManager &battery, FaultManager &faultManager)
{
    e.packVoltage  = battery.getPackVoltage();
    e.avgSOC       = battery.getAverageSOC();
    e.imbalance    = battery.getCurrentImbalance();
    e.weakestIdx   = battery.getLowestCellIndex();
    e.strongestIdx = battery.getHighestCellIndex();
    e.weakestV     = battery.getLowestCellVoltage();
    e.strongestV   = battery.getHighestCellVoltage();
    e.relayState   = faultManager.getRelayState();
    e.state        = faultManager.getState();
    e.timestamp    = millis();
}

void TelemetryManager::recordHistory(BatteryManager &battery, FaultManager &faultManager)
{
    unsigned long now = millis();

    // Store one analytics sample every 5 seconds
    if (now - lastHistorySample < 5000)
        return;

    lastHistorySample = now;

    history[historyIndex].timestamp = now;
    history[historyIndex].packVoltage = battery.getPackVoltage();
    history[historyIndex].avgSOC = battery.getAverageSOC();
    history[historyIndex].imbalance = battery.getCurrentImbalance();
    history[historyIndex].riskScore = getRiskScore(battery, faultManager);
    history[historyIndex].state = faultManager.getState();

    historyIndex = (historyIndex + 1) % HISTORY_SIZE;

    if (historyCount < HISTORY_SIZE)
        historyCount++;
}
void TelemetryManager::update(BatteryManager &battery, FaultManager &faultManager, bool blynkConnected)
{
    unsigned long now = millis();

    recordHistory(battery, faultManager);

    if (now - lastRSSICheck >= 5000)
    {
        lastRSSICheck = now;
        lastRSSI = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -100;
    }

    if (blynkConnected && !wasConnected)
    {
        Serial.print("Blynk reconnected - ");
        Serial.print(queueCount);
        Serial.println(" queued event(s) will drain.");
    }
    wasConnected = blynkConnected;

    // Don't overwrite a pending event sketch.ino hasn't sent yet.
    if (pendingReady) return;

    bool significant  = hasSignificantChange(battery, faultManager);
    bool heartbeatDue = (now - lastHeartbeat >= TELEMETRY_HEARTBEAT_MS);

    if (significant || heartbeatDue)
    {
        TelemetryEvent e;
        captureEvent(e, battery, faultManager);

        if (blynkConnected)
        {
            pendingEvent      = e;
            pendingFromQueue  = false;
            pendingReady      = true;
        }
        else
        {
            enqueue(e);
        }

        lastSentPack      = e.packVoltage;
        lastSentImbalance = e.imbalance;
        lastSentState     = e.state;
        lastSentRelay     = e.relayState;
        lastHeartbeat     = now;

        if (pendingReady) return;
    }

    if (blynkConnected && queueCount > 0 && (now - lastQueueFlush >= 200))
    {
        lastQueueFlush = now;

        pendingEvent = queue[queueHead];
        queueHead = (queueHead + 1) % TELEMETRY_QUEUE_SIZE;
        queueCount--;

        pendingFromQueue = true;
        pendingReady = true;
    }
}

bool TelemetryManager::hasPendingSend() { return pendingReady; }
TelemetryEvent TelemetryManager::getPendingEvent() { return pendingEvent; }
bool TelemetryManager::isPendingFromQueue() { return pendingFromQueue; }

void TelemetryManager::markSent()
{
    pendingReady = false;
    pendingFromQueue = false;
}

void TelemetryManager::enqueue(TelemetryEvent &e)
{
    if (queueCount >= TELEMETRY_QUEUE_SIZE)
    {
        queueHead = (queueHead + 1) % TELEMETRY_QUEUE_SIZE;
        queueCount--;
        Serial.println("Offline queue full - oldest event dropped");
    }

    queue[queueTail] = e;
    queueTail = (queueTail + 1) % TELEMETRY_QUEUE_SIZE;
    queueCount++;

    Serial.print("Offline - event queued (depth=");
    Serial.print(queueCount);
    Serial.println(")");
}

int TelemetryManager::getQueueDepth() { return queueCount; }
int TelemetryManager::getRSSI() { return lastRSSI; }

float TelemetryManager::getRiskScore(BatteryManager &battery, FaultManager &faultManager)
{
    float score = 0.0;

    float threshold = battery.getAdaptiveThreshold();
    float imbRatio = battery.getCurrentImbalance() / (threshold > 0.001 ? threshold : 0.01);
    score += constrain(imbRatio * 40.0, 0.0, 40.0);

    if (battery.getImbalanceTrend() == "Increasing") score += 15.0;

    score += constrain(faultManager.getFaultCount() * 2.5, 0.0, 25.0);

    float soc = battery.getAverageSOC();
    score += constrain((100.0 - soc) * 0.2, 0.0, 20.0);

    return constrain(score, 0.0, 100.0);
}

String TelemetryManager::getMaintenanceSuggestion(BatteryManager &battery, FaultManager &faultManager)
{
    if (faultManager.getState() == SHUTDOWN)
        return "Immediate service required - system in SHUTDOWN";

    if (faultManager.getState() == FAILSAFE)
        return "Inspect " + faultManager.getFaultName() + " before resuming operation";

    if (battery.getImbalanceTrend() == "Increasing" &&
        battery.getCurrentImbalance() > battery.getAdaptiveThreshold() * 0.7)
        return "Schedule cell balancing soon - imbalance is rising";

    if (faultManager.getFaultCount() > 5)
        return "Frequent faults this session - inspect sensors/wiring";

    return "Battery pack operating normally";
}
float TelemetryManager::getImbalanceTrendValue()
{
    if (historyCount < 2)
        return 0.0;

    uint8_t latestIndex =
        (historyIndex + HISTORY_SIZE - 1) % HISTORY_SIZE;

    uint8_t oldestIndex;

    if (historyCount < HISTORY_SIZE)
        oldestIndex = 0;
    else
        oldestIndex = historyIndex;

    return history[latestIndex].imbalance -
           history[oldestIndex].imbalance;
}


float TelemetryManager::getRiskTrendValue()
{
    if (historyCount < 2)
        return 0.0;

    uint8_t latestIndex =
        (historyIndex + HISTORY_SIZE - 1) % HISTORY_SIZE;

    uint8_t oldestIndex;

    if (historyCount < HISTORY_SIZE)
        oldestIndex = 0;
    else
        oldestIndex = historyIndex;

    return history[latestIndex].riskScore -
           history[oldestIndex].riskScore;
}


String TelemetryManager::getBatteryHealth(BatteryManager &battery, FaultManager &faultManager)
{
    float risk = getRiskScore(battery, faultManager);
    float imbalance = battery.getCurrentImbalance();
    float threshold = battery.getAdaptiveThreshold();

    if (faultManager.getState() == SHUTDOWN)
        return "CRITICAL";

    if (faultManager.getState() == FAILSAFE)
        return "POOR";

    if (risk >= 60.0 || imbalance > threshold)
        return "DEGRADED";

    if (risk >= 30.0)
        return "FAIR";

    return "GOOD";
}