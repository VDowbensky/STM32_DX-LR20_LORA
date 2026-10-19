// Meshtastic OTA header: 4+4+4+1+1 = 14 bytes minimum

bool displayMeshtasticLite(const uint8_t* data, uint8_t len) 
{
  // Meshtastic OTA: binary header, NOT protobuf
  // Minimum packet: 14 header + 1 encrypted = 15 bytes
  if (len < 15) return false;
  // Parse binary header (all little-endian)
  uint32_t toNode, fromNode, pktId;
  memcpy(&toNode,   &data[0], 4);
  memcpy(&fromNode, &data[4], 4);
  memcpy(&pktId,    &data[8], 4);
  uint8_t flags   = data[12];
  uint8_t chHash  = data[13];
  uint8_t hopLimit = flags & 0x07;
  uint8_t hopStart = (flags >> 5) & 0x07;
  // Determine header size: try 16 first (v2), fall back to 14
  uint8_t hdrLen = 16;
  if (len < 17) hdrLen = 14;  // not enough for v2 header + payload
  uint8_t encLen = len - hdrLen;
  const uint8_t* encData = data + hdrLen;
  Serial1.println("  -- Mesh --");
  Serial1.print("  To:!"); Serial1.println(toNode, HEX);
  Serial1.print("  Fr:!"); Serial1.println(fromNode, HEX);
  Serial1.print("  ID:"); Serial1.println(pktId, HEX);
  Serial1.print("  Ch:"); Serial1.println(chHash, HEX);
  Serial1.print("  Hop:"); Serial1.print(hopLimit); Serial1.print("/"); Serial1.println(hopStart);
  // Decrypt
  uint8_t plain[240];
  if (encLen > sizeof(plain)) encLen = sizeof(plain);
  meshAesCtrDecrypt(meshAesKey, pktId, fromNode, encData, encLen, plain);
  // Parse decrypted protobuf Data message
  uint8_t pos = 0;
  uint32_t portNum = 0;
  bool hasPort = false;
  const uint8_t* appPayload = nullptr;
  uint8_t appPayloadLen = 0;
  while (pos < encLen) 
  {
    uint64_t key = 0;
    if (!protoReadVarint(plain, encLen, pos, key)) break;
    uint32_t field = (uint32_t)(key >> 3);
    uint8_t wire = (uint8_t)(key & 0x07);
    if (wire == 0) 
    {
      uint64_t v = 0;
      if (!protoReadVarint(plain, encLen, pos, v)) break;
      if (field == 1) 
      { 
        hasPort = true; 
        portNum = (uint32_t)v; 
      }
    } 
    else if (wire == 2) 
    {
      uint64_t l = 0;
      if (!protoReadVarint(plain, encLen, pos, l)) break;
      if (l > (uint64_t)(encLen - pos)) break;
      if (field == 2) 
      {
        appPayload = &plain[pos];
        appPayloadLen = (uint8_t)l;
      }
      pos += (uint8_t)l;
    } 
    else if (wire == 5) 
    {
      if (pos + 4 > encLen) break;
      pos += 4;
    } 
    else 
    {
      if (!protoSkipField(plain, encLen, pos, wire)) break;
    }
  }
  if (!hasPort) 
  {
    Serial1.println("  [NoDecrypt]");
    hexDump(plain, encLen);
    return true;
  }
  Serial1.print("  Port: "); Serial1.println(portNum);
  if (appPayload && appPayloadLen > 0) 
  {
    if (portNum == 1) 
    {
      Serial1.print("  Msg:  \"");
      for (uint8_t i = 0; i < appPayloadLen; i++) 
      {
        char c = (char)appPayload[i];
        Serial1.print((c >= 32 && c <= 126) ? c : '.');
      }
      Serial1.println("\"");
    } 
    else hexDump(appPayload, appPayloadLen);
  }
  return true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Sniff-Mode Packet Display
// ═════════════════════════════════════════════════════════════════════════════

void displayPacket(const PacketInfo &pkt) 
{
  float snrDb = (float)pkt.snr / 4.0f;
  Serial1.println();
  Serial1.print("RX #");
  Serial1.print(radio.getPacketCount());
  Serial1.print(" @ ");
  Serial1.print(pkt.timestamp / 1000.0f, 3);
  Serial1.println("s");
  if (pkt.crcError) Serial1.println("  *** CRC ERROR ***");
  Serial1.print("  RSSI:       "); 
  Serial1.print(pkt.rssi);       
  Serial1.println(" dBm");
  Serial1.print("  SNR:        "); 
  Serial1.print(snrDb, 1);       
  Serial1.println(" dB");
  Serial1.print("  Signal RSSI:"); 
  Serial1.print(pkt.signalRssi); 
  Serial1.println(" dBm");
  Serial1.print("  Length:     "); 
  Serial1.print(pkt.length);      
  Serial1.println(" bytes");
  if (pkt.length > 0) 
  {
    Serial1.println("  --- Hex Dump ---");
    hexDump(pkt.data, pkt.length);
    bool printable = true;
    for (uint8_t i = 0; i < pkt.length && printable; i++)
    {
      if (pkt.data[i] < 0x20 && pkt.data[i] != '\n' && pkt.data[i] != '\r') printable = false;
    }
    if (printable) 
    {
      Serial1.print("  ASCII:      ");
      Serial1.write(pkt.data, pkt.length);
      Serial1.println();
    }
    if (meshDecodeHint) displayMeshtasticLite(pkt.data, pkt.length);
  }
}
