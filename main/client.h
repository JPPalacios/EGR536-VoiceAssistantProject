#ifndef client_h
#define client_h

#include "audio_element.h"
#include "audio_common.h"
#include "http_stream.h"
#include "board.h"
#include "esp_http_client.h"


/* NVS FLASH PARAMETERS */
#define STORAGE_NAMESPACE "storage"

/* WI-FI PARAMETERS */
#define SELECTED_NETWORK 1

#if SELECTED_NETWORK == 0
#define WIFI_SSID     "SH1100SW2.4"
#define WIFI_PASSWORD "Cocohydra123"
#define SERVER "10.0.0.107"
#elif SELECTED_NETWORK == 1
#define WIFI_SSID     "iphone"
#define WIFI_PASSWORD "74515900"
#define SERVER "172.20.10.7"
// #define SERVER "JuanPalacios.pythonanywhere.com"
#else
#define WIFI_SSID     "GV-Student"
#define WIFI_PASSWORD "G01783155iwzj-gvsu"
#define SERVER "35.40.169.239"
#endif

#define PORT "8000"

#define CHIME_PATH "chime"
#define LOG_PATH "log"
#define UPLOAD_PATH "upload"

#define SERVER_CHIME_URI  "http://" SERVER ":" PORT "/" CHIME_PATH
#define SERVER_LOG_URI    "http://" SERVER ":" PORT "/" LOG_PATH
#define SERVER_UPLOAD_URI "http://" SERVER ":" PORT "/" UPLOAD_PATH

#define RESPONSE_PATH "speech_response.mp3"
#define RESPONSE_URI  "http://" SERVER ":" PORT "/" RESPONSE_PATH

#define MP3_STREAM_URI_0 "http://edge.audioxi.com/IRADNW"
#define MP3_STREAM_URI_1 "http://playerservices.streamtheworld.com/pls/WUOMFM.pls"
#define MP3_STREAM_URI_2 "http://26183.live.streamtheworld.com/CHTGFM.mp3"

/* AUDIO PARAMETERS */
#define AUDIO_SAMPLE_RATE  24000
#define AUDIO_BITS         16
#define AUDIO_CHANNELS     1

/* NVS FUNCTIONS */

esp_err_t save_prompt_count(void);

int get_prompt_count(void);

esp_err_t save_run_time(void);

esp_err_t print_what_saved(void);


/* WI-FI FUNCTIONS */

esp_err_t _http_stream_event_handler(http_stream_event_msg_t *msg);

esp_err_t _http_event_handler(esp_http_client_event_t *evt);

esp_err_t http_post_request(const char* url, int count);

/* AUDIO-ELEMENT FUNCTIONS */

audio_element_handle_t create_i2s_stream(audio_stream_type_t type);

audio_element_handle_t create_http_stream(audio_stream_type_t type);

audio_element_handle_t create_mp3_decoder(void);

int get_audio_hal_volume(audio_board_handle_t board_handle);

#endif /* client_h */