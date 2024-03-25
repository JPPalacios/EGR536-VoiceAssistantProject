<!-- # Record and Upload WAV Files to HTTP Server -->
# ESP32 Voice Assistant

<!-- - Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example") -->

## Example Brief

This example demonstrates how to create a simple OpenAI voice assistant on an ESP32 LyraT-Mini development board.

This project creates two pipelines. The recording pipeline captures sound and streams the raw data over Wi-Fi to an HTTP server where it is saved as a `.wav` file. The playback pipeline reads back an `.mp3` file from the server to play on the board speaker.

Follow these steps to make it run:
1. Press the [Rec] key on the audio board to record and upload to the server over Wi-Fi, release the key to stop recording.
2. Press the [Vol+]/[Vol-] key to turn up/down the volume.
3. Press the [Mode] key to end the program.

The recording pipeline looks like this:

```c
microphone --> codec_chip --> i2s_stream --> http_stream ))) (2.4 GHz Wi-Fi) ))) [http_server]
```

The playback pipeline is as follows:

```c
[http_server] ))) (2.4 GHz Wi-Fi) ))) http_stream --> mp3_decoder --> i2s_stream --> codec_chip --> speaker
```

The responses are handled by OpenAI's Python API for voice transcription, chat completion, and text-to-speech audio generation.

## Environment Setup


#### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Build and Flash


### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.


### Configuration

The default board for this example is `ESP32-Lyrat V4.3`, if you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```c
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

Configure the Wi-Fi connection information first. Go to `menuconfig> Example Configuration` and fill in the `Wi-Fi SSID` and `Wi-Fi Password`.

```c
menuconfig > Example Configuration > (myssid) WiFi SSID > (myssid) WiFi Password
```

Then, configure the URI of the HTTP server on the PC. Please make sure that the development board and the HTTP server are in the same Wi-Fi local area network (LAN). For example, if the LAN IP address of the HTTP server is `192.168.5.72`, go to `menuconfig> Example Configuration` and configure the URI as `http://192.168.5.72:8000/upload`.

```c
menuconfig > Example Configuration > (http://192.168.5.72:8000/upload) Server URL to send data
```


### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) for full steps to configure and build an ESP-IDF project.


## How to Use the Example


### Example Functionality

- Fist of all, run a Python script with python 2.7. The development board and HTTP server should be connected to the same Wi-Fi network.
-The script server.py is in the root directory of this example, and make sure that this directory is writable. Then, run `python server.py`, and the log is as follows:

```c
python2 server.py
Serving HTTP on 192.168.5.72 port 8000
```

- After the example starts running, it will connect to Wi-Fi first. After a successful connection, press the [Rec] key to record, and the recorded voice will be uploaded to the directory of the HTTP server. The file is named after the time and date of uploading, the audio sampling rate, and the number of channels, which are separated by underscores, such as `20210918T070420Z_16000_16_2.wav`.

- The log of the development board is as follows:

```c
I (0) cpu_start: App cpu up.
I (526) heap_init: Initializing. RAM available for dynamic allocation:
I (533) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (539) heap_init: At 3FFB9AD8 len 00026528 (153 KiB): DRAM
I (545) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (552) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (558) heap_init: At 40095C68 len 0000A398 (40 KiB): IRAM
I (564) cpu_start: Pro cpu start user code
I (247) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (277) REC_RAW_HTTP: [ 1 ] Initialize Button Peripheral & Connect to wifi network
I (297) wifi:wifi driver task: 3ffc2c98, prio:23, stack:3584, core=0
I (347) wifi:wifi firmware version: 5f8804c
I (347) wifi:config NVS flash: enabled
I (347) wifi:config nano formating: disabled
I (347) wifi:Init dynamic tx buffer num: 32
I (347) wifi:Init data frame dynamic rx buffer num: 32
I (357) wifi:Init management frame dynamic rx buffer num: 32
I (357) wifi:Init management short buffer num: 32
I (367) wifi:Init static rx buffer size: 1600
I (367) wifi:Init static rx buffer num: 10
I (367) wifi:Init dynamic rx buffer num: 32
W (377) phy_init: failed to load RF calibration data (0xffffffff), falling back to full calibration
I (547) phy: phy_version: 4180, cb3948e, Sep 12 2019, 16:39:13, 0, 2
I (567) wifi:mode : sta (94:b9:7e:65:c2:44)
I (1907) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (2887) wifi:state: init -> auth (b0)
I (2897) wifi:state: auth -> assoc (0)
I (2907) wifi:state: assoc -> run (10)
I (2917) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (2917) wifi:security type: 4, phy: bgn, rssi: -32
I (2917) wifi:pm start, type: 1

I (2977) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (4777) REC_RAW_HTTP: [ 2 ] Start codec chip
E (4777) gpio: gpio_install_isr_service(412): GPIO isr service already installed
I (4797) REC_RAW_HTTP: [3.0] Create audio pipeline for recording
I (4797) REC_RAW_HTTP: [3.1] Create http stream to post data to server
I (4807) REC_RAW_HTTP: [3.2] Create i2s stream to read audio data from codec chip
I (4817) REC_RAW_HTTP: [3.3] Register all elements to audio pipeline
I (4817) REC_RAW_HTTP: [3.4] Link it together [codec_chip]-->i2s_stream->http_stream-->[http_server]
W (4847) PERIPH_TOUCH: _touch_init
I (4847) REC_RAW_HTTP: [ 4 ] Press [Rec] button to record, Press [Mode] to exit
I (11447) REC_RAW_HTTP: [ * ] [Rec] input key event, resuming pipeline ...
Total bytes written: 141312
I (14937) REC_RAW_HTTP: [ * ] [Rec] key released, stop pipeline ...
W (14937) AUDIO_ELEMENT: IN-[http] AEL_IO_ABORT
W (14937) HTTP_STREAM: No output due to stopping
I (14937) REC_RAW_HTTP: [ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker
E (14957) TRANS_TCP: tcp_poll_read select error 104, errno = Connection reset by peer, fd = 54
I (14957) REC_RAW_HTTP: [ + ] HTTP client HTTP_STREAM_FINISH_REQUEST
W (14967) AUDIO_PIPELINE: Without stop, st:1
W (14967) AUDIO_PIPELINE: Without wait stop, st:1
```

- The log on the HTTP server is as follows:

```c
python2 server.py
Serving HTTP on 192.168.5.72 port 8000
Audio information, sample rates: 16000, bits: 16, channel(s): 2
Total bytes received: 141312
192.168.5.187 - - [18/Sep/2021 15:04:20] "POST /upload HTTP/1.1" 200 -
```

- Finally, press the [Mode] key to exit the example.

```c
W (197635) REC_RAW_HTTP: [ * ] [Set] input key event, exit the demo ...
I (197635) REC_RAW_HTTP: [ 5 ] Stop audio_pipeline
W (197635) AUDIO_PIPELINE: Without stop, st:1
W (197635) AUDIO_PIPELINE: Without wait stop, st:1
W (197645) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (197655) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (197655) AUDIO_PIPELINE: There are no listener registered
W (197675) AUDIO_PIPELINE: There are no listener registered
W (197675) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (197685) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
I (197695) wifi:state: run -> init (0)
I (197695) wifi:pm stop, total sleep time: 186442255 us / 194856817 us

I (197695) wifi:new:<11,0>, old:<11,0>, ap:<255,255>, sta:<11,0>, prof:1
W (197705) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
I (197715) wifi:flush txq
I (197715) wifi:stop sw txq
I (197725) wifi:lmac stop hw txq
I (197725) wifi:Deinit lldesc rx mblock:10
```


### Example Log

A complete log is as follows:

