#include "display_adafruit_ssd1306.h"

Display_Adafruit_SSD1306::Display_Adafruit_SSD1306()
	: ada(128, 64, &Wire)
{

}

void Display_Adafruit_SSD1306::doInit()
{
	ada.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false);

	ada.clearDisplay();
	ada.setTextSize(1);
	ada.setTextColor(WHITE);
	ada.setCursor(0,0);
}

void Display_Adafruit_SSD1306::doPrint(const char* line)
{
	ada.print(line);
}

void Display_Adafruit_SSD1306::doClear()
{
	ada.clearDisplay();
	ada.setCursor(0,0);
}

void Display_Adafruit_SSD1306::doSync()
{
	ada.display();
}

void Display_Adafruit_SSD1306::doSetCursor(int x, int y)
{
	ada.setCursor(x, y);
}

void Display_Adafruit_SSD1306::doSetLine(int y)
{
	ada.setCursor(0, y * 8);
}
