#ifndef SAGENT
#define SAGENT
#ifndef MAX_MESSAGE
#define MAX_MESSAGE 30
#endif
#ifndef MAX_MESSAGE_POOL
#define MAX_MESSAGE_POOL 1024*20
#endif
#ifndef MESSAGE_CHUNK_SIZE
#define MESSAGE_CHUNK_SIZE 1024*4
#endif
#ifndef SAGENT_NAMESPACE
#define SAGENT_NAMESPACE "sagent_config"
#endif
#ifndef SAGENT_CHAT_CACHE_NAMESPACE
#define SAGENT_CHAT_CACHE_NAMESPACE "sagent_data"
#endif
#ifndef AGENT_DET_API
#define AGENT_DET_API ""
#endif
#ifndef AGENT_DET_MODEL
#define AGENT_DET_MODEL DeepSeek_Chat
#endif
#ifndef AGENT_DET_PROVIDER
#define AGENT_DET_PROVIDER DEEPSEEK
#endif
#ifndef AGENT_DET_TEMPERATURE
#define AGENT_DET_TEMPERATURE 0.7f
#endif
#ifndef AGENT_DET_TOOL_CHOICE
#define AGENT_DET_TOOL_CHOICE 'a'
#endif
#ifndef AGENT_DET_MAX_TOKEN
#define AGENT_DET_MAX_TOKEN 1600
#endif
#ifndef AGENT_DET_STREAM
#define AGENT_DET_STREAM 0
#endif
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "nvs.h"
extern esp_event_base_t const AGENT_EVENT;
enum {
    AGENT_EVENT_SEND,
    AGENT_EVENT_REPLYING,
    AGENT_EVENT_REPLIED
};


typedef enum {
    ROLE_SYSTEM = 0,
    ROLE_USER = 1,
    ROLE_ASSISTANT = 2,
    ROLE_TOOL=3
} RoleType;

typedef enum {
    DEEPSEEK=0,
    OPENAI=1,
    XAI=2,
    QWEN=3
} ProviderType;
typedef enum{
    DeepSeek_Chat=0,
    DeepSeek_Reasoner=1,
    OpenAI_gpt_4o_mini=2,
    OpenAI_gpt_4o=3,
    OpenAI_gpt_turbo=4,
    XAI_grok_4_1_fast_non_reasoning=5,
    XAI_grok_4_1_fast_reasoning=6,
    Qwen_max=7,
    Qwen_plus=8,
    Qwen_turbo=9
} ModelType;

extern const char* llm_provider[];
extern const char* llm_models_[];

// typedef struct {
//     uint8_t role;
//     uint16_t start_index;
//     uint16_t end_index;
//     uint16_t message_size;
// } single_message;

typedef struct{
    char* message_pool;
    char* buffer;//the message in the buffer,could be the raw string from json
    uint16_t buffer_size;
    uint16_t buffer_limit;
    uint16_t start_index[MAX_MESSAGE];
    uint16_t end_index[MAX_MESSAGE];
    uint16_t message_size[MAX_MESSAGE];
    uint8_t role[MAX_MESSAGE];
    uint8_t pin_now;
    uint8_t mpe;//Interval Logic  
}history;

typedef struct{
    char api[120];
    ModelType model;
    ProviderType provider;
    char tool_choice;
    float temperature;
    uint16_t max_token;
    uint8_t stream;
}agent_profile;

esp_err_t init_agent_chat(history*,const char* what_you_are);
void shrink_agent_chat(history*);
void add_message(history*,char*,RoleType);
void send_chat(history* chat,agent_profile* profile,SemaphoreHandle_t*);//build char* based json package and send with esp http, build-in function will handle the message (Make Sure Default Event loop is ENABLED), Will give signal to the SemaphoreHandle_t in parameter once the transmission is done
void save_chat(history* chat);
void load_chat(history* chat);
void save_agent_config(agent_profile* agent);
void load_agent_config(agent_profile* agent);
void agent_event_handler(void* args,esp_event_base_t event,int32_t id,void* eventdata);
//int init_agent_profile_default(agent_profile* profile,char* api,ProviderType Provider,ModelType Model);//use default Parameters for agent which is Promised Optimized for micro controller
//int init_agent_profile_full(agent_profile* profile,char* api,ProviderType Provider,ModelType Model,float temprerature,uint16_t max_token,char tool_choice,uint8_t stream);//Specify more Parameters . for max_token:choose from 'a'/'r'/'n' and for stream choose from 0/1

//tool
char* build_payload(agent_profile* prof, history* hist);//return the string of LLM Server Acceptable Json in OpenAI Protocol
unsigned int get_message_lenth(history* h,uint8_t pin);//Abandoned,please use history.message_n[index].message_size instead
void get_message(history* h,uint8_t which ,char* buffer);//which : the index of message , buffer : where to copy the message char array to . MAKE SURE the buffer Has Larger size than h->message_n[pin].message_size
char* get_role_string(int role_id);//simply return the string of RoleType
void pre_process_string(char* str);
void print_last_message(history* chat);//set the buffer as the latest message and print it 
void print_last_message_(history* chat,uint8_t method);//method can be select from 1,2 for different debugging output.
#endif
