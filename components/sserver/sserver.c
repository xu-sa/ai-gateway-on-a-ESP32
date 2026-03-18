// components/my_web_server/my_web_server.c
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "my_web_server.h"

static const char *TAG = "WebServer";
static httpd_handle_t server = NULL;

// 网页HTML（简化示例）
static const char* HTML_PAGE = 
"<html><body>"
"<h1>ESP32 Control Panel</h1>"
"<form action='/set' method='post'>"
"Parameter: <input type='text' name='param'><br>"
"<input type='submit' value='Submit'>"
"</form>"
"<button onclick=\"fetch('/action')\">Call Function</button>"
"</body></html>";

// 处理根路径请求
static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, HTML_PAGE, strlen(HTML_PAGE));
    return ESP_OK;
}

// 处理参数设置
static esp_err_t set_post_handler(httpd_req_t *req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (ret > 0) {
        buf[ret] = 0;
        ESP_LOGI(TAG, "Received param: %s", buf);
        // 解析参数并更新系统
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// 处理函数调用
static esp_err_t action_get_handler(httpd_req_t *req) {
    // 调用对应函数
    ESP_LOGI(TAG, "Function called");
    httpd_resp_send(req, "Executed", 8);
    return ESP_OK;
}

void start_web_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler
        };
        httpd_uri_t set = {
            .uri = "/set",
            .method = HTTP_POST,
            .handler = set_post_handler
        };
        httpd_uri_t action = {
            .uri = "/action",
            .method = HTTP_GET,
            .handler = action_get_handler
        };
        
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &set);
        httpd_register_uri_handler(server, &action);
        
        ESP_LOGI(TAG, "Web server started on port 80");
    }
}

void stop_web_server(void) {
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
}