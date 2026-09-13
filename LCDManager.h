#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <LiquidCrystal_I2C.h>
#include "BatteryManager.h"
#include "FaultManager.h"

class LCDManager
{
private:
    LiquidCrystal_I2C lcd;

    unsigned long lastRefresh;
    unsigned long lastPageChange;

    uint8_t currentPage;

    String previousLine1;
    String previousLine2;

public:
    LCDManager();

    void begin();

    void update(BatteryManager &battery,
                FaultManager &faultManager);
};

#endif