#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PxMatrix.h>
#include <Fonts/TomThumb.h>

// Replace with your network credentials as defined in platformio.ini
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
const char* hostname = "esp32-pxmatrix";

// Data timeout in seconds
#define DATA_TIMEOUT 2

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

enum displayState {
  DISPLAY_STATE_OFFLINE,
  DISPLAY_STATE_ONLINE,
  DISPLAY_STATE_ACTIVE,
  DISPLAY_STATE_TIMEOUT
};

displayState state = DISPLAY_STATE_OFFLINE;
displayState lastState = DISPLAY_STATE_OFFLINE;

int lastPacketTime = 0xFFFFFFFF;

// Symbols

// 'data', 8x8px
// 'data', 8x8px
const unsigned char symbol_data [] PROGMEM = {
	0x00, 0x02, 0x0a, 0x2a, 0xaa, 0xaa, 0x00, 0x00
};
// 'wifi', 8x8px
const unsigned char symbol_wifi [] PROGMEM = {
	0x00, 0x7c, 0x82, 0x38, 0x44, 0x10, 0x00, 0x00
};
// 'id', 8x8px
const unsigned char symbol_id [] PROGMEM = {
	0x00, 0x00, 0x58, 0x54, 0x54, 0x58, 0x00, 0x00
};

int bufferSize = 0;

void IRAM_ATTR display_updater() {
  display.display();
}

void displayInformation() {
  display.setTextWrap(false);
  display.drawBitmap(1, 0, symbol_wifi, 8, 8, WHITE);
  display.setCursor(10, 6);
  display.println(ssid);
  display.setCursor(10, 14);
  display.print(WiFi.localIP().toString());
  display.drawBitmap(1, 16, symbol_id, 8, 8, WHITE);
  display.setCursor(10, 22);
  display.print(WiFi.getHostname());
  display.drawBitmap(1, 24, symbol_data, 8, 8, WHITE);
  display.setCursor(10, 30);
  if (state == DISPLAY_STATE_TIMEOUT) {
    display.print("Timeout");
  } else if (state == DISPLAY_STATE_ONLINE) {
    display.print("-");
  } else if (state == DISPLAY_STATE_ACTIVE) {
    display.print(String(bufferSize)+" B/P");
  }

  display.showBuffer();
}

void setup() {
  Serial.begin(115200); // Debug output
  Serial.println("PxMatrix TPM2.NET Receiver"); // Identify program
  display.begin(16);
  display.flushDisplay();
  display.setTextWrap(true);
  display.fillScreen(BLACK);
  display.setTextColor(WHITE);
  display.setFont(&TomThumb);

  // Connect to Wi-Fi network
  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(200);
  }
  state = DISPLAY_STATE_ONLINE;

  Serial.println(" Connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  display.fillScreen(BLACK);
  displayInformation();

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
    lastPacketTime = millis() / 1000;
  }
  if (state == DISPLAY_STATE_ACTIVE) {
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
  } else if (state == DISPLAY_STATE_TIMEOUT || state == DISPLAY_STATE_ONLINE) {
    if (packetSize > 0) {
      state = DISPLAY_STATE_ACTIVE;
    } else {
      displayInformation();
    }
  }
  if (millis() / 1000 - lastPacketTime > DATA_TIMEOUT && state == DISPLAY_STATE_ACTIVE) {
    state = DISPLAY_STATE_TIMEOUT;
  }
  if (state != lastState) {
    display.fillScreen(BLACK);
    lastState = state;
  }
  udp.flush();
}