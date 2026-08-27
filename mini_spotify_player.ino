#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <mbedtls/md.h>

// =========================================================
// WIFI
// =========================================================
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// =========================================================
// SPOTIFY
// =========================================================
const char* CLIENT_ID = "YOUR_CLIENT_ID";
const char* REDIRECT_URI = "http://127.0.0.1:8888/callback";
const char* SPOTIFY_AUTHORIZE = "https://accounts.spotify.com/authorize";
const char* SPOTIFY_TOKEN = "https://accounts.spotify.com/api/token";
const char* SPOTIFY_API = "https://api.spotify.com/v1";

// =========================================================
// OLED & CUSTOM RETRO LOGO
// =========================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 21
#define OLED_SCL 22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Chunky, pixelated 8-Bit style Spotify Logo (12x12)
const unsigned char spotify_logo_12x12[] PROGMEM = {
  0x3f, 0xc0, // 001111111100
  0x7f, 0xe0, // 011111111110
  0xc0, 0x30, // 110000000011
  0xdf, 0xb0, // 110111111011
  0xdf, 0xb0, // 110111111011
  0xc0, 0x30, // 110000000011
  0xcf, 0x30, // 110011110011
  0xcf, 0x30, // 110011110011
  0xc0, 0x30, // 110000000011
  0x7f, 0xe0, // 011111111110
  0x3f, 0xc0, // 001111111100
  0x00, 0x00  // 000000000000
};

// =========================================================
// BUTTON PINS
// =========================================================
#define BTN_PREV      25
#define BTN_PLAY      26
#define BTN_NEXT      27
#define BTN_VOL_DOWN  32
#define BTN_MODE      33
#define BTN_VOL_UP    14

// =========================================================
// SERVER / STORAGE
// =========================================================
WebServer server(80);
Preferences prefs;
WiFiClientSecure apiSecureClient;

// =========================================================
// SPOTIFY VARIABLES
// =========================================================
String accessToken = "";
String refreshToken = "";
String codeVerifier = "";

String currentTrack = "";
String currentArtist = "";
String currentTrackId = "";
String deviceName = "";

bool isPlaying = false;
int volume = 50;

unsigned long trackProgress = 0;
unsigned long trackDuration = 0;
unsigned long lastSpotifyUpdate = 0;

// =========================================================
// SCROLLING TEXT (MARQUEE) VARIABLES
// =========================================================
int trackScrollX = 0;
int artistScrollX = 0;

String lastTrackForScroll = "";
String lastArtistForScroll = "";

unsigned long lastScrollUpdate = 0;
unsigned long lastDisplayRefresh = 0;

const int CHAR_WIDTH = 6;
const int SCROLL_GAP = 24; // Spacing between the end of the text and the loop restarting

// =========================================================
// STATUS MESSAGE OVERLAY
// =========================================================
String statusMessage = "";
unsigned long statusMessageExpire = 0;

// =========================================================
// BUTTON VARIABLES
// =========================================================
bool lastPrev = HIGH, lastPlay = HIGH, lastNext = HIGH;
bool lastVolDown = HIGH, lastMode = HIGH, lastVolUp = HIGH;

unsigned long lastButtonTime = 0;
const unsigned long BUTTON_DEBOUNCE_MS = 120;

// =========================================================
// URL ENCODE
// =========================================================
String urlEncode(const String& input) {
    String output = "";
    const char hex[] = "0123456789ABCDEF";
    for (unsigned int i = 0; i < input.length(); i++) {
        unsigned char c = input[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            output += char(c);
        } else {
            output += '%';
            output += hex[(c >> 4) & 0x0F];
            output += hex[c & 0x0F];
        }
    }
    return output;
}

// =========================================================
// RANDOM STRING
// =========================================================
String randomString(int length) {
    const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    String result = "";
    for (int i = 0; i < length; i++) {
        result += chars[random(sizeof(chars) - 1)];
    }
    return result;
}

