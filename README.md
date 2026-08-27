# Mini Spotify Player

An ESP32-based Spotify mini player that displays the currently playing song on an OLED screen and lets you control Spotify playback using physical buttons.

## Features

- Display the currently playing song
- Display the artist name
- Scrolling song title and artist text
- Play and pause
- Previous track
- Next track
- Volume up and down
- Save the current track to Liked Songs
- OLED display
- Wi-Fi connectivity
- Spotify OAuth authentication
- Python callback server
- Serial Monitor button debugging

## Hardware Requirements

- ESP32 development board
- SSD1306 OLED display
- Push buttons
- Breadboard
- Jumper wires
- USB cable
- Computer for programming

## Software Requirements

- Arduino IDE
- ESP32 board support for Arduino IDE
- Python 3
- A Spotify account
- A Spotify Developer account

## Project Structure

```text
mini_spotify_player/
├── README.md
├── .gitignore
├── esp32/
│   └── mini_spotify_player.ino
└── python/
    └── spotify_callback.py
```

## 1. Create a Spotify Developer Application

Open the Spotify Developer Dashboard:

https://developer.spotify.com/dashboard

Sign in with your Spotify account and create a new application.

Example application name:

```text
Mini Spotify Player
```

Example description:

```text
ESP32 Spotify hardware controller
```

## 2. Get Your Spotify Client ID

Open your application in the Spotify Developer Dashboard and copy the Client ID.

The public repository should contain only a placeholder:

```cpp
const char* CLIENT_ID = "YOUR_SPOTIFY_CLIENT_ID";
```

Replace the placeholder with your own Client ID in your local copy of the sketch.

Keep your Spotify Client Secret private.

## 3. Configure the Redirect URI

In your Spotify Developer application settings, add:

```text
http://127.0.0.1:8888/callback
```

The redirect URI must exactly match the URI used by the Python callback server and ESP32 application.

## 4. Configure Wi-Fi

Open:

```text
esp32/mini_spotify_player.ino
```

Find the Wi-Fi configuration and enter your own credentials locally:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

Do not commit your real Wi-Fi password to a public repository.

## 5. Install the Arduino Libraries

Open Arduino IDE and go to:

```text
Sketch
> Include Library
> Manage Libraries
```

Install the libraries required by the sketch.

Common libraries used by this project include:

- Adafruit GFX Library
- Adafruit SSD1306
- ArduinoJson

The ESP32 Wi-Fi and HTTP functionality is provided by the ESP32 Arduino core.

## 6. Select and Upload the ESP32 Board

Connect the ESP32 to your computer using USB.

In Arduino IDE:

```text
Tools
> Board
> ESP32
```

Select the appropriate ESP32 board.

Then select the correct COM port:

```text
Tools
> Port
> COM...
```

Compile and upload the sketch.

## 7. Check the Serial Monitor

Open:

```text
Tools
> Serial Monitor
```

Set the baud rate to:

```text
115200
```

After connecting to Wi-Fi, the ESP32 should display its IP address.

Example:

```text
IP address: YOUR_ESP32_IP_ADDRESS
```

Your IP address will normally be different.

## 8. Configure the Python Callback Server

Open:

```text
python/spotify_callback.py
```

Find:

```python
ESP32_IP = "YOUR_ESP32_IP_ADDRESS"
```

Replace it locally with the IP address displayed by your ESP32.

Do not commit your personal IP address if you want the public repository to remain generic.

## 9. Start the Python Callback Server

Open a terminal in the `python` directory and run:

```bash
python spotify_callback.py
```

You should see:

```text
ESP32 Spotify Callback Server
Listening on http://127.0.0.1:8888/callback
Waiting for Spotify authorization...
```

Keep this terminal running during Spotify authentication.

## 10. Connect Spotify

Make sure the ESP32 and computer are connected to the same local network.

Open:

```text
http://YOUR_ESP32_IP/login
```

Authorize the application with Spotify.

Spotify will redirect the browser to:

```text
http://127.0.0.1:8888/callback
```

The Python callback server receives the authorization code and forwards it to the ESP32.

## 11. Required Spotify Permissions

The project may require these Spotify scopes:

```text
user-read-playback-state
user-modify-playback-state
user-library-modify
```

The exact scopes depend on the functionality enabled in the sketch.

`user-library-modify` is required for saving the currently playing track to Liked Songs.

If the application was previously authorized without a newly required permission, authenticate again.

## 12. Button Controls

The physical buttons can be assigned to functions such as:

| Button | Function |
|---|---|
| Button 1 | Play / Pause |
| Button 2 | Previous Track |
| Button 3 | Next Track |
| Button 4 | Volume Up |
| Button 5 | Volume Down |
| Button 6 | Save to Liked Songs |
| Button 7 | Mode |

The exact GPIO assignments depend on the wiring and the definitions in the ESP32 sketch.

## 13. Button Debugging

The project can report button presses through the Serial Monitor.

Open:

```text
Tools
> Serial Monitor
```

Use:

```text
115200 baud
```

Press a button and check the serial output.

This helps determine whether a problem is caused by button wiring, GPIO configuration, or Spotify communication.

## 14. OLED Display

The OLED can display:

- Spotify status
- Currently playing song
- Artist
- Playback progress
- Wi-Fi status
- Mode or volume information

Long song titles and artist names can be displayed using scrolling text.

## 15. Troubleshooting

### ESP32 does not connect to Wi-Fi

Check:

- Wi-Fi SSID
- Wi-Fi password
- ESP32 power
- Wi-Fi signal
- Serial Monitor baud rate

Use `115200` baud for the Serial Monitor.

### Spotify authorization fails

Make sure the Spotify Developer Dashboard contains exactly:

```text
http://127.0.0.1:8888/callback
```

Also make sure the Python callback server is running before starting authentication.

### Browser shows "connection refused"

Make sure the callback server is running:

```bash
python spotify_callback.py
```

Keep the terminal open while Spotify redirects the browser.

### Python cannot contact the ESP32

Check:

1. The ESP32 is powered on.
2. The ESP32 is connected to Wi-Fi.
3. The computer and ESP32 are on the same network.
4. The IP address in `spotify_callback.py` is correct.
5. The ESP32 callback endpoint is running.

### Buttons do not work

Check:

- GPIO pin assignments
- Button orientation
- Button wiring
- Common ground
- `INPUT_PULLUP` configuration
- Jumper wires

Use the Serial Monitor button test to determine whether the ESP32 detects the button presses.

### Spotify playback controls do not work

Check that:

- Spotify authentication completed successfully.
- A Spotify playback device is active.
- The ESP32 has internet access.
- The required Spotify scopes were granted.
- Your Spotify account supports the requested playback functionality.

## 16. Security

Never commit sensitive information to a public GitHub repository.

Do not publish:

```text
Wi-Fi passwords
Spotify Client Secret
Spotify Access Tokens
Spotify Refresh Tokens
Private API keys
Other authentication credentials
```

Use placeholders in the public repository:

```text
YOUR_WIFI_SSID
YOUR_WIFI_PASSWORD
YOUR_SPOTIFY_CLIENT_ID
YOUR_ESP32_IP_ADDRESS
```

Check all files before making the repository public.

## 17. Development Notes

The ESP32 communicates with Spotify through the Spotify Web API.

The Python callback server acts as a local OAuth callback bridge between Spotify's authorization process and the ESP32.

The project is intended primarily as an educational and personal hardware project.

## 18. License

This project is provided for educational and personal use.

Spotify and the Spotify logo are trademarks of Spotify AB.

This project is not affiliated with, sponsored by, or endorsed by Spotify.
