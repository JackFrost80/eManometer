#include "OLED.h"
#include <Wire.h>


static const uint8_t ssd1306oled_font6x8 [] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // sp
	0x00, 0x00, 0x00, 0x2f, 0x00, 0x00, // !
	0x00, 0x00, 0x07, 0x00, 0x07, 0x00, // "
	0x00, 0x14, 0x7f, 0x14, 0x7f, 0x14, // #
	0x00, 0x24, 0x2a, 0x7f, 0x2a, 0x12, // $
	0x00, 0x62, 0x64, 0x08, 0x13, 0x23, // %
	0x00, 0x36, 0x49, 0x55, 0x22, 0x50, // &
	0x00, 0x00, 0x05, 0x03, 0x00, 0x00, // '
	0x00, 0x00, 0x1c, 0x22, 0x41, 0x00, // (
	0x00, 0x00, 0x41, 0x22, 0x1c, 0x00, // )
	0x00, 0x14, 0x08, 0x3E, 0x08, 0x14, // *
	0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, // +
	0x00, 0x00, 0x00, 0xA0, 0x60, 0x00, // ,
	0x00, 0x08, 0x08, 0x08, 0x08, 0x08, // -
	0x00, 0x00, 0x60, 0x60, 0x00, 0x00, // .
	0x00, 0x20, 0x10, 0x08, 0x04, 0x02, // /
	0x00, 0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
	0x00, 0x00, 0x42, 0x7F, 0x40, 0x00, // 1
	0x00, 0x42, 0x61, 0x51, 0x49, 0x46, // 2
	0x00, 0x21, 0x41, 0x45, 0x4B, 0x31, // 3
	0x00, 0x18, 0x14, 0x12, 0x7F, 0x10, // 4
	0x00, 0x27, 0x45, 0x45, 0x45, 0x39, // 5
	0x00, 0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
	0x00, 0x01, 0x71, 0x09, 0x05, 0x03, // 7
	0x00, 0x36, 0x49, 0x49, 0x49, 0x36, // 8
	0x00, 0x06, 0x49, 0x49, 0x29, 0x1E, // 9
	0x00, 0x00, 0x36, 0x36, 0x00, 0x00, // :
	0x00, 0x00, 0x56, 0x36, 0x00, 0x00, // ;
	0x00, 0x08, 0x14, 0x22, 0x41, 0x00, // <
	0x00, 0x14, 0x14, 0x14, 0x14, 0x14, // =
	0x00, 0x00, 0x41, 0x22, 0x14, 0x08, // >
	0x00, 0x02, 0x01, 0x51, 0x09, 0x06, // ?
	0x00, 0x32, 0x49, 0x59, 0x51, 0x3E, // @
	0x00, 0x7C, 0x12, 0x11, 0x12, 0x7C, // A
	0x00, 0x7F, 0x49, 0x49, 0x49, 0x36, // B
	0x00, 0x3E, 0x41, 0x41, 0x41, 0x22, // C
	0x00, 0x7F, 0x41, 0x41, 0x22, 0x1C, // D
	0x00, 0x7F, 0x49, 0x49, 0x49, 0x41, // E
	0x00, 0x7F, 0x09, 0x09, 0x09, 0x01, // F
	0x00, 0x3E, 0x41, 0x49, 0x49, 0x7A, // G
	0x00, 0x7F, 0x08, 0x08, 0x08, 0x7F, // H
	0x00, 0x00, 0x41, 0x7F, 0x41, 0x00, // I
	0x00, 0x20, 0x40, 0x41, 0x3F, 0x01, // J
	0x00, 0x7F, 0x08, 0x14, 0x22, 0x41, // K
	0x00, 0x7F, 0x40, 0x40, 0x40, 0x40, // L
	0x00, 0x7F, 0x02, 0x0C, 0x02, 0x7F, // M
	0x00, 0x7F, 0x04, 0x08, 0x10, 0x7F, // N
	0x00, 0x3E, 0x41, 0x41, 0x41, 0x3E, // O
	0x00, 0x7F, 0x09, 0x09, 0x09, 0x06, // P
	0x00, 0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
	0x00, 0x7F, 0x09, 0x19, 0x29, 0x46, // R
	0x00, 0x46, 0x49, 0x49, 0x49, 0x31, // S
	0x00, 0x01, 0x01, 0x7F, 0x01, 0x01, // T
	0x00, 0x3F, 0x40, 0x40, 0x40, 0x3F, // U
	0x00, 0x1F, 0x20, 0x40, 0x20, 0x1F, // V
	0x00, 0x3F, 0x40, 0x38, 0x40, 0x3F, // W
	0x00, 0x63, 0x14, 0x08, 0x14, 0x63, // X
	0x00, 0x07, 0x08, 0x70, 0x08, 0x07, // Y
	0x00, 0x61, 0x51, 0x49, 0x45, 0x43, // Z
	0x00, 0x00, 0x7F, 0x41, 0x41, 0x00, // [
	0x00, 0x55, 0x2A, 0x55, 0x2A, 0x55, // backslash
	0x00, 0x00, 0x41, 0x41, 0x7F, 0x00, // ]
	0x00, 0x04, 0x02, 0x01, 0x02, 0x04, // ^
	0x00, 0x40, 0x40, 0x40, 0x40, 0x40, // _
	0x00, 0x00, 0x01, 0x02, 0x04, 0x00, // '
	0x00, 0x20, 0x54, 0x54, 0x54, 0x78, // a
	0x00, 0x7F, 0x48, 0x44, 0x44, 0x38, // b
	0x00, 0x38, 0x44, 0x44, 0x44, 0x20, // c
	0x00, 0x38, 0x44, 0x44, 0x48, 0x7F, // d
	0x00, 0x38, 0x54, 0x54, 0x54, 0x18, // e
	0x00, 0x08, 0x7E, 0x09, 0x01, 0x02, // f
	0x00, 0x18, 0xA4, 0xA4, 0xA4, 0x7C, // g
	0x00, 0x7F, 0x08, 0x04, 0x04, 0x78, // h
	0x00, 0x00, 0x44, 0x7D, 0x40, 0x00, // i
	0x00, 0x40, 0x80, 0x84, 0x7D, 0x00, // j
	0x00, 0x7F, 0x10, 0x28, 0x44, 0x00, // k
	0x00, 0x00, 0x41, 0x7F, 0x40, 0x00, // l
	0x00, 0x7C, 0x04, 0x18, 0x04, 0x78, // m
	0x00, 0x7C, 0x08, 0x04, 0x04, 0x78, // n
	0x00, 0x38, 0x44, 0x44, 0x44, 0x38, // o
	0x00, 0xFC, 0x24, 0x24, 0x24, 0x18, // p
	0x00, 0x18, 0x24, 0x24, 0x18, 0xFC, // q
	0x00, 0x7C, 0x08, 0x04, 0x04, 0x08, // r
	0x00, 0x48, 0x54, 0x54, 0x54, 0x20, // s
	0x00, 0x04, 0x3F, 0x44, 0x40, 0x20, // t
	0x00, 0x3C, 0x40, 0x40, 0x20, 0x7C, // u
	0x00, 0x1C, 0x20, 0x40, 0x20, 0x1C, // v
	0x00, 0x3C, 0x40, 0x30, 0x40, 0x3C, // w
	0x00, 0x44, 0x28, 0x10, 0x28, 0x44, // x
	0x00, 0x1C, 0xA0, 0xA0, 0xA0, 0x7C, // y
	0x00, 0x44, 0x64, 0x54, 0x4C, 0x44, // z
	0x00, 0x00, 0x08, 0x77, 0x41, 0x00, // {
	0x00, 0x00, 0x00, 0x63, 0x00, 0x00, // ¦
	0x00, 0x00, 0x41, 0x77, 0x08, 0x00, // }
	0x00, 0x08, 0x04, 0x08, 0x08, 0x04, // ~
	0x00, 0x3D, 0x40, 0x40, 0x20, 0x7D, // ü
	0x00, 0x3D, 0x40, 0x40, 0x40, 0x3D, // Ü
	0x00, 0x21, 0x54, 0x54, 0x54, 0x79, // ä
	0x00, 0x7D, 0x12, 0x11, 0x12, 0x7D, // Ä
	0x00, 0x39, 0x44, 0x44, 0x44, 0x39, // ö
	0x00, 0x3D, 0x42, 0x42, 0x42, 0x3D, // Ö
	0x00, 0x02, 0x05, 0x02, 0x00, 0x00, // °
	0x00, 0x7E, 0x01, 0x49, 0x55, 0x73, // ß
};

