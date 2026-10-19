// ═════════════════════════════════════════════════════════════════════════════
//  Frequency Scanner
// ═════════════════════════════════════════════════════════════════════════════

bool scanFreqRange(float startMHz, float endMHz, float stepKHz,const char* label) 
{
  uint32_t startHz = (uint32_t)(startMHz * 1e6f);
  uint32_t endHz   = (uint32_t)(endMHz   * 1e6f);
  uint32_t stepHz  = (uint32_t)(stepKHz  * 1e3f);
  int16_t  peakRssi    = -200;
  uint32_t peakFreq    = 0;
  uint16_t activeCount = 0;
  uint16_t totalSteps  = 0;
  Serial1.println();
  Serial1.print("--- "); Serial1.print(label);
  Serial1.print("  ");   Serial1.print(startMHz, 1);
  Serial1.print(" - ");  Serial1.print(endMHz, 1);
  Serial1.print(" MHz  step "); Serial1.print(stepKHz, 0);
  Serial1.println(" kHz ---");
  Serial1.println("  Freq (MHz)   RSSI      Level");
  Serial1.println("  ----------  ------  --------------------");
  for (uint32_t freq = startHz; freq <= endHz; freq += stepHz) 
  {
    if (Serial1.available()) 
    {
      char c = Serial1.peek();
      if (c == 0x03 || c == 'q' || c == 'Q') 
      {
        Serial1.read();
        Serial1.println("\n  [Aborted by user]");
        return false;
      }
    }
    totalSteps++;
    radio.setFrequency(freq);
    radio.startReceive();
    delayMicroseconds(800);
    int16_t rssi = radio.readRssi();
    if (rssi > peakRssi) 
    { 
      peakRssi = rssi; 
      peakFreq = freq; 
    }
    bool active = (rssi > -100);
    if (active) activeCount++;
    int bar = (rssi + 130) / 5;
    if (bar < 0)  bar = 0;
    if (bar > 20) bar = 20;
    Serial1.print("  ");
    Serial1.print(freq / 1e6f, 3);
    Serial1.print("   ");
    if (rssi > -100) Serial1.print(' ');
    if (rssi > -10)  Serial1.print(' ');
    Serial1.print(rssi);
    Serial1.print(" dBm  ");
    for (int i = 0; i < bar; i++) Serial1.print('#');
    if (active) Serial1.print("  << SIGNAL");
    Serial1.println();
    if (active) 
    {
      PacketInfo pkt;
      uint32_t listenStart = millis();
      while (millis() - listenStart < 50) 
      {
        if (radio.checkForPacket(pkt)) 
        {
          Serial1.print("            ^^ PACKET ");
          Serial1.print(pkt.length);
          Serial1.print("B  RSSI:");
          Serial1.print(pkt.rssi);
          Serial1.print("  SNR:");
          Serial1.print((float)pkt.snr / 4.0f, 1);
          Serial1.println(" dB");
          hexDump(pkt.data, pkt.length);
          break;
        }
      }
    }
    radio.goStandby();
  }
  Serial1.print("  >> Peak: ");
  Serial1.print(peakFreq / 1e6f, 3);
  Serial1.print(" MHz @ ");
  Serial1.print(peakRssi);
  Serial1.print(" dBm   Active: ");
  Serial1.print(activeCount); Serial1.print("/"); Serial1.println(totalSteps);
  return true;
}
