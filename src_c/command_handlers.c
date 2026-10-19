// ═════════════════════════════════════════════════════════════════════════════
//  Command Handlers
// ═════════════════════════════════════════════════════════════════════════════
static char    cmdBuf[256];
static uint16_t cmdLen = 0;

void cmdHelp(void) 
{
  Serial1.println();
  Serial1.println("LoRa Terminal Help");
  Serial1.println("TX/RX");
  Serial1.println("  send <msg>          Send text payload");
  Serial1.println("  sendhex <hex>       Send raw hex bytes");
  Serial1.println("  sniff               Continuous RX monitor");
  Serial1.println("  beacon <ms> <msg>   Periodic TX");
  Serial1.println("  stop                Stop active mode");
  Serial1.println("Chat");
  Serial1.println("  chatjoin <room> [key] Join encrypted room");
  Serial1.println("  chatnick chat chatstatus chatleave");
  Serial1.println("Radio");
  Serial1.println("  freq sf bw cr power preamble crc iq");
  Serial1.println("  syncword header ldro modem rxboost");
  Serial1.println("  bitrate fdev fskbw whitening (GFSK)");
  Serial1.println("Scan");
  Serial1.println("  scan [start end step] | 433|868|915");
  Serial1.println("  analyze analyzecfg scanpreset");
  Serial1.println("Meshtastic");
  Serial1.println("  meshlisten <preset> [region|MHz]");
  Serial1.println("    regions: us eu868 eu433 cn jp anz kr tw in ru");
  Serial1.println("  meshkey <32hex>|default    AES key");
  Serial1.println("System");
  Serial1.println("  status rssi reset uptime version");
  Serial1.println("  sleep clear reboot");
  Serial1.println("Use <cmd> -help for details");
}

void cmdSend(const char* arg) 
{
  uint8_t len = strlen(arg);
  if (len == 0) 
  { 
    Serial1.println("[ERR] Usage: send <message>"); 
    return; 
  }
  if (len > 255) 
  { 
    Serial1.println("[ERR] Message too long (max 255)"); 
    return; 
  }
  bool resume = (appMode == MODE_SNIFFING);
  Serial1.print("[TX] Sending "); Serial1.print(len); Serial1.println(" bytes...");
  digitalWrite(LED_PIN, LOW);
  uint32_t t0 = millis();
  bool ok = radio.send((const uint8_t*)arg, len);
  uint32_t elapsed = millis() - t0;
  digitalWrite(LED_PIN, HIGH);
  if (ok) 
  {
      Serial1.print("[TX] Complete in "); 
      Serial1.print(elapsed); 
      Serial1.println(" ms");
  } 
  else Serial1.println("[TX] FAILED (timeout)");
  if (resume) 
  {
    radio.startReceive();
    Serial1.println("[RX] Sniff resumed");
  }
}

void cmdSendHex(const char* arg) 
{
  uint8_t buf[255];
  uint8_t len = 0;
  while (*arg && len < 255) 
  {
    while (*arg == ' ') arg++;
    if (!*arg) break;
    uint8_t hi = hexNibble(*arg++);
    if (hi > 15 || !*arg) { Serial1.println("[ERR] Invalid hex"); return; }
    uint8_t lo = hexNibble(*arg++);
    if (lo > 15) { Serial1.println("[ERR] Invalid hex"); return; }
    buf[len++] = (hi << 4) | lo;
  }
  if (len == 0) { Serial1.println("[ERR] Usage: sendhex <hex bytes>"); return; }
  bool resume = (appMode == MODE_SNIFFING);
  Serial1.print("[TX] Sending "); Serial1.print(len); Serial1.println(" hex bytes...");
  digitalWrite(LED_PIN, LOW);
  uint32_t t0 = millis();
  bool ok = radio.send(buf, len);
  uint32_t elapsed = millis() - t0;
  digitalWrite(LED_PIN, HIGH);
  if (ok) 
  {
    Serial1.print("[TX] Complete in "); 
    Serial1.print(elapsed); 
    Serial1.println(" ms");
  } 
  else Serial1.println("[TX] FAILED (timeout)");
  if (resume) 
  {
    radio.startReceive();
    Serial1.println("[RX] Sniff resumed");
  }
}

void cmdSniff(void) 
{
  if (appMode == MODE_BEACONING) 
  {
    appMode = MODE_IDLE;
    radio.goStandby();
  }
  appMode = MODE_SNIFFING;
  radio.resetStats();
  radio.startReceive();
  lastSniffStatus = millis();
  const LoRaConfig& c = radio.getConfig();
  Serial1.print("[SNIFF] Listening on ");
  Serial1.print((float)c.frequencyHz / 1e6f, 3);
  Serial1.print(" MHz  SF");
  Serial1.print(c.spreadingFactor);
  Serial1.print("  BW ");
  Serial1.print(SX1262Radio::bwToStr(c.bandwidth));
  Serial1.println("  (type 'stop' to exit)");
}

void cmdBeacon(const char* arg) 
{
  char* end = nullptr;
  uint32_t interval = strtoul(arg, &end, 10);

  if (interval == 0) 
  {
    if (appMode == MODE_BEACONING) 
    {
      appMode = MODE_IDLE;
      radio.goStandby();
      Serial1.println("[BEACON] Stopped");
    } 
    else Serial1.println("[ERR] Beacon not active");
    return;
  }
  while (*end == ' ') end++;
  if (*end == '\0') { Serial1.println("[ERR] Usage: beacon <ms> <message>"); return; }
  if (appMode == MODE_SNIFFING) radio.goStandby();
  beaconInterval = interval;
  beaconLen = strlen(end);
  if (beaconLen > 255) beaconLen = 255;
  memcpy(beaconMsg, end, beaconLen);
  beaconMsg[beaconLen] = '\0';
  appMode = MODE_BEACONING;
  lastBeaconTime = 0;
  Serial1.print("[BEACON] Sending \"");
  Serial1.print(beaconMsg);
  Serial1.print("\" every ");
  Serial1.print(beaconInterval);
  Serial1.println(" ms");
}

void cmdStop(void) 
{
    radio.goStandby();
    if (appMode == MODE_SNIFFING) 
    {
      Serial1.print("[SNIFF] Stopped.  Packets: ");
      Serial1.print(radio.getPacketCount());
      Serial1.print("  CRC errors: ");
      Serial1.println(radio.getCrcErrorCount());
    } 
    else if (appMode == MODE_BEACONING) Serial1.println("[BEACON] Stopped");
    else if (appMode == MODE_ANALYZE) 
    {
      radio.setFrequency(analyzePrevFreq);
      Serial1.println("[ANALYZE] Stopped");
    } 
    else if (appMode == MODE_CHAT) Serial1.println("[CHAT] Stopped listening");
    else Serial1.println("[INFO] Already idle");
    appMode = MODE_IDLE;
}

