/*
A lot of this is based on the Uncanny Eyes sketch from Adafruit, including the eye textures used.
I found to use two screens with one esp32 you need to use the SDA and SCL Pin, and declare -1 or an unused pin as the declared CS pin for the TFT object.
Manually set the CS pin to low to render to that screen, but make sure to set the other CS pin to high.
Although three screens are possible the third partially renders one of the other screens no matter what causing it to behave erratically.
Terminator Eye library and the Uncanny Eyes sketch is avialable at https://github.com/adafruit/Uncanny_Eyes/
You should be able to use this along with any of the other eye types if you so desire.

  const char* apSSID = "ESP32-RoboEyes";
  const char* apPassword = "robot";  // optional; use NULL for open network
For some reason the SSID doesn't work though so it is ESP_D84689
*/

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <ESPmDNS.h>
#include "terminatorEye.h"

// === WiFi + OTA Setup ===
const char* ssid = "SonOfPaulSwift";
const char* password = "thankyou";
WebServer server(80);
WiFiUDP Udp;
const int localPort = 8000; // OSC port
int displayTime = 0;
char ipBuf[20];

// === Display Config ===
#define TFT_CS     21
#define TFT_CS2    22
#define TFT_RST    4
#define TFT_DC     2
//SCK 18
//SDA 23

Adafruit_ST7735 tft = Adafruit_ST7735(-1, TFT_DC, TFT_RST);
GFXcanvas16 canvas(128, 128);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128
#define IRIS_WIDTH 80
#define IRIS_HEIGHT 80

// === Eye Struct ===
struct Eye {
  int x = 64, y = 64;
  int goalX = 64, goalY = 64;

  unsigned long lastBlink = 0;
  unsigned long nextBlink = 2000;
  int blinkStage = 0;
  unsigned long blinkTimer = 0;

  int csPin;
  bool glow = false;
  uint8_t glowAlpha = 0;
  uint16_t tintColor = 0xFFFF;
  unsigned long emotionStart = 0;
  unsigned long emotionDuration = 0;
  unsigned long moveStart = 0;
  unsigned long moveDuration = 0;
};

Eye eyeL = {.csPin = TFT_CS};
Eye eyeR = {.csPin = TFT_CS2};

void setup() {
  Serial.begin(115200);

  //KEEP ONLY ONE ACTIVE DEPENDING ON IF YOU WANT TO
  //CONNECT TO A ROUTER OR HOST.
  WiFi.begin(ssid, password);
  //WiFi.softAP(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  IPAddress myIP = WiFi.localIP();
  String ipStr = myIP.toString();
  ipStr.toCharArray(ipBuf, 20);
  drawtext(ipBuf, ST77XX_BLACK);

  if (!MDNS.begin("cabinet2")) {  // <<<<<<<<<<<< UNIQUE PER DEVICE
    Serial.println("Error starting mDNS");
  } else {
    Serial.println("mDNS responder started as cabinet2.local");
  }

  Udp.begin(localPort);
  server.on("/", []() {
    server.send(200, "text/html", "<h1>ESP32 Eye Online</h1><a href=\"/update\">Update Firmware</a>");
  });
  ElegantOTA.begin(&server);  // OTA Updating
  server.begin();

  tft.initR(INITR_144GREENTAB);
  tft.setSPISpeed(24000000);
  tft.fillScreen(ST77XX_BLACK);

  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_CS2, OUTPUT);
}

void loop() {
  server.handleClient();  // Handles OTA updates
  ElegantOTA.loop();
  handleOSC();

  if (displayTime <= 5) {
    unsigned long t = millis() % 1000;
    if (t <= 1000) {
      drawtext(ipBuf, ST77XX_BLACK);
      displayTime++;
    }
  } else {
    updateEye(eyeL);
    updateEye(eyeR);
    drawEye(eyeL);
    drawEye(eyeR);
  }
  delay(30);
}

void drawtext(char *text, uint16_t color) {
  digitalWrite(TFT_CS2, HIGH);
  digitalWrite(TFT_CS, LOW);
  tft.fillScreen(ST77XX_WHITE);
  tft.setCursor(0, 0);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
  digitalWrite(TFT_CS, HIGH);
  delay(5);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TFT_CS2, LOW);
  tft.fillScreen(ST77XX_WHITE);
  tft.setCursor(0, 0);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
  digitalWrite(TFT_CS2, HIGH);
  delay(5);
}

// === Eye Update Logic ===
void updateEye(Eye &eye) {
  unsigned long now = millis();

  //Blink Handling
  if (now - eye.lastBlink > eye.nextBlink && eye.blinkStage == 0) {
    eye.blinkStage = 1;
    eye.blinkTimer = now;
    eye.lastBlink = now;
    eye.nextBlink = random(2000, 7000); //change these numbers to make longer/shorter blinks
  }

 if (abs(eye.x - eye.goalX) < 8 && abs(eye.y - eye.goalY) < 8 && eye.moveStart == 0) {
    eye.goalX = random(52, 76);  // within sclera bounds
    eye.goalY = random(52, 76);
 } else if (millis() - eye.moveStart <= eye.moveDuration) {
    eye.moveStart = 0;
    eye.moveDuration = 0;
 }
  eye.x = lerp(eye.x, eye.goalX, 0.15);
  eye.y = lerp(eye.y, eye.goalY, 0.15);

  if (eye.glow && millis() - eye.emotionStart > eye.emotionDuration) {
    eye.glowAlpha = max(0, eye.glowAlpha - 8);
    if (eye.glowAlpha == 0) {
      eye.glow = false;
    }
  }
}

