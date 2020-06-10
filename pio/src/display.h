#ifndef EMANOMETER_DISPLAY_H
#define EMANOMETER_DISPLAY_H

class DisplayInterface {
	public:
		void init();
		void print(const char* line);
		void printf(const char *format, ...);
		void clear();
		void setCursor(int x, int y);
		void setLine(int y);
		void sync();

	private:
		virtual void doInit() = 0;
		virtual void doPrint(const char* line);
		virtual void doClear() = 0;

		virtual void doSetCursor(int x, int y) = 0;
		virtual void doSetLine(int y) = 0;

		virtual void doSync() = 0;
};

#endif