void cmdFreq(const char* arg) 
{
  float mhz = atof(arg);
  if (mhz < 40.0f || mhz > 1400.0f) 
  {
    Serial1.println("[ERR] Frequency out of range (40-1400 MHz)");
    return;
  }
  uint32_t hz = (uint32_t)(mhz * 1e6f);
  radio.setFrequency(hz);
  Serial1.print("[CFG] Frequency: ");
  Serial1.print(mhz, 3);
  Serial1.println(" MHz");
}

void cmdSF(const char* arg) 
{
  int sf = atoi(arg);
  if (sf < 5 || sf > 12) 
  {
    Serial1.println("[ERR] SF must be 5-12");
    return;
  }
  radio.setSpreadingFactor((uint8_t)sf);
  Serial1.print("[CFG] Spreading Factor: SF"); 
  Serial1.println(sf);
}

void cmdBW(const char* arg) 
{
  float kHz = atof(arg);
  uint8_t code = SX1262Radio::bwFromKHz(kHz);
  radio.setBandwidth(code);
  Serial1.print("[CFG] Bandwidth: ");
  Serial1.println(SX1262Radio::bwToStr(code));
}

void cmdCR(const char* arg) 
{
  int cr = atoi(arg);
  if (cr < 5 || cr > 8) 
  {
    Serial1.println("[ERR] CR must be 5-8 (meaning 4/5 to 4/8)");
    return;
  }
  radio.setCodingRate((uint8_t)cr);
  Serial1.print("[CFG] Coding Rate: 4/"); 
  Serial1.println(cr);
}

void cmdPower(const char* arg) 
{
  int p = atoi(arg);
  if (p < -9 || p > 22) 
  {
    Serial1.println("[ERR] Power must be -9 to 22 dBm");
    return;
  }
  radio.setTxPower((int8_t)p);
  Serial1.print("[CFG] TX Power: "); 
  Serial1.print(p); 
  Serial1.println(" dBm");
}

void cmdPreamble(const char* arg) 
{
    int n = atoi(arg);
    if (n < 1 || n > 65535) 
    {
        Serial1.println("[ERR] Preamble must be 1-65535");
        return;
    }
    radio.setPreambleLength((uint16_t)n);
    Serial1.print("[CFG] Preamble: "); Serial1.print(n); Serial1.println(" symbols");
}

static void cmdCrc(const char* arg) 
{
    if (strcasecmp(arg, "on") == 0) 
    {
        radio.setCrc(true);
        Serial1.println("[CFG] CRC: ON");
    } 
    else if (strcasecmp(arg, "off") == 0) 
    {
        radio.setCrc(false);
        Serial1.println("[CFG] CRC: OFF");
    } 
    else 
    {
        Serial1.println("[ERR] Usage: crc <on|off>");
    }
}

static void cmdIQ(const char* arg) 
{
    if (strcasecmp(arg, "normal") == 0) 
    {
        radio.setIqInverted(false);
        Serial1.println("[CFG] IQ: Normal");
    } 
    else if (strcasecmp(arg, "invert") == 0 || strcasecmp(arg, "inverted") == 0) 
    {
        radio.setIqInverted(true);
        Serial1.println("[CFG] IQ: Inverted");
    } 
    else 
    {
        Serial1.println("[ERR] Usage: iq <normal|invert>");
    }
}

static void cmdSyncWord(const char* arg) 
{
    uint8_t sw = (uint8_t)strtoul(arg, nullptr, 16);
    if (sw == 0 && arg[0] != '0') 
    {
        Serial1.println("[ERR] Usage: syncword <hex> (e.g. 12 or 34)");
        return;
    }
    radio.setSyncWord(sw);
    Serial1.print("[CFG] Sync Word: 0x");
    if (sw < 0x10) Serial1.print('0');
    Serial1.print(sw, HEX);
    if (sw == 0x12)      Serial1.println(" (Private)");
    else if (sw == 0x34) Serial1.println(" (Public / LoRaWAN)");
    else                 Serial1.println();
}

static float meshRegionFreq(const char* r) 
{
    if (strcasecmp(r, "us") == 0)     return 906.875f;
    if (strcasecmp(r, "eu868") == 0)  return 869.525f;
    if (strcasecmp(r, "eu433") == 0)  return 433.175f;
    if (strcasecmp(r, "cn") == 0)     return 470.0f;
    if (strcasecmp(r, "jp") == 0)     return 920.0f;
    if (strcasecmp(r, "anz") == 0)    return 916.0f;
    if (strcasecmp(r, "kr") == 0)     return 921.9f;
    if (strcasecmp(r, "tw") == 0)     return 923.0f;
    if (strcasecmp(r, "in") == 0)     return 865.0625f;
    if (strcasecmp(r, "ru") == 0)     return 868.9f;
    return 0.0f;
}