void sendCommand(uint8_t data)
{
    Wire.beginTransmission(OLED_Adress);
    Wire.write(COMMAND_LCD);
    Wire.write(data);
    Wire.endTransmission(1);
}

void sendCommand_array(uint8_t *data,uint8_t command,uint8_t length)
{
	Wire.beginTransmission(OLED_Adress);
    Wire.write(command);
	for(uint8_t i = 0;i<length;i++)
	{
		Wire.write(*data);
		data++;
	}
	
	Wire.endTransmission(1);
}

void lcd_clrscr(uint8_t display)
{
	switch(display)
	{
		case 0:  // no display
		break;
		case 1:  //SSD1306
		{
			for(uint8_t i=0;i<32;i++)
			{
				Wire.beginTransmission(OLED_Adress);
    			Wire.write(DATA_LCD);
				for(uint8_t i = 0;i<32;i++)  // Wire buffer 32 Byte
				{
					Wire.write(0x00); // Clear display
				}
				Wire.endTransmission(1);
			}
			
		}
		break; 
		case 2: //SH1106
		{
			for (uint8_t page = 0; page < 8; page++) 
			{
				// Set the current RAM pointer to the start of the currently
				// selected page.
				sendCommand(0x00); // Set column number 0 (low nibble)
				sendCommand(0x10); // Set column number 0 (high nibble)
				sendCommand(0xB0 | (page & 0x0F)); // Set page number
				// Send a single page (128 bytes) of data.
				// Without knowing the rest of your program I don't know the right
				// data sending routines to use, so I made them up. Change it to
				// do it how your program expects.  Maybe with your SendArray
				// function.
				uint8_t helper = 0x00 ;
				for (uint8_t col = 0; col < 132; col++) 
				{
					sendCommand_array(&helper,DATA_LCD,1);
				}		
			}
		}
		break;
		default:
		break;
	}
}

