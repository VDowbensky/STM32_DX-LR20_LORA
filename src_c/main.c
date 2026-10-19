#include <Arduino.h>
#include "sx1262_driver.h"

// ═════════════════════════════════════════════════════════════════════════════
//  Application State
// ═════════════════════════════════════════════════════════════════════════════

enum AppMode : uint8_t 
{
  MODE_IDLE,
  MODE_SNIFFING,
  MODE_BEACONING,
  MODE_ANALYZE,
  MODE_CHAT
};

static SX1262Radio radio;


static AppMode appMode = MODE_IDLE;

static char     beaconMsg[256];
static uint8_t  beaconLen      = 0;
static uint32_t beaconInterval = 0;
static uint32_t lastBeaconTime = 0;
static uint32_t lastSniffStatus = 0;
static bool     meshDecodeHint = false;

#define LED_PIN PC13

// ═════════════════════════════════════════════════════════════════════════════
//  Arduino Setup & Main Loop
// ═════════════════════════════════════════════════════════════════════════════

void main(void) 
{
  aes_init_sbox();
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  for (int i = 0; i < 2; i++) 
  {
    digitalWrite(LED_PIN, LOW);  delay(80);
    digitalWrite(LED_PIN, HIGH); delay(80);
  }
  Serial1.begin(115200);
  delay(200);

  Serial1.println("\n");
  Serial1.println("STM32 LoRa Terminal v1.0");
  Serial1.print("SX1262 (DX-LR20) @ "); Serial1.print(SystemCoreClock / 1000000); Serial1.println(" MHz");
  Serial1.println("Type 'help' for commands");
  Serial1.println("[INIT] Starting radio...");
  if (!radio.begin(915000000)) 
  {
    Serial1.println("[ERR] Radio init failed!  Halting.");
    while (true) 
    {
      digitalWrite(LED_PIN, LOW);  
      delay(100);
      digitalWrite(LED_PIN, HIGH); 
      delay(100);
    }
  }
  Serial1.println("[INIT] Radio OK!");
  chatSenderId = deviceId16();
  digitalWrite(LED_PIN, LOW);  
  delay(200);
  digitalWrite(LED_PIN, HIGH);
  prompt();
  
  while(1)
  {
    readSerial();
    if (appMode == MODE_SNIFFING) 
    {
      PacketInfo pkt;
      if (radio.checkForPacket(pkt)) 
      {
        digitalWrite(LED_PIN, LOW);
        displayPacket(pkt);
        digitalWrite(LED_PIN, HIGH);
      }
      if (millis() - lastSniffStatus >= 10000) 
      {
        lastSniffStatus = millis();
        Serial1.print("[SNIFF] ");
        Serial1.print(millis() / 1000);
        Serial1.print("s  Listening...  RSSI: ");
        Serial1.print(radio.readRssi());
        Serial1.print(" dBm  Pkts: ");
        Serial1.println(radio.getPacketCount());
      }
    }
    if (appMode == MODE_BEACONING && millis() - lastBeaconTime >= beaconInterval) 
    {
      lastBeaconTime = millis();
      digitalWrite(LED_PIN, LOW);

      uint32_t t0 = millis();
      bool ok = radio.send((const uint8_t*)beaconMsg, beaconLen);
      uint32_t elapsed = millis() - t0;
      digitalWrite(LED_PIN, HIGH);
      if (ok) 
      {
          Serial1.print("[BEACON] TX \"");
          Serial1.print(beaconMsg);
          Serial1.print("\" (");
          Serial1.print(elapsed);
          Serial1.println(" ms)");
      } 
      else Serial1.println("[BEACON] TX FAILED");
    }
    if (appMode == MODE_ANALYZE && millis() - lastAnalyzeFrame >= 350) 
    {
      lastAnalyzeFrame = millis();
      renderAnalyzeFrame();
    }
    if (appMode == MODE_CHAT) 
    {
      PacketInfo pkt;
      if (radio.checkForPacket(pkt)) 
      {
        char nick[16] = {};
        char msg[128] = {};
        if (parseChatPacket(pkt.data, pkt.length, nick, sizeof(nick), msg, sizeof(msg))) 
        {
          Serial1.print(nick);
          Serial1.print(" :> \"");
          Serial1.print(msg);
          Serial1.println("\"");
          digitalWrite(LED_PIN, LOW);
          delay(15);
          digitalWrite(LED_PIN, HIGH);
        }
      }
      if (millis() - lastChatStatus >= 15000) 
      {
        lastChatStatus = millis();
        Serial1.print("[CHAT] Listening room ");
        Serial1.print(chatRoomId);
        Serial1.print(" RSSI: ");
        Serial1.print(radio.readRssi());
        Serial1.println(" dBm");
      }
    }
    delay(1);
  }
}


