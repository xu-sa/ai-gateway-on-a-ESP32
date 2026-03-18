#include "./include/sagent.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//ESP32 lib
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
//PRIV_REQUIRES esp_http_client
#include "esp_http_client.h"
//PRIV_REQUIRES mbedtls
#include "esp_crt_bundle.h"
#include "nvs.h"
#define STORAGE_CHUNKS (MAX_MESSAGE_POOL+MESSAGE_CHUNK_SIZE+1)/MESSAGE_CHUNK_SIZE
esp_event_base_t const AGENT_EVENT="AGENT_EVENT_BASE";

static const char* TAG="Sagent ";
typedef struct {
    history* hist_ptr;
    agent_profile* profile_ptr;
    SemaphoreHandle_t* sem_handle;
} http_agent_data_t;

const char* llm_provider[]={
    "https://api.deepseek.com/v1/chat/completions","https://api.openai.com/v1/chat/completions",
    "https://api.x.ai/v1/chat/completions","https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"
};

const char* llm_models_[]={
    "deepseek-chat","deepseek-reasoner",
    "gpt-4o-mini","gpt-4o","gpt-4-turbo",
    "grok-4-1-fast-non-reasoning","grok-4-1-fast-reasoning",
    "qwen-max","qwen-plus","qwen-turbo"
};

void save_agent_config(agent_profile* agent){
    nvs_handle_t handle;
    if(nvs_open(SAGENT_NAMESPACE,NVS_READWRITE,&handle)==ESP_OK){
        nvs_set_str(handle,"api",agent->api);
        
        nvs_set_i8(handle, "model", (int8_t)agent->model);
        nvs_set_i8(handle,"provider",(int8_t)agent->provider);
        nvs_set_i8(handle,"tool_choice",(int8_t)agent->tool_choice);
        nvs_set_i8(handle,"stream",(int8_t)agent->stream);
    
        nvs_set_i16(handle,"max_token",(int16_t)agent->max_token);
        nvs_set_i16(handle,"temprature",(int16_t)agent->temperature);
        
        nvs_commit(handle);
        nvs_close(handle);
    }else{
        ESP_LOGI(TAG,"Cant open NVS Storage,NVS Partition May Be Corrupted! Failed to save agent config");
    }
}

void load_agent_config(agent_profile* agent) {
    nvs_handle_t handle;
    size_t api_len = sizeof(agent->api);
    memset(agent->api,0,api_len);
    if(nvs_open(SAGENT_NAMESPACE, NVS_READONLY, &handle)==ESP_OK){
        
        ESP_LOGI(TAG,"Reading NVS for agent config.");
         
       
        if(nvs_get_str(handle, "api", agent->api, &api_len)!=ESP_OK)memcpy(agent->api,AGENT_DET_API,sizeof(AGENT_DET_API));
        
        int8_t temp_val;
        if(nvs_get_i8(handle, "model", &temp_val)==ESP_OK){
            agent->model = (ModelType)temp_val;

        }else{
            agent->model=AGENT_DET_MODEL;
        }
        
        if(nvs_get_i8(handle, "provider", &temp_val)==ESP_OK){
            agent->provider = (ProviderType)temp_val;
        }else{
            agent->provider=AGENT_DET_PROVIDER;
        }
        
        if(nvs_get_i8(handle, "tool_choice", &temp_val)==ESP_OK){
            agent->tool_choice = (char)temp_val;
        }else{
            agent->tool_choice=AGENT_DET_TOOL_CHOICE;
        }
    
        if(nvs_get_i8(handle, "stream", &temp_val)==ESP_OK){
            agent->stream = (uint8_t)temp_val;
    
        }else{
            agent->stream=AGENT_DET_STREAM;
        }
        
        int16_t temp_val16;
        if(nvs_get_i16(handle, "max_token", &temp_val16)==ESP_OK){
            agent->max_token = (uint16_t)temp_val16;

        }else{
            agent->max_token=AGENT_DET_MAX_TOKEN;
        }
        nvs_close(handle);
    
        if(nvs_get_i16(handle, "temperature", &temp_val16)==ESP_OK){
            agent->temperature = (float)temp_val16 / 100.0f;
        }else{
            agent->temperature=AGENT_DET_TEMPERATURE;
        }
        
    }
    else{
        ESP_LOGI(TAG,"Cant open NVS Storage,NVS Partition May Be Corrupted or not Exist! using the default setting...");
        strcpy(agent->api, AGENT_DET_API);
        agent->model = AGENT_DET_MODEL;
        agent->provider = AGENT_DET_PROVIDER;
        agent->tool_choice = AGENT_DET_TOOL_CHOICE;
        agent->temperature = AGENT_DET_TEMPERATURE;
        agent->max_token = AGENT_DET_MAX_TOKEN;
        agent->stream = AGENT_DET_STREAM;
        return;
    }
}