// =========================================================
// SHA256 / PKCE
// =========================================================
String createCodeChallenge(String verifier) {
    unsigned char hash[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_setup(&ctx, info, 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*)verifier.c_str(), verifier.length());
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String base64 = "";
    for (int i = 0; i < 32; i += 3) {
        uint32_t n = ((uint32_t)hash[i]) << 16;
        if (i + 1 < 32) n |= ((uint32_t)hash[i + 1]) << 8;
        if (i + 2 < 32) n |= hash[i + 2];
        base64 += table[(n >> 18) & 63];
        base64 += table[(n >> 12) & 63];
        if (i + 1 < 32) base64 += table[(n >> 6) & 63];
        if (i + 2 < 32) base64 += table[n & 63];
    }
    base64.replace("+", "-");
    base64.replace("/", "_");
    base64.replace("=", "");
    return base64;
}

// =========================================================
// CREATE SPOTIFY LOGIN URL
// =========================================================
String createLoginURL() {
    codeVerifier = randomString(64);
    prefs.putString("verifier", codeVerifier);
    String challenge = createCodeChallenge(codeVerifier);

    String scope =
        "user-read-playback-state "
        "user-read-currently-playing "
        "user-modify-playback-state "
        "user-library-modify";

    String url = String(SPOTIFY_AUTHORIZE);
    url += "?client_id=" + urlEncode(CLIENT_ID);
    url += "&response_type=code";
    url += "&redirect_uri=" + urlEncode(REDIRECT_URI);
    url += "&scope=" + urlEncode(scope);
    url += "&code_challenge_method=S256";
    url += "&code_challenge=" + urlEncode(challenge);
    return url;
}

// =========================================================
// LOGIN PAGE
// =========================================================
void handleLogin() {
    String url = createLoginURL();
    Serial.println("\n================================");
    Serial.println("SPOTIFY LOGIN");
    Serial.println("================================");
    Serial.println(url);
    Serial.println();

    String html =
        "<html><head><meta name='viewport' content='width=device-width'></head>"
        "<body><h2>ESP32 8-BIT Spotify Player</h2><p>Connect your Spotify account:</p>"
        "<a href='" + url + "'><button style='font-size:24px'>Connect Spotify</button></a></body></html>";
    server.send(200, "text/html", html);
}

// =========================================================
// SPOTIFY CALLBACK
// =========================================================
void handleCallback() {
    if (!server.hasArg("code")) {
        server.send(400, "text/plain", "Missing authorization code.");
        return;
    }
    String code = server.arg("code");
    Serial.println("\nSpotify authorization code received.");

    if (exchangeAuthorizationCode(code)) {
        server.send(200, "text/html", "<h2>Spotify connected!</h2><p>You can close this window.</p>");
        Serial.println("Spotify authentication SUCCESS.");
        updateSpotify();
        drawDisplay();
    } else {
        server.send(500, "text/plain", "Spotify token exchange failed.");
    }
}

