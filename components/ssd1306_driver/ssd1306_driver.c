#include <stdio.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "ssd1306_driver.h"
#include "esp_log.h"
#include "string.h"
#include "../font/font.h"
static const char* TAG="SSD1306 Display";
static uint8_t buffer_128x8[SSD1306_WIDTH * SSD1306_HEIGHT / 8];//128X8
 
static void ssd1306_write_cmd(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};  // Continue(7th bit)=0(yes/not the last byte), data/command(6th bit)=0(command)
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, (SSD1306_I2C_ADDR << 1) | 0, true);//0000 000->device address ,0->write-in signal
    i2c_master_write(cmd, data, 2, true);//Multiple data, so need pointer and size indication
    
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}

static void ssd1306_write_data(uint8_t* data, size_t len) {
    i2c_cmd_handle_t cmd=i2c_cmd_link_create();
    i2c_master_start(cmd);
    
    i2c_master_write_byte(cmd,SSD1306_I2C_ADDR<<1|0x00,true);//address+write
    i2c_master_write_byte(cmd,0x40,true);//0100 0000 -> consistently sending and are all data 
    i2c_master_write(cmd,data,len,true);
    
    i2c_master_stop(cmd);
    //esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
}

void ssd1306_clear() {
    memset(buffer_128x8, 0, sizeof(buffer_128x8));//set all byte buffer and all bit 0
}

void ssd1306_display() {
    for (uint8_t page = 0; page < 8; page++) {
        ssd1306_write_cmd(0xB0 + page);//set page as target buffer address
        ssd1306_write_cmd(0x00);//Column low
        ssd1306_write_cmd(0x10);//Column high 
        ssd1306_write_data(&buffer_128x8[page * SSD1306_WIDTH], SSD1306_WIDTH);//write in whole page buffers(128 Bytes)
    }
}

void ssd1306_init() {
    // 初始化命令序列
    ssd1306_write_cmd(0xAE);  // 关闭显示
    
    ssd1306_write_cmd(0x20);  // 内存地址模式
    ssd1306_write_cmd(0x00);  // 水平模式
    
    ssd1306_write_cmd(0xB0);  // 页起始地址
    
    ssd1306_write_cmd(0xC8);  // 扫描方向
    ssd1306_write_cmd(0x00);  // 列低地址
    ssd1306_write_cmd(0x10);  // 列高地址
    
    ssd1306_write_cmd(0x40);  // 显示起始行
    
    ssd1306_write_cmd(0x81);  // 对比度
    ssd1306_write_cmd(0x7F);
    
    ssd1306_write_cmd(0xA1);  // 段重映射
    ssd1306_write_cmd(0xA6);  // 正常显示
    ssd1306_write_cmd(0xA8);  // 多路复用比
    ssd1306_write_cmd(0x3F);  // 64行
    
    ssd1306_write_cmd(0xD3);  // 显示偏移
    ssd1306_write_cmd(0x00);
    
    ssd1306_write_cmd(0xD5);  // 时钟分频
    ssd1306_write_cmd(0x80);
    
    ssd1306_write_cmd(0xD9);  // 预充电周期
    ssd1306_write_cmd(0xF1);
    
    ssd1306_write_cmd(0xDA);  // COM引脚配置
    ssd1306_write_cmd(0x12);
    
    ssd1306_write_cmd(0xDB);  // VCOMH电平
    ssd1306_write_cmd(0x40);
    
    ssd1306_write_cmd(0x8D);  // 电荷泵
    ssd1306_write_cmd(0x14);
    
    ssd1306_write_cmd(0xAF);  // 开启显示
    
    ssd1306_clear();
    ssd1306_display();
    ESP_LOGI(TAG, "SSD1306 Initialized!");
}

void ssd1306_draw_pixel(int16_t x, int16_t y, bool color) {
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) return;
    uint16_t index = (y / 8) * SSD1306_WIDTH +x;
    if (color) {
        buffer_128x8[index] |= (1 << (y % 8));//Downside higher bit 0000 0000 Upside lower bit , the higher y is the lower the pixel is 
    } else {
        buffer_128x8[index] &= ~(1 << (y % 8));//set 0
    }
}

void ssd1306_draw_string(uint16_t start, char* str) {
    //0x34 -> 0011 0100
    uint8_t page=start>>4;//0011 0100 -> 0000 0011 -> page 3
    uint8_t col=(start&0x0f);//0011 0100 -> 0000 0100 -> column 4
    while (*str!='\0') {
        get_8x8_buffer_by_row(*str,buffer_128x8,SSD1306_WIDTH*SSD1306_HEIGHT/8,page*128+col*8);//max 128*7 + 8*15
        str++;
        col++;
        if(col==16){page++;col=0;};
        if(page==8)page=0;
    }
}

