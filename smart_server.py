import os, datetime, sys
import wave
import argparse
import socket

import requests # use for OpenWeather API
import json # use to parse through city.list.json

from urllib import parse
from http.server import HTTPServer
from http.server import BaseHTTPRequestHandler

from openai import OpenAI

# todo: include city.json for searching, less API calls

PORT = 8000
MAX_PROMPT_TOKENS = 100

CHIME_FILE = 'chime.mp3'
SPEECH_RESPONSE_FILE = 'speech_response.mp3'
TEXT_RESPONSE_FILE   = 'text_response.txt'

selected_file = SPEECH_RESPONSE_FILE

OPENWEATHER_API_KEY = os.environ.get('OPENWEATHER_API_KEY')

client = OpenAI()

class Handler(BaseHTTPRequestHandler):
    def _set_headers(self, length):
        self.send_response(200)
        if length > 0:
            self.send_header('Content-length', str(length))
        self.end_headers()

    def _get_chunk_size(self):
        data = self.rfile.read(2)
        while data[-2:] != b"\r\n":
            data += self.rfile.read(1)
        return int(data[:-2], 16)

    def _get_chunk_data(self, chunk_size):
        data = self.rfile.read(chunk_size)
        self.rfile.read(2)
        return data

    def _write_wav(self, data, rates, bits, ch):
        t = datetime.datetime.utcnow()
        time = t.strftime('%Y%m%dT%H%M%SZ')
        filename = str.format('{}_{}_{}_{}.wav', time, rates, bits, ch)

        wavfile = wave.open(filename, 'wb')
        wavfile.setparams((ch, int(bits/8), rates, 0, 'NONE', 'NONE'))
        wavfile.writeframesraw(bytearray(data))
        wavfile.close()
        return filename

    def _copy_mp3(self, source_file, destination_file):
        try:
            # Open the source MP3 file for reading in binary mode
            with open(source_file, 'rb') as file:
                # Read the contents of the source file
                mp3_data = file.read()

            # Open the destination MP3 file for writing in binary mode
            with open(destination_file, 'wb') as file:
                # Write the contents of the source file to the destination file
                file.write(mp3_data)

            print(f"Successfully copied {source_file} to {destination_file}")

        except Exception as e:
            print(f"An error occurred: {e}")

    def do_POST(self):
        urlparts = parse.urlparse(self.path)
        request_file_path = urlparts.path.strip('/')
        total_bytes = 0
        sample_rates = 0
        bits = 0
        channel = 0
        print("Do Post......")

        if (request_file_path == 'upload'
            and self.headers.get('Transfer-Encoding', '').lower() == 'chunked'):
            data = []
            sample_rates = self.headers.get('x-audio-sample-rates', '').lower()
            bits = self.headers.get('x-audio-bits', '').lower()
            channel = self.headers.get('x-audio-channel', '').lower()
            sample_rates = self.headers.get('x-audio-sample-rates', '').lower()

            print("Audio information, sample rates: {}, bits: {}, channel(s): {}".format(sample_rates, bits, channel))
            # https://stackoverflow.com/questions/24500752/how-can-i-read-exactly-one-response-chunk-with-pythons-http-client
            while True:
                chunk_size = self._get_chunk_size()
                total_bytes += chunk_size
                print("Total bytes received: {}".format(total_bytes))
                sys.stdout.write("\033[F")
                if (chunk_size == 0):
                    break
                else:
                    chunk_data = self._get_chunk_data(chunk_size)
                    data += chunk_data

            # note: store our byte data to .wav file
            speech_prompt = self._write_wav(data, int(sample_rates), int(bits), int(channel))

            # note: speech-to-text prompt transcription
            text_prompt = client.audio.transcriptions.create(
                model="whisper-1",
                file=open(speech_prompt, "rb"),
                response_format="text"
            )

            # note: parse through the user's prompt for key words like 'weather' or 'music'
            if 'weather' in text_prompt:
                print("requesting weather information...")

                # note: parse out city name from text prompt
                city_name = client.chat.completions.create(
                    model="gpt-3.5-turbo",
                    messages=[
                        {"role": "system", "content": "Only return the city name embedded within text responses"},
                        {"role": "user", "content": text_prompt}
                    ],
                    max_tokens=MAX_PROMPT_TOKENS
                ).choices[0].message.content

                country_code = 'us'

                url = f'https://api.openweathermap.org/data/2.5/weather?q={city_name},{country_code}&appid={OPENWEATHER_API_KEY}&units=imperial'

                weather_data = f"{requests.get(url).json()}"

                content = f'''
                Your job is to summarize the following 'weather' section of the json file into natural English.
                Make sure imperial units are spelled out in english. Keep it within {MAX_PROMPT_TOKENS} tokens.
                '''

                # note: assistant chat text response
                text_response = client.chat.completions.create(
                    model="gpt-3.5-turbo",
                    messages=[
                        {"role": "system", "content": content},
                        {"role": "user", "content": weather_data}
                    ],
                    max_tokens=MAX_PROMPT_TOKENS
                ).choices[0].message.content
            else:
                # note: assistant chat text response
                text_response = client.chat.completions.create(
                    model="gpt-3.5-turbo",
                    messages=[
                        {"role": "system", "content": "You are a helpful assistant."},
                        {"role": "user", "content": text_prompt}
                    ],
                    max_tokens=MAX_PROMPT_TOKENS
                ).choices[0].message.content

            # note: store our latest response in text form
            with open(TEXT_RESPONSE_FILE, "w") as file:
                file.write(text_response)

            # note: text-to-speech response generation
            speech_response = client.audio.speech.create(
                model="tts-1",
                voice="echo",
                input=text_response
            )

            # note: stream and read our response back
            speech_response.stream_to_file(SPEECH_RESPONSE_FILE)

            self.send_response(200)
            self.send_header("Content-type", "text/html;charset=utf-8")
            self.send_header("Content-Length", str(total_bytes))
            self.end_headers()
            body = 'File {} was written, size {}'.format(speech_prompt, total_bytes)
            self.wfile.write(body.encode('utf-8'))
            selected_file = SPEECH_RESPONSE_FILE

        elif (request_file_path == 'log'):
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length).decode('utf-8')
            data = json.loads(post_data)
            counter = data.get('counter')

            print("Received counter:", counter)

            # note: text-to-speech response generation
            speech_response = client.audio.speech.create(
                model="tts-1",
                voice="echo",
                input=f"this device has been prompted {counter} times."
            )

            # note: stream and read our response back
            speech_response.stream_to_file(SPEECH_RESPONSE_FILE)
            selected_file = SPEECH_RESPONSE_FILE

        elif (request_file_path == 'chime'):
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length).decode('utf-8')
            data = json.loads(post_data)
            chime = data.get('counter')

            print("Received chime:", chime)
            
            # note: copy chime.mp3 into speech_response.mp3
            self._copy_mp3("./chime.mp3", "./speech_response.mp3")

    def do_GET(self):
        print("Do GET")

        with open(SPEECH_RESPONSE_FILE, "rb") as file:
            speech_response_data = file.read()

        self.send_response(200)
        self.send_header("Content-type", "audio/mpeg")
        self.send_header("Content-Disposition", f"attachment; filename={SPEECH_RESPONSE_FILE}")
        self.send_header("Content-Length", str(len(speech_response_data)))
        self.end_headers()
        self.wfile.write(speech_response_data)


def get_host_ip():
    # https://www.cnblogs.com/z-x-y/p/9529930.html
    try:
        s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        s.connect(('8.8.8.8',80))
        ip=s.getsockname()[0]
    finally:
        s.close()
    return ip

def main():
    parser = argparse.ArgumentParser(description='HTTP Server save EGR536-VoiceAssistantProject example speech data to wav file')
    parser.add_argument('--ip', '-i', nargs='?', type = str)
    parser.add_argument('--port', '-p', nargs='?', type = int)
    args = parser.parse_args()
    if not args.ip:
        args.ip = get_host_ip()
    if not args.port:
        args.port = PORT

    httpd = HTTPServer((args.ip, args.port), Handler)

    print("Serving HTTP on {} port {}".format(args.ip, args.port))
    httpd.serve_forever()

if __name__ == "__main__":
    main()