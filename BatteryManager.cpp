#include "BatteryManager.h"

BatteryManager::BatteryManager()
{
}

void BatteryManager::begin()
{
    for (int i = 0; i < NUM_CELLS; i++)
    {
        pinMode(CELL_PINS[i], INPUT);
        cellVoltages[i] = 0.0;
        cellSOC[i] = 0.0;

        previousADC[i] = -1;
        filteredADC[i] = -1;
        stuckCounter[i] = 0;
        invalidReadingCounter[i] = 0;
        jumpCounter[i] = 0;
        frozenCounter[i] = 0;
        rangeFaultCounter[i] = 0;

        windowIndex[i] = 0;
        windowCount[i] = 0;

for (int j = 0; j < WINDOW_SIZE; j++)
{
    adcWindow[i][j] = 0;
}
    }

    averageSOC = 0.0;

    previousImbalance = 0.0;
    currentImbalance = 0.0;
    imbalanceTrend = "Stable";
    adaptiveThreshold = 0.0;
}

void BatteryManager::update()
{
    float totalSOC = 0;
    
    for (int i = 0; i < NUM_CELLS; i++)
    {
         int adc = analogRead(CELL_PINS[i]);

        // Convert ADC reading to voltage (0–3.3 V)
        float adcVoltage = (adc * ADC_REFERENCE) / ADC_RESOLUTION;

        // Simulate Li-ion cell voltage (3.0–4.2 V)
        cellVoltages[i] = 3.0 + (adcVoltage / 3.3) * 1.2;

        // Estimate State of Charge (0–100%)
        cellSOC[i] = ((cellVoltages[i] - 3.0) / 1.2) * 100.0;

        // Limit SoC to 0–100%
        if (cellSOC[i] < 0) cellSOC[i] = 0;
        if (cellSOC[i] > 100) cellSOC[i] = 100;

        totalSOC += cellSOC[i];
    }

    averageSOC = totalSOC / NUM_CELLS;

    if (averageSOC > 80)
    {
         adaptiveThreshold = 0.05;
    }
    else if (averageSOC > 50)
    {
         adaptiveThreshold = 0.10;
    }
    else if (averageSOC > 20)
    {
         adaptiveThreshold = 0.15;
    } 
    else
    {
         adaptiveThreshold = 0.20;
    }

    previousImbalance = currentImbalance;

    currentImbalance = getHighestCellVoltage() - getLowestCellVoltage();

    if (currentImbalance > previousImbalance + 0.02)
   {
    imbalanceTrend = "Increasing";
   }
    else if (currentImbalance < previousImbalance - 0.02)
   {
    imbalanceTrend = "Decreasing";
   }
    else
   {
    imbalanceTrend = "Stable";
   }
   checkADCFault();
}


float BatteryManager::getCellVoltage(int cell)
{
    return cellVoltages[cell];
}

float BatteryManager::getPackVoltage()
{
    float total = 0;
    for (int i = 0; i < NUM_CELLS; i++)
    {
        total += cellVoltages[i];
    }
    return total;
}

float BatteryManager::getHighestCellVoltage()
{
    float highest = cellVoltages[0];
    for (int i = 1; i < NUM_CELLS; i++)
    {
        if (cellVoltages[i] > highest)
            highest = cellVoltages[i];
    }
    return highest;
}

float BatteryManager::getLowestCellVoltage()
{
    float lowest = cellVoltages[0];
    for (int i = 1; i < NUM_CELLS; i++)
    {
        if (cellVoltages[i] < lowest)
            lowest = cellVoltages[i];
    }
    return lowest;
}

int BatteryManager::getHighestCellIndex()
{
    int index = 0;
    for (int i = 1; i < NUM_CELLS; i++)
    {
        if (cellVoltages[i] > cellVoltages[index])
            index = i;
    }
    return index;
}

int BatteryManager::getLowestCellIndex()
{
    int index = 0;
    for (int i = 1; i < NUM_CELLS; i++)
    {
        if (cellVoltages[i] < cellVoltages[index])
            index = i;
    }
    return index;
}

float BatteryManager::getImbalance()
{
    return getHighestCellVoltage() - getLowestCellVoltage();
}

float BatteryManager::getCellSOC(int cell)
{
    return cellSOC[cell];
}

