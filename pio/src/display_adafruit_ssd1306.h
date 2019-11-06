#ifndef DISPLAY_ADAFRUIT_SSD1306_H
#define DISPLAY_ADAFRUIT_SSD1306_H

#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class Display_Adafruit_SSD1306 : public DisplayInterface {
	public:
		Display_Adafruit_SSD1306();

	private:
		virtual void doInit();
		virtual void doPrint(const char* line);
		virtual void doClear();

		virtual void doSetCursor(int x, int y);
		virtual void doSetLine(int y);

		virtual void doSync();

		Adafruit_SSD1306 ada;
};

#endif