static void cmdMeshListen(const char* arg) 
{
    char preset[16] = {};
    const char* p = arg;

    while (*p == ' ') p++;
    if (*p == '\0') 
    {
        Serial1.println("[ERR] meshlisten <preset> [region|MHz]");
        Serial1.println("  Presets: longfast longslow shortfast");
        Serial1.println("  Regions: us eu868 eu433 cn jp anz kr tw in ru");
        return;
    }

    uint8_t presetLen = 0;
    while (*p != '\0' && *p != ' ') 
    {
        if (presetLen < sizeof(preset) - 1) 
        {
            preset[presetLen++] = *p;
        }
        p++;
    }
    preset[presetLen] = '\0';

    while (*p == ' ') p++;

    float mhz = 0.0f;
    if (*p == '\0') 
    {
        // No freq/region given — default to US
        mhz = 906.875f;
    } 
    else 
    {
        // Try region name first
        char region[8] = {};
        uint8_t ri = 0;
        const char* rp = p;
        while (*rp && *rp != ' ' && ri < sizeof(region) - 1) region[ri++] = *rp++;
        region[ri] = '\0';
        mhz = meshRegionFreq(region);
        if (mhz == 0.0f) 
        {
            // Not a region, try as MHz number
            char* end = nullptr;
            mhz = strtof(p, &end);
            if (end == p || mhz < 150.0f || mhz > 960.0f) 
            {
                Serial1.println("[ERR] Bad freq/region. Use: us eu868 eu433 cn jp anz kr tw in ru");
                return;
            }
        }
    }

    radio.setModem(MODEM_LORA);
    radio.setFrequency((uint32_t)(mhz * 1e6f));
    radio.setHeaderImplicit(false);
    radio.setCrc(true);
    radio.setIqInverted(false);
    radio.setPreambleLength(16);
    radio.setSyncWord(0x2B);

    if (strcasecmp(preset, "longfast") == 0) 
    {
        radio.setSpreadingFactor(11);
        radio.setBandwidth(SX1262Radio::bwFromKHz(250.0f));
        radio.setCodingRate(5);
    } 
    else if (strcasecmp(preset, "longslow") == 0) 
    {
        radio.setSpreadingFactor(12);
        radio.setBandwidth(SX1262Radio::bwFromKHz(125.0f));
        radio.setCodingRate(8);
    } 
    else if (strcasecmp(preset, "shortfast") == 0) 
    {
        radio.setSpreadingFactor(7);
        radio.setBandwidth(SX1262Radio::bwFromKHz(250.0f));
        radio.setCodingRate(5);
    } 
    else 
    {
        Serial1.println("[ERR] Unknown preset. Use longfast, longslow, or shortfast.");
        return;
    }

    Serial1.print("[MESH] Preset: ");
    Serial1.print(preset);
    Serial1.print("  Freq: ");
    Serial1.print(mhz, 3);
    Serial1.println(" MHz");
    meshDecodeHint = true;
    Serial1.println("[MESH] Decode+AES on");
    Serial1.print("[MESH] Key: ");
    Serial1.println(meshKeyIsDefault ? "default (LongFast)" : "custom");

    cmdSniff();
}

static void cmdMeshKey(const char* arg) 
{
    while (*arg == ' ') arg++;

    if (*arg == '\0' || strcasecmp(arg, "default") == 0) 
    {
        static const uint8_t dk[16] = 
        {
            0xd4,0xf1,0xbb,0x3a,0x20,0x29,0x07,0x59,
            0xf0,0xbc,0xff,0xab,0xcf,0x4e,0x69,0x01
        };
        memcpy(meshAesKey, dk, 16);
        meshKeyIsDefault = true;
        Serial1.println("[MESH] Key=default");
        return;
    }

    size_t slen = strlen(arg);
    if (slen == 32) 
    {
        uint8_t tmp[16];
        for (uint8_t i = 0; i < 16; i++) 
        {
            uint8_t hi = hexNibble(arg[i * 2]);
            uint8_t lo = hexNibble(arg[i * 2 + 1]);
            if (hi > 15 || lo > 15) 
            {
              Serial1.println("[ERR] Bad hex"); 
              return; 
            }
            tmp[i] = (uint8_t)((hi << 4) | lo);
        }
        memcpy(meshAesKey, tmp, 16);
        meshKeyIsDefault = false;
        Serial1.print("[MESH] Key="); 
        printHex16(meshAesKey);
        return;
    }
    Serial1.println("[ERR] meshkey <32hex>|default");
}

static void cmdHeader(const char* arg) 
{
    if (strncasecmp(arg, "explicit", 8) == 0) 
    {
        radio.setHeaderImplicit(false);
        Serial1.println("[CFG] Header: Explicit");
        return;
    }

    if (strncasecmp(arg, "implicit", 8) == 0) 
    {
        uint8_t len = 0xFF;
        const char* p = arg + 8;
        while (*p == ' ') p++;
        if (*p != '\0') 
        {
            int n = atoi(p);
            if (n < 1 || n > 255) 
            {
                Serial1.println("[ERR] Implicit length must be 1-255");
                return;
            }
            len = (uint8_t)n;
        }
        radio.setHeaderImplicit(true, len);
        Serial1.print("[CFG] Header: Implicit  Len=");
        Serial1.println(len);
        return;
    }

    Serial1.println("[ERR] Usage: header <explicit|implicit [len]>");
}

static void cmdLdro(const char* arg) 
{
    if (strcasecmp(arg, "auto") == 0) 
    {
        radio.setLdroMode(LDRO_AUTO);
        Serial1.println("[CFG] LDRO: AUTO");
    } 
    else if (strcasecmp(arg, "on") == 0) 
    {
        radio.setLdroMode(LDRO_ON);
        Serial1.println("[CFG] LDRO: ON");
    } 
    else if (strcasecmp(arg, "off") == 0) 
    {
        radio.setLdroMode(LDRO_OFF);
        Serial1.println("[CFG] LDRO: OFF");
    } 
    else 
    {
        Serial1.println("[ERR] Usage: ldro <auto|on|off>");
    }
}

static void cmdSymTimeout(const char* arg) 
{
    int n = atoi(arg);
    if (n < 0 || n > 255) 
    {
        Serial1.println("[ERR] Usage: symtimeout <0-255>");
        return;
    }
    radio.setSymbolTimeout((uint8_t)n);
    Serial1.print("[CFG] Symbol timeout: ");
    Serial1.println(n);
}

static void cmdRxBoost(const char* arg) 
{
    if (strcasecmp(arg, "on") == 0) 
    {
        radio.setRxBoosted(true);
        Serial1.println("[CFG] RX boost: ON");
    } 
    else if (strcasecmp(arg, "off") == 0) 
    {
        radio.setRxBoosted(false);
        Serial1.println("[CFG] RX boost: OFF");
    } 
    else 
    {
        Serial1.println("[ERR] Usage: rxboost <on|off>");
    }
}

static void cmdStandby(const char* arg) 
{
    if (strcasecmp(arg, "xosc") == 0) 
    {
        radio.setStandbyXosc(true);
        Serial1.println("[CFG] Standby: XOSC");
    } 
    else if (strcasecmp(arg, "rc") == 0) 
    {
        radio.setStandbyXosc(false);
        Serial1.println("[CFG] Standby: RC");
    } 
    else 
    {
        Serial1.println("[ERR] Usage: standby <xosc|rc>");
    }
}

static void cmdRegulator(const char* arg) 
{
    if (strcasecmp(arg, "dcdc") == 0) 
    {
        radio.setRegulatorDcdc(true);
        Serial1.println("[CFG] Regulator: DC-DC");
    } 
    else if (strcasecmp(arg, "ldo") == 0) 
    {
        radio.setRegulatorDcdc(false);
        Serial1.println("[CFG] Regulator: LDO");
    } 
    else 
    {
        Serial1.println("[ERR] Usage: regulator <dcdc|ldo>");
    }
}

