#include <SPI.h>
#include <LoRa.h>
#include <DFRobotDFPlayerMini.h>

// ================= LOK =================
#define MY_LOK_ID 1

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

// ================= PWM =================
#define PWM_CH   0
#define PWM_FREQ 20000
#define PWM_RES  8

// ================= DFPLAYER =================
#define DF_RX 16
#define DF_TX 4

HardwareSerial dfSerial(1);
DFRobotDFPlayerMini dfplayer;
bool soundReady = false;

// ================= SYSTEM =================
bool systemReady = false;   // <-- HIER

// ================= SOUND =================
enum SoundMode {
  SOUND_OFF = 0,
  SOUND_STAND = 1,
  SOUND_DRIVE = 2,
  SOUND_SIGNAL = 3
};

// Ordner 01
#define FOLDER_SOUND 1

#define FILE_DRIVE  1   // 001.mp3
#define FILE_STAND  2   // 002.mp3
#define FILE_SIGNAL 3   // 003.mp3

SoundMode currentSound = SOUND_OFF;

// ================= FAILSAFE =================
unsigned long lastPacketTime = 0;
const unsigned long TIMEOUT_MS = 800;

// =================================================

void motorStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  ledcWrite(PWM_CH, 0);
  digitalWrite(STBY, LOW);
}

void motorSet(uint8_t dir, uint8_t spd) {
  if (dir == 0 || spd == 0) {
    motorStop();
    return;
  }

  digitalWrite(STBY, HIGH);

  digitalWrite(IN1, dir == 1);
  digitalWrite(IN2, dir == 2);

  ledcWrite(PWM_CH, spd);
}

// =================================================

void setSound(uint8_t cmd) {
  if (!soundReady) return;

  // Signal darf immer
  if (cmd == currentSound && cmd != SOUND_SIGNAL) return;

  dfplayer.stop();
  delay(80);

  switch (cmd) {
    case SOUND_OFF:
      // nichts
      break;

    case SOUND_STAND:
      dfplayer.playFolder(FOLDER_SOUND, FILE_STAND);
      break;

    case SOUND_DRIVE:
      dfplayer.playFolder(FOLDER_SOUND, FILE_DRIVE);
      break;

    case SOUND_SIGNAL:
      dfplayer.playFolder(FOLDER_SOUND, FILE_SIGNAL);
      return;   // kein Statuswechsel
  }

  currentSound = (SoundMode)cmd;
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

  // DFPlayer
  dfSerial.begin(9600, SERIAL_8N1, DF_RX, DF_TX);
  delay(2000);

  if (dfplayer.begin(dfSerial)) {
    delay(500);
    dfplayer.outputDevice(DFPLAYER_DEVICE_SD);
    delay(200);
    dfplayer.volume(30);
    dfplayer.stop();
    soundReady = true;
    Serial.println("DFPlayer OK");
  } else {
    Serial.println("DFPlayer FEHLER");
  }

  // LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(868E6)) {
    Serial.println("LoRa Fehler");
    while (1);
  }

  Serial.println("Empfaenger bereit");
  motorStop();
  setSound(SOUND_OFF);
  lastPacketTime = millis();

}

// =================================================

void loop() {
  int size = LoRa.parsePacket();
  if (size == 4) {
    uint8_t lok  = LoRa.read();
    uint8_t dir  = LoRa.read();
    uint8_t spd  = LoRa.read();
    uint8_t snd  = LoRa.read();

    if (lok == MY_LOK_ID) {
      lastPacketTime = millis();
      systemReady = true;
      motorSet(dir, spd);
      setSound(snd);

      Serial.printf(
        "RX Lok:%d Dir:%d Speed:%d Sound:%d\n",
        lok, dir, spd, snd
      );
    }
  }

  // FAILSAFE
  if (millis() - lastPacketTime > TIMEOUT_MS) {
    motorStop();
    setSound(SOUND_OFF);
  }
  if (!systemReady) {
    motorStop();
    setSound(SOUND_OFF);
    return;
  }

}
