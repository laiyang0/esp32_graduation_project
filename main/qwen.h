#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 千问实时语音API地址
extern const char *websocket_url;

// API Key
extern const char *api_key;

//定义千问实时多模态客户端事件类型
typedef enum{
    session_update=0,            //客户端建立 WebSocket 连接后，需首先发送该事件，用于更新会话的默认配置。服务端收到 session.update 事件后会校验参数。如果参数不合法，则返回错误；如果参数合法，则更新并返回完整的配置
    response_create=1,          //指示服务端创建模型响应。在VAD模式下，服务端会自动创建模型响应，无需发送该事件
    response_cancel=2,          //客户端发送此事件用以取消正在进行的响应。如果没有任何响应可供取消，服务端将响应错误事件
    input_audio_buffer_append=3, //用于将音频字节追加到输入音频缓冲区
    input_audio_buffer_commit=4, //用于提交输入音频缓冲区中的音频数据。在VAD模式下，服务端会自动提交音频数据，无需发送该事件,Manual 模式：客户端必须提交音频缓冲区才能创建用户消息项
    input_audio_buffer_clear=5,  //用于清空输入音频缓冲区。在VAD模式下，服务端会自动清空音频缓冲区，无需发送该事件
    input_image_buffer_append=6, //用于将图像字节追加到输入图像缓冲区
}qwen_client_event_t;
//定义千问实时多模态服务器端事件类型
typedef enum{
    error=0,
    session_created=1,                      //server在连接后主动发出，通知client session创建成功
    session_updated=2,                      //server接收到client的session.update事件后响应
    input_audio_buffer_speech_started=3,    //在 VAD 模式下，当服务端在音频缓冲区中检测到语音开始时，会返回此事件
    input_audio_buffer_speech_stopped=4,    //在 VAD 模式下，当音频缓冲区中检测到语音结束时，服务端会返回此事件
    input_audio_buffer_committed=5,         //当服务端将音频缓冲区中的音频数据提交给后端处理时，会返回此事件
    input_audio_buffer_cleared=6,           //客户端发送input_audio_buffer.clear事件后，服务端将返回此事件
    conversation_item_created=7,            //当对话项创建时返回此事件
    conversation_item_input_completed=8,    //用户音频写入缓冲区后生成的转录结果。其转录由独立的语音识别模型（当前固定为 gummy-realtime-v1）处理
    conversation_item_input_failed=9,        //启用输入音频转录后，若用户音频转录失败，服务端会返回此事件。此事件独立于 error 事件，便于客户端识别
    response_created=10,                    //服务端生成新的模型响应时，会返回此事件
    response_done=11,                       //响应生成完成后，服务端会返回此事件。事件中的 response 对象包含除原始音频数据外的全部输出项
    response_text_delta=12,                 //当输出模态仅包含文本，且模型增量生成新的文本时，服务端将返回此事件
    response_text_done=13,                   //当输出模态仅包含文本，且模型生成的文本结束时，服务端将返回此事件
    response_audio_delta=14,                 //当输出模态包含音频，且模型增量生成新的音频时，服务端将返回此事件
    response_audio_done=15,                    //当输出模态包含音频，且模型生成的音频结束时，服务端将返回此事件
    response_audio_transcript_delta=16,        //当输出模态包含音频，且模型增量生成新的音频对应的文本时，服务端将返回此事件
    response_audio_transcript_done=17,         //当输出模态包含音频，且模型完成音频转录后，服务端将返回
    response_output_item_added=18,             //在响应生成过程中创建新项目时，服务端返回此事件
    response_output_item_done=19,             //当新的项目输出完成时，服务端返回此事件
    response_content_part_added=20,           //在响应生成过程中，向助手消息项中添加新内容部分时，服务端返回此事件
    response_content_part_done=21,            //在助手消息项中的内容部分完成流式传输时，服务端返回此事件
}qwen_server_event_t;

// 客户端事件字符串数组
static const char * const qwen_client_event_str[7] = {
    "session.update",                 // 0
    "response.create",                // 1
    "response.cancel",                // 2
    "input_audio_buffer.append",      // 3
    "input_audio_buffer.commit",      // 4
    "input_audio_buffer.clear",       // 5
    "input_image_buffer.append"       // 6
};

// 服务端事件字符串数组
static const char * const qwen_server_event_str[22] = {
    "error",                                 // 0
    "session.created",                       // 1
    "session.updated",                       // 2
    "input_audio_buffer.speech_started",     // 3
    "input_audio_buffer.speech_stopped",     // 4
    "input_audio_buffe.committed",          // 5
    "input_audio_buffer.cleared",            // 6
    "conversation.item.created",             // 7
    "conversation.item.input_audio_transcription.completed",     // 8
    "conversation.item.input_audio_transcription.failed",        // 9
    "response.created",                      // 10
    "response.done",                         // 11
    "response.text.delta",                   // 12
    "response.text.done",                    // 13
    "response.audio.delta",                  // 14
    "response.audio.done",                    // 15 
    "response.audio_transcript.delta",        // 16
    "response.audio_transcript.done",         // 17
    "response.output_item.added",             // 18
    "response.output_item.done",             // 19
    "response.content_part.added",           // 20
    "response.content_part.done"             // 21
};

esp_err_t qwen_init(void);
esp_err_t qwen_send_audio(const char * audio_date,int len);



#ifdef __cplusplus
}
#endif