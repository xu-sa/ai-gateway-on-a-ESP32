//my data definition
#include "project_data.h"
//espressif default lib
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"  
#include "esp_event.h"
//gpio driver
#include "driver/gpio.h"
#include "driver/i2c.h"
//Flash RAM Storage  
#include "nvs_flash.h"
//wifi module 
#include "esp_wifi.h"
#include "esp_netif.h"
//my components
#include "../components/sagent/include/sagent.h"//need Wifi and NVS Initiated

#include "../components/ssd1306_driver/ssd1306_driver.h"//need i2c Initiated
//global Variable
static const char *TAG = "MAIN";
static const char* TAG_TEST="MAIN_TEST_APP";
SemaphoreHandle_t sem_1,sem_wifi,sem_pin;
System_ system_summerize;
history chat;
agent_profile profile;

static esp_err_t i2c_write(uint8_t addr, uint8_t* data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | 0, true);//0000 000->device address ,0->write or read
    i2c_master_write(cmd, data, len, true);//Multiple data, so need pointer and size indication
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t i2c_read(uint8_t addr, uint8_t* data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) |1 , true);
    if (len > 1) i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);//send ACK to contiune
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);//send NACK to stop 
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

void scan_iic_device(){
    for(uint8_t i=0;i<128;i++){
        if(i2c_write(i,NULL,0)==ESP_OK){
            switch (i)
            {
            case SSD1306_I2C_ADDR:
                system_summerize.iic_ssd_1306=1;
                break;
            case 30:
                system_summerize.iic_sensor_1=1;
            default:
                break;
            }
            
        }
    }

}

void wifi_init_ap_mode(){
    esp_netif_create_default_wifi_ap();//create API for AP mode
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t ap_config = {
    .ap = {
        .ssid_len = strlen(ssid_ap),
        .channel = 6,
        .authmode = WIFI_AUTH_WPA2_PSK,
        .max_connection = 2,
        .beacon_interval = 100
    }
    };
    memset(ap_config.ap.ssid, 0, 32);
    memset(ap_config.ap.password, 0, 64);
    memcpy(ap_config.ap.ssid, ssid_ap, 32);
    memcpy(ap_config.ap.password, password_ap, 64);
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_init_station_mode(const char* ssid, const char* pass) {
    esp_netif_create_default_wifi_sta();//create station mode API 
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.rssi = -127,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .bssid_set=0,
        }
    };
    memset(wifi_config.sta.ssid, 0, 32);
    memset(wifi_config.sta.password, 0, 64);
    memcpy(wifi_config.sta.ssid, ssid, 32);
    memcpy(wifi_config.sta.password, pass, 64);
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));//set wifi mode as station mode
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));//apply config
    ESP_ERROR_CHECK(esp_wifi_start());//try to connect
}

void wifi_shut_down(){
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_t *ap_netif=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (sta_netif) {
        esp_netif_destroy(sta_netif);
        ESP_LOGI(TAG,"Wifi Station mode off.");
    }else
    if(ap_netif){
        esp_netif_destroy(ap_netif);
        ESP_LOGI(TAG,"Wifi AP mode turn off.");
    }else{
        ESP_LOGI(TAG,"Unexpected Wifi mode, was it Actually turned on?");
    }
   
}

void handle_wifi_ip_event(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                ESP_LOGI(TAG," Starting Connecting to network\n");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                wifi_event_sta_connected_t* event_std_connect = (wifi_event_sta_connected_t*) event_data;
                ESP_LOGI(TAG," Succeed Connecting to network AP: %s , Allocating IP....\n",event_std_connect->ssid);
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "Wi-Fi disconnected, attempting to reconnect...");
                vTaskDelay(2000/portTICK_PERIOD_MS);
                esp_wifi_connect(); 
                break;

            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "AP mode started");
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                wifi_event_ap_staconnected_t* event_ap_connect = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "Station joined, AID=%d", event_ap_connect->aid);
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                wifi_event_ap_stadisconnected_t* event_ap_disconnect = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG, "Station  left, AID=%d",event_ap_disconnect->aid);
                break;
            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "AP mode stopped");
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            xSemaphoreGive(sem_wifi);
        }
    }
}

