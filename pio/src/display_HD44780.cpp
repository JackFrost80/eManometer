#include "display_HD44780.h"
#include "LiquidCrystal_PCF8574.h"

LiquidCrystal_PCF8574 lcd(0x27);

Display_HD44780::Display_HD44780()
{

}

void Display_HD44780::doInit()
{
    lcd.begin(20,4);
	lcd.setBacklight(255);
}

void Display_HD44780::doPrint(const char* line)
{
	lcd.print(line);
}

void Display_HD44780::doClear()
{
	lcd.clear();
}

void Display_HD44780::doSync()
{

}

void Display_HD44780::doSetCursor(int x, int y)
{
	lcd.setCursor(x,y);
}

void Display_HD44780::doSetLine(int y)
{
	lcd.setCursor(0,y);
}