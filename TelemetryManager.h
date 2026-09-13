#ifndef TELEMETRY_MANAGER_H
#define TELEMETRY_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "Config.h"
#include "BatteryManager.h"
#include "FaultManager.h"

// NOTE: This header intentionally does NOT include <BlynkSimpleEsp32.h>.
// Blynk defines its global `Blynk` object directly inside that header,
// so it can only be included in ONE .cpp file (sketch.ino) or you get
// a "multiple definition of `Blynk'" linker error. TelemetryManager
// never talks to Blynk directly -- it just decides what needs sending
// and hands the data back to sketch.ino to actually send.

struct TelemetryEvent
{
    float packVoltage;
    float avgSOC;
    float imbalance;
    uint8_t weakestIdx;
    uint8_t strongestIdx;
    float weakestV;
    float strongestV;
    bool relayState;
    SystemState state;
    unsigned long timestamp;
};

class TelemetryManager
{
private:
    TelemetryEvent queue[TELEMETRY_QUEUE_SIZE];
    uint8_t queueHead;
    uint8_t queueTail;
    uint8_t queueCount;

    float lastSentPack;
    float lastSentImbalance;
    SystemState lastSentState;
    bool lastSentRelay;

    unsigned long lastHeartbeat;
    unsigned long lastRSSICheck;
    unsigned long lastQueueFlush;
    int lastRSSI;

    bool wasConnected;

    bool pendingReady;
    bool pendingFromQueue;
    TelemetryEvent pendingEvent;

    // Fixed-size historical data for analytics
static const uint8_t HISTORY_SIZE = 60;

struct HistorySample
{
    unsigned long timestamp;
    float packVoltage;
    float avgSOC;
    float imbalance;
    float riskScore;
    SystemState state;
};

HistorySample history[HISTORY_SIZE];
uint8_t historyIndex;
uint8_t historyCount;
unsigned long lastHistorySample;

void recordHistory(BatteryManager &battery, FaultManager &faultManager);



    bool hasSignificantChange(BatteryManager &battery, FaultManager &faultManager);
    void captureEvent(TelemetryEvent &e, BatteryManager &battery, FaultManager &faultManager);
    void enqueue(TelemetryEvent &e);

public:
    TelemetryManager();

    void begin();

    // Pass Blynk.connected() in from sketch.ino each call.
    void update(BatteryManager &battery, FaultManager &faultManager, bool blynkConnected);

    bool hasPendingSend();
    TelemetryEvent getPendingEvent();
    bool isPendingFromQueue();
    void markSent(); // call after sketch.ino does the virtualWrite() calls

    int getQueueDepth();
    int getRSSI();

    float getRiskScore(BatteryManager &battery, FaultManager &faultManager);
    String getMaintenanceSuggestion(BatteryManager &battery, FaultManager &faultManager);

    float getImbalanceTrendValue();
    float getRiskTrendValue();
    String getBatteryHealth(BatteryManager &battery, FaultManager &faultManager);
    
};

#endif