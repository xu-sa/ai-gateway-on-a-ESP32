#ifndef SSD1306_DRIVER
#define SSD1306_DRIVER

#ifndef I2C_MASTER_NUM
#define I2C_MASTER_NUM I2C_NUM_0
#endif
#define SSD1306_I2C_ADDR    0x3C //the slave address is either “b0111100” or “b0111101” by changing the SA0 to LOW or HIGH 
#define SSD1306_WIDTH       128
#define SSD1306_HEIGHT      64
#include "stdint.h"
void ssd1306_init() ;
void ssd1306_clear() ;
void ssd1306_display() ;
void ssd1306_draw_pixel(int16_t x, int16_t y, bool color);
void ssd1306_draw_string(uint16_t page_column,char* str);//page_column stands for the starting position for the string,each char occupy 8X8 size, Meaning for each page there is a Maximum of 128/8=16 chars to be printed

#endif