static void cmdModem(const char* arg) 
{
    if (strcasecmp(arg, "lora") == 0) 
    {
        radio.setModem(MODEM_LORA);
        Serial1.println("[CFG] Modem: LoRa");
    } 
    else if (strcasecmp(arg, "gfsk") == 0 || strcasecmp(arg, "fsk") == 0) 
    {
        radio.setModem(MODEM_GFSK);
        Serial1.println("[CFG] Modem: GFSK");
    } 
    else 
    {
        Serial1.println("[ERR] Usage: modem <lora|gfsk>");
    }
}

static void cmdBitrate(const char* arg) 
{
    uint32_t bps = (uint32_t)strtoul(arg, nullptr, 10);
    if (bps < 600 || bps > 300000) 
    {
        Serial1.println("[ERR] Usage: bitrate <600..300000>");
        return;
    }
    radio.setGfskBitrate(bps);
    Serial1.print("[CFG] GFSK bitrate: ");
    Serial1.println(bps);
}

static void cmdFdev(const char* arg) 
{
    uint32_t hz = (uint32_t)strtoul(arg, nullptr, 10);
    if (hz < 100 || hz > 200000) 
    {
        Serial1.println("[ERR] Usage: fdev <100..200000>");
        return;
    }
    radio.setGfskFdev(hz);
    Serial1.print("[CFG] GFSK fdev: ");
    Serial1.println(hz);
}

static void cmdFskBw(const char* arg) 
{
    uint8_t code = (uint8_t)strtoul(arg, nullptr, 16);
    radio.setGfskBw(code);
    Serial1.print("[CFG] GFSK BW code: 0x");
    if (code < 0x10) Serial1.print('0');
    Serial1.println(code, HEX);
}

static void cmdWhitening(const char* arg) 
{
    if (strcasecmp(arg, "on") == 0) 
    {
        radio.setGfskWhitening(true);
        Serial1.println("[CFG] GFSK whitening: ON");
    } 
    else if (strcasecmp(arg, "off") == 0) 
    {
        radio.setGfskWhitening(false);
        Serial1.println("[CFG] GFSK whitening: OFF");
    } 
    else 
    {
        Serial1.println("[ERR] Usage: whitening <on|off>");
    }
}

static void cmdChatStatus() 
{
    Serial1.print("[CHAT] Joined: ");
    Serial1.println(chatJoined ? "YES" : "NO");
    Serial1.print("[CHAT] Room: ");
    Serial1.println(chatRoomId);
    Serial1.print("[CHAT] Nick: ");
    Serial1.println(chatNick);
    Serial1.print("[CHAT] Sender ID: 0x");
    if (chatSenderId < 0x1000) Serial1.print('0');
    if (chatSenderId < 0x0100) Serial1.print('0');
    if (chatSenderId < 0x0010) Serial1.print('0');
    Serial1.println(chatSenderId, HEX);
}

static void cmdChatNick(const char* arg) 
{
    if (*arg == '\0') 
    {
        Serial1.println("[ERR] Usage: chatnick <name>");
        return;
    }
    size_t n = strlen(arg);
    if (n > 12) n = 12;
    memcpy(chatNick, arg, n);
    chatNick[n] = '\0';
    Serial1.print("[CHAT] Nick set: ");
    Serial1.println(chatNick);
}

static void cmdChatJoin(const char* arg) 
{
    char roomStr[8] = {};
    char keyStr[80] = {};
    int n = sscanf(arg, "%7s %79s", roomStr, keyStr);
    if (n < 1) 
    {
        Serial1.println("[ERR] Usage: chatjoin <room 0-255> [hexkey]");
        return;
    }

    int room = atoi(roomStr);
    if (room < 0 || room > 255) 
    {
        Serial1.println("[ERR] Room must be 0-255");
        return;
    }

    if (n >= 2) 
    {
        if (!parseHexFlexible(keyStr, chatKeyEnc)) 
        {
            Serial1.println("[ERR] Key must be 8..32 hex chars (even length)");
            return;
        }
    } 
    else 
    {
        generateChatKey(chatKeyEnc);
        Serial1.println("[CHAT] Generated room key:");
        printHex16(chatKeyEnc);
        Serial1.println("[CHAT] Share this key with peers to join the same room.");
    }

    deriveMacKey(chatKeyEnc, chatKeyMac);
    chatRoomId = (uint8_t)room;
    chatJoined = true;
    chatSeq = 0;
    memset(chatPeers, 0, sizeof(chatPeers));

    appMode = MODE_CHAT;
    radio.startReceive();
    Serial1.print("[CHAT] Joined room ");
    Serial1.println(chatRoomId);
    Serial1.println("[CHAT] Type message text directly. Use 'chatleave' to exit.");
}

static void cmdChatLeave() 
{
    if (!chatJoined) 
    {
        Serial1.println("[CHAT] Not joined");
        return;
    }
    chatJoined = false;
    memset(chatKeyEnc, 0, sizeof(chatKeyEnc));
    memset(chatKeyMac, 0, sizeof(chatKeyMac));
    memset(chatPeers, 0, sizeof(chatPeers));
    if (appMode == MODE_CHAT) 
    {
        appMode = MODE_IDLE;
        radio.goStandby();
    }
    Serial1.println("[CHAT] Left room");
}

static void cmdChatSend(const char* arg) 
{
    if (!chatJoined) 
    {
        Serial1.println("[ERR] Join a room first: chatjoin <room> [hexkey]");
        return;
    }
    if (*arg == '\0') 
    {
        Serial1.println("[ERR] Usage: chat <message>");
        return;
    }

    uint8_t pkt[220];
    uint8_t len = buildChatPacket(arg, pkt);
    if (len == 0) 
    {
        Serial1.println("[ERR] Chat message too long");
        return;
    }

    bool wasChat = (appMode == MODE_CHAT);
    bool ok = radio.send(pkt, len);
    if (ok) 
    {
        chatSeq++;
        Serial1.print(chatNick);
        Serial1.print(" :> \"");
        Serial1.print(arg);
        Serial1.println("\"");
    } 
    else 
    {
        Serial1.println("[CHAT] TX FAIL");
    }

    if (wasChat) radio.startReceive();
}