float BatteryManager::getAverageSOC()
{
    return averageSOC;
}

float BatteryManager::getCurrentImbalance()
{
    return currentImbalance;
}

String BatteryManager::getImbalanceTrend()
{
    return imbalanceTrend;
}

float BatteryManager::getAdaptiveThreshold()
{
    return adaptiveThreshold;
}
bool BatteryManager::hasADCFault()
{
    return adcFault;
}

void BatteryManager::checkADCFault()
{
    if (millis() < 2000)
    {
        adcFault = false;
        return;
    }

    adcFault = false;

    for (int i = 0; i < NUM_CELLS; i++)
    {
        int adc = analogRead(CELL_PINS[i]);
        // Simple moving average filter
        if (filteredADC[i] == -1)
    {
        filteredADC[i] = adc;
    }
    else
    {
        filteredADC[i] = (filteredADC[i] * 3 + adc) / 4;
    }

    adc = filteredADC[i];

    // Store filtered ADC in rolling window
adcWindow[i][windowIndex[i]] = adc;
windowIndex[i] = (windowIndex[i] + 1) % WINDOW_SIZE;

if (windowCount[i] < WINDOW_SIZE)
{
    windowCount[i]++;
}

// Statistical check: compare window mean and spread
if (windowCount[i] == WINDOW_SIZE)
{
    float mean = 0.0;

    for (int j = 0; j < WINDOW_SIZE; j++)
    {
        mean += adcWindow[i][j];
    }

    mean /= WINDOW_SIZE;

    float variance = 0.0;

    for (int j = 0; j < WINDOW_SIZE; j++)
    {
        float difference = adcWindow[i][j] - mean;
        variance += difference * difference;
    }

    variance /= WINDOW_SIZE;

    float standardDeviation = sqrt(variance);

    // Small variation = sensor noise / stable reading
    // Large sustained variation = genuine rapid change
    if (standardDeviation > 150)
    {
        Serial.print("Rapid load change detected on Cell ");
        Serial.println(i + 1);
    }
}
        // Detect unrealistic sudden ADC jumps
        if (previousADC[i] != -1)
    {
        if (abs(adc - previousADC[i]) > 800)
    {
        jumpCounter[i]++;

        if (jumpCounter[i] >= 3)
        {
            adcFault = true;

            Serial.print("Sudden ADC Jump on Cell ");
            Serial.println(i + 1);

            return;
        }
    }
    else
    {
        jumpCounter[i] = 0;
    }

        // Detect a genuinely FROZEN sensor: reading stays exactly the
        // same (within +/-1 ADC count) for many consecutive samples,
        // regardless of where in the range it is stuck (not just at
        // the rails). This is separate from the rail-stuck check below.
        if (SIMULATE_FROZEN_ADC && abs(adc - previousADC[i]) <= 1)
        {
            frozenCounter[i]++;

            if (frozenCounter[i] >= 10)
            {
                adcFault = true;

                Serial.print("Frozen ADC reading on Cell ");
                Serial.println(i + 1);

                return;
            }
        }
        else
        {
            frozenCounter[i] = 0;
        }
    }
// Store current reading for next comparison
previousADC[i] = adc;

        // Detect sensor stuck at minimum or maximum ADC value (rail fault)
        if (adc <= 5 || adc >= 4090)
        {
            invalidReadingCounter[i]++;

            if (invalidReadingCounter[i] >= 20)
            {
                adcFault = true;

                Serial.print("ADC Fault on Cell ");
                Serial.println(i + 1);

                return;
            }
        }
        else
        {
            invalidReadingCounter[i] = 0;
        }

        // Detect the converted cell voltage sitting outside the
        // physically valid Li-ion range for a sustained period
        // (uses CELL_MIN_VOLTAGE / CELL_MAX_VOLTAGE from Config.h).
        if (cellVoltages[i] < CELL_MIN_VOLTAGE - 0.05 || cellVoltages[i] > CELL_MAX_VOLTAGE + 0.05)
        {
            rangeFaultCounter[i]++;

            if (rangeFaultCounter[i] >= 10)
            {
                adcFault = true;

                Serial.print("Out-of-range voltage on Cell ");
                Serial.println(i + 1);

                return;
            }
        }
        else
        {
            rangeFaultCounter[i] = 0;
        }
    }
}