void lcd_gotoxy(uint8_t x, uint8_t y,uint8_t display){
	uint8_t helper[4];
	switch(display)
	{
		case 0:  // no display
		break;
		case 1:
		{
		helper[0] = 0xb0 + y;	// set page start to y
		helper[1] = 0x21;		// set column start
		helper[2] = x; 			// to x
		helper[3] = 0x7f;		// set column end to 127
		sendCommand_array(helper,COMMAND_LCD,4); // Write data to I2C
		}
		break; 
		case 2: //SH1106
		{
			x *= 6;
			
			helper[0] = (x+2) & 0x0F;	// lower nibble
			sendCommand_array(helper,COMMAND_LCD,1);  // Write data to I2C
			helper[0] = 0x10 | (x+2) >> 4;		// set column start
			sendCommand_array(helper,COMMAND_LCD,1);  // Write data to I2C
			helper[0] = 0xB0 | (y & 0x0F); //page
			sendCommand_array(helper,COMMAND_LCD,1);  // Write data to I2C
			helper[0] = 0x7f;		// set column end to 127
			sendCommand_array(helper,COMMAND_LCD,1);  // Write data to I2C	
		}
		break;
		default:
		break;
	}
	
	
}

void lcd_home(uint8_t display){
	lcd_gotoxy(0, 0,display);
}