static void cmdStatus() 
{
    const LoRaConfig& c = radio.getConfig();
    Serial1.println();
    Serial1.println("Radio Configuration");

    Serial1.print("  Modem:       ");
    Serial1.println(c.modem == MODEM_LORA ? "LoRa" : "GFSK");

    Serial1.print("  Frequency:   ");
    Serial1.print((float)c.frequencyHz / 1e6f, 3);
    Serial1.println(" MHz");

    Serial1.print("  SF:          ");
    Serial1.println(c.spreadingFactor);

    Serial1.print("  BW:          ");
    Serial1.println(SX1262Radio::bwToStr(c.bandwidth));

    Serial1.print("  CR:          4/");
    Serial1.println(c.codingRate + 4);

    Serial1.print("  Power:       ");
    Serial1.print(c.txPowerDbm);
    Serial1.println(" dBm");

    Serial1.print("  Preamble:    ");
    Serial1.print(c.preambleLength);
    Serial1.println(" sym");

    Serial1.print("  CRC:         ");
    Serial1.println(c.crcOn ? "ON" : "OFF");

    Serial1.print("  IQ:          ");
    Serial1.println(c.iqInverted ? "Inverted" : "Normal");

    Serial1.print("  Sync:        0x");
    if (c.syncWord < 0x10) Serial1.print('0');
    Serial1.print(c.syncWord, HEX);
    if (c.syncWord == 0x12)      Serial1.println(" (Priv)");
    else if (c.syncWord == 0x34) Serial1.println(" (Pub)");
    else                         Serial1.println();

    Serial1.print("  Header:      ");
    if (c.implicitHeader) 
    {
        Serial1.print("Implicit (");
        Serial1.print(c.implicitPayloadLen);
        Serial1.println(")");
    } 
    else 
    {
        Serial1.println("Explicit");
    }

    Serial1.print("  LDRO:        ");
    if (c.ldroMode == LDRO_AUTO) Serial1.println("AUTO");
    else if (c.ldroMode == LDRO_ON) Serial1.println("ON");
    else Serial1.println("OFF");

    Serial1.print("  SymTimeout:  ");
    Serial1.println(c.symbolTimeout);

    Serial1.print("  RX Gain:     ");
    Serial1.println(c.rxBoosted ? "Boosted" : "Normal");

    Serial1.print("  Standby:     ");
    Serial1.println(c.standbyXosc ? "XOSC" : "RC");

    Serial1.print("  Regulator:   ");
    Serial1.println(c.regulatorDcdc ? "DC-DC" : "LDO");

    if (c.modem == MODEM_GFSK) 
    {
        Serial1.print("  GFSK BR:     ");
        Serial1.print(c.gfskBitrate);
        Serial1.println(" bps");

        Serial1.print("  GFSK Fdev:   ");
        Serial1.print(c.gfskFdev);
        Serial1.println(" Hz");

        Serial1.print("  GFSK BW:     0x");
        if (c.gfskBw < 0x10) Serial1.print('0');
        Serial1.println(c.gfskBw, HEX);

        Serial1.print("  Whitening:   ");
        Serial1.println(c.gfskWhiteningOn ? "ON" : "OFF");
    }

    Serial1.print("  State:       ");
    switch (appMode) {
        case MODE_IDLE:      
        Serial1.println("IDLE");      
        break;
        
        case MODE_SNIFFING:  
        Serial1.println("SNIFFING");  
        break;
        
        case MODE_BEACONING: 
        Serial1.println("BEACONING"); 
        break;
        
        case MODE_ANALYZE:   
        Serial1.println("ANALYZE");   
        break;
        
        case MODE_CHAT:      
        Serial1.println("CHAT");      
        break;
    }

    Serial1.print("  Packets RX:  ");
    Serial1.println(radio.getPacketCount());
    Serial1.print("  CRC Errors:  ");
    Serial1.println(radio.getCrcErrorCount());
}

static void cmdRssi() 
{
    int16_t rssi = radio.readRssi();
    Serial1.print("[RSSI] "); Serial1.print(rssi); Serial1.println(" dBm");
}

static void cmdReset() 
{
    appMode = MODE_IDLE;
    uint32_t freq = radio.getConfig().frequencyHz;
    Serial1.println("[RESET] Re-initialising radio...");
    radio.begin(freq);
}

// ═════════════════════════════════════════════════════════════════════════════
//  New System Commands
// ═════════════════════════════════════════════════════════════════════════════

static void cmdClear(void) 
{
    Serial1.print("\033[2J\033[H");
}

static void cmdUptime(void) 
{
    uint32_t ms  = millis();
    uint32_t sec = ms / 1000;
    uint32_t min = sec / 60;
    uint32_t hrs = min / 60;
    uint32_t days = hrs / 24;
    Serial1.print("[UPTIME] ");
    if (days > 0) 
    {
      Serial1.print(days); 
      Serial1.print("d "); 
    }
    Serial1.print(hrs % 24);  
    Serial1.print("h ");
    Serial1.print(min % 60);  
    Serial1.print("m ");
    Serial1.print(sec % 60);  
    Serial1.println("s");
}

static void cmdVersion(void) 
{
    Serial1.println();
    Serial1.println("+-------------- System Info -----------------+");
    Serial1.println("| Firmware:    STM32 LoRa Terminal v1.0");
    Serial1.print(  "| Built:       "); Serial1.print(__DATE__); 
    Serial1.print(" "); 
    Serial1.println(__TIME__);
    Serial1.println("| MCU:         STM32F103C8T6");
    Serial1.print(  "| Clock:       "); 
    Serial1.print(SystemCoreClock / 1000000); 
    Serial1.println(" MHz");
    Serial1.println("| Radio:       SX1262 (DX-LR20)");
    Serial1.println("| SPI:         SPI1 (PA5/PA6/PA7)");
    Serial1.println("| Serial:      UART1 (PA9/PA10) 115200");
    Serial1.println("+----------------------------------------------+");
}

static void cmdSleep(void) 
{
    if (appMode != MODE_IDLE) 
    {
        radio.goStandby();
        appMode = MODE_IDLE;
    }
    radio.goStandby();
    Serial1.println("[SLEEP] Radio in standby (low power).");
    Serial1.println("        Use 'reset' to re-init or any TX/RX command to resume.");
}

