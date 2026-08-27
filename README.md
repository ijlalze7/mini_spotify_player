# Mini Spotify Player 

An ESP32-based Spotify mini player with an OLED display and physical buttons.

## Features

- Spotify playback control
- Play / Pause
- Previous track
- Next track
- Volume Up / Down
- Save currently playing track to Liked Songs
- OLED music information display
- Scrolling song title and artist
- Wi-Fi connectivity
- Spotify OAuth authentication
- Python callback server

## Hardware

- ESP32
- OLED display
- Push buttons
- Breadboard
- Jumper wires

## Software

- Arduino IDE
- ESP32 Arduino Core
- Spotify Web API
- Python 3

## Project Structure

```text
mini_spotify_player/
├── esp32/
│   └── spotify-mini-player.ino
├── python/
│   └── spotify_callback.py
├── .gitignore
└── README.md