// =========================================================
// EXCHANGE AUTHORIZATION CODE
// =========================================================
bool exchangeAuthorizationCode(String code) {
    String verifier = prefs.getString("verifier", "");
    if (verifier == "") {
        Serial.println("ERROR: PKCE verifier missing.");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;

    if (!https.begin(client, SPOTIFY_TOKEN)) {
        Serial.println("ERROR: Spotify HTTPS connection failed.");
        return false;
    }
    https.setTimeout(6000);

    String body = "client_id=" + urlEncode(CLIENT_ID) +
                  "&grant_type=authorization_code&code=" + urlEncode(code) +
                  "&redirect_uri=" + urlEncode(REDIRECT_URI) +
                  "&code_verifier=" + urlEncode(verifier);

    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    https.addHeader("Content-Length", String(body.length()));

    int status = https.POST(body);
    String response = https.getString();

    Serial.print("Token HTTP status: ");
    Serial.println(status);

    if (status != 200) {
        Serial.println(response);
        https.end();
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
        Serial.println("ERROR: Invalid token response.");
        https.end();
        return false;
    }

    accessToken = doc["access_token"].as<String>();
    refreshToken = doc["refresh_token"].as<String>();
    prefs.putString("access", accessToken);
    prefs.putString("refresh", refreshToken);

    https.end();
    return true;
}

// =========================================================
// REFRESH TOKEN
// =========================================================
bool refreshAccessToken() {
    if (refreshToken == "") return false;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;

    if (!https.begin(client, SPOTIFY_TOKEN)) return false;
    https.setTimeout(6000);

    String body = "grant_type=refresh_token&refresh_token=" + urlEncode(refreshToken) +
                  "&client_id=" + urlEncode(CLIENT_ID);

    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    https.addHeader("Content-Length", String(body.length()));

    int status = https.POST(body);
    String response = https.getString();

    Serial.print("Refresh HTTP status: ");
    Serial.println(status);

    if (status != 200) {
        Serial.println(response);
        https.end();
        return false;
    }

    JsonDocument doc;
    deserializeJson(doc, response);

    accessToken = doc["access_token"].as<String>();
    if (!doc["refresh_token"].isNull()) {
        refreshToken = doc["refresh_token"].as<String>();
        prefs.putString("refresh", refreshToken);
    }
    prefs.putString("access", accessToken);

    https.end();
    return true;
}

// =========================================================
// SPOTIFY REQUEST
// =========================================================
int spotifyRequest(const char* method, const char* endpoint, String body = "") {
    if (accessToken == "") {
        Serial.println("ERROR: No Spotify access token.");
        return -1;
    }

    HTTPClient https;
    String url = String(SPOTIFY_API) + endpoint;

    if (!https.begin(apiSecureClient, url)) {
        Serial.println("ERROR: HTTPS connection failed.");
        return -1;
    }

    https.setReuse(true);
    https.setTimeout(4000);
    https.setConnectTimeout(4000);
    https.addHeader("Authorization", "Bearer " + accessToken);

    if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0) {
        https.addHeader("Content-Type", "application/json");
        https.addHeader("Content-Length", String(body.length()));
    }

    int status = -1;
    if (strcmp(method, "GET") == 0) status = https.GET();
    else if (strcmp(method, "POST") == 0) status = https.POST(body);
    else if (strcmp(method, "PUT") == 0) status = https.PUT(body);

    Serial.print("Spotify ");
    Serial.print(method);
    Serial.print(" ");
    Serial.print(endpoint);
    Serial.print(" -> HTTP ");
    Serial.println(status);

    if (status == 401) {
        https.end();
        Serial.println("Access token expired. Refreshing...");
        if (refreshAccessToken()) {
            return spotifyRequest(method, endpoint, body);
        }
        return 401;
    }

    if (status >= 400 || status < 0) {
        String error = https.getString();
        Serial.println("Spotify error:");
        Serial.println(error);
    }

    if (strcmp(method, "GET") == 0 && status == 200) {
        String payload = https.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            if (doc["item"].is<JsonObject>()) {
                currentTrack = doc["item"]["name"].as<String>();
                currentTrack.toUpperCase(); // Force retro uppercase
                
                currentArtist = doc["item"]["artists"][0]["name"].as<String>();
                currentArtist.toUpperCase(); // Force retro uppercase
                
                currentTrackId = doc["item"]["id"].as<String>();
                trackProgress = doc["progress_ms"].as<unsigned long>();
                trackDuration = doc["item"]["duration_ms"].as<unsigned long>();
            } else {
                currentTrack = "";
                currentArtist = "";
                currentTrackId = "";
                trackProgress = 0;
                trackDuration = 0;
            }

            isPlaying = doc["is_playing"].as<bool>();

            if (!doc["device"].isNull()) {
                volume = doc["device"]["volume_percent"].as<int>();
                deviceName = doc["device"]["name"].as<String>();
            }

            if (currentTrack != lastTrackForScroll) {
                trackScrollX = 0;
                lastTrackForScroll = currentTrack;
            }
            if (currentArtist != lastArtistForScroll) {
                artistScrollX = 0;
                lastArtistForScroll = currentArtist;
            }
        }
    }

    https.end();
    return status;
}