static void cmdReboot() 
{
    Serial1.println("[REBOOT] Resetting MCU...");
    Serial1.flush();
    delay(100);
    NVIC_SystemReset();
}
static void cmdScan(const char* arg) 
{
    uint32_t savedFreq = radio.getConfig().frequencyHz;

    if (appMode != MODE_IDLE) 
    {
        radio.goStandby();
        appMode = MODE_IDLE;
    }

    Serial1.println();
    Serial1.println("[SCAN] Starting frequency scan...");
    Serial1.println("       Press 'q' or Ctrl+C to abort.");

    uint32_t t0 = millis();
    bool completed = true;

    if (*arg == '\0' || strcasecmp(arg, "all") == 0) 
    {
        // Default: scan the three main ISM bands
        completed = scanFreqRange(430.0f, 440.0f, 200.0f, "ISM 433 MHz");
        if (completed) completed = scanFreqRange(863.0f, 870.0f, 200.0f, "ISM 868 MHz");
        if (completed) completed = scanFreqRange(902.0f, 928.0f, 500.0f, "ISM 915 MHz");
    } 
    else if (strcasecmp(arg, "433") == 0) 
    {
        completed = scanFreqRange(430.0f, 440.0f, 100.0f, "ISM 433 MHz");
    } 
    else if (strcasecmp(arg, "868") == 0) 
    {
        completed = scanFreqRange(863.0f, 870.0f, 100.0f, "ISM 868 MHz");
    } 
    else if (strcasecmp(arg, "915") == 0) 
    {
        completed = scanFreqRange(902.0f, 928.0f, 250.0f, "ISM 915 MHz");
    } 
    else 
    {
        char* p1;
        float startMHz = strtof(arg, &p1);
        while (*p1 == ' ') p1++;
        char* p2;
        float endMHz = strtof(p1, &p2);
        while (*p2 == ' ') p2++;
        float stepKHz = 200.0f;
        if (*p2 != '\0') stepKHz = strtof(p2, nullptr);

        if (startMHz < 150.0f || endMHz > 960.0f ||
            startMHz >= endMHz || stepKHz < 1.0f) 
        {
            Serial1.println("[ERR] Usage: scan <startMHz> <endMHz> [stepKHz]");
            Serial1.println("       Range 150-960 MHz, step >= 1 kHz");
            radio.setFrequency(savedFreq);
            return;
        }

        completed = scanFreqRange(startMHz, endMHz, stepKHz, "Custom");
    }

    uint32_t elapsed = millis() - t0;
    Serial1.println();
    Serial1.print("[SCAN] ");
    Serial1.print(completed ? "Complete" : "Aborted");
    Serial1.print(" in ");
    Serial1.print(elapsed);
    Serial1.println(" ms");

    radio.setFrequency(savedFreq);
    Serial1.print("[SCAN] Restored freq: ");
    Serial1.print(savedFreq / 1e6f, 3);
    Serial1.println(" MHz");
}

static void cmdScanPreset(const char* arg) 
{
    if (*arg == '\0') 
    {
        Serial1.println("[ERR] Usage: scanpreset <433|868|915|all>");
        return;
    }
    cmdScan(arg);
}

static void cmdAnalyze(const char* arg) 
{
    uint32_t startHz = 0, endHz = 0, stepHz = 0;
    if (!parseAnalyzeArgs(arg, startHz, endHz, stepHz)) 
    {
        Serial1.println("[ERR] Usage: analyze [startMHz endMHz [stepKHz]]");
        Serial1.println("      Example: analyze 432 434 25");
        return;
    }

    if (appMode != MODE_IDLE) radio.goStandby();

    analyzePrevFreq = radio.getConfig().frequencyHz;
    analyzeStartHz = startHz;
    analyzeEndHz = endHz;
    analyzeStepHz = stepHz;
    analyzeNeedsClear = true;
    analyzeHoldPeakRssi = -200;
    analyzeHoldPeakFreq = analyzeStartHz;
    lastAnalyzeFrame = 0;
    appMode = MODE_ANALYZE;
}