void save_chat(history* chat){
    nvs_handle_t handle;
    if(nvs_open(SAGENT_NAMESPACE,NVS_READWRITE,&handle)==ESP_OK){
        nvs_set_i8(handle,"pin_now",chat->pin_now);
        nvs_set_i8(handle,"mpe",chat->mpe);
        size_t blob_size = MAX_MESSAGE * sizeof(uint16_t);
        nvs_set_blob(handle,"start_index",chat->start_index, blob_size);
        nvs_set_blob(handle,"end_index",chat->end_index,blob_size);
        nvs_set_blob(handle,"message_size",chat->message_size,blob_size);
        nvs_set_blob(handle,"role",chat->role,MAX_MESSAGE*sizeof(uint8_t));
        nvs_commit(handle);
        nvs_close(handle);
    }else{
        ESP_LOGI(TAG,"Cant open NVS config Storage,NVS Partition May Be Corrupted! Cant save chat config");
        return;
    }
    if(nvs_open(SAGENT_CHAT_CACHE_NAMESPACE, NVS_READWRITE, &handle)==ESP_OK){
        char chunk_key[10]={0};//MAX chunk_999
        for(uint8_t i=0;i<STORAGE_CHUNKS;i++){
           //size_t chunk_size=(i==chunk_count-1?MAX_MESSAGE_POOL-MESSAGE_CHUNK_SIZE*i:MESSAGE_CHUNK_SIZE);
           snprintf(chunk_key,10,"chunk_%d",i);
           nvs_set_blob(handle,chunk_key,chat->message_pool+i*MESSAGE_CHUNK_SIZE,(i==STORAGE_CHUNKS-1?MAX_MESSAGE_POOL-MESSAGE_CHUNK_SIZE*i:MESSAGE_CHUNK_SIZE));
        }
        nvs_commit(handle);
        nvs_close(handle);
    }else{
        ESP_LOGI(TAG,"Cant open NVS data Storage,NVS Partition May Be Corrupted! Cant save chat.");
    }
}

