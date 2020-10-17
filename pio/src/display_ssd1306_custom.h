#ifndef DISPLAY_SSD1306_CUSTOM_H
#define DISPLAY_SSD1306_CUSTOM_H

#include "display.h"

class Display_SSD1306_Custom : public DisplayInterface {
	public:
		Display_SSD1306_Custom(int displayType);

	private:
		virtual void doInit();
		virtual void doPrint(const char* line);
		virtual void doClear();
		virtual void doSync();
		virtual void doSetCursor(int x, int y);
		virtual void doSetLine(int y);

	private:
		int mLine{0};
		int mDisplayType;
};

#endif