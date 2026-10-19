// ═════════════════════════════════════════════════════════════════════════════
//  Utility Helpers
// ═════════════════════════════════════════════════════════════════════════════

void hexDump(const uint8_t* data, uint8_t len) 
{
  char tmp[8];
  for (uint16_t i = 0; i < len; i += 16) 
  {
    // Address
    snprintf(tmp, sizeof(tmp), "  %04X:", i);
    Serial1.print(tmp);
    // Hex bytes with mid-row separator
    for (uint8_t j = 0; j < 16; j++) 
    {
      if (j == 8) Serial1.print(' ');
      if (i + j < len) 
      {
        snprintf(tmp, sizeof(tmp), " %02X", data[i + j]);
        Serial1.print(tmp);
      } 
      else Serial1.print("   ");
    }
    // ASCII column
    Serial1.print("  ");
    for (uint8_t j = 0; j < 16 && (i + j) < len; j++) 
    {
      char c = (char)data[i + j];
      Serial1.print((c >= 32 && c <= 126) ? c : '.');
    }
    Serial1.println();
  }
}

uint8_t hexNibble(char c) 
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0xFF;
}

void prompt(void) 
{
  if (appMode == MODE_IDLE) Serial1.print("\n> ");
}

uint16_t deviceId16(void) 
{
  volatile const uint32_t* uid0 = (uint32_t*)0x1FFFF7E8;
  volatile const uint32_t* uid1 = (uint32_t*)0x1FFFF7EC;
  volatile const uint32_t* uid2 = (uint32_t*)0x1FFFF7F0;
  uint32_t mix = (*uid0) ^ (*uid1) ^ (*uid2);
  uint16_t id = (uint16_t)(mix ^ (mix >> 16));
  return id ? id : 1;
}

uint64_t rotl64(uint64_t x, uint8_t b) 
{
  return (x << b) | (x >> (64 - b));
}

uint64_t readU64LE(const uint8_t* p) 
{
  uint64_t v = 0;
  for (uint8_t i = 0; i < 8; i++) v |= ((uint64_t)p[i]) << (8 * i);
  return v;
}

void writeU64LE(uint8_t* p, uint64_t v) 
{
  for (uint8_t i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static void printHex16(const uint8_t key[16]) 
{
  for (uint8_t i = 0; i < 16; i++) 
  {
    if (key[i] < 0x10) Serial1.print('0');
    Serial1.print(key[i], HEX);
  }
  Serial1.println();
}