void load_chat(history* chat){
    nvs_handle_t handle;
    if(nvs_open(SAGENT_NAMESPACE,NVS_READONLY,&handle)==ESP_OK){
        int8_t pin_now=0;
        int8_t mpe=0;
        nvs_get_i8(handle, "pin_now", &pin_now);
        nvs_get_i8(handle, "mpe", &mpe);
        chat->pin_now=(uint8_t)pin_now;
        chat->mpe=(uint8_t)mpe;
        size_t blob_size = MAX_MESSAGE * sizeof(uint16_t);
        nvs_get_blob(handle, "start_index", chat->start_index, &blob_size);
        nvs_get_blob(handle, "end_index", chat->end_index, &blob_size);
        nvs_get_blob(handle, "message_size", chat->message_size, &blob_size);
        blob_size = MAX_MESSAGE * sizeof(uint8_t);
        nvs_get_blob(handle, "role", chat->role, &blob_size);
        nvs_close(handle);
    }else{
        ESP_LOGI(TAG,"Cant open CONFIG Storage,NVS Partition May Be Corrupted!");
    }
    if(nvs_open(SAGENT_CHAT_CACHE_NAMESPACE,NVS_READONLY,&handle)==ESP_OK){
        char chunk_key[10]={0};//MAX chunk_999
        size_t chunk_size;
        for(uint8_t i=0;i<STORAGE_CHUNKS;i++){
           chunk_size=(i==STORAGE_CHUNKS-1?MAX_MESSAGE_POOL-MESSAGE_CHUNK_SIZE*i:MESSAGE_CHUNK_SIZE);
           snprintf(chunk_key,10,"chunk_%d",i);
           nvs_get_blob(handle,chunk_key,chat->message_pool+i*MESSAGE_CHUNK_SIZE,&chunk_size);
        }
        nvs_close(handle);
    }else{
        ESP_LOGI(TAG,"Cant open DATA Storage,NVS Partition May Be Corrupted Or not Existing!");
    }

}

esp_err_t init_agent_chat(history* h,const char* whoyouare){
    if(!h)return ESP_FAIL;
    h->message_pool=(char*)malloc(MAX_MESSAGE_POOL);
    memset(h->message_pool,0,MAX_MESSAGE_POOL);
    size_t len=strlen(whoyouare);
    memcpy(h->message_pool,whoyouare,len);//[0,len-1]->[0,N]
    h->pin_now=0;
    h-> role[0]=ROLE_SYSTEM;
    h->start_index[0]=0;
    h->end_index[0]=(uint16_t)len-1;
    h->message_size[0]=len; 
    //h->last_larest_message=0;
    h->buffer=(char*)malloc(MAX_MESSAGE_POOL/4+1024);
    h->buffer_size=0;
    h->buffer_limit=MAX_MESSAGE_POOL/4+1024;
    return ESP_OK;
}

void shrink_agent_chat(history* c){
    free(c->message_pool);
    free(c->buffer);
    ESP_LOGI(TAG,"Agent memory cleaned...");
}

void add_message(history* h,char* m,RoleType i){
    size_t len=strlen(m);
    uint16_t index1, index2;
    uint16_t n=h->end_index[0];
    uint8_t next_pin=(h->pin_now==MAX_MESSAGE-1?1:h->pin_now+1);
    index1=h->end_index[h->pin_now]+1;
    index2=len+index1-1;

    if(index2>MAX_MESSAGE_POOL-1){
        size_t I=MAX_MESSAGE_POOL-index1;
        size_t II=len-MAX_MESSAGE_POOL+index1;
        memcpy(h->message_pool+index1,m,I);//[0,I-1]->[index1,index1+I-1 (MAX-1)] //I=MAX-index1 //index_A = index1 
        memcpy(h->message_pool+(n+1),m+I,II);//[I,I+II-1]->[N+1,N+II] //II //index_B=N+II //I+II=len
        index2=n+II;
        h->mpe=1;
    }
    else {
        memcpy(h->message_pool+index1,m,len);//[0,len-1]->[index1,index1+len-1] //len=index_B-index_A+1
    };
    
    h->pin_now=next_pin;
    h->role[next_pin]=i;
    h->start_index[next_pin]=index1;
    h->end_index[next_pin]=index2;
    h->message_size[next_pin]=len;
    
    if(h->mpe){
        uint8_t front_pin=(next_pin==MAX_MESSAGE?1:next_pin+1);
        uint16_t a=h->start_index[next_pin];
        uint16_t b=h->end_index[next_pin];
        
        while(!(index2<a&&a<b)){
            h->start_index[front_pin]=0;
            h->end_index[front_pin]=0;
            h->message_size[front_pin]=0;
            if((b<index2&&index2<a)||(a<b&&b<index2)||b==a){
                front_pin=(front_pin==MAX_MESSAGE-1?1:front_pin+1);
                a=h->start_index[front_pin];
                b=h->end_index[front_pin];
                continue;
            }else break;
        }
    }
    
    //for(uint8_t i=0;i<MAX_MESSAGE;i++)if(h->message_n[i].end_index==0)continue;
    //else if(h->message_n[i].message_size>h->message_n[h->last_larest_message].message_size)h->last_larest_message=i;

}

