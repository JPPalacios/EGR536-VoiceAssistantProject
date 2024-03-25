/* Voice Assistant

  This example code is in the Public Domain (or CC0 licensed, at your option.)

  Unless required by applicable law or agreed to in writing, this
  software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
  CONDITIONS OF ANY KIND, either express or implied.
*/

#include "client.h"
#include "sdkconfig.h"
#include "board.h"
#include "esp_peripherals.h"
#include "periph_button.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "audio_pipeline.h"
#include "audio_element.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "wav_encoder.h"
#include "mp3_decoder.h"

#include "esp_netif.h"
#include "periph_wifi.h"
#include "esp_http_client.h"
#include "http_stream.h"


static esp_periph_set_handle_t periph_set;
audio_board_handle_t board_handle;

static const char *TAG = "VOICE_ASSISTANT";

esp_err_t configure_flash(void)
{
  esp_err_t err;

  ESP_LOGI(TAG, "configuring NVS flash...");
  err = nvs_flash_init();

  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGE(TAG, "NVS partition was truncated, attempting to reset NVS flash");
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }

  ESP_ERROR_CHECK(print_what_saved());
  return err;
}

void connect_to_wifi(void)
{
  ESP_LOGI(TAG, "connecting to Wi-Fi network...");

  ESP_ERROR_CHECK(esp_netif_init());

  periph_wifi_cfg_t wifi_cfg = {
    .wifi_config.sta.ssid = WIFI_SSID,
    .wifi_config.sta.password = WIFI_PASSWORD,
  };

  esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
  ESP_ERROR_CHECK(esp_periph_start(periph_set, wifi_handle));

  ESP_ERROR_CHECK(periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY));
}