static void cmdAnalyzeCfg(const char* arg) 
{
    char key[16] = {};
    char val[16] = {};
    int n = sscanf(arg, "%15s %15s", key, val);
    if (n < 2) 
    {
        Serial1.println("[ERR] Usage: analyzecfg <peak|threshold> <value>");
        return;
    }

    if (strcasecmp(key, "peak") == 0) 
    {
        if (strcasecmp(val, "on") == 0) analyzePeakHold = true;
        else if (strcasecmp(val, "off") == 0) analyzePeakHold = false;
        else 
        {
          Serial1.println("[ERR] analyzecfg peak <on|off>"); 
          return; 
        }
        Serial1.print("[CFG] Analyze peak hold: "); 
        Serial1.println(analyzePeakHold ? "ON" : "OFF");
    } 
    else if (strcasecmp(key, "threshold") == 0) 
    {
        int t = atoi(val);
        if (t < -130 || t > -20) 
        {
            Serial1.println("[ERR] analyzecfg threshold <-130..-20>");
            return;
        }
        analyzeThresholdDbm = (int16_t)t;
        Serial1.print("[CFG] Analyze threshold: "); Serial1.println(analyzeThresholdDbm);
    } 
    else 
    {
        Serial1.println("[ERR] Unknown analyzecfg key");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Per-Command Help System
// ═════════════════════════════════════════════════════════════════════════════

static void showCmdHelp(const char* cmd) 
{
    Serial1.println();
    if (strcasecmp(cmd, "send") == 0) 
    {
        Serial1.println("send <message>");
        Serial1.println("Transmits ASCII text as one packet.");
        Serial1.println("If sniff is active, RX resumes automatically after TX.");
    } 
    else if (strcasecmp(cmd, "sendhex") == 0) 
    {
        Serial1.println("sendhex <hexbytes>");
        Serial1.println("Transmits raw bytes. Two hex chars = one byte.");
        Serial1.println("Example: sendhex 48656C6C6F");
    } 
    else if (strcasecmp(cmd, "sniff") == 0) 
    {
        Serial1.println("sniff");
        Serial1.println("Continuous receive monitor with RSSI/SNR and payload dump.");
    } 
    else if (strcasecmp(cmd, "beacon") == 0) 
    {
        Serial1.println("beacon <interval_ms> <message>");
        Serial1.println("Sends the same message periodically until stopped.");
    } 
    else if (strcasecmp(cmd, "stop") == 0) 
    {
        Serial1.println("stop");
        Serial1.println("Stops active mode: sniff, beacon, analyze, or chat.");
    } 
    else if (strcasecmp(cmd, "scan") == 0) 
    {
        Serial1.println("scan [startMHz endMHz stepKHz] | 433 | 868 | 915 | all");
        Serial1.println("Examples: scan 430 440 50, scan 433, scan all");
    } 
    else if (strcasecmp(cmd, "scanpreset") == 0) 
    {
        Serial1.println("scanpreset <433|868|915|all>");
    } 
    else if (strcasecmp(cmd, "meshlisten") == 0) 
    {
        Serial1.println("meshlisten <preset> [region|MHz]");
        Serial1.println("  preset: longfast longslow shortfast");
        Serial1.println("  region: us eu868 eu433 cn jp anz kr tw in ru");
        Serial1.println("  Default region: us (906.875 MHz)");
        Serial1.println("  Or specify MHz directly: meshlisten longfast 869.525");
    } 
    else if (strcasecmp(cmd, "meshkey") == 0) 
    {
        Serial1.println("meshkey <32hex>|default");
        Serial1.println("Sets AES key for Meshtastic decryption.");
    } 
    else if (strcasecmp(cmd, "analyze") == 0) 
    {
        Serial1.println("analyze [startMHz endMHz stepKHz] | 433 | 868 | 915");
        Serial1.println("Shows per-frequency RSSI table, threshold markers, and peaks.");
        Serial1.println("Examples: analyze, analyze 433, analyze 432 434 25");
    } 
    else if (strcasecmp(cmd, "analyzecfg") == 0) 
    {
        Serial1.println("analyzecfg peak on|off");
        Serial1.println("analyzecfg threshold -95");
        Serial1.println("Threshold range: -130..-20 dBm");
    } 
    else if (strcasecmp(cmd, "modem") == 0) 
    {
        Serial1.println("modem <lora|gfsk>");
        Serial1.println("Switches packet type and applies modem-specific params.");
    } 
    else if (strcasecmp(cmd, "bitrate") == 0) 
    {
        Serial1.println("bitrate <600..300000>");
    } 
    else if (strcasecmp(cmd, "fdev") == 0) 
    {
        Serial1.println("fdev <100..200000>");
    } 
    else if (strcasecmp(cmd, "fskbw") == 0) 
    {
        Serial1.println("fskbw <hexcode>");
    } 
    else if (strcasecmp(cmd, "whitening") == 0) 
    {
        Serial1.println("whitening <on|off>");
    } 
    else if (strcasecmp(cmd, "header") == 0) 
    {
        Serial1.println("header <explicit|implicit [len]>");
    } 
    else if (strcasecmp(cmd, "ldro") == 0) 
    {
        Serial1.println("ldro <auto|on|off>");
    } 
    else if (strcasecmp(cmd, "symtimeout") == 0) 
    {
        Serial1.println("symtimeout <0..255>");
    } 
    else if (strcasecmp(cmd, "rxboost") == 0) 
    {
        Serial1.println("rxboost <on|off>");
    } 
    else if (strcasecmp(cmd, "standby") == 0) 
    {
        Serial1.println("standby <xosc|rc>");
    } 
    else if (strcasecmp(cmd, "regulator") == 0) 
    {
        Serial1.println("regulator <dcdc|ldo>");
    } 
    else if (strcasecmp(cmd, "syncword") == 0) 
    {
        Serial1.println("syncword <hex>");
        Serial1.println("Common values: 12 (private), 34 (public)");
    } 
    else if (strcasecmp(cmd, "freq") == 0) 
    {
        Serial1.println("freq <MHz>");
    } 
    else if (strcasecmp(cmd, "sf") == 0) 
    {
        Serial1.println("sf <5..12>");
    } 
    else if (strcasecmp(cmd, "bw") == 0) 
    {
        Serial1.println("bw <kHz>");
    } 
    else if (strcasecmp(cmd, "cr") == 0) 
    {
        Serial1.println("cr <5..8>");
    } 
    else if (strcasecmp(cmd, "power") == 0) 
    {
        Serial1.println("power <-9..22>");
    } 
    else if (strcasecmp(cmd, "preamble") == 0) 
    {
        Serial1.println("preamble <1..65535>");
    } 
    else if (strcasecmp(cmd, "crc") == 0 || strcasecmp(cmd, "iq") == 0) 
    {
        Serial1.print(cmd); Serial1.println(" <on|off>");
    } 
    else if (strcasecmp(cmd, "send") == 0) 
    {
        Serial1.println("send <message>");
        Serial1.println("Sends ASCII text as payload.");
    } 
    else if (strcasecmp(cmd, "sendhex") == 0) 
    {
        Serial1.println("sendhex <hexbytes>");
        Serial1.println("Each two hex chars are one payload byte.");
    } 
    else if (strcasecmp(cmd, "beacon") == 0) 
    {
        Serial1.println("beacon <interval_ms> <message>");
    } 
    else if (strcasecmp(cmd, "chatjoin") == 0) 
    {
        Serial1.println("chatjoin <room 0..255> [hexkey]");
        Serial1.println("Join encrypted room and enter chat listen mode.");
        Serial1.println("If key is omitted, a key is auto-generated and printed.");
        Serial1.println("Provided key can be 8..32 hex chars (even length).");
        Serial1.println("All nodes must use the same room and effective key.");
    } 
    else if (strcasecmp(cmd, "chatleave") == 0) 
    {
        Serial1.println("chatleave");
        Serial1.println("Leaves room and clears key material from RAM.");
    } 
    else if (strcasecmp(cmd, "chatnick") == 0) 
    {
        Serial1.println("chatnick <name>");
        Serial1.println("Sets display nickname for chat messages.");
    } 
    else if (strcasecmp(cmd, "chat") == 0) 
    {
        Serial1.println("chat <message>");
        Serial1.println("Encrypts and sends chat message to joined room.");
        Serial1.println("In chat mode, you can also type message text directly.");
    } 
    else if (strcasecmp(cmd, "chatstatus") == 0) 
    {
        Serial1.println("chatstatus");
        Serial1.println("Shows joined state, room, nickname, and sender ID.");
    } 
    else if (strcasecmp(cmd, "status") == 0) 
    {
        Serial1.println("status");
        Serial1.println("Prints full modem and runtime configuration.");
    } 
    else if (strcasecmp(cmd, "rssi") == 0) 
    {
        Serial1.println("rssi");
        Serial1.println("Reads instantaneous RSSI on current channel.");
    } 
    else if (strcasecmp(cmd, "reset") == 0) 
    {
        Serial1.println("reset");
        Serial1.println("Re-initializes the radio with current frequency.");
    } 
    else if (strcasecmp(cmd, "uptime") == 0) 
    {
        Serial1.println("uptime");
        Serial1.println("Shows MCU uptime in days/hours/minutes/seconds.");
    } 
    else if (strcasecmp(cmd, "version") == 0 || strcasecmp(cmd, "ver") == 0) 
    {
        Serial1.println("version | ver");
        Serial1.println("Shows firmware version and board information.");
    } 
    else if (strcasecmp(cmd, "sleep") == 0) 
    {
        Serial1.println("sleep");
        Serial1.println("Puts radio into standby (low power, MCU still active).");
    } 
    else if (strcasecmp(cmd, "clear") == 0 || strcasecmp(cmd, "cls") == 0) 
    {
        Serial1.println("clear | cls");
        Serial1.println("Clears terminal screen with ANSI escape codes.");
    } 
    else if (strcasecmp(cmd, "reboot") == 0) 
    {
        Serial1.println("reboot");
        Serial1.println("Software reset of MCU.");
    } 
    else if (strcasecmp(cmd, "help") == 0) 
    {
        Serial1.println("help");
        Serial1.println("Shows command groups and quick usage tips.");
    } 
    else 
    {
        Serial1.println("See: help");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Command Router
// ═════════════════════════════════════════════════════════════════════════════

static void processCommand(char* line) 
{
    char originalLine[sizeof(cmdBuf)] = {};
    strncpy(originalLine, line, sizeof(originalLine) - 1);

    while (*line == ' ') line++;
    if (*line == '\0') return;

    char* arg = line;
    while (*arg && *arg != ' ') arg++;
    if (*arg) { *arg = '\0'; arg++; }
    while (*arg == ' ') arg++;

    if (strcmp(arg, "-help") == 0 || strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) 
    {
        showCmdHelp(line);
        prompt();
        return;
    }

    if      (strcasecmp(line, "help")     == 0) cmdHelp();
    else if (strcasecmp(line, "send")     == 0) cmdSend(arg);
    else if (strcasecmp(line, "sendhex")  == 0) cmdSendHex(arg);
    else if (strcasecmp(line, "sniff")    == 0) cmdSniff();
    else if (strcasecmp(line, "beacon")   == 0) cmdBeacon(arg);
    else if (strcasecmp(line, "stop")     == 0) cmdStop();
    else if (strcasecmp(line, "freq")     == 0) cmdFreq(arg);
    else if (strcasecmp(line, "sf")       == 0) cmdSF(arg);
    else if (strcasecmp(line, "bw")       == 0) cmdBW(arg);
    else if (strcasecmp(line, "cr")       == 0) cmdCR(arg);
    else if (strcasecmp(line, "power")    == 0) cmdPower(arg);
    else if (strcasecmp(line, "preamble") == 0) cmdPreamble(arg);
    else if (strcasecmp(line, "modem")    == 0) cmdModem(arg);
    else if (strcasecmp(line, "bitrate")  == 0) cmdBitrate(arg);
    else if (strcasecmp(line, "fdev")     == 0) cmdFdev(arg);
    else if (strcasecmp(line, "fskbw")    == 0) cmdFskBw(arg);
    else if (strcasecmp(line, "whitening") == 0) cmdWhitening(arg);
    else if (strcasecmp(line, "header")   == 0) cmdHeader(arg);
    else if (strcasecmp(line, "ldro")     == 0) cmdLdro(arg);
    else if (strcasecmp(line, "symtimeout") == 0) cmdSymTimeout(arg);
    else if (strcasecmp(line, "rxboost")  == 0) cmdRxBoost(arg);
    else if (strcasecmp(line, "standby")  == 0) cmdStandby(arg);
    else if (strcasecmp(line, "regulator") == 0) cmdRegulator(arg);
    else if (strcasecmp(line, "crc")      == 0) cmdCrc(arg);
    else if (strcasecmp(line, "iq")       == 0) cmdIQ(arg);
    else if (strcasecmp(line, "syncword") == 0) cmdSyncWord(arg);
    else if (strcasecmp(line, "status")   == 0) cmdStatus();
    else if (strcasecmp(line, "rssi")     == 0) cmdRssi();
    else if (strcasecmp(line, "reset")    == 0) cmdReset();
    else if (strcasecmp(line, "scan")     == 0) cmdScan(arg);
    else if (strcasecmp(line, "scanpreset") == 0) cmdScanPreset(arg);
    else if (strcasecmp(line, "meshlisten") == 0) cmdMeshListen(arg);
    else if (strcasecmp(line, "meshkey")  == 0) cmdMeshKey(arg);
    else if (strcasecmp(line, "analyze")  == 0) cmdAnalyze(arg);
    else if (strcasecmp(line, "analyzecfg") == 0) cmdAnalyzeCfg(arg);
    else if (strcasecmp(line, "chatjoin") == 0) cmdChatJoin(arg);
    else if (strcasecmp(line, "chatleave") == 0) cmdChatLeave();
    else if (strcasecmp(line, "chatnick") == 0) cmdChatNick(arg);
    else if (strcasecmp(line, "chat") == 0) cmdChatSend(arg);
    else if (strcasecmp(line, "chatstatus") == 0) cmdChatStatus();
    else if (strcasecmp(line, "clear")    == 0) cmdClear();
    else if (strcasecmp(line, "cls")      == 0) cmdClear();
    else if (strcasecmp(line, "uptime")   == 0) cmdUptime();
    else if (strcasecmp(line, "version")  == 0) cmdVersion();
    else if (strcasecmp(line, "ver")      == 0) cmdVersion();
    else if (strcasecmp(line, "sleep")    == 0) cmdSleep();
    else if (strcasecmp(line, "reboot")   == 0) cmdReboot();
    else 
    {
        if (appMode == MODE_CHAT && chatJoined) 
        {
            cmdChatSend(originalLine);
        } 
        else 
        {
            Serial1.print("[ERR] Unknown command: ");
            Serial1.println(line);
            Serial1.println("      Type 'help' for a list of commands");
        }
    }

    prompt();
}

static void readSerial() 
{
    while (Serial1.available()) 
    {
        char c = Serial1.read();

        if (c == '\n' || c == '\r') 
        {
            if (cmdLen > 0) 
            {
                cmdBuf[cmdLen] = '\0';
                processCommand(cmdBuf);
                cmdLen = 0;
            }
        }
        else if (c == 0x03) 
        {
            if (appMode != MODE_IDLE) 
            {
                Serial1.println("^C");
                cmdStop();
                cmdLen = 0;
                prompt();
            } 
            else 
            {
                Serial1.println("^C");
                cmdLen = 0;
                prompt();
            }
        }
        else if ((c == 'q' || c == 'Q') && appMode == MODE_ANALYZE) 
        {
            cmdStop();
            cmdLen = 0;
            prompt();
        }
        else if (c == 0x0C) 
        {
            Serial1.print("\033[2J\033[H");
            prompt();
        }
        else if (c == '\b' || c == 0x7F) 
        {
            if (cmdLen > 0) 
            {
                cmdLen--;
                Serial1.print("\b \b");
            }
        }
        else if (cmdLen < sizeof(cmdBuf) - 1) 
        {
            cmdBuf[cmdLen++] = c;
            Serial1.print(c);
        }
    }
}