// =========================================================
// STATUS MESSAGE OVERLAY
// =========================================================
void showStatusMessage(const String& msg, unsigned long durationMs) {
    statusMessage = msg;
    statusMessageExpire = millis() + durationMs;
}

// =========================================================
// PLAY / PAUSE
// =========================================================
void togglePlay() {
    Serial.println("\n========== PLAY / PAUSE ==========");
    isPlaying = !isPlaying;
    drawDisplay();

    int status;
    if (isPlaying) {
        status = spotifyRequest("PUT", "/me/player/play");
    } else {
        status = spotifyRequest("PUT", "/me/player/pause");
    }

    Serial.print("Playback result: HTTP ");
    Serial.println(status);

    updateSpotify();
    drawDisplay();
}

// =========================================================
// NEXT TRACK
// =========================================================
void nextTrack() {
    Serial.println("\n========== NEXT ==========");
    showStatusMessage("> NEXT", 600);
    drawDisplay();

    int status = spotifyRequest("POST", "/me/player/next");
    Serial.print("Next result: HTTP ");
    Serial.println(status);

    updateSpotify();
    statusMessage = "";
    drawDisplay();
}

// =========================================================
// PREVIOUS TRACK
// =========================================================
void previousTrack() {
    Serial.println("\n========== PREVIOUS ==========");
    showStatusMessage("< PREV", 600);
    drawDisplay();

    int status = spotifyRequest("POST", "/me/player/previous");
    Serial.print("Previous result: HTTP ");
    Serial.println(status);

    updateSpotify();
    statusMessage = "";
    drawDisplay();
}

// =========================================================
// VOLUME
// =========================================================
void setVolume(int newVolume) {
    newVolume = constrain(newVolume, 0, 100);
    int previousVolume = volume;
    volume = newVolume;
    drawDisplay();

    Serial.print("Setting volume: ");
    Serial.println(newVolume);

    String endpoint = "/me/player/volume?volume_percent=" + String(newVolume);
    int status = spotifyRequest("PUT", endpoint.c_str());

    Serial.print("Volume result: HTTP ");
    Serial.println(status);

    if (status != 204) {
        volume = previousVolume;
        drawDisplay();
    }
}

// =========================================================
// SAVE CURRENT TRACK TO LIKED SONGS
// =========================================================
void saveCurrentTrackToLiked() {
    Serial.println("\n========== SAVE TO LIKED SONGS ==========");

    if (currentTrackId == "") {
        Serial.println("No track currently playing - nothing to save.");
        showStatusMessage("NO TRACK", 1200);
        drawDisplay();
        return;
    }

    showStatusMessage("SAVING..", 1000);
    drawDisplay();

    String endpoint = "/me/library?uris=spotify%3Atrack%3A" + currentTrackId;
    int status = spotifyRequest("PUT", endpoint.c_str());

    Serial.print("Save result: HTTP ");
    Serial.println(status);

    if (status >= 200 && status <= 204) {
        showStatusMessage("SAVED <3", 1400);
    } 
    else if (status == 403) {
        showStatusMessage("NO SCOPE", 2000);
        Serial.println("ERROR 403: You need to re-authenticate to grant the user-library-modify scope!");
        Serial.println("Please visit the /logout URL in your browser, then log in again.");
    } 
    else {
        showStatusMessage("ERR FAIL", 1400);
    }

    drawDisplay();
}

// =========================================================
// UPDATE SPOTIFY STATE
// =========================================================
void updateSpotify() {
    spotifyRequest("GET", "/me/player");
}

