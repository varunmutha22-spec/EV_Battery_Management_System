#include "LCDManager.h"

LCDManager::LCDManager() : lcd(0x27, 16, 2)
{
}
void LCDManager::begin()
{
    lcd.init();
    lcd.backlight();

    lastRefresh = 0;
    lastPageChange = 0;

    currentPage = 0;

    previousLine1 = "";
    previousLine2 = "";
}

void LCDManager::update(BatteryManager &battery,
                        FaultManager &faultManager)
{
    if (millis() - lastRefresh < 500)
        return;

    lastRefresh = millis();
    if (millis() - lastPageChange >= 3000)
{
    currentPage++;

    if (currentPage > 2)
    {
        currentPage = 0;
    }

    lastPageChange = millis();

    previousLine1 = "";
    previousLine2 = "";
}

    String line1;
    String line2;

    if (faultManager.getState() != NORMAL)
{
    line1 = "SYSTEM FAULT";
    line2 = faultManager.getStateName();
}
else
{

switch (currentPage)
{
case 0:

    line1 = "Pack:" + String(battery.getPackVoltage(), 2) + "V";
    line2 = "SOC:" + String(battery.getAverageSOC(), 1) + "%";

    break;

case 1:

    line1 = "High:" + String(battery.getHighestCellVoltage(), 2);
    line2 = "Low :" + String(battery.getLowestCellVoltage(), 2);

    break;

case 2:

    line1 = "State:" + faultManager.getStateName();
    line2 = "Fault:" + String(faultManager.getFault());

    break;
}
}
   if (line1 != previousLine1)
{
    lcd.setCursor(0, 0);
    lcd.print("                ");
    lcd.setCursor(0, 0);
    lcd.print(line1);

    previousLine1 = line1;
}

if (line2 != previousLine2)
{
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print(line2);

    previousLine2 = line2;
}
}