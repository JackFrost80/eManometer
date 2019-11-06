#ifndef DISPLAY_SSD1306_CUSTOM_H
#define DISPLAY_SSD1306_CUSTOM_H

#include "display.h"

class Display_SSD1306_Custom : public DisplayInterface {
	public:
		Display_SSD1306_Custom();
		
		virtual void init();
		virtual void print(const char* line);
		virtual void clear();
		virtual void sync();
		virtual void setCursor(int x, int y);
		virtual void setLine(int y);

	private:
		int mLine{0};
};

#endif