from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs, quote
from urllib.request import urlopen


# Enter the IP address of your ESP32 here
ESP32_IP = "YOUR_ESP32_IP_ADDRESS"

PORT = 8888


class CallbackHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        parsed = urlparse(self.path)

        if parsed.path != "/callback":
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b"Not found.")
            return

        params = parse_qs(parsed.query)

        code = params.get("code", [None])[0]

        if not code:
            self.send_response(400)
            self.end_headers()
            self.wfile.write(
                b"Missing Spotify authorization code."
            )
            return

        # Safely encode the authorization code
        encoded_code = quote(
            code,
            safe=""
        )

        # Forward the code to the ESP32
        esp32_url = (
            f"http://{ESP32_IP}"
            f"/callback?code={encoded_code}"
        )

        print(
            "Forwarding Spotify authorization "
            "code to ESP32..."
        )

        try:
            response = urlopen(
                esp32_url,
                timeout=10
            )

            message = response.read()

            self.send_response(
                response.status
            )

            self.send_header(
                "Content-Type",
                "text/html"
            )

            self.end_headers()

            self.wfile.write(message)

            print(
                "Spotify authorization code "
                "forwarded successfully."
            )

        except Exception as e:

            print(
                "Error contacting ESP32:",
                e
            )

            self.send_response(500)

            self.send_header(
                "Content-Type",
                "text/html"
            )

            self.end_headers()

            self.wfile.write(
                b"Could not contact the ESP32."
            )

    def log_message(self, format, *args):
        print(
            "[HTTP]",
            format % args
        )


print(
    "===================================="
)

print(
    "ESP32 Spotify Callback Server"
)

print(
    "===================================="
)

print(
    f"ESP32 IP: {ESP32_IP}"
)

print(
    f"Listening on "
    f"http://127.0.0.1:{PORT}/callback"
)

print(
    "Waiting for Spotify authorization..."
)

print(
    "Press Ctrl+C to stop."
)


server = HTTPServer(
    ("127.0.0.1", PORT),
    CallbackHandler
)


try:
    server.serve_forever()

except KeyboardInterrupt:

    print(
        "\nStopping callback server..."
    )

    server.server_close()
