#ifndef DISPLAY_HD44780_H
#define DISPLAY_HD44780_H

#include "display.h"

class Display_HD44780: public DisplayInterface {
	public:
		Display_HD44780();

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