```c
I (0) cpu_start: App cpu up.
I (526) heap_init: Initializing. RAM available for dynamic allocation:
I (533) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (539) heap_init: At 3FFB9AD8 len 00026528 (153 KiB): DRAM
I (545) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (552) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (558) heap_init: At 40095C68 len 0000A398 (40 KiB): IRAM
I (564) cpu_start: Pro cpu start user code
I (247) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (277) REC_RAW_HTTP: [ 1 ] Initialize Button Peripheral & Connect to wifi network
I (297) wifi:wifi driver task: 3ffc2c98, prio:23, stack:3584, core=0
I (347) wifi:wifi firmware version: 5f8804c
I (347) wifi:config NVS flash: enabled
I (347) wifi:config nano formating: disabled
I (347) wifi:Init dynamic tx buffer num: 32
I (347) wifi:Init data frame dynamic rx buffer num: 32
I (357) wifi:Init management frame dynamic rx buffer num: 32
I (357) wifi:Init management short buffer num: 32
I (367) wifi:Init static rx buffer size: 1600
I (367) wifi:Init static rx buffer num: 10
I (367) wifi:Init dynamic rx buffer num: 32
W (377) phy_init: failed to load RF calibration data (0xffffffff), falling back to full calibration
I (547) phy: phy_version: 4180, cb3948e, Sep 12 2019, 16:39:13, 0, 2
I (567) wifi:mode : sta (94:b9:7e:65:c2:44)
I (1907) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (2887) wifi:state: init -> auth (b0)
I (2897) wifi:state: auth -> assoc (0)
I (2907) wifi:state: assoc -> run (10)
I (2917) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (2917) wifi:security type: 4, phy: bgn, rssi: -32
I (2917) wifi:pm start, type: 1

I (2977) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (4777) REC_RAW_HTTP: [ 2 ] Start codec chip
E (4777) gpio: gpio_install_isr_service(412): GPIO isr service already installed
I (4797) REC_RAW_HTTP: [3.0] Create audio pipeline for recording
I (4797) REC_RAW_HTTP: [3.1] Create http stream to post data to server
I (4807) REC_RAW_HTTP: [3.2] Create i2s stream to read audio data from codec chip
I (4817) REC_RAW_HTTP: [3.3] Register all elements to audio pipeline
I (4817) REC_RAW_HTTP: [3.4] Link it together [codec_chip]-->i2s_stream->http_stream-->[http_server]
W (4847) PERIPH_TOUCH: _touch_init
I (4847) REC_RAW_HTTP: [ 4 ] Press [Rec] button to record, Press [Mode] to exit
I (11447) REC_RAW_HTTP: [ * ] [Rec] input key event, resuming pipeline ...
Total bytes written: 141312
I (14937) REC_RAW_HTTP: [ * ] [Rec] key released, stop pipeline ...
W (14937) AUDIO_ELEMENT: IN-[http] AEL_IO_ABORT
W (14937) HTTP_STREAM: No output due to stopping
I (14937) REC_RAW_HTTP: [ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker
E (14957) TRANS_TCP: tcp_poll_read select error 104, errno = Connection reset by peer, fd = 54
I (14957) REC_RAW_HTTP: [ + ] HTTP client HTTP_STREAM_FINISH_REQUEST
W (14967) AUDIO_PIPELINE: Without stop, st:1
W (14967) AUDIO_PIPELINE: Without wait stop, st:1
W (197635) REC_RAW_HTTP: [ * ] [Set] input key event, exit the demo ...
I (197635) REC_RAW_HTTP: [ 5 ] Stop audio_pipeline
W (197635) AUDIO_PIPELINE: Without stop, st:1
W (197635) AUDIO_PIPELINE: Without wait stop, st:1
W (197645) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (197655) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (197655) AUDIO_PIPELINE: There are no listener registered
W (197675) AUDIO_PIPELINE: There are no listener registered
W (197675) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (197685) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
I (197695) wifi:state: run -> init (0)
I (197695) wifi:pm stop, total sleep time: 186442255 us / 194856817 us

I (197695) wifi:new:<11,0>, old:<11,0>, ap:<255,255>, sta:<11,0>, prof:1
W (197705) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
I (197715) wifi:flush txq
I (197715) wifi:stop sw txq
I (197725) wifi:lmac stop hw txq
I (197725) wifi:Deinit lldesc rx mblock:10
```

The complete log on the HTTP server is as follows:

```c
python2 server.py
Serving HTTP on 192.168.5.72 port 8000
Audio information, sample rates: 16000, bits: 16, channel(s): 2
Total bytes received: 141312
192.168.5.187 - - [18/Sep/2021 15:04:20] "POST /upload HTTP/1.1" 200 -
```


## Troubleshooting

If your development board cannot upload the voice to the HTTP server, please check the following configuration:
1. Whether the Wi-Fi configuration of the development board is correct.
2. Whether the development board has been connected to Wi-Fi and obtained the IP address successfully.
3. Whether the HTTP script URI on the server side is correctly configured.
4. Whether the HTTP script on the server side is running correctly.
5. Whether the development board and HTTP server are in the same Wi-Fi network.


```c
export IDF_PATH=/Users/jppalacios/esp-adf/esp-idf
/Users/jppalacios/.espressif/python_env/idf4.4_py3.11_env/bin/python /Users/jppalacios/esp-adf/esp-idf/tools/idf_monitor.py -p /dev/cu.usbserial-2130 -b 115200 --toolchain-prefix xtensa-esp32-elf- --target esp32 /Users/jppalacios/esp/EGR536-VoiceAssistantProject/build/EGR536-VoiceAssistantProject.elf
(base) jppalacios@macbook EGR536-VoiceAssistantProject % export IDF_PATH=/Users/jppalacios/esp-adf/esp-idf
(base) jppalacios@macbook EGR536-VoiceAssistantProject % /Users/jppalacios/.espressif/python_env/idf4.4_py3.11_env/bin/python /Users/jppalacios/esp-adf/esp-idf/tools/idf_monitor.py -p /dev/cu.usbserial-2130 -b 115200 --toolchain-prefix 
xtensa-esp32-elf- --target esp32 /Users/jppalacios/esp/EGR536-VoiceAssistantProject/build/EGR536-VoiceAssistantProject.elf
--- idf_monitor on /dev/cu.usbserial-2130 115200 ---
--- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
ets Jul 29 2019 12:21:46

rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:6628
load:0x40078000,len:15076
load:0x40080400,len:3816
0x40080400: _init at ??:?

entry 0x40080698
I (27) boot: ESP-IDF v4.4.4-278-g3c8bc2213c-dirty 2nd stage bootloader
I (27) boot: compile time 12:13:14
I (28) boot: chip revision: v3.0
I (32) boot.esp32: SPI Speed      : 80MHz
I (37) boot.esp32: SPI Mode       : DIO
I (41) boot.esp32: SPI Flash Size : 8MB
I (46) boot: Enabling RNG early entropy source...
I (51) boot: Partition Table:
I (55) boot: ## Label            Usage          Type ST Offset   Length
I (62) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (70) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (77) boot:  2 factory          factory app      00 00 00010000 00200000
I (85) boot: End of partition table
I (89) esp_image: segment 0: paddr=00010020 vaddr=3f400020 size=252e4h (152292) map
I (143) esp_image: segment 1: paddr=0003530c vaddr=3ffb0000 size=03018h ( 12312) load
I (148) esp_image: segment 2: paddr=0003832c vaddr=40080000 size=07cech ( 31980) load
I (160) esp_image: segment 3: paddr=00040020 vaddr=400d0020 size=9ee3ch (650812) map
I (357) esp_image: segment 4: paddr=000dee64 vaddr=40087cec size=0d680h ( 54912) load
I (387) boot: Loaded app from partition at offset 0x10000
I (387) boot: Disabling RNG early entropy source...
I (399) cpu_start: Pro cpu up.
I (399) cpu_start: Starting app cpu, entry point is 0x400812d0
0x400812d0: call_start_cpu1 at /Users/jppalacios/esp-adf/esp-idf/components/esp_system/port/cpu_start.c:148

I (0) cpu_start: App cpu up.
I (413) cpu_start: Pro cpu start user code
I (413) cpu_start: cpu freq: 160000000
I (413) cpu_start: Application information:
I (418) cpu_start: Project name:     EGR536-VoiceAssistantProject
I (424) cpu_start: App version:      b72e90a-dirty
I (430) cpu_start: Compile time:     Mar 14 2024 23:24:23
I (436) cpu_start: ELF file SHA256:  9560b91bb5f1a7e0...
I (442) cpu_start: ESP-IDF:          v4.4.4-278-g3c8bc2213c-dirty
I (449) cpu_start: Min chip rev:     v0.0
I (453) cpu_start: Max chip rev:     v3.99 
I (458) cpu_start: Chip rev:         v3.0
I (463) heap_init: Initializing. RAM available for dynamic allocation:
I (470) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (476) heap_init: At 3FFB7378 len 00028C88 (163 KiB): DRAM
I (482) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (489) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (495) heap_init: At 4009536C len 0000AC94 (43 KiB): IRAM
I (502) spi_flash: detected chip: generic
I (506) spi_flash: flash io: dio
I (511) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (560) VOICE_ASSISTANT: [ . ] Start and wait for Wi-Fi network
W (12030) PERIPH_WIFI: Wi-Fi disconnected from SSID iphone, auto-reconnect enabled, reconnect after 1000 ms
W (15430) PERIPH_WIFI: Wi-Fi disconnected from SSID iphone, auto-reconnect enabled, reconnect after 1000 ms
W (16720) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
Prompt counter = 1
Measured current = 20 mA
Run time:
Nothing saved yet!
I (17730) VOICE_ASSISTANT: [ . ] Start codec chip
W (17750) I2C_BUS: I2C bus has been already created, [port:0]
I (17770) VOICE_ASSISTANT: [1.1] Initialize all pipelines
I (17770) VOICE_ASSISTANT: [1.2] Create audio elements for recorder pipeline
I (17770) VOICE_ASSISTANT: [1.3] Register audio elements to recorder pipeline
I (17780) VOICE_ASSISTANT: [2.2] Create audio elements for playback pipeline
E (17780) I2S: register I2S object to platform failed
I (17790) VOICE_ASSISTANT: [2.3] Register audio elements to playback pipeline
I (17800) VOICE_ASSISTANT: [ 3 ] Set up  event listener
W (17800) VOICE_ASSISTANT: Press [Rec] to start recording
E (32560) VOICE_ASSISTANT: Now recording, release [Rec] to STOP
W (32560) AUDIO_PIPELINE: Without stop, st:1
W (32560) AUDIO_PIPELINE: Without wait stop, st:1
W (32560) AUDIO_ELEMENT: [http_reader] Element has not create when AUDIO_ELEMENT_TERMINATE
W (32570) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
W (32580) AUDIO_ELEMENT: [i2s_writer] Element has not create when AUDIO_ELEMENT_TERMINATE
Prompt counter = 2
Measured current = 20 mA
Run time:
Nothing saved yet!
I (32610) VOICE_ASSISTANT: Sending recording to server...
W (32610) AUDIO_THREAD: Make sure selected the `CONFIG_SPIRAM_BOOT_INIT` and `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` by `make menuconfig`
Total bytes written: 98304
I (35620) VOICE_ASSISTANT: START Playback
W (35620) AUDIO_ELEMENT: IN-[http_writer] AEL_IO_ABORT
I (35620) VOICE_ASSISTANT: [ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker
I (40390) VOICE_ASSISTANT: [ + ] HTTP client HTTP_STREAM_FINISH_REQUEST
W (40390) HTTP_CLIENT: esp_transport_read returned:-1 and errno:128 
I (40390) VOICE_ASSISTANT: Got HTTP Response = File 20240315T033321Z_24000_16_1.wav was written, size 98304
W (40400) AUDIO_THREAD: Make sure selected the `CONFIG_SPIRAM_BOOT_INIT` and `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` by `make menuconfig`
W (40420) AUDIO_THREAD: Make sure selected the `CONFIG_SPIRAM_BOOT_INIT` and `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` by `make menuconfig`
W (44080) HTTP_STREAM: No more data,errno:0, total_bytes:42720, rlen = 0

```

## Technical Support and Feedback

<!-- Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible. -->