void init_LCD(uint8_t display)
{
	
switch(display)
	{
		case 0:  // no display
		break;
		case 1:  //SSD1306
		{
			uint8_t init1[] = {
				SSD1306_DISPLAYOFF,                   // 0xAE
				SSD1306_SETDISPLAYCLOCKDIV,           // 0xD5
				0x80,                                 // the suggested ratio 0x80
				SSD1306_SETMULTIPLEX }; // 0xA8
			sendCommand_array(init1,DATA_LCD,4);
			sendCommand(SSD1306_LCDHEIGHT - 1);
			uint8_t init2[] = {
				SSD1306_SETDISPLAYOFFSET,             // 0xD3
				0x0,                                  // no offset
				SSD1306_SETSTARTLINE | 0x0,           // line #0
				SSD1306_CHARGEPUMP }; // 0x8D
			sendCommand_array(init2,DATA_LCD,4);
			sendCommand(0x10);  
			//helper = (dispAttr == SSD1306_EXTERNALVCC) ? 0x10 : 0x14;
			uint8_t init3[] = {
				SSD1306_MEMORYMODE,                   // 0x20
				0x00,                                 // 0x0 act like ks0108
				SSD1306_SEGREMAP | 0x1,
				SSD1306_COMSCANDEC };
			sendCommand_array(init3,DATA_LCD,4);
			uint8_t init4[] = {
				SSD1306_SETCOMPINS,                 // 0xDA
				0x12,
				SSD1306_SETCONTRAST };              // 0x81
			sendCommand_array(init4,DATA_LCD,3);
			//helper = (dispAttr == SSD1306_EXTERNALVCC) ? 0x9F : 0xCF;
			sendCommand(0x9F); 
			sendCommand(SSD1306_SETPRECHARGE); 
			//helper = (dispAttr == SSD1306_EXTERNALVCC) ? 0x22 : 0xF1;
			sendCommand(0x22); 
			uint8_t  init5[] = {
				SSD1306_SETVCOMDETECT,               // 0xDB
				0x40,
				SSD1306_DISPLAYALLON_RESUME,         // 0xA4
				SSD1306_NORMALDISPLAY,               // 0xA6
				SSD1306_DEACTIVATE_SCROLL,
				SSD1306_DISPLAYON }; // Main screen turn on
			sendCommand_array(init5,DATA_LCD,6);
		}
		break; 
		case 2: //SH1106
		{
			sendCommand(0xAE);    /*display off*/
			sendCommand(0x02);    /*set lower column address*/
			sendCommand(0x10);    /*set higher column address*/
			sendCommand(0x40);//0x40);    /*set display start line*/
			sendCommand(0xB0);    /*set page address*/
			sendCommand(0x81);    /*contract control*/
			sendCommand(0x80);//contrast);    /*128*/
			sendCommand(0xA1);    /*set segment remap*/
			sendCommand(0xA6);//invertSetting);    /*normal / reverse*/
			sendCommand(0xA8);    /*multiplex ratio*/
			sendCommand(0x3F);    /*duty = 1/32*/
			sendCommand(0xAD);    /*set charge pump enable*/
			sendCommand(0x8B);     /*external VCC   */
			sendCommand(0x30);    // | Vpp);    /*0X30---0X33  set VPP   9V liangdu!!!!*/
			sendCommand(0xC8);    /*Com scan direction*/
			sendCommand(0xD3);    /*set display offset*/
			sendCommand(0x00);   /*   0x20  */
			sendCommand(0xD5);    /*set osc division*/
			sendCommand(0x80);
			sendCommand(0xD9);    /*set pre-charge period*/
			sendCommand(0x1F);    /*0x22*/
			sendCommand(0xDA);    /*set COM pins*/
			sendCommand(0x12);
			sendCommand(0xDB);    /*set vcomh*/
			sendCommand(0x40);
			sendCommand(0xAF);    /*display ON*/
		}
		break;
		default:
		break;
	}
	lcd_clrscr(display);
}

void lcd_putc(char c){
	static uint8_t data[6];
	if((c > 127 ||
	c < 32) &&
	(c != 188 &&
	c != 182 &&
	c != 176 &&
	c != 164 &&
	c != 159 &&
	c != 156 &&
	c != 150 &&
	c != 132 ) ) return;
	c -= 32;
	if( c < 127-32 ) {
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i];	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		return;
	}
	switch (c) {
		case 156:
		c = 95; // ü
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i];	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		case 150:
		c = 99; // ö
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i];	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		case 144:
		c = 101; // °
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i];	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		case 132:
		c = 97; // ä
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i];	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		case 127:
		c = 102; // ß
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i];	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		case 124:
		c = 96; // Ü
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i];	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		case 118:
		c = 100; // Ö
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i];	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		case 100:
		c = 98; // Ä
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i];	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		default:
		break;
	}
	
}

void lcd_putc_invert(char c){
	static uint8_t data[6];
	if((c > 127 ||
	c < 32) &&
	(c != 188 &&
	c != 182 &&
	c != 176 &&
	c != 164 &&
	c != 159 &&
	c != 156 &&
	c != 150 &&
	c != 132 ) ) return;
	c -= 32;
	if( c < 127-32 ) {
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i]^0xff;	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		return;
	}
	switch (c) {
		case 156:
		c = 95; // ü
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i]^0xff;	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		case 150:
		c = 99; // ö
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i]^0xff;	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		case 144:
		c = 101; // °
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i]^0xff;	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		case 132:
		c = 97; // ä
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i]^0xff;	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		case 127:
		c = 102; // ß
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i]^0xff;	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		case 124:
		c = 96; // Ü
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i]^0xff;	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		case 118:
		c = 100; // Ö
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i]^0xff;	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		case 100:
		c = 98; // Ä
		for (uint8_t i = 0; i < 6; i++)
		{
			data[i] = ssd1306oled_font6x8[c * 6 + i]^0xff;	// print font to ram, print 6 columns
		}
		sendCommand_array(data,DATA_LCD,6);
		break;
		default:
		break;
	}
	
}
void lcd_puts(const char* s){
	while (*s) {
		lcd_putc(*s++);
	}
}
void lcd_puts_invert(const char* s){
	while (*s) {
		lcd_putc_invert(*s++);
	}
}

void lcd_puts_invert_pos(const char* s,uint8_t start,uint8_t ende){
	uint8_t position  = 0;
	while (*s) {
		if(position >= start && position <= ende)
		lcd_putc_invert(*s++);
		else
		lcd_putc(*s++);
		position++;
	}
}