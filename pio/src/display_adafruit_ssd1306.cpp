#include "display_adafruit_ssd1306.h"

Display_Adafruit_SSD1306::Display_Adafruit_SSD1306()
	: ada(128, 64, &Wire)
{

}

void Display_Adafruit_SSD1306::init()
{
	ada.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false);

	ada.clearDisplay();
	ada.setTextSize(1);
	ada.setTextColor(WHITE);
	ada.setCursor(0,0);
}

void Display_Adafruit_SSD1306::print(const char* line)
{
	ada.print(line);
}

void Display_Adafruit_SSD1306::clear()
{
	ada.clearDisplay();
	ada.setCursor(0,0);
}

void Display_Adafruit_SSD1306::sync()
{
	ada.display();
}

void Display_Adafruit_SSD1306::setCursor(int x, int y)
{
	ada.setCursor(x, y);
}

void Display_Adafruit_SSD1306::setLine(int y)
{
	ada.setCursor(0, y * 8);
}
