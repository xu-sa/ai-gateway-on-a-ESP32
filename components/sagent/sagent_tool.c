#include "./include/sagent.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "Sagent TOOL ";

char *build_payload(agent_profile *prof, history *hist)
{
    char *json_buf = (char *)malloc(MAX_MESSAGE_POOL + 1024 * 5);
    memset(json_buf, 0, MAX_MESSAGE_POOL + 1024 * 5);
    uint16_t pos = 0;

    const char *model_key = "{\"model\":\"";
    const char *messages_key = "\",\"messages\":[";
    const char *role_key = "{\"role\":\"";
    const char *content_key = "\",\"content\":\"";

    memcpy(json_buf + pos, model_key, strlen(model_key));
    pos += strlen(model_key);

    memcpy(json_buf + pos, llm_models_[prof->model], strlen(llm_models_[prof->model]));
    pos += strlen(llm_models_[prof->model]);

    memcpy(json_buf + pos, messages_key, strlen(messages_key));
    pos += strlen(messages_key);
    {
        char *role = get_role_string(hist->role[0]);
        get_message(hist, 0, hist->buffer);
        memcpy(json_buf + pos, role_key, strlen(role_key));
        pos += strlen(role_key);
        memcpy(json_buf + pos, role, strlen(role));
        pos += strlen(role);
        memcpy(json_buf + pos, content_key, strlen(content_key));
        pos += strlen(content_key);
        memcpy(json_buf + pos, hist->buffer, hist->message_size[0]);
        pos += hist->message_size[0];
        const char *msg_suffix = "\"},";
        memcpy(json_buf + pos, msg_suffix, strlen(msg_suffix));
        pos += strlen(msg_suffix);
    }

    if (!hist->pin_now == 0)
    {
        uint8_t pin = (hist->pin_now == MAX_MESSAGE - 1 ? 1 : hist->pin_now + 1);
        while (hist->end_index[pin] == 0)
            pin = (pin == MAX_MESSAGE - 1 ? 1 : pin + 1);
        while (1)
        {
            char *role = get_role_string(hist->role[pin]);
            get_message(hist, pin, hist->buffer);
            memcpy(json_buf + pos, role_key, strlen(role_key));
            pos += strlen(role_key);
            memcpy(json_buf + pos, role, strlen(role));
            pos += strlen(role);
            memcpy(json_buf + pos, content_key, strlen(content_key));
            pos += strlen(content_key);
            memcpy(json_buf + pos, hist->buffer, hist->message_size[pin]);
            pos += hist->message_size[pin];
            const char *msg_suffix = (pin == hist->pin_now) ? "\"}" : "\"},";
            memcpy(json_buf + pos, msg_suffix, strlen(msg_suffix));
            pos += strlen(msg_suffix);

            if (pin == hist->pin_now)
                break;
            pin = (pin == MAX_MESSAGE - 1 ? 1 : pin + 1);
        }
    };
    const char *temperature_key = "],\"temperature\":";
    const char *max_token_key = ",\"max_tokens\":";
    const char *stream_key = ",\"stream\":";
    memcpy(json_buf + pos, temperature_key, strlen(temperature_key));
    pos += strlen(temperature_key);
    pos += snprintf(json_buf + pos, 5, "%.1f", prof->temperature);

    memcpy(json_buf + pos, max_token_key, strlen(max_token_key));
    pos += strlen(max_token_key);
    pos += snprintf(json_buf + pos, 5, "%d", prof->max_token);

    memcpy(json_buf + pos, stream_key, strlen(stream_key));
    pos += strlen(stream_key);
    pos += snprintf(json_buf + pos, 6, "%s", prof->stream == 1 ? "true" : "false");
    memcpy(json_buf + pos, "}", 1);
    pos += 1;
    json_buf[pos] = '\0'; // 确保以 null 结尾
    return json_buf;
}

size_t get_message_lenth(history *h, uint8_t which)
{
    uint16_t a = h->start_index[which];
    uint16_t b = h->end_index[which];
    uint16_t n = h->end_index[which];
    if (b < a)
    {
        size_t I = MAX_MESSAGE_POOL - a; // MAX-a
        size_t II = b - n;               // b-n
        return I + II;                   // MAX-a+b-n
    }
    return b - a + 1;
}

void get_message(history *h, uint8_t which, char *buffer)
{
    uint16_t a = h->start_index[which];
    uint16_t b = h->end_index[which];
    uint16_t n = h->end_index[which];
    if (b < a)
    {
        memcpy(buffer, h->message_pool + a, MAX_MESSAGE_POOL - a);               //[a,MAX-1]->[0,MAX-a-1] //MAX-a
        memcpy(buffer + MAX_MESSAGE_POOL - a, h->message_pool + (n + 1), b - n); //[N+1,b]->[MAX-a,MAX-a+b-n-1] //b-n //MAX-a+b-n
        buffer[MAX_MESSAGE_POOL - a + b - n] = '\0';
    }
    else
    {
        memcpy(buffer, h->message_pool + a, b - a + 1); //[a,b]->[0,b-a] //b-a+1
        buffer[b - a + 1] = '\0';
    }
}

char *get_role_string(int role_id)
{
    switch (role_id)
    {
    case ROLE_SYSTEM:
        return "system";
    case ROLE_USER:
        return "user";
    case ROLE_ASSISTANT:
        return "assistant";
    case ROLE_TOOL:
        return "tool";
    default:
        return "user";
    }
}

void pre_process_string(char *str)
{
    if (!str)
        return;
    for (char *p = str; *p; p++)
        if (*p == '"')
            *p = '\'';
}

void print_last_message(history *a)
{
    get_message(a, a->pin_now, a->buffer);
    ESP_LOGI(TAG, "%s: %.*s", get_role_string(a->role[a->pin_now]), a->message_size[a->pin_now], a->buffer);
}

void print_last_message_(history *a, uint8_t I)
{
    if (I == 2)
    {
        ESP_LOGI(TAG, "<%d|%d|%d>", 0, a->start_index[0], a->end_index[0]);
        uint8_t pin = (a->pin_now == MAX_MESSAGE - 1 ? 1 : a->pin_now + 1);
        if (a->pin_now == 0)
            return;
        while (a->end_index[pin] == 0)
            pin = (pin == MAX_MESSAGE - 1 ? 1 : pin + 1);
        while (1)
        {
            ESP_LOGI(TAG, "<%d|%d|%d>", pin, a->start_index[pin], a->end_index[pin]);
            if (pin == a->pin_now)
                break;
            pin = (pin == MAX_MESSAGE - 1 ? 1 : pin + 1);
        }
    }
    else if (I == 1)
    {
        get_message(a, a->pin_now, a->buffer);
        ESP_LOGI(TAG, "%s: %.*s", get_role_string(a->role[a->pin_now]), a->message_size[a->pin_now], a->buffer);
        ESP_LOGI(TAG, "Index %d|%d", a->start_index[a->pin_now], a->end_index[a->pin_now]);
    }
}