void sync_system_state(){
    for(int i=0;i<SYSTEM_CONFIG_NUM;i++){
        switch(i){
            case 0:
                if(system_summerize.wifi_mode==1){
                    wifi_init_station_mode(ssid,password);
                    ESP_LOGI(TAG, "waiting for network..");
                    xSemaphoreTake(sem_wifi,1000*120/portTICK_PERIOD_MS);//waiting for wifi connection,max 120s of duration
                }
                else if(system_summerize.wifi_mode==2){
                    wifi_init_ap_mode();
                    ESP_LOGI(TAG,"AP mode is on");
                }else if(system_summerize.wifi_mode==0){
                    wifi_shut_down();
                }
                break;
            case 1:
            if(system_summerize.ai_agent==1){
                load_agent_config(&profile);
                ESP_ERROR_CHECK(init_agent_chat(&chat,WHOYOUARE_));
            }else{
                save_agent_config(&profile);
                save_chat(&chat);
            }
            break;

            case 2:
                if(system_summerize.iic_ssd_1306==1){
                    ssd1306_init();
                }
            break;
            default:
            break;

    }
}
}

void System_initiation(){

    ESP_ERROR_CHECK(nvs_flash_init());//Initiate Non-Volatile Storage in Flash memory
    ESP_LOGI(TAG, "None volatile storage Initiated...");
    ESP_ERROR_CHECK(esp_netif_init());//initiate Network Data module
    ESP_LOGI(TAG, "Network driver Initiated...");
    ESP_ERROR_CHECK(esp_event_loop_create_default());//Initiate Event loop thread
    ESP_LOGI(TAG, "Event loop thread activated...");
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&handle_wifi_ip_event,NULL,NULL));//register function for handling wifi events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,&handle_wifi_ip_event,NULL,NULL));//the same as wifi event handler
    
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        .clk_flags = 0,
    };
    
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 
                      I2C_MASTER_RX_BUF, I2C_MASTER_TX_BUF, 0));
    ESP_LOGI(TAG, "I2C Initiating...     SCL: %d|SDA: %d|Frq: %dKhz",I2C_MASTER_SCL_IO,I2C_MASTER_SDA_IO,I2C_MASTER_FREQ_HZ/1024);

    gpio_set_direction(WAKE_UP_GPIO,GPIO_MODE_INPUT);//set input
    gpio_set_pull_mode(WAKE_UP_GPIO,GPIO_PULLUP_ONLY);//set default high
    gpio_wakeup_enable(WAKE_UP_GPIO,GPIO_INTR_LOW_LEVEL);//set wake up when low 
    esp_sleep_enable_gpio_wakeup();//enable
    ESP_LOGI(TAG, "Wake up Pin set as %d,input mode: input, pull mode: PULLUP\nsystem activating..",WAKE_UP_GPIO);
}

void main_(){
    System_initiation();
    sem_1=xSemaphoreCreateBinary();
    sem_wifi=xSemaphoreCreateBinary();
    sem_pin=xSemaphoreCreateBinary();
    system_summerize.ai_agent=1;
    system_summerize.wifi_mode=1;
    //sync_system_state();
    //xTaskCreate(test_message,"test_app",1024*4,NULL,2,NULL);
    //vTaskDelete(a);
    //xTaskCreate(test_loop,"chater",1024*6,NULL,2,NULL);
    //xSemaphoreGive(sem_1);
    
}

void monitor_pin(){

}

void test_ssd1306(){
    ssd1306_init();
    uint8_t page=0;
    uint8_t column=0;
    while (1)
    {
        ssd1306_clear();
        ssd1306_draw_string((page<<4)|column,"hi ssd1306!");
        ssd1306_display();
        column+=1;
        page+=1;
        if(column==16)column=0;
        if(page==8)page=0;
        ESP_LOGI(TAG_TEST," continue....");
        vTaskDelay(5000/portTICK_PERIOD_MS);
    }

}

void test_sagent(){


}
void app_main(void)
{
    
    
    return;
}