void agent_event_handler(void* args,esp_event_base_t event,int32_t id,void* eventdata){
    switch(id){
        case 0:
        ESP_LOGI(TAG," Sent message...... ");
            break;
        case 1:
            break;
        case 2:
        ESP_LOGI(TAG," Finished Replying");
            break;
        default :
            break;
    }
}
 
void write_message_buffer(history* h,esp_http_client_event_t *evt,uint8_t stream){
    ESP_LOGI(TAG,"write_in_buffer");
    if(stream){
    }else {
        if(h->buffer_size+evt->data_len > h->buffer_limit){
            memcpy(h->buffer+h->buffer_size,evt->data,h->buffer_limit-1024-h->buffer_size);//index from N -> L-1024-1
            h->buffer_size=h->buffer_limit-1024;
            h->buffer[h->buffer_size]='\0';//index -> L-1024 # limitation of buffer lenth
        }else{
            memcpy(h->buffer + h->buffer_size, evt->data, evt->data_len);
            h->buffer_size += evt->data_len;
            h->buffer[h->buffer_size] = '\0';
        }
    }
}

void parse_message_buffer(history* h,uint8_t stream){
    ESP_LOGI(TAG,"parsing json data in buffer");
    if(stream){

    }else{
        uint16_t index_A=0;
        uint16_t index_B=0;
        char* content_key=",\"content\":\"";
        char* content_key_starting_point=strstr(h->buffer,content_key);
        index_A=content_key_starting_point-(h->buffer)+12;
        index_B=index_A;
        while (h->buffer[index_B]!='\0')
        {
            if(h->buffer[index_B]=='"'){
                uint16_t temp = index_B - 1;
                while (h->buffer[temp] == '\\')temp--;
                if ((index_B-temp+1) % 2 == 0)break;
            };
            index_B++;
        } 
        memmove(h->buffer,h->buffer+index_A,index_B-index_A);//A,B-1 -> 0,B-A-1
        h->buffer[index_B-index_A]='\0';//B-A
    }

}

esp_err_t agent_http_handler(esp_http_client_event_t *evt) {
    http_agent_data_t* data=(http_agent_data_t*)evt->user_data;
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            write_message_buffer(data->hist_ptr,evt,data->profile_ptr->stream);
            break;
        case HTTP_EVENT_ON_FINISH:
            parse_message_buffer(data->hist_ptr,data->profile_ptr->stream);
            add_message(data->hist_ptr,data->hist_ptr->buffer,ROLE_ASSISTANT);
            xSemaphoreGive(*data->sem_handle);
            break;
        default:
            break;
    }
    return ESP_OK;
}

void send_chat(history* chat,agent_profile* profile,SemaphoreHandle_t* Sem){
    if(chat->buffer==NULL)chat->buffer=(char*)malloc(chat->buffer_limit);
    char* payload=build_payload(profile,chat);//Largest Memory and Token Consumption!
    http_agent_data_t user_data={
        .hist_ptr=chat,
        .profile_ptr=profile,
        .sem_handle=Sem
    };
    esp_http_client_config_t config = {
        .url = llm_provider[profile->provider],//url
        .event_handler = agent_http_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000, //Timeout
        .user_data = &user_data
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);//heap memory
    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", profile->api);//api
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_post_field(client, payload, strlen(payload));//payload
    if (esp_http_client_perform(client) != ESP_OK)ESP_LOGE("HTTP", "Request failed");
    esp_http_client_cleanup(client);//free
    free(payload);//free
}


