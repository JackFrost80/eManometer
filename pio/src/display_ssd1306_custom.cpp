#include "OLED.h"
#include "display_ssd1306_custom.h"

Display_SSD1306_Custom::Display_SSD1306_Custom()
{

}

void Display_SSD1306_Custom::init()
{
	init_LCD();
	lcd_gotoxy(0,0);
	lcd_clrscr();
}

void Display_SSD1306_Custom::print(const char* line)
{
	lcd_puts(line);
}

void Display_SSD1306_Custom::clear()
{
//	lcd_clrscr();
}

void Display_SSD1306_Custom::sync()
{

}

void Display_SSD1306_Custom::setCursor(int x, int y)
{
	//lcd_gotoxy(x, y);
}

void Display_SSD1306_Custom::setLine(int y)
{
	lcd_gotoxy(0, y);
}