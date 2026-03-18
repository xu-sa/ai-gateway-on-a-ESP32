#ifndef PROJECT_DATA
#define PROJECT_DATA
#include <stdint.h>

#define MAX_MESSAGE_POOL 1024*20
#define MAX_MESSAGE 30

#define AGENT_DET_API ""
#define AGENT_DET_MODEL DeepSeek_Chat
#define AGENT_DET_PROVIDER DEEPSEEK
#define AGENT_DET_TEMPERATURE 0.7f
#define AGENT_DET_TOOL_CHOICE 'a'
#define AGENT_DET_MAX_TOKEN 1600
#define AGENT_DET_STREAM 0

#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_SCL_IO    17
#define I2C_MASTER_SDA_IO    18     
#define I2C_MASTER_FREQ_HZ   400000//400Khz
#define I2C_MASTER_RX_BUF    0
#define I2C_MASTER_TX_BUF    0
#define WAKE_UP_GPIO GPIO_NUM_10

#define WHOYOUARE_ "you are a helpful and Dedicated Ai Assistant on a ESP32 Micro Controller That Never Deviate from the Topic and Always reply user with Efficient and Non-Massive words"
#define USING_NAMESPACE "Storage_C"

static char* ssid="A845";
static char* password="12345678ss";
static char* ssid_ap="ssespsif";
static char* password_ap="12345678sss";

#define SYSTEM_CONFIG_NUM 2
typedef struct {
    uint16_t wifi_mode : 2;//0:disbale 1:station mode 2:Access point mode 3:
    uint16_t ai_agent : 1;//
    uint16_t iic_ssd_1306 : 1;//2
    uint16_t iic_sensor_1: 1;

}System_;
#endif