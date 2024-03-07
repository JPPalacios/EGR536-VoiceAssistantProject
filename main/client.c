/* Record WAV file to SD Card

  This example code is in the Public Domain (or CC0 licensed, at your option.)

  Unless required by applicable law or agreed to in writing, this
  software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
  CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "esp_http_client.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "board.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "wav_encoder.h"
#include "mp3_decoder.h"
#include "esp_peripherals.h"
#include "periph_button.h"
#include "periph_wifi.h"
#include "periph_sdcard.h"
#include "filter_resample.h"
#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

#define RESPONSE_URI "http://10.0.0.107:8000/speech_response.mp3"

#define AUDIO_SAMPLE_RATE  24000
#define AUDIO_BITS         16
#define AUDIO_CHANNELS     1

static const char *TAG = "VOICE_ASSISTANT";
static esp_periph_set_handle_t set;
int player_volume;

// todo: fix Wi-Fi, talk to IT

esp_err_t _http_stream_event_handle(http_stream_event_msg_t *msg)
{
  esp_http_client_handle_t http = (esp_http_client_handle_t)msg->http_client;
  char len_buf[16];
  static int total_write = 0;

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



static audio_element_handle_t create_i2s_stream(audio_stream_type_t type)
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

static audio_element_handle_t create_http_stream(audio_stream_type_t type)
{
  http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
  http_cfg.type = type;

  if (http_cfg.type == AUDIO_STREAM_WRITER)
  {
    http_cfg.event_handle = _http_stream_event_handle;
  }

  audio_element_handle_t http_stream = http_stream_init(&http_cfg);
  mem_assert(http_stream);
  return http_stream;
}

static audio_element_handle_t create_mp3_decoder()
{
  mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
  return mp3_decoder_init(&mp3_cfg);
}



void record_playback_task(audio_board_handle_t board_handle)
{
  audio_pipeline_handle_t pipeline_record = NULL;
  audio_pipeline_handle_t pipeline_play   = NULL;
  audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();

  ESP_LOGI(TAG, "[1.1] Initialize recorder pipeline");
  pipeline_record = audio_pipeline_init(&pipeline_cfg);
  pipeline_play = audio_pipeline_init(&pipeline_cfg);


  //> setup our recorder pipeline elements
  ESP_LOGI(TAG, "[1.2] Create audio elements for recorder pipeline");
  audio_element_handle_t i2s_stream_reader = create_i2s_stream(AUDIO_STREAM_READER);
  audio_element_handle_t http_stream_writer = create_http_stream(AUDIO_STREAM_WRITER);

  ESP_LOGI(TAG, "[1.3] Register audio elements to recorder pipeline");
  audio_pipeline_register(pipeline_record, i2s_stream_reader, "i2s_reader");
  audio_pipeline_register(pipeline_record, http_stream_writer, "http_writer");

  const char *link_rec[2] = {"i2s_reader", "http_writer"};
  audio_pipeline_link(pipeline_record, &link_rec[0], 2);


  //> setup our play pipeline elements
  ESP_LOGI(TAG, "[2.2] Create audio elements for playback pipeline");
  audio_element_handle_t i2s_stream_writer = create_i2s_stream(AUDIO_STREAM_WRITER);
  audio_element_handle_t http_stream_reader = create_http_stream(AUDIO_STREAM_READER);
  audio_element_handle_t mp3_decoder = create_mp3_decoder();

  ESP_LOGI(TAG, "[2.3] Register audio elements to playback pipeline");
  audio_pipeline_register(pipeline_play, http_stream_reader, "http_reader");
  audio_pipeline_register(pipeline_play, mp3_decoder, "mp3");
  audio_pipeline_register(pipeline_play, i2s_stream_writer, "i2s_writer");

  const char *link_play[3] = {"http_reader", "mp3", "i2s_writer"};
  audio_pipeline_link(pipeline_play, &link_play[0], 3);


  //> setup event listener
  ESP_LOGI(TAG, "[ 3 ] Set up  event listener");
  audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
  audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);
  audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);
  ESP_LOGW(TAG, "Press [Rec] to start recording");

  audio_element_set_uri(http_stream_writer, CONFIG_SERVER_URI);
  audio_element_set_uri(http_stream_reader, RESPONSE_URI);

  //> run event loop
  while(1)
  {
    audio_event_iface_msg_t msg;

    esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
    if (ret != ESP_OK)
    {
      ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
      continue;
    }

    if ((int) msg.data == get_input_volup_id())
    {
      ESP_LOGI(TAG, "[ * ] [Vol+] touch tap event");
      player_volume += 10;
      if (player_volume > 100) {
        player_volume = 100;
      }
      audio_hal_set_volume(board_handle->audio_hal, player_volume);
      ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
    }

    if ((int) msg.data == get_input_voldown_id()) {
      ESP_LOGI(TAG, "[ * ] [Vol-] touch tap event");
      player_volume -= 10;
      if (player_volume < 0) {
        player_volume = 0;
      }
      audio_hal_set_volume(board_handle->audio_hal, player_volume);
      ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
    }

    if ((int) msg.data == get_input_mode_id())
    {
      if ((msg.cmd == PERIPH_BUTTON_LONG_PRESSED) || (msg.cmd == PERIPH_BUTTON_PRESSED))
      {
        ESP_LOGW(TAG, "[ MODE ] Button pressed, stop event received");
        break;
      }
    }

    if ((int) msg.data == get_input_rec_id())
    {
      if (msg.cmd == PERIPH_BUTTON_PRESSED)
      {
        ESP_LOGE(TAG, "Now recording, release [Rec] to STOP");
        audio_pipeline_stop(pipeline_play);
        audio_pipeline_wait_for_stop(pipeline_play);
        audio_pipeline_terminate(pipeline_play);
        audio_pipeline_reset_ringbuffer(pipeline_play);
        audio_pipeline_reset_elements(pipeline_play);

        /**
         * Audio Recording Flow:
         * [codec_chip]-->i2s_stream--->wav_encoder-->fatfs_stream-->[sdcard]
         */
        // ESP_LOGI(TAG, "Setup file path to save recorded audio");
        i2s_stream_set_clk(i2s_stream_reader, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
        // audio_element_set_uri(fatfs_writer_el, "/sdcard/rec.wav");
        ESP_LOGI(TAG, "Sending recording to server...");
        audio_pipeline_run(pipeline_record);
      }
      else if (msg.cmd == PERIPH_BUTTON_RELEASE || msg.cmd == PERIPH_BUTTON_LONG_RELEASE)
      {
        ESP_LOGI(TAG, "START Playback");
        // audio_element_set_ringbuf_done(i2s_stream_reader);
        audio_pipeline_stop(pipeline_record);
        audio_pipeline_wait_for_stop(pipeline_record);
        audio_pipeline_terminate(pipeline_record);
        audio_pipeline_reset_ringbuffer(pipeline_record);
        audio_pipeline_reset_elements(pipeline_record);

        /**
         * Audio Playback Flow:
         * [sdcard]-->fatfs_stream-->wav_decoder-->sonic-->i2s_stream-->[codec_chip]
         */
        // ESP_LOGI(TAG, "Setup file path to read the wav audio to play");
        i2s_stream_set_clk(i2s_stream_writer, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
        // audio_element_set_uri(fatfs_reader_el, "/sdcard/rec.wav");
        // if (is_modify_speed) {
          // sonic_set_pitch_and_speed_info(sonic_el, 1.0f, SONIC_SPEED);
        // } else {
          // sonic_set_pitch_and_speed_info(sonic_el, SONIC_PITCH, 1.0f);
        // }
        audio_pipeline_run(pipeline_play);
      }
    }

    // note: not seeing this if-statement executed...still works from playback pipeline?
    if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
      && msg.source == (void *) mp3_decoder
      && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO)
    {
      audio_element_info_t music_info = {0};
      audio_element_getinfo(mp3_decoder, &music_info);

      ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
              music_info.sample_rates, music_info.bits, music_info.channels);

      i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
      continue;
    }
  }

  ESP_LOGI(TAG, "[ 4 ] Stop audio_pipeline");
  audio_pipeline_stop(pipeline_record);
  audio_pipeline_wait_for_stop(pipeline_record);
  audio_pipeline_terminate(pipeline_record);
  audio_pipeline_stop(pipeline_play);
  audio_pipeline_wait_for_stop(pipeline_play);
  audio_pipeline_terminate(pipeline_play);

  // audio_pipeline_unregister(pipeline_play, fatfs_reader_el);
  // audio_pipeline_unregister(pipeline_play, wav_decoder_el);
  audio_pipeline_unregister(pipeline_play, i2s_stream_writer);
  audio_pipeline_unregister(pipeline_play, http_stream_reader);
  audio_pipeline_unregister(pipeline_play, mp3_decoder);

  audio_pipeline_unregister(pipeline_record, i2s_stream_reader);
  audio_pipeline_unregister(pipeline_record, http_stream_writer);
  // audio_pipeline_unregister(pipeline_rec, sonic_el);
  // audio_pipeline_unregister(pipeline_rec, wav_encoder_el);
  // audio_pipeline_unregister(pipeline_rec, fatfs_writer_el);

  /* Terminate the pipeline before removing the listener */
  audio_pipeline_remove_listener(pipeline_record);
  audio_pipeline_remove_listener(pipeline_play);

  /* Stop all peripherals before removing the listener */
  esp_periph_set_stop_all(set);
  audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

  /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
  audio_event_iface_destroy(evt);

  /* Release all resources */
  audio_pipeline_deinit(pipeline_record);
  audio_pipeline_deinit(pipeline_play);

  // audio_element_deinit(fatfs_reader_el);
  // audio_element_deinit(wav_decoder_el);
  audio_element_deinit(i2s_stream_writer);
  audio_element_deinit(http_stream_reader);
  audio_element_deinit(mp3_decoder);

  audio_element_deinit(i2s_stream_reader);
  audio_element_deinit(http_stream_writer);
  // audio_element_deinit(sonic_el);
  // audio_element_deinit(wav_encoder_el);
  // audio_element_deinit(fatfs_writer_el);
}

