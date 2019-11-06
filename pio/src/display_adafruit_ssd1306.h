#ifndef DISPLAY_ADAFRUIT_SSD1306_H
#define DISPLAY_ADAFRUIT_SSD1306_H

#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class Display_Adafruit_SSD1306 : public DisplayInterface {
	public:
		Display_Adafruit_SSD1306();

		virtual void init();
		virtual void print(const char* line);
		virtual void clear();

		virtual void setCursor(int x, int y);
		virtual void setLine(int y);

		virtual void sync();

	private:
		Adafruit_SSD1306 ada;
};

#endif