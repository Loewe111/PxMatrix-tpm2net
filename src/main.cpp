#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PxMatrix.h>

// Replace with your network credentials as defined in platformio.ini
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

// UDP port for TPM2.NET protocol
#define PORT 65506

// LED matrix configuration
#define FRAME_RATE 512

#define MATRIX_WIDTH 64
#define MATRIX_HEIGHT 32

#define P_LAT 22
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_E 15
#define P_OE 16

WiFiUDP udp;
PxMATRIX display(MATRIX_WIDTH, MATRIX_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C, P_D);

hw_timer_t * timer = NULL;

// Colors

const uint16_t BLACK = display.color565(0, 0, 0);
const uint16_t WHITE = display.color565(255, 255, 255);

int bufferSize = 0;

void IRAM_ATTR display_updater() {
  display.display();
}

void setup() {
  Serial.begin(115200); // Debug output
  Serial.println("PxMatrix TPM2.NET Receiver"); // Identify program
  display.begin(16);
  display.flushDisplay();
  display.setTextWrap(true);
  display.fillScreen(BLACK);
  display.setTextColor(WHITE);

  // Connect to Wi-Fi network
  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(200);
  }

  Serial.println(" Connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  display.fillScreen(BLACK);
  display.setCursor(1, 1);
  display.print(WiFi.localIP().toString());
  display.showBuffer();

  // Start UDP server for TPM2.net protocol
  if (!udp.begin(PORT)) {
    Serial.println("Failed to start UDP server");
    while (1) {
      delay(1000);
    }
  }

  Serial.print("UDP server started on port ");
  Serial.println(PORT);

  // Setup timer
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &display_updater, true);
  timerAlarmWrite(timer, 1000000 / FRAME_RATE, true);
  timerAlarmEnable(timer);
}

void updateMatrix(uint8_t* data, int size, int packetNumber, int pixelCount) {
  const int offset = size * (packetNumber - 1) / 3;
  int x, y;
  uint8_t r, g, b;

  for (int i = 0; i < pixelCount; i++) {
    r = data[i * 3 + 6];
    g = data[i * 3 + 7];
    b = data[i * 3 + 8];

    const int index = offset + i; // 3 bytes per pixel (RGB)
    x = index % MATRIX_WIDTH;
    y = index / MATRIX_WIDTH;
    display.drawPixel(x, y, display.color565(r, g, b));
  }
}


void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    uint8_t packetBuffer[packetSize];
    udp.readBytes(packetBuffer, packetSize);

    if (packetBuffer[0] != 0x9C || packetBuffer[1] != 0xDA) {
      return;
    }

    int frameSize = packetBuffer[2] << 8 | packetBuffer[3];
    int packetNumber = packetBuffer[4];
    
    bufferSize = max(frameSize, bufferSize);

    const int pixelCount = frameSize / 3;
    updateMatrix(packetBuffer, bufferSize, packetNumber, pixelCount);
  }
}