// === Eye Draw Routine ===
void drawEye(Eye &eye) {
  canvas.fillScreen(ST77XX_BLACK);

  drawSclera(canvas);
  drawIris(canvas, eye.x, eye.y, eye.tintColor, eye.glow ? eye.glowAlpha : 0);
  canvas.fillCircle(eye.x, eye.y, 12, ST77XX_BLACK); // Pupil
  canvas.fillCircle(eye.x - 5, eye.y - 5, 3, ST77XX_WHITE); // Reflection

  uint8_t blink = getBlinkMask(eye);
  if (blink > 0) {
    drawEyelid(canvas, blink);
  }

  maskCorners(canvas);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TFT_CS2, HIGH);
  digitalWrite(eye.csPin, LOW);
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 128, 128);
  digitalWrite(eye.csPin, HIGH);
}

// === Blink Logic ===
uint8_t getBlinkMask(Eye &eye) {
  unsigned long now = millis();
  uint8_t blink = 0;

  if (eye.blinkStage == 0) return 0;

  if (eye.blinkStage == 1)
    blink = map(now - eye.blinkTimer, 0, 100, 0, 255);
  else if (eye.blinkStage == 2)
    blink = 255;
  else if (eye.blinkStage == 3)
    blink = map(now - eye.blinkTimer, 0, 100, 255, 0);

  if ((eye.blinkStage == 1 || eye.blinkStage == 3) && now - eye.blinkTimer > 100) {
    eye.blinkStage++;
    eye.blinkTimer = now;
  } else if (eye.blinkStage == 2 && now - eye.blinkTimer > 100) {
    eye.blinkStage = 3;
    eye.blinkTimer = now;
  } else if (eye.blinkStage == 4) {
    eye.blinkStage = 0;
  }

  return blink;
}

// === Draw Textured Sclera ===
void drawSclera(GFXcanvas16 &c) {
  int offsetX = (SCLERA_WIDTH - SCREEN_WIDTH) / 2;
  int offsetY = (SCLERA_HEIGHT - SCREEN_HEIGHT) / 2;

  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      c.drawPixel(x, y, sclera[y + offsetY][x + offsetX]);
    }
  }
}

void maskCorners(GFXcanvas16 &c) {
  // Manually black out corners if needed
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      if (x + y < 20) {
        c.drawPixel(x, y, ST77XX_BLACK);                     // top-left
        c.drawPixel(127 - x, y, ST77XX_BLACK);               // top-right
        c.drawPixel(x, 127 - y, ST77XX_BLACK);               // bottom-left
        c.drawPixel(127 - x, 127 - y, ST77XX_BLACK);         // bottom-right
      }
    }
  }
}

// === Draw Iris from Polar Map ===
void drawIris(GFXcanvas16 &c, int cx, int cy, uint16_t tintColor, uint8_t glowAlpha) {
  int radius = IRIS_WIDTH / 2;

  for (int y = 0; y < IRIS_HEIGHT; y++) {
    for (int x = 0; x < IRIS_WIDTH; x++) {
      int px = cx - radius + x;
      int py = cy - radius + y;
      if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
        uint16_t color = polar[y][x];
        if (color != 0x007F) {
          if (glowAlpha > 0) {
            color = blendColor(color, tintColor, glowAlpha);
          }
          c.drawPixel(px, py, color);
        }
      }
    }
  }
}

// === Apply Eyelid Masks ===
void drawEyelid(GFXcanvas16 &c, uint8_t stage) {
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      if (upper[y][x] > stage || lower[y][x] > stage) {
        c.drawPixel(x, y, ST77XX_BLACK);
      }
    }
  }
}

//blends colors together to make ting/glow
uint16_t blendColor(uint16_t base, uint16_t tint, uint8_t alpha) {
  // Blend 16-bit RGB565 color
  uint8_t r1 = (base >> 11) & 0x1F;
  uint8_t g1 = (base >> 5) & 0x3F;
  uint8_t b1 = base & 0x1F;

  uint8_t r2 = (tint >> 11) & 0x1F;
  uint8_t g2 = (tint >> 5) & 0x3F;
  uint8_t b2 = tint & 0x1F;

  uint8_t r = r1 + ((r2 - r1) * alpha) / 255;
  uint8_t g = g1 + ((g2 - g1) * alpha) / 255;
  uint8_t b = b1 + ((b2 - b1) * alpha) / 255;

  return (r << 11) | (g << 5) | b;
}

//adds an expression and causes the eye to grow the color passed in.
void triggerExpression(Eye &eye, uint16_t tint, int duration = 1000) {
  eye.glow = true;
  eye.tintColor = tint;
  eye.glowAlpha = 192; // strong glow
  eye.emotionStart = millis();
  eye.emotionDuration = duration;
}

//change the goalX or goalY to make the eye look in other directions.
//Once I know the number range map it to the eye movement range
void lookDirection(Eye &eye, float dir, int duration = 1000) {
  if (dir > 0.5) {
    eye.goalX = 88; // look right
    eye.goalY = 64;
  }
  else {
    eye.goalX = 50; //look left
    eye.goalY = 64;
  }
  eye.moveStart = millis();
  eye.moveDuration = duration;
}

// === OSC Input ===
void handleOSC() {
  OSCMessage msg;
  int size = Udp.parsePacket();
  if (size > 0) {
    uint8_t buffer[255];
    size = Udp.read(buffer, 255);
    msg.fill(buffer, size);

    if (!msg.hasError()) {
      if (msg.fullMatch("/cabinet")) {
        float value = msg.getFloat(0);
        triggerExpression(eyeL, ST77XX_GREEN, 3000);
        triggerExpression(eyeR, ST77XX_RED, 3000);
      }
    } else {
      OSCErrorCode error = msg.getError();
      Serial.print("OSC Error: ");
      Serial.println(error);
    }
  }
}