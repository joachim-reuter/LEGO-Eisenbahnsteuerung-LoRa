#include <SPI.h>
#include <LoRa.h>
#include <EEPROM.h>

// ================= LoRa =================
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS     5
#define LORA_RST   17
#define LORA_DIO0  26

// ================= DRV8833 =================
const int IN1  = 25;
const int IN2  = 33;
const int ENA  = 32;
const int STBY = 27;

// ================= EEPROM =================
#define EEPROM_SIZE 32
#define EEPROM_ADDR_WEICHE 0

// ================= PWM =================
#define PWM_CH   0
#define PWM_FREQ 20000
#define PWM_RES  8

// ================= Status =================
uint8_t weichenStatus = 0; // 1=GERADE, 2=ABZWEIG

// =================================================

void motorStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  ledcWrite(PWM_CH, 0);
  digitalWrite(STBY, LOW);
}

void motorImpulse(uint8_t dir) {
  digitalWrite(STBY, HIGH);
  digitalWrite(IN1, dir == 1);
  digitalWrite(IN2, dir == 2);
  ledcWrite(PWM_CH, 80);     // Kraft für Weiche

  delay(400);               // Impulsdauer
  motorStop();
}

// =================================================

void schalteWeiche(uint8_t cmd) {
  if (cmd == weichenStatus) return; // nichts tun

  Serial.printf("Weiche schalten: %s\n",
                cmd == 1 ? "GERADE" : "ABZWEIG");

  motorImpulse(cmd);

  weichenStatus = cmd;
  EEPROM.write(EEPROM_ADDR_WEICHE, weichenStatus);
  EEPROM.commit();
}

// =================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  // Motor
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, LOW);

  ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENA, PWM_CH);

  motorStop();

  // EEPROM
  EEPROM.begin(EEPROM_SIZE);
  weichenStatus = EEPROM.read(EEPROM_ADDR_WEICHE);
  if (weichenStatus < 1 || weichenStatus > 2) {
    weichenStatus = 1; // Default: GERADE
  }

  Serial.printf("Gespeicherter Zustand: %d\n", weichenStatus);

  // LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(868300000)) {
    Serial.println("LoRa Fehler");
    while (1);
  }

  Serial.println("Weichen-Empfaenger bereit");
}

// =================================================

void loop() {
  int size = LoRa.parsePacket();
  if (size == 1) {
    uint8_t cmd = LoRa.read();
    if (cmd == 1 || cmd == 2) {
      schalteWeiche(cmd);
    }
  }
}