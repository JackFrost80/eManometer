#include "OLED.h"
#include "display_ssd1306_custom.h"

Display_SSD1306_Custom::Display_SSD1306_Custom(int displayType)
	: mDisplayType(displayType)
{

}

void Display_SSD1306_Custom::doInit()
{
	init_LCD(mDisplayType);
	lcd_gotoxy(0,0, mDisplayType);
	lcd_clrscr(mDisplayType);
}

void Display_SSD1306_Custom::doPrint(const char* line)
{
	lcd_puts(line);
}

void Display_SSD1306_Custom::doClear()
{
//	lcd_clrscr();
}

void Display_SSD1306_Custom::doSync()
{

}

void Display_SSD1306_Custom::doSetCursor(int x, int y)
{
	//lcd_gotoxy(x, y);
}

void Display_SSD1306_Custom::doSetLine(int y)
{
	lcd_gotoxy(0, y, mDisplayType);
}