// =========================================================
// CONTINUOUS MARQUEE SCROLL UPDATE
// =========================================================
void updateScrollOffsets() {
    unsigned long now = millis();
    // 25ms interval = ~40 Frames Per Second for smooth sliding
    if (now - lastScrollUpdate < 25) return;
    lastScrollUpdate = now;

    int trackWidth = currentTrack.length() * CHAR_WIDTH;
    if (trackWidth > SCREEN_WIDTH) {
        trackScrollX -= 1;
        // Reset seamlessly when the first text instance goes off screen
        if (trackScrollX <= -(trackWidth + SCROLL_GAP)) {
            trackScrollX = 0;
        }
    } else {
        trackScrollX = 0;
    }

    int artistWidth = currentArtist.length() * CHAR_WIDTH;
    if (artistWidth > SCREEN_WIDTH) {
        artistScrollX -= 1;
        // Reset seamlessly when the first artist instance goes off screen
        if (artistScrollX <= -(artistWidth + SCROLL_GAP)) {
            artistScrollX = 0;
        }
    } else {
        artistScrollX = 0;
    }
}

// =========================================================
// DRAW A CONTINUOUS SCROLLING LINE OF TEXT
// =========================================================
void drawScrollingLine(const String& text, int y, int scrollX) {
    int textWidth = text.length() * CHAR_WIDTH;
    
    if (textWidth <= SCREEN_WIDTH) {
        // Fits perfectly, no need to scroll
        display.setCursor(0, y);
        display.print(text);
        return;
    }

    // Draw the text twice to create a seamless continuous loop
    display.setCursor(scrollX, y);
    display.print(text);
    
    display.setCursor(scrollX + textWidth + SCROLL_GAP, y);
    display.print(text);
}

// =========================================================
// RETRO SEGMENTED PROGRESS BAR
// =========================================================
void drawProgressBar() {
    display.drawRect(0, 36, 128, 8, SSD1306_WHITE);
    
    if (trackDuration > 0) {
        int maxBlocks = 24; 
        int filledBlocks = (long)maxBlocks * trackProgress / trackDuration;
        
        for(int i = 0; i < filledBlocks; i++) {
            display.fillRect(2 + (i * 5), 38, 4, 4, SSD1306_WHITE); 
        }
    }
}

// =========================================================
// 8-BIT OLED DISPLAY RENDERING
// =========================================================
void drawDisplay() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextWrap(false);

    // Expire status message
    if (statusMessage != "" && millis() >= statusMessageExpire) {
        statusMessage = "";
    }

    // Top Header (Notification OR Retro Logo + Title)
    if (statusMessage != "") {
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        display.setCursor(0, 2);
        display.print(" " + statusMessage + " ");
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK); 
    } else {
        display.drawBitmap(0, 0, spotify_logo_12x12, 12, 12, SSD1306_WHITE);
        display.setCursor(14, 2); 
        display.print("SPOTIFY");
    }

    // Bracketed WiFi status
    display.setCursor(94, 2);
    if (WiFi.status() == WL_CONNECTED) {
        display.print("WIFI");
    } else {
        display.print("NO-W");
    }
    
    // Header Divider Line
    display.drawLine(0, 13, 128, 13, SSD1306_WHITE);

    // Track
    if (currentTrack == "") {
        display.setCursor(0, 17);
        display.print("NO TAPE LOADED");
    } else {
        drawScrollingLine(currentTrack, 17, trackScrollX);
    }

    // Artist
    drawScrollingLine(currentArtist, 27, artistScrollX);

    // Progress Bar
    drawProgressBar();
    
    // Footer Divider Line
    display.drawLine(0, 47, 128, 47, SSD1306_WHITE);

    // Current time
    unsigned long sec = trackProgress / 1000;
    display.setCursor(0, 52);
    display.printf("%02lu:%02lu", sec / 60, sec % 60);

    // Total time
    unsigned long total = trackDuration / 1000;
    display.setCursor(98, 52);
    display.printf("%02lu:%02lu", total / 60, total % 60);

    // Bracketed Play/Pause symbol
    display.setCursor(53, 52);
    if (isPlaying) {
        display.print("||");
    } else {
        display.print("|>");
    }

    display.display();
}

