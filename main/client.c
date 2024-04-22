/* Voice Assistant

  This example code is in the Public Domain (or CC0 licensed, at your option.)

  Unless required by applicable law or agreed to in writing, this
  software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
  CONDITIONS OF ANY KIND, either express or implied.
*/


#include "client.h"
#include "sdkconfig.h"
#include "board.h"

#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "audio_hal.h"
#include "audio_element.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "wav_encoder.h"
#include "mp3_decoder.h"

#include "esp_netif.h"
#include "periph_wifi.h"
#include "esp_http_client.h"
#include "http_stream.h"
#include "esp_http_client.h"

static const char *TAG = "CLIENT";

/* NVS FUNCTIONS */

esp_err_t save_prompt_count(void)
{
  nvs_handle_t my_handle;
  esp_err_t err;

  // Open
  err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
  if (err != ESP_OK) return err;

  // Read
  int32_t prompt_count = 0; // value will default to 0, if not set yet in NVS
  err = nvs_get_i32(my_handle, "prompt_count", &prompt_count);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

  // Write
  prompt_count++;
  err = nvs_set_i32(my_handle, "prompt_count", prompt_count);
  if (err != ESP_OK) return err;

  // Commit written value.
  // After setting any values, nvs_commit() must be called to ensure changes are written
  // to flash storage. Implementations may write to storage at other times,
  // but this is not guaranteed.
  err = nvs_commit(my_handle);
  if (err != ESP_OK) return err;

  /* Close */
  nvs_close(my_handle);

  return ESP_OK;
}

int get_prompt_count(void)
{
  nvs_handle_t my_handle;
  esp_err_t err;

  // Open
  err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
  if (err != ESP_OK) return err;

  // Read
  int32_t prompt_count = 0; // value will default to 0, if not set yet in NVS
  err = nvs_get_i32(my_handle, "prompt_count", &prompt_count);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

  return prompt_count;
}

esp_err_t save_run_time(void)
{
  nvs_handle_t my_handle;
  esp_err_t err;

  // Open
  err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
  if (err != ESP_OK) return err;

  // Read the size of memory space required for blob
  size_t required_size = 0;  // value will default to 0, if not set yet in NVS
  err = nvs_get_blob(my_handle, "run_time", NULL, &required_size);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

  // Read previously saved blob if available
  uint32_t* run_time = malloc(required_size + sizeof(uint32_t));
  if (required_size > 0) {
    err = nvs_get_blob(my_handle, "run_time", run_time, &required_size);
    if (err != ESP_OK) {
      free(run_time);
      return err;
    }
  }

  // Write value including previously saved blob if available
  required_size += sizeof(uint32_t);
  run_time[required_size / sizeof(uint32_t) - 1] = xTaskGetTickCount() * portTICK_PERIOD_MS;
  err = nvs_set_blob(my_handle, "run_time", run_time, required_size);
  free(run_time);

  if (err != ESP_OK) return err;

  // Commit
  err = nvs_commit(my_handle);
  if (err != ESP_OK) return err;

  // Close
  nvs_close(my_handle);
  return ESP_OK;
}

esp_err_t print_what_saved(void)
{
  nvs_handle_t my_handle;
  esp_err_t err;

  printf("Voice Assistant Logs\n");

  // Open
  err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
  if (err != ESP_OK) return err;

  // Read prompt counter
  int32_t prompt_count = 0; // value will default to 0, if not set yet in NVS
  err = nvs_get_i32(my_handle, "prompt_count", &prompt_count);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
  printf("\tPrompt count: %d\n", prompt_count);

  // Read run time blob
  size_t required_size = 0;  // value will default to 0, if not set yet in NVS
  // obtain required memory space to store blob being read from NVS
  err = nvs_get_blob(my_handle, "run_time", NULL, &required_size);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
  printf("\tRun time:\n");
  if (required_size == 0) {
    printf("\tNothing saved yet!\n");
  } else {
    uint32_t* run_time = malloc(required_size);
    err = nvs_get_blob(my_handle, "run_time", run_time, &required_size);
    if (err != ESP_OK) {
      free(run_time);
      return err;
    }
    for (int i = 0; i < required_size / sizeof(uint32_t); i++) {
      printf("\t\t%d: %d\n", i + 1, run_time[i]);
    }
    free(run_time);
  }

  // Close
  nvs_close(my_handle);
  return ESP_OK;
}

/* WI-FI FUNCTIONS */

