#ifndef FONT
#define FONT0
#include "stdint.h"
#define SIZE_OF_UINT_BUFFER(buffer) sizeof(buffer)

extern uint8_t english_letter_8x8[36][8];

void get_8x8_buffer_by_row(char character,uint8_t* buffer,uint16_t size_buffer,uint16_t start_index);// buffer Must be more than 8 Bytes left
void get_8x8_buffer_by_column(char character,uint8_t* buffer,uint16_t size_buffer,uint16_t current_index);//buffer must be more than 8 Bytes left

#endif