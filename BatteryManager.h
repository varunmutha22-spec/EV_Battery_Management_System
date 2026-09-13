#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include "Config.h"

class BatteryManager
{
private:
    float cellVoltages[NUM_CELLS];
    float cellSOC[NUM_CELLS];
    float averageSOC;

    float previousImbalance;
    float currentImbalance;
    String imbalanceTrend;
    float adaptiveThreshold;

    bool adcFault = false;

    int previousADC[NUM_CELLS];
    int filteredADC[NUM_CELLS];
    uint8_t stuckCounter[NUM_CELLS];
    uint8_t invalidReadingCounter[NUM_CELLS];
    uint8_t jumpCounter[NUM_CELLS];
    uint8_t frozenCounter[NUM_CELLS];
    uint8_t rangeFaultCounter[NUM_CELLS];

    // Rolling window for distinguishing sensor noise from rapid load changes
    static const uint8_t WINDOW_SIZE = 5;
    int adcWindow[NUM_CELLS][WINDOW_SIZE];
    uint8_t windowIndex[NUM_CELLS];
    uint8_t windowCount[NUM_CELLS];

public:
    BatteryManager();

    void begin();
    void update();

    float getCellVoltage(int cell);
    float getPackVoltage();

    float getHighestCellVoltage();
    float getLowestCellVoltage();

    int getHighestCellIndex();
    int getLowestCellIndex();

    float getImbalance();
    float getCellSOC(int cell);
    float getAverageSOC();

    float getCurrentImbalance();
    String getImbalanceTrend();
    float getAdaptiveThreshold();

    bool hasADCFault();
    void checkADCFault();
};

#endif