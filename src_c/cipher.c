// ═════════════════════════════════════════════════════════════════════════════
//  AES-128 (compact, for Meshtastic decryption)
// ═════════════════════════════════════════════════════════════════════════════

// Meshtastic AES key (default = LongFast channel key)
static uint8_t meshAesKey[16] = 
{
  0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
  0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01
};

static bool meshKeyIsDefault = true;

uint8_t aes_sbox[256];  // computed at init, stored in RAM

void aes_init_sbox(void) 
{
  // Compute S-box using GF(2^8) log/antilog
  uint8_t p = 1, q = 1;
  do 
  {
    p ^= (uint8_t)((p << 1) ^ ((p & 0x80) ? 0x1B : 0));
    q ^= (uint8_t)(q << 1); q ^= (uint8_t)(q << 2);
    q ^= (uint8_t)(q << 4); q ^= (uint8_t)((q & 0x80) ? 0x09 : 0);
    uint8_t x = q ^ ((q<<1)|(q>>7)) ^ ((q<<2)|(q>>6)) ^ ((q<<3)|(q>>5)) ^ ((q<<4)|(q>>4));
    aes_sbox[p] = x ^ 0x63;
  } while (p != 1);
  aes_sbox[0] = 0x63;
}

uint8_t xtime(uint8_t x) 
{ 
  return (uint8_t)((x << 1) ^ ((x >> 7) * 0x1b)); 
}

void aes128_expandKey(const uint8_t key[16], uint8_t roundKeys[176]) 
{
  memcpy(roundKeys, key, 16);
  uint8_t rcon = 1;
  for (uint8_t i = 4; i < 44; i++) 
  {
    uint8_t temp[4];
    memcpy(temp, &roundKeys[(i - 1) * 4], 4);
    if (i % 4 == 0) 
    {
      uint8_t t = temp[0];
      temp[0] = aes_sbox[temp[1]] ^ rcon;
      temp[1] = aes_sbox[temp[2]];
      temp[2] = aes_sbox[temp[3]];
      temp[3] = aes_sbox[t];
      rcon = xtime(rcon);
    }
    for (uint8_t j = 0; j < 4; j++) roundKeys[i * 4 + j] = roundKeys[(i - 4) * 4 + j] ^ temp[j];
  }
}

void aes128_encryptBlock(const uint8_t roundKeys[176], const uint8_t in[16], uint8_t out[16]) 
{
  uint8_t s[16];
  for (uint8_t i = 0; i < 16; i++) s[i] = in[i] ^ roundKeys[i];

  for (uint8_t round = 1; round <= 10; round++) 
  {
    for (uint8_t i = 0; i < 16; i++) s[i] = aes_sbox[s[i]];
    // ShiftRows
    uint8_t t;
    t=s[1]; s[1]=s[5]; s[5]=s[9]; s[9]=s[13]; s[13]=t;
    t=s[2]; s[2]=s[10]; s[10]=t; t=s[6]; s[6]=s[14]; s[14]=t;
    t=s[15]; s[15]=s[11]; s[11]=s[7]; s[7]=s[3]; s[3]=t;
    // MixColumns (skip last round)
    if (round < 10) 
    {
      for (uint8_t c = 0; c < 16; c += 4) 
      {
        uint8_t a=s[c], b=s[c+1], d=s[c+2], e=s[c+3], x=a^b^d^e;
        s[c]^=x^xtime(a^b); s[c+1]^=x^xtime(b^d);
        s[c+2]^=x^xtime(d^e); s[c+3]^=x^xtime(e^a);
      }
    }
    const uint8_t* rk = &roundKeys[round * 16];
    for (uint8_t i = 0; i < 16; i++) s[i] ^= rk[i];
  }
  memcpy(out, s, 16);
}

void meshAesCtrDecrypt(const uint8_t key[16], uint32_t packetId, uint32_t fromNode,
                               const uint8_t* cipher, uint8_t cipherLen, uint8_t* plain) 
{
  uint8_t roundKeys[176];
  aes128_expandKey(key, roundKeys);

  uint8_t nonce[16] = {};
  memcpy(nonce, &packetId, 4);
  memcpy(nonce + 8, &fromNode, 4);

  for (uint8_t offset = 0; offset < cipherLen; offset += 16) 
  {
    uint8_t ks[16];
    aes128_encryptBlock(roundKeys, nonce, ks);
    uint8_t n = (cipherLen - offset > 16) ? 16 : (cipherLen - offset);
    for (uint8_t i = 0; i < n; i++) plain[offset + i] = cipher[offset + i] ^ ks[i];
    // Big-endian increment (matches mbedTLS AES-CTR)
    for (int8_t i = 15; i >= 0; i--) 
    { 
      if (++nonce[i]) break; 
    }
  }
}

void xteaEncryptBlock(uint32_t v[2], const uint32_t k[4]) 
{
  uint32_t v0 = v[0], v1 = v[1], sum = 0, delta = 0x9E3779B9;
  for (uint8_t i = 0; i < 32; i++) 
  {
    v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + k[sum & 3]);
    sum += delta;
    v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + k[(sum >> 11) & 3]);
  }
  v[0] = v0;
  v[1] = v1;
}

