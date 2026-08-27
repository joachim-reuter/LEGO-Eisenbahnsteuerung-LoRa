#include <TFT_eSPI.h>
#include <SPI.h>
#include <LoRa.h>

// ================= TFT =================
TFT_eSPI tft = TFT_eSPI();
uint16_t calData[5] = { 526, 3172, 449, 3087, 1 };

// ================= LoRa Pins =================
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS     5
#define LORA_RST   17
#define LORA_DIO0  26

// ================= Steuerdaten =================
uint8_t lokID     = 1;   // aktive Lok
uint8_t direction = 0;   // 0=STOP,1=VOR,2=ZURUECK
uint8_t speed     = 0;   // 0–100 % (UI)

enum SoundMode {
  SOUND_OFF    = 0,
  SOUND_STAND  = 1,
  SOUND_DRIVE  = 2,
  SOUND_SIGNAL = 3
};

uint8_t soundMode = SOUND_OFF;


// ================= Timing =================
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 300;

// ================= UI Geometrie =================
#define BTN_W 90
#define BTN_H 50
#define BTN_Y 20

#define BTN_VOR_X     10
#define BTN_STOP_X  115
#define BTN_BACK_X  220

#define SLIDER_X 40
#define SLIDER_Y 80
#define SLIDER_W 240
#define SLIDER_H 20

#define LOK_Y 130
#define LOK_BTN_W 60
#define LOK_BTN_H 30
#define LOK_MINUS_X 40
#define LOK_PLUS_X  220

// #define BTN_S_X  200
#define BTN_S_Y  185
#define BTN_S_W   70
#define BTN_S_H    40

#define BTN_S1_X   10   // Fahrt
#define BTN_S2_X   90   // Stand
#define BTN_S3_X  170   // Signal
#define BTN_S4_X  250   // Aus




// =================================================

bool inRect(int x, int y, int rx, int ry, int rw, int rh) {
  return (x >= rx && x <= rx + rw && y >= ry && y <= ry + rh);
}

enum LoRaStatus {
  LORA_INIT,
  LORA_SEND,
  LORA_ERROR
};

LoRaStatus loraStatus = LORA_INIT;
unsigned long loraStatusTime = 0;



