/*  Sender Motorsteuerung über LoRa*/
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

uint8_t weicheID = 1;      // 1 oder 2 (Toggle)
uint8_t direction = 0;   // 1=Gerade,2=Abzweig



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


#define BTN_TOGGLE_X  60
#define BTN_TOGGLE_Y  80
#define BTN_TOGGLE_W  200
#define BTN_TOGGLE_H  40


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

void drawToggle() {
  String text = (weicheID == 1) ? "Weiche 1" : "Weiche 2";
  uint16_t color = (weicheID == 1) ? TFT_ORANGE : TFT_CYAN;

  tft.fillRoundRect(BTN_TOGGLE_X, BTN_TOGGLE_Y,
                    BTN_TOGGLE_W, BTN_TOGGLE_H, 8, color);
  tft.drawRoundRect(BTN_TOGGLE_X, BTN_TOGGLE_Y,
                    BTN_TOGGLE_W, BTN_TOGGLE_H, 8, TFT_WHITE);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK);
  tft.drawString(text,
                 BTN_TOGGLE_X + BTN_TOGGLE_W / 2,
                 BTN_TOGGLE_Y + BTN_TOGGLE_H / 2, 4);
}





void drawUI() {
  tft.fillScreen(TFT_BLACK);

  drawButton(BTN_VOR_X,  BTN_Y, BTN_W, BTN_H, "<",     direction == 1, TFT_GREEN);
  drawButton(BTN_BACK_X, BTN_Y, BTN_W, BTN_H, ">", direction == 2, TFT_BLUE);

  drawToggle();
  drawLoRaStatus();

}

// ================= LoRa =================
void sendLoRa() {
  
  LoRa.beginPacket();
  LoRa.write(weicheID);
  LoRa.write(direction);
  LoRa.endPacket();

  Serial.printf("Weiche:%d Cmd:%d\n", weicheID, direction);

  loraStatus = LORA_SEND;
  
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
  
  if (!LoRa.begin(868300000)) {
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

    // Toggle Weiche
    if (inRect(x, y,
               BTN_TOGGLE_X, BTN_TOGGLE_Y,
               BTN_TOGGLE_W, BTN_TOGGLE_H)) {
      weicheID = (weicheID == 1) ? 2 : 1;
      drawUI();
      delay(200); // Entprellen
    }

    // Gerade
    else if (inRect(x, y, BTN_VOR_X, BTN_Y, BTN_W, BTN_H)) {
      direction = 1;
      changed = true;
    }

    // Abzweig
    else if (inRect(x, y, BTN_BACK_X, BTN_Y, BTN_W, BTN_H)) {
      direction = 2;
      changed = true;
    }
  }

  if (changed) {
    sendLoRa();
  }

}