void xteaCtrCrypt(uint8_t* data, uint8_t len, const uint8_t key[16], uint64_t nonceBase) 
{
  uint32_t k[4];
  for (uint8_t i = 0; i < 4; i++) 
  {
      k[i] = ((uint32_t)key[i * 4 + 0] << 24) |
             ((uint32_t)key[i * 4 + 1] << 16) |
             ((uint32_t)key[i * 4 + 2] << 8) |
             ((uint32_t)key[i * 4 + 3]);
  }
  uint8_t offset = 0;
  uint64_t ctr = 0;
  while (offset < len) 
  {
    uint8_t ks[8];
    uint64_t block = nonceBase ^ ctr;
    writeU64LE(ks, block);
    uint32_t v[2] = 
    {
      ((uint32_t)ks[0] << 24) | ((uint32_t)ks[1] << 16) | ((uint32_t)ks[2] << 8) | ks[3],
      ((uint32_t)ks[4] << 24) | ((uint32_t)ks[5] << 16) | ((uint32_t)ks[6] << 8) | ks[7]
    };
    xteaEncryptBlock(v, k);
    ks[0] = (uint8_t)(v[0] >> 24); ks[1] = (uint8_t)(v[0] >> 16);
    ks[2] = (uint8_t)(v[0] >> 8);  ks[3] = (uint8_t)(v[0]);
    ks[4] = (uint8_t)(v[1] >> 24); ks[5] = (uint8_t)(v[1] >> 16);
    ks[6] = (uint8_t)(v[1] >> 8);  ks[7] = (uint8_t)(v[1]);
    uint8_t n = (uint8_t)min((int)(len - offset), 8);
    for (uint8_t i = 0; i < n; i++) data[offset + i] ^= ks[i];
    offset += n;
    ctr++;
  }
}

uint64_t sipHash24(const uint8_t* data, uint16_t len, const uint8_t key[16]) 
{
  uint64_t k0 = readU64LE(key);
  uint64_t k1 = readU64LE(key + 8);
  uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
  uint64_t v1 = 0x646f72616e646f6dULL ^ k1;
  uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
  uint64_t v3 = 0x7465646279746573ULL ^ k1;

  auto sipRound = [&]() 
  {
    v0 += v1; v1 = rotl64(v1, 13); v1 ^= v0; v0 = rotl64(v0, 32);
    v2 += v3; v3 = rotl64(v3, 16); v3 ^= v2;
    v0 += v3; v3 = rotl64(v3, 21); v3 ^= v0;
    v2 += v1; v1 = rotl64(v1, 17); v1 ^= v2; v2 = rotl64(v2, 32);
  };

  const uint8_t* p = data;
  uint16_t left = len;
  while (left >= 8) 
  {
    uint64_t m = readU64LE(p);
    v3 ^= m;
    sipRound(); sipRound();
    v0 ^= m;
    p += 8;
    left -= 8;
  }
  uint64_t b = ((uint64_t)len) << 56;
  for (uint8_t i = 0; i < left; i++) b |= ((uint64_t)p[i]) << (8 * i);
  v3 ^= b;
  sipRound(); sipRound();
  v0 ^= b;
  v2 ^= 0xff;
  sipRound(); sipRound(); sipRound(); sipRound();
  return v0 ^ v1 ^ v2 ^ v3;
}

bool parseHex16(const char* s, uint8_t out[16]) 
{
  if (strlen(s) != 32) return false;
  for (uint8_t i = 0; i < 16; i++) 
  {
    uint8_t hi = hexNibble(s[i * 2]);
    uint8_t lo = hexNibble(s[i * 2 + 1]);
    if (hi > 15 || lo > 15) return false;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

bool parseHexFlexible(const char* s, uint8_t out[16]) 
{
  size_t n = strlen(s);
  if (n < 8 || n > 32 || (n & 1) != 0) return false;
  uint8_t tmp[16] = {0};
  uint8_t bytes = (uint8_t)(n / 2);
  for (uint8_t i = 0; i < bytes; i++) 
  {
    uint8_t hi = hexNibble(s[i * 2]);
    uint8_t lo = hexNibble(s[i * 2 + 1]);
    if (hi > 15 || lo > 15) return false;
    tmp[i] = (uint8_t)((hi << 4) | lo);
  }
  for (uint8_t i = 0; i < 16; i++) out[i] = tmp[i % bytes] ^ (uint8_t)(0xA5 + i * 11);
  return true;
}

static uint32_t prngState = 0;

uint32_t xorshift32() 
{
  uint32_t x = prngState;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  prngState = x ? x : 0x6C8E9CF5UL;
  return prngState;
}

void generateChatKey(uint8_t out[16]) 
{
  if (prngState == 0) prngState = ((uint32_t)deviceId16() << 16) ^ millis() ^ micros() ^ 0xA341316CUL;
  for (uint8_t i = 0; i < 16; i += 4) 
  {
    uint32_t r = xorshift32();
    out[i + 0] = (uint8_t)(r >> 24);
    out[i + 1] = (uint8_t)(r >> 16);
    out[i + 2] = (uint8_t)(r >> 8);
    out[i + 3] = (uint8_t)r;
  }
}