esp_err_t _http_stream_event_handler(http_stream_event_msg_t *msg)
{
  esp_http_client_handle_t http = (esp_http_client_handle_t)msg->http_client;
  char len_buf[16];
  static int total_write = 0;

  /* EVENTS FOR MUSIC STREAM */
  if (msg->event_id == HTTP_STREAM_RESOLVE_ALL_TRACKS) {
    return ESP_OK;
  }

  if (msg->event_id == HTTP_STREAM_FINISH_TRACK) {
    return http_stream_next_track(msg->el);
  }
  if (msg->event_id == HTTP_STREAM_FINISH_PLAYLIST) {
    return http_stream_fetch_again(msg->el);
  }

  /* EVENTS FOR SERVER STREAMS */

  if (msg->event_id == HTTP_STREAM_PRE_REQUEST) {
    // set header
    ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_PRE_REQUEST, lenght=%d", msg->buffer_len);
    esp_http_client_set_method(http, HTTP_METHOD_POST);
    char dat[10] = {0};
    snprintf(dat, sizeof(dat), "%d", AUDIO_SAMPLE_RATE);
    esp_http_client_set_header(http, "x-audio-sample-rates", dat);
    memset(dat, 0, sizeof(dat));
    snprintf(dat, sizeof(dat), "%d", AUDIO_BITS);
    esp_http_client_set_header(http, "x-audio-bits", dat);
    memset(dat, 0, sizeof(dat));
    snprintf(dat, sizeof(dat), "%d", AUDIO_CHANNELS);
    esp_http_client_set_header(http, "x-audio-channel", dat);
    total_write = 0;
    return ESP_OK;
  }

  if (msg->event_id == HTTP_STREAM_ON_REQUEST) {
    // write data
    int wlen = sprintf(len_buf, "%x\r\n", msg->buffer_len);
    if (esp_http_client_write(http, len_buf, wlen) <= 0) {
      return ESP_FAIL;
    }
    if (esp_http_client_write(http, msg->buffer, msg->buffer_len) <= 0) {
      return ESP_FAIL;
    }
    if (esp_http_client_write(http, "\r\n", 2) <= 0) {
      return ESP_FAIL;
    }
    total_write += msg->buffer_len;
    printf("\033[A\33[2K\rTotal bytes written: %d\n", total_write);
    return msg->buffer_len;
  }

  if (msg->event_id == HTTP_STREAM_ON_RESPONSE){
    ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_ON_RESPONSE received!");
  }

  if (msg->event_id == HTTP_STREAM_POST_REQUEST) {
    ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker");
    if (esp_http_client_write(http, "0\r\n\r\n", 5) <= 0) {
      return ESP_FAIL;
    }
    return ESP_OK;
  }

  if (msg->event_id == HTTP_STREAM_FINISH_REQUEST) {
    ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_FINISH_REQUEST");
    char *buf = calloc(1, 64);
    assert(buf);
    int read_len = esp_http_client_read(http, buf, 64);
    if (read_len <= 0) {
      free(buf);
      return ESP_FAIL;
    }
    buf[read_len] = 0;
    ESP_LOGI(TAG, "Got HTTP Response = %s", (char *)buf);
    free(buf);
    return ESP_OK;
  }
  return ESP_OK;
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{

  switch(evt->event_id) {
    case HTTP_EVENT_ERROR:
      ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
      ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
      break;
    case HTTP_EVENT_ON_HEADER:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
      printf("%.*s", evt->data_len, (char*)evt->data);
      break;
    case HTTP_EVENT_ON_DATA:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      break;
    case HTTP_EVENT_ON_FINISH:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
      break;
    case HTTP_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
      break;
  }

  return ESP_OK;
}

esp_err_t http_post_request(const char* url, int count)
{
  esp_err_t error = ESP_OK;

  esp_http_client_config_t config = {
    .url = url,
    .event_handler = _http_event_handler,
  };
  ESP_LOGW(TAG, "sending out to %s", url);
  esp_http_client_handle_t client = esp_http_client_init(&config);

  char data[20]; // Adjust the size according to your data
  // snprintf(data, sizeof(data), "%d", count);
  snprintf(data, sizeof(data), "{\"counter\": %d}", count);

  esp_http_client_set_method(client, HTTP_METHOD_POST);
  // esp_http_client_set_header(client, "prompt_count", data);
  esp_http_client_set_header(client, "Content-Type", "application/json");

  esp_http_client_set_post_field(client, data, strlen(data));

  error = esp_http_client_perform(client);
  if (error == ESP_OK)
  {
    ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
            esp_http_client_get_status_code(client),
            esp_http_client_get_content_length(client));
  }
  else
  {
    ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(error));
  }
  esp_http_client_cleanup(client);

  return error;
}

/* AUDIO-ELEMENT FUNCTIONS */

audio_element_handle_t create_i2s_stream(audio_stream_type_t type)
{
  if (type == AUDIO_STREAM_READER)
  {
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(CODEC_ADC_I2S_PORT, 44100, AUDIO_BITS, AUDIO_STREAM_READER);
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_cfg.out_rb_size = 16 * 1024; // Increase buffer to avoid missing data in bad network conditions
    audio_element_handle_t i2s_stream = i2s_stream_init(&i2s_cfg);
    return i2s_stream;
  }
  else
  {
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t i2s_stream = i2s_stream_init(&i2s_cfg);
    return i2s_stream;
  }
}

audio_element_handle_t create_http_stream(audio_stream_type_t type)
{
  http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
  http_cfg.type = type;

  if (http_cfg.type == AUDIO_STREAM_WRITER)
  {
    http_cfg.event_handle = _http_stream_event_handler;
  }
  else
  {
    http_cfg.enable_playlist_parser = true; // enables music streaming
  }

  audio_element_handle_t http_stream = http_stream_init(&http_cfg);
  mem_assert(http_stream);
  return http_stream;
}

audio_element_handle_t create_mp3_decoder(void)
{
  mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
  return mp3_decoder_init(&mp3_cfg);
}

int get_audio_hal_volume(audio_board_handle_t board_handle)
{
  int player_volume;
  ESP_ERROR_CHECK(audio_hal_get_volume(board_handle->audio_hal, &player_volume));
  return player_volume;
}