void run_voice_assistant_task()
{
  ESP_LOGI(TAG, "[ ENTER ] Launching voice assistant task...");

  esp_err_t error = ESP_OK;

  audio_pipeline_handle_t record_pipeline = NULL;
  audio_pipeline_handle_t play_pipeline   = NULL;

  audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();

  int player_volume = get_audio_hal_volume(board_handle);


  ESP_LOGI(TAG, "[1.1] Initialize all pipelines");
  record_pipeline = audio_pipeline_init(&pipeline_cfg);
  play_pipeline   = audio_pipeline_init(&pipeline_cfg);

  ESP_LOGI(TAG, "[1.2] Create audio elements for recorder pipeline");
  audio_element_handle_t i2s_stream_reader  = create_i2s_stream(AUDIO_STREAM_READER);
  audio_element_handle_t http_stream_writer = create_http_stream(AUDIO_STREAM_WRITER);

  ESP_LOGI(TAG, "[1.3] Register audio elements to recorder pipeline");
  audio_pipeline_register(record_pipeline, i2s_stream_reader, "i2s_reader");
  audio_pipeline_register(record_pipeline, http_stream_writer, "http_writer");

  const char *link_rec[2] = {"i2s_reader", "http_writer"};
  audio_pipeline_link(record_pipeline, &link_rec[0], 2);


  ESP_LOGI(TAG, "[2.2] Create audio elements for play pipeline");
  audio_element_handle_t i2s_stream_writer = create_i2s_stream(AUDIO_STREAM_WRITER);
  audio_element_handle_t http_stream_reader = create_http_stream(AUDIO_STREAM_READER);
  audio_element_handle_t mp3_decoder = create_mp3_decoder();

  ESP_LOGI(TAG, "[2.3] Register audio elements to play pipeline");
  audio_pipeline_register(play_pipeline, http_stream_reader, "http_reader");
  audio_pipeline_register(play_pipeline, mp3_decoder, "mp3");
  audio_pipeline_register(play_pipeline, i2s_stream_writer, "i2s_writer");

  const char *link_play[3] = {"http_reader", "mp3", "i2s_writer"};
  audio_pipeline_link(play_pipeline, &link_play[0], 3);


  ESP_LOGI(TAG, "[ 3.1 ] Set up event listener");
  audio_event_iface_cfg_t event_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
  audio_event_iface_handle_t event  = audio_event_iface_init(&event_cfg);
  audio_event_iface_set_listener(esp_periph_set_get_event_iface(periph_set), event);

  ESP_LOGI(TAG, "[ 3.2 ] Set up http writer/reader URI's");
  audio_element_set_uri(http_stream_writer, SERVER_UPLOAD_URI);
  audio_element_set_uri(http_stream_reader, RESPONSE_URI);

  ESP_LOGW(TAG, "[ LOOP ] Press [Rec] to start recording, press [Mode] to request data log");
  while(1)
  {
    audio_event_iface_msg_t msg;

    error = audio_event_iface_listen(event, &msg, portMAX_DELAY);
    if (error != ESP_OK)
    {
      ESP_LOGE(TAG, "[ * ] Event interface error : %d", error);
      continue;
    }

    if ((int) msg.data == get_input_volup_id())
    {
      player_volume += 10;
      if (player_volume > 100)
      {
        player_volume = 100;
      }
      audio_hal_set_volume(board_handle->audio_hal, player_volume);
      ESP_LOGI(TAG, "[ * ] Volume +: %d %%", player_volume);
    }

    if ((int) msg.data == get_input_voldown_id())
    {
      player_volume -= 10;
      if (player_volume < 0)
      {
        player_volume = 0;
      }
      audio_hal_set_volume(board_handle->audio_hal, player_volume);
      ESP_LOGI(TAG, "[ * ] Volume -: %d %%", player_volume);
    }

    if ((int) msg.data == get_input_mode_id())
    {
      if ((msg.cmd == PERIPH_BUTTON_PRESSED))
      {
        /**
         * Audio record flow:
         * nvs_storage --> http_post_msg ))) (2.4 GHz Wi-Fi) ))) [http_server]
         */

        /* Pause record pipelines */
        audio_pipeline_stop(play_pipeline);
        audio_pipeline_wait_for_stop(play_pipeline);
        audio_pipeline_reset_ringbuffer(play_pipeline);
        audio_pipeline_reset_elements(play_pipeline);

        /* Send HTTP POST request with prompt_counter data */
        char data[20];
        snprintf(data, sizeof(data), "\"prompt_counter\": %d", get_prompt_count());
        http_post_request(SERVER_LOG_URI, get_prompt_count());

        continue;
      }
      else if (msg.cmd == PERIPH_BUTTON_RELEASE || msg.cmd == PERIPH_BUTTON_LONG_RELEASE)
      {
        /**
         * Audio play flow:
         * [http_server] ))) (2.4 GHz Wi-Fi) ))) http_stream --> mp3_decoder --> i2s_stream --> codec_chip --> [speaker]
         */

        /* Pause record pipelines */
        audio_pipeline_stop(record_pipeline);
        audio_pipeline_wait_for_stop(record_pipeline);
        audio_pipeline_reset_ringbuffer(record_pipeline);
        audio_pipeline_reset_elements(record_pipeline);

        ESP_LOGI(TAG, "Now playing server response");
        i2s_stream_set_clk(i2s_stream_writer, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
        audio_pipeline_run(play_pipeline);
      }
    }

    if ((int) msg.data == get_input_rec_id())
    {
      if (msg.cmd == PERIPH_BUTTON_PRESSED)
      {
        /**
         * Audio record flow:
         * [microphone] --> codec_chip --> i2s_stream --> http_stream ))) (2.4 GHz Wi-Fi) ))) [http_server]
         */

        /* Pause play pipeline */
        audio_pipeline_stop(play_pipeline);
        audio_pipeline_wait_for_stop(play_pipeline);
        audio_pipeline_reset_ringbuffer(play_pipeline);
        audio_pipeline_reset_elements(play_pipeline);

        /* Log data */
        ESP_ERROR_CHECK(save_run_time());
        ESP_ERROR_CHECK(save_prompt_count());
        ESP_ERROR_CHECK(print_what_saved());

        ESP_LOGE(TAG, "Now recording, release [Rec] to STOP");
        i2s_stream_set_clk(i2s_stream_reader, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
        audio_pipeline_run(record_pipeline);
      }
      else if (msg.cmd == PERIPH_BUTTON_RELEASE || msg.cmd == PERIPH_BUTTON_LONG_RELEASE)
      {
        /**
         * Audio play flow:
         * [http_server] ))) (2.4 GHz Wi-Fi) ))) http_stream --> mp3_decoder --> i2s_stream --> codec_chip --> [speaker]
         */

        /* Pause recorder pipeline */
        audio_pipeline_stop(record_pipeline);
        audio_pipeline_wait_for_stop(record_pipeline);
        audio_pipeline_reset_ringbuffer(record_pipeline);
        audio_pipeline_reset_elements(record_pipeline);

        ESP_LOGI(TAG, "Now playing server response");
        i2s_stream_set_clk(i2s_stream_writer, AUDIO_SAMPLE_RATE, AUDIO_BITS, AUDIO_CHANNELS);
        audio_pipeline_run(play_pipeline);
      }
    }
  }


  ESP_LOGI(TAG, "[ EXIT ] stopping pipelines, launching cleanup...");
  audio_pipeline_stop(record_pipeline);
  audio_pipeline_wait_for_stop(record_pipeline);
  audio_pipeline_terminate(record_pipeline);

  audio_pipeline_unregister(record_pipeline, i2s_stream_reader);
  audio_pipeline_unregister(record_pipeline, http_stream_writer);

  audio_pipeline_stop(play_pipeline);
  audio_pipeline_wait_for_stop(play_pipeline);
  audio_pipeline_terminate(play_pipeline);

  audio_pipeline_unregister(play_pipeline, i2s_stream_writer);
  audio_pipeline_unregister(play_pipeline, http_stream_reader);
  audio_pipeline_unregister(play_pipeline, mp3_decoder);

  ESP_LOGI(TAG, "[ EXIT ] Terminate the pipeline before removing the listener");
  audio_pipeline_remove_listener(record_pipeline);
  audio_pipeline_remove_listener(play_pipeline);

  ESP_LOGI(TAG, "[ EXIT ] Stop all peripherals before removing the listener");
  esp_periph_set_stop_all(periph_set);
  audio_event_iface_remove_listener(esp_periph_set_get_event_iface(periph_set), event);

  /**
   * note: Make sure audio_pipeline_remove_listener &
   * audio_event_iface_remove_listener are called before removing event_iface
  */
  audio_event_iface_destroy(event);

  ESP_LOGI(TAG, "[ EXIT ] Releasing all resources");
  audio_pipeline_deinit(record_pipeline);
  audio_pipeline_deinit(play_pipeline);

  audio_element_deinit(i2s_stream_writer);
  audio_element_deinit(http_stream_reader);
  audio_element_deinit(mp3_decoder);

  audio_element_deinit(i2s_stream_reader);
  audio_element_deinit(http_stream_writer);
}



void app_main(void)
{
  esp_log_level_set("*", ESP_LOG_WARN);
  esp_log_level_set(TAG, ESP_LOG_INFO);

  /* Wipe/Setup non-volatile storage */
  ESP_ERROR_CHECK(configure_flash());

  /* Initialize peripherals management */
  esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
  periph_set= esp_periph_set_init(&periph_cfg);

  /* Setup audio codec */
  board_handle = audio_board_init();
  ESP_ERROR_CHECK(audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START));

  /* Initialize Button peripheral */
  ESP_ERROR_CHECK(audio_board_key_init(periph_set));

  /* Launch app tasks...*/
  connect_to_wifi();
  run_voice_assistant_task();

  /* Board cleanup */
  ESP_ERROR_CHECK(esp_periph_set_destroy(periph_set));
}
