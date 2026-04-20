#include "qwen.h"
#include "esp_log.h"
#include "cJSON.h"
#include "bsp_websocket.h"
#include "mbedtls/base64.h"


// 千问实时语音API地址

const char *websocket_url = "wss://dashscope.aliyuncs.com/api-ws/v1/realtime?model=qwen3-omni-flash-realtime";
// API Key
const char *api_key ="sk-adcb41c70e374e238ea4a9e36b5d083f";


static const char *TAG="qwen";

esp_err_t qwen_init(void)
{
    if(bsp_websocket_is_connected()==true)
    {

    }
    else{
        ESP_LOGE(TAG, "websocket is not connected");
        return ESP_FAIL;
    }
 // 创建根对象
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        ESP_LOGE(TAG,"CJSON create object failed");
        return ESP_FAIL;
    }
    cJSON_AddStringToObject(root, "event_id", "event_ToPZqeobitzUJnt3QqtWg");
    cJSON_AddStringToObject(root, "type", "session.update");

    // 创建 session 对象
    cJSON *session = cJSON_CreateObject();
    if (!session) {
        cJSON_Delete(root);
        ESP_LOGE(TAG,"CJSON create object failed");
        return ESP_FAIL;
    }
    cJSON_AddItemToObject(root, "session", session);
    // 添加 modalities 数组
    cJSON *modalities = cJSON_CreateArray();
    if (!modalities) {
        cJSON_Delete(root);
        ESP_LOGE(TAG,"CJSON create object failed");
        return ESP_FAIL;
    }

    cJSON_AddItemToObject(session, "modalities", modalities);
    cJSON_AddItemToArray(modalities, cJSON_CreateString("text"));
    cJSON_AddItemToArray(modalities, cJSON_CreateString("audio"));
    cJSON_AddStringToObject(session, "voice", "Cherry");
    cJSON_AddStringToObject(session, "input_audio_format", "pcm");
    cJSON_AddStringToObject(session, "output_audio_format", "pcm");
    cJSON_AddStringToObject(session, "instructions", "你是一个五星酒店的女服务员，请准确且友好地解答客户关于房型、设施、价格、预订政策的咨询。请始终以专业和乐于助人的态度回应，杜绝提供未经证实或超出酒店服务范围的信息。");
    
       // 创建 turn_detection 对象
    cJSON *turn_detection = cJSON_CreateObject();
    cJSON_AddStringToObject(turn_detection, "type", "server_vad");
    cJSON_AddNumberToObject(turn_detection, "threshold", 0.5);
    cJSON_AddNumberToObject(turn_detection, "silence_duration_ms", 800);
    cJSON_AddItemToObject(session, "turn_detection", turn_detection);

    // cJSON_AddBoolToObject(session, "enable_search", true);

    // cJSON *search_options = cJSON_CreateObject();
    // cJSON_AddBoolToObject(search_options, "enable_source", true);
    // cJSON_AddItemToObject(session, "search_options", search_options);

    // cJSON_AddNumberToObject(session, "seed", 1314);
    // cJSON_AddNumberToObject(session, "max_tokens", 16384);
    // cJSON_AddNumberToObject(session, "repetition_penalty", 1.05);
    // cJSON_AddNumberToObject(session, "presence_penalty", 0.0);
    // cJSON_AddNumberToObject(session, "top_k", 50);
    // cJSON_AddNumberToObject(session, "top_p", 1.0);
    // cJSON_AddNumberToObject(session, "temperature", 0.9);


    char *json_string = cJSON_PrintUnformatted(root);
    if(json_string)
    {
        //bsp_websocket_send_text(json_string,sizeof(json_string),1000);
        ESP_LOGI(TAG, "json_string: %s", json_string);
        cJSON_free(json_string);
    }

    cJSON_Delete(root);
    return ESP_OK;
}
//base64编码文件，组合成json文件发出
esp_err_t qwen_send_audio(const char * audio_date,int len)
{
    if(bsp_websocket_is_connected()==true)
    {

    }
    else{
        ESP_LOGE(TAG, "websocket is not connected");
        return ESP_FAIL;
    }
     // 创建根对象
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        ESP_LOGE(TAG,"CJSON create object failed");
        return ESP_FAIL;
    }
    cJSON_AddStringToObject(root, "type", qwen_client_event_str[input_audio_buffer_append]);
    cJSON_AddStringToObject(root, "audio", audio_date);
    char *json_string = cJSON_PrintUnformatted(root);
    if(json_string)
    {
        bsp_websocket_send_text(json_string,strlen(json_string),1000);
        cJSON_free(json_string);    //一定要释放该内存，该指针指向的字符串是由malloc申请的
        // ESP_LOGI(TAG, "json_string: %s", json_string);
        
    }
    cJSON_Delete(root);
    return ESP_OK;
}