// =========================================================
// BUTTON HANDLING + SERIAL OUTPUT
// =========================================================
void checkButtons() {
    bool prev = digitalRead(BTN_PREV);
    bool play = digitalRead(BTN_PLAY);
    bool next = digitalRead(BTN_NEXT);
    bool volDown = digitalRead(BTN_VOL_DOWN);
    bool mode = digitalRead(BTN_MODE);
    bool volUp = digitalRead(BTN_VOL_UP);

    if (millis() - lastButtonTime > BUTTON_DEBOUNCE_MS) {
        if (prev == LOW && lastPrev == HIGH) {
            lastButtonTime = millis();
            previousTrack();
        } else if (play == LOW && lastPlay == HIGH) {
            lastButtonTime = millis();
            togglePlay();
        } else if (next == LOW && lastNext == HIGH) {
            lastButtonTime = millis();
            nextTrack();
        } else if (volDown == LOW && lastVolDown == HIGH) {
            lastButtonTime = millis();
            setVolume(volume - 5);
        } else if (mode == LOW && lastMode == HIGH) {
            lastButtonTime = millis();
            saveCurrentTrackToLiked();
        } else if (volUp == LOW && lastVolUp == HIGH) {
            lastButtonTime = millis();
            setVolume(volume + 5);
        }
    }

    lastPrev = prev;
    lastPlay = play;
    lastNext = next;
    lastVolDown = volDown;
    lastMode = mode;
    lastVolUp = volUp;
}

// =========================================================
// WIFI
// =========================================================
void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("Connecting WiFi...");
    display.display();

    Serial.print("Connecting WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nIP address: ");
    Serial.println(WiFi.localIP());
}

// =========================================================
// SETUP
// =========================================================
void setup() {
    Serial.begin(115200);
    randomSeed(micros());

    pinMode(BTN_PREV, INPUT_PULLUP);
    pinMode(BTN_PLAY, INPUT_PULLUP);
    pinMode(BTN_NEXT, INPUT_PULLUP);
    pinMode(BTN_VOL_DOWN, INPUT_PULLUP);
    pinMode(BTN_MODE, INPUT_PULLUP);
    pinMode(BTN_VOL_UP, INPUT_PULLUP);

    Wire.begin(OLED_SDA, OLED_SCL);
    // Overclock I2C for fast & smooth 40 FPS display updates
    Wire.setClock(400000);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED failed");
        while (true) delay(100);
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("8-BIT SPOTIFY INIT.");
    display.display();

    prefs.begin("spotify", false);
    accessToken = prefs.getString("access", "");
    refreshToken = prefs.getString("refresh", "");

    connectWiFi();
    apiSecureClient.setInsecure();

    server.on("/login", HTTP_GET, handleLogin);
    server.on("/callback", HTTP_GET, handleCallback);
    
    server.on("/logout", HTTP_GET, []() {
        prefs.remove("access");
        prefs.remove("refresh");
        accessToken = "";
        refreshToken = "";
        server.send(200, "text/html", "<h2>Logged Out</h2><p>Device memory cleared. Please go back to <a href='/login'>/login</a> to grant the new track saving permissions.</p>");
    });

    server.begin();

    Serial.println("\n================================");
    Serial.println("ESP32 8-BIT SPOTIFY PLAYER");
    Serial.println("================================");
    Serial.print("Login: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/login");
    Serial.print("Logout: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/logout (Use this to refresh permissions)");

    if (accessToken != "") {
        delay(1000);
        updateSpotify();
        drawDisplay();
    }
}

// =========================================================
// LOOP
// =========================================================
void loop() {
    server.handleClient();
    checkButtons();

    if (millis() - lastSpotifyUpdate > 2000) {
        lastSpotifyUpdate = millis();
        updateSpotify();
    }

    if (isPlaying) {
        static unsigned long lastProgress = millis();
        unsigned long now = millis();
        if (now - lastProgress >= 1000) {
            trackProgress += now - lastProgress;
            lastProgress = now;
            if (trackProgress > trackDuration) trackProgress = trackDuration;
        }
    }

    updateScrollOffsets();

    // Redraw the OLED at 40 FPS (25ms) so scrolling is smooth
    if (millis() - lastDisplayRefresh >= 25) {
        lastDisplayRefresh = millis();
        drawDisplay();
    }

    // Lowered delay for higher loop polling speed
    delay(1);
}
