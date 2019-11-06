#ifndef EMANOMETER_DISPLAY_H
#define EMANOMETER_DISPLAY_H

class DisplayInterface {
	public:
		virtual void init() = 0;
		virtual void print(const char* line);
		virtual void clear() = 0;

		virtual void setCursor(int x, int y) = 0;
		virtual void setLine(int y) = 0;

		virtual void sync() = 0;
};

#endif