void drawButton(int x, int y, int w, int h, const char* label, bool active, uint16_t color) {
  uint16_t fill = active ? color : TFT_DARKGREY;
  tft.fillRoundRect(x, y, w, h, 8, fill);
  tft.drawRoundRect(x, y, w, h, 8, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(label, x + w / 2, y + h / 2, 4);
}

void drawSoundButtons() {

  uint16_t c1 = (soundMode == SOUND_DRIVE)  ? TFT_GREEN  : TFT_DARKGREEN;
  uint16_t c2 = (soundMode == SOUND_STAND)  ? TFT_BROWN  : TFT_DARKGREY;
  uint16_t c3 = (soundMode == SOUND_SIGNAL) ? TFT_ORANGE : TFT_DARKGREY;
  uint16_t c4 = (soundMode == SOUND_OFF)    ? TFT_RED    : TFT_DARKGREY;

  tft.fillRect(BTN_S1_X, BTN_S_Y, BTN_S_W, BTN_S_H, c1);
  tft.drawString("            Stand", BTN_S1_X + 6, BTN_S_Y + 18, 4);

  tft.fillRect(BTN_S2_X, BTN_S_Y, BTN_S_W, BTN_S_H, c2);
  tft.drawString("           Fahrt", BTN_S2_X + 6, BTN_S_Y + 18, 4);

  tft.fillRect(BTN_S3_X, BTN_S_Y, BTN_S_W, BTN_S_H, c3);
  tft.drawString("            Signal", BTN_S3_X + 4, BTN_S_Y + 18, 4);

  tft.fillRect(BTN_S4_X, BTN_S_Y, BTN_S_W, BTN_S_H, c4);
  tft.drawString("      Aus", BTN_S4_X + 18, BTN_S_Y + 18, 4);
}





void drawSlider() {
  tft.drawRect(SLIDER_X, SLIDER_Y, SLIDER_W, SLIDER_H, TFT_WHITE);
  tft.fillRect(SLIDER_X + 1, SLIDER_Y + 1, SLIDER_W - 2, SLIDER_H - 2, TFT_BLACK);

  int fillW = map(speed, 0, 100, 0, SLIDER_W);
  if (fillW > 0) {
    tft.fillRect(SLIDER_X + 1, SLIDER_Y + 1, fillW - 2, SLIDER_H - 2, TFT_YELLOW);
  }

  tft.setTextDatum(TC_DATUM);
  tft.drawString(String(speed) + " %", SLIDER_X + SLIDER_W / 2, SLIDER_Y + 30);
}

void drawLokSelector() {
  tft.fillRect(0, LOK_Y - 10, 320, 50, TFT_BLACK);

  drawButton(LOK_MINUS_X, LOK_Y, LOK_BTN_W, LOK_BTN_H, "-", false, TFT_DARKGREY);
  drawButton(LOK_PLUS_X,  LOK_Y, LOK_BTN_W, LOK_BTN_H, "+", false, TFT_DARKGREY);

  tft.setTextDatum(MC_DATUM);
  tft.drawString("Lok: " + String(lokID), 160, LOK_Y + 15,  4);
}

void drawUI() {
  tft.fillScreen(TFT_BLACK);

  drawButton(BTN_VOR_X,  BTN_Y, BTN_W, BTN_H, "<",     direction == 1, TFT_GREEN);
  drawButton(BTN_STOP_X, BTN_Y, BTN_W, BTN_H, "STOP",    direction == 0, TFT_RED);
  drawButton(BTN_BACK_X, BTN_Y, BTN_W, BTN_H, ">", direction == 2, TFT_BLUE);

  //tft.drawString("    Geschwindigkeit", SLIDER_X, SLIDER_Y - 30);
  drawSlider();
  drawLokSelector();
  drawSoundButtons();
  drawLoRaStatus();

}

// ================= LoRa =================
void sendLoRa() {
  uint8_t spdByte = map(speed, 0, 100, 0, 255);

  LoRa.beginPacket();
  LoRa.write(lokID);
  LoRa.write(direction);
  LoRa.write(spdByte);
  LoRa.write(soundMode);
  LoRa.endPacket();

  Serial.printf("TX -> Lok:%d Dir:%d Speed:%d\n", lokID, direction, spdByte);

  loraStatus = LORA_SEND;
  loraStatusTime = millis();
  drawLoRaStatus(); 


}

void drawLoRaStatus() {
  uint16_t color;
  String text;

  switch (loraStatus) {
    case LORA_INIT:
      color = TFT_YELLOW;
      text = "LoRa: AKTIV";
      break;
    case LORA_SEND:
      color = TFT_GREEN;
      text = "LoRa: SEND";
      break;
    case LORA_ERROR:
      color = TFT_RED;
      text = "LoRa: ERROR";
      break;
  }

  // Bereich löschen
  tft.fillRect(180, 0, 140, 20, TFT_BLACK);

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(text, 315, 5);
}








// =================================================

void setup() {
  Serial.begin(115200);

  // TFT
  tft.init();
  tft.setRotation(3);
  tft.setTouch(calData);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // SPI & LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  
  if (!LoRa.begin(868100000)) {
    Serial.println("LoRa Fehler!");
    loraStatus = LORA_ERROR;
  } else {
    loraStatus = LORA_INIT;
  }


  drawUI();
  Serial.println("Sender bereit");
}

void loop() {
  uint16_t x, y;
  bool changed = false;
  if (tft.getTouch(&x, &y)) {

    // ---------- Richtung ----------
    if (inRect(x, y, BTN_VOR_X, BTN_Y, BTN_W, BTN_H) && direction != 1) {
      direction = 1; changed = true; drawUI();
    }
    else if (inRect(x, y, BTN_STOP_X, BTN_Y, BTN_W, BTN_H) && direction != 0) {
      direction = 0; changed = true; drawUI();
    }
    else if (inRect(x, y, BTN_BACK_X, BTN_Y, BTN_W, BTN_H) && direction != 2) {
      direction = 2; changed = true; drawUI();
    }

    // ---------- Slider ----------
    if (inRect(x, y, SLIDER_X, SLIDER_Y, SLIDER_W, SLIDER_H)) {
      uint8_t newSpeed = constrain(map(x, SLIDER_X, SLIDER_X + SLIDER_W, 0, 100), 0, 100);
      if (newSpeed != speed) {
        speed = newSpeed;
        drawSlider();
        changed = true;
      }
    }

    // ---------- Lok ----------
    if (inRect(x, y, LOK_MINUS_X, LOK_Y, LOK_BTN_W, LOK_BTN_H) && lokID > 1) {
      lokID--; drawLokSelector(); changed = true;
    }
    else if (inRect(x, y, LOK_PLUS_X, LOK_Y, LOK_BTN_W, LOK_BTN_H) && lokID < 255) {
      lokID++; drawLokSelector(); changed = true;
    }

    // ---------- SOUND ----------
    if (inRect(x, y, BTN_S1_X, BTN_S_Y, BTN_S_W, BTN_S_H)) {
      soundMode = SOUND_DRIVE; changed = true;
    }   
    else if (inRect(x, y, BTN_S2_X, BTN_S_Y, BTN_S_W, BTN_S_H)) {
      soundMode = SOUND_STAND; changed = true;
    }
    else if (inRect(x, y, BTN_S3_X, BTN_S_Y, BTN_S_W, BTN_S_H)) {
      soundMode = SOUND_SIGNAL; changed = true;
    }
    else if (inRect(x, y, BTN_S4_X, BTN_S_Y, BTN_S_W, BTN_S_H)) {
      soundMode = SOUND_OFF; changed = true;
    }

  }

  if (changed || millis() - lastSend > SEND_INTERVAL) {
    sendLoRa();
    lastSend = millis();
  }

}
