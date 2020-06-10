#include <stdio.h>
#include "display.h"

void DisplayInterface::init()
{
	doInit();
}

void DisplayInterface::print(const char* line)
{
	doPrint(line);
}

void DisplayInterface::printf(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    char temp[64];
    char* buffer = temp;
    size_t len = vsnprintf(temp, sizeof(temp), format, arg);
    va_end(arg);
    if (len > sizeof(temp) - 1) {
        buffer = new char[len + 1];
        if (!buffer) {
            return;
        }
        va_start(arg, format);
        vsnprintf(buffer, len + 1, format, arg);
        va_end(arg);
    }
    print(buffer);
    if (buffer != temp) {
        delete[] buffer;
    }
}

void DisplayInterface::clear()
{
	doClear();
}

void DisplayInterface::setCursor(int x, int y)
{
	doSetCursor(x, y);
}

void DisplayInterface::setLine(int y)
{
	doSetLine(y);
}

void DisplayInterface::sync()
{
	doSync();
}