void app_main(void)
{
  esp_log_level_set("*", ESP_LOG_WARN);
  esp_log_level_set(TAG, ESP_LOG_INFO);

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
  ESP_ERROR_CHECK(esp_netif_init());
#else
  tcpip_adapter_init();
#endif

  // Initialize peripherals management
  esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
  set = esp_periph_set_init(&periph_cfg);

  // Connect to the Wi-Fi network
  ESP_LOGI(TAG, "[ . ] Start and wait for Wi-Fi network");
  periph_wifi_cfg_t wifi_cfg = {
      .wifi_config.sta.ssid = CONFIG_WIFI_SSID,
      .wifi_config.sta.password = CONFIG_WIFI_PASSWORD,
  };
  esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
  esp_periph_start(set, wifi_handle);
  periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

  // Initialize SD Card peripheral
  // audio_board_sdcard_init(set, SD_MODE_1_LINE);

  // Initialize Button peripheral
  audio_board_key_init(set);

  // Setup audio codec
  ESP_LOGI(TAG, "[ . ] Start codec chip");
  audio_board_handle_t board_handle = audio_board_init();
  audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

  audio_hal_get_volume(board_handle->audio_hal, &player_volume);

  // Start record/playback task
  record_playback_task(board_handle);
  esp_periph_set_destroy(set);
}