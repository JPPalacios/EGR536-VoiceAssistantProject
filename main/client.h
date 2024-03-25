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
#define SELECTED_NETWORK 0
#if SELECTED_NETWORK == 0
#define WIFI_SSID     "ssd0"
#define WIFI_PASSWORD "password0"
#define SERVER "ipaddr0"
#elif SELECTED_NETWORK == 1
#define WIFI_SSID     "ssd1"
#define WIFI_PASSWORD "password1"
#define SERVER "ipaddr1"
#elif SELECTED_NETWORK == 2
#define WIFI_SSID     "ssd2"
#define WIFI_PASSWORD "password2"
#define SERVER "ipaddr2"
#else
#define WIFI_SSID     "myssd"
#define WIFI_PASSWORD "password"
#define SERVER "ipaddr"
#endif

#define PORT "8000"

#define LOG_PATH "log"
#define UPLOAD_PATH "upload"

#define SERVER_LOG_URI    "http://" SERVER ":" PORT "/" LOG_PATH
#define SERVER_UPLOAD_URI "http://" SERVER ":" PORT "/" UPLOAD_PATH

#define RESPONSE_PATH "speech_response.mp3"
#define RESPONSE_URI  "http://" SERVER ":" PORT "/" RESPONSE_PATH

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