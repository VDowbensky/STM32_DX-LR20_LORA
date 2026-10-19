static bool     chatJoined = false;
static uint8_t  chatRoomId = 0;
static char     chatNick[13] = "node";
static uint8_t  chatKeyEnc[16] = {0};
static uint8_t  chatKeyMac[16] = {0};
static uint16_t chatSenderId = 1;
static uint32_t chatSeq = 0;
static uint32_t lastChatStatus = 0;

static const uint8_t CHAT_MAGIC0 = 0xC7;
static const uint8_t CHAT_MAGIC1 = 0xA7;
static const uint8_t CHAT_MAX_MSG = 120;
static const uint8_t CHAT_MAX_PEERS = 8;

struct ChatPeerState 
{
  uint16_t senderId;
  uint32_t lastSeq;
  bool used;
};

static ChatPeerState chatPeers[CHAT_MAX_PEERS];

void deriveMacKey(const uint8_t enc[16], uint8_t mac[16]) 
{
  static const uint8_t salt[16] = 
  {
    0x3A,0x7C,0x11,0xE2,0x94,0x57,0x26,0xC1,
    0x5D,0xB0,0x48,0xF3,0x9E,0x62,0x17,0xAD
  };
  for (uint8_t i = 0; i < 16; i++) mac[i] = enc[i] ^ salt[i];
}

static int8_t findPeerSlot(uint16_t senderId) 
{
  for (uint8_t i = 0; i < CHAT_MAX_PEERS; i++) 
  {
    if (chatPeers[i].used && chatPeers[i].senderId == senderId) return (int8_t)i;
  }
  for (uint8_t i = 0; i < CHAT_MAX_PEERS; i++) 
  {
    if (!chatPeers[i].used) return (int8_t)i;
  }
  return -1;
}

bool acceptSeq(uint16_t senderId, uint32_t seq) 
{
  int8_t slot = findPeerSlot(senderId);
  if (slot < 0) return false;
  if (!chatPeers[slot].used) 
  {
    chatPeers[slot].used = true;
    chatPeers[slot].senderId = senderId;
    chatPeers[slot].lastSeq = seq;
    return true;
  }
  if (seq <= chatPeers[slot].lastSeq) return false;
  chatPeers[slot].lastSeq = seq;
  return true;
}

uint8_t buildChatPacket(const char* msg, uint8_t* out) 
{
  uint8_t msgLen = (uint8_t)strlen(msg);
  if (msgLen == 0 || msgLen > CHAT_MAX_MSG) return 0;
  uint8_t nickLen = (uint8_t)strlen(chatNick);
  if (nickLen > 12) nickLen = 12;
  uint8_t plain[160];
  uint8_t plainLen = 0;
  plain[plainLen++] = nickLen;
  memcpy(&plain[plainLen], chatNick, nickLen);
  plainLen += nickLen;
  plain[plainLen++] = msgLen;
  memcpy(&plain[plainLen], msg, msgLen);
  plainLen += msgLen;
  out[0] = CHAT_MAGIC0;
  out[1] = CHAT_MAGIC1;
  out[2] = chatRoomId;
  out[3] = (uint8_t)(chatSenderId >> 8);
  out[4] = (uint8_t)(chatSenderId & 0xFF);
  out[5] = (uint8_t)(chatSeq >> 24);
  out[6] = (uint8_t)(chatSeq >> 16);
  out[7] = (uint8_t)(chatSeq >> 8);
  out[8] = (uint8_t)(chatSeq);
  out[9] = plainLen;
  memcpy(&out[10], plain, plainLen);
  uint64_t nonce = ((uint64_t)chatRoomId << 56) | ((uint64_t)chatSenderId << 40) | (uint64_t)chatSeq;
  xteaCtrCrypt(&out[10], plainLen, chatKeyEnc, nonce);
  uint64_t tag = sipHash24(&out[2], (uint16_t)(8 + plainLen), chatKeyMac);
  writeU64LE(&out[10 + plainLen], tag);
  return (uint8_t)(10 + plainLen + 8);
}

bool parseChatPacket(const uint8_t* in, uint8_t len, char* nickOut, uint8_t nickOutLen, char* msgOut, uint8_t msgOutLen) 
{
  if (len < 18) return false;
  if (in[0] != CHAT_MAGIC0 || in[1] != CHAT_MAGIC1) return false;
  if (!chatJoined) return false;
  if (in[2] != chatRoomId) return false;
  uint16_t senderId = ((uint16_t)in[3] << 8) | in[4];
  uint32_t seq = ((uint32_t)in[5] << 24) | ((uint32_t)in[6] << 16) | ((uint32_t)in[7] << 8) | in[8];
  uint8_t cLen = in[9];
  if ((uint16_t)(10 + cLen + 8) != len) return false;
  if (senderId == chatSenderId) return false;
  uint64_t tagCalc = sipHash24(&in[2], (uint16_t)(8 + cLen), chatKeyMac);
  uint64_t tagRx = readU64LE(&in[10 + cLen]);
  if (tagCalc != tagRx) return false;
  if (!acceptSeq(senderId, seq)) return false;
  uint8_t plain[160];
  memcpy(plain, &in[10], cLen);
  uint64_t nonce = ((uint64_t)in[2] << 56) | ((uint64_t)senderId << 40) | (uint64_t)seq;
  xteaCtrCrypt(plain, cLen, chatKeyEnc, nonce);
  uint8_t idx = 0;
  if (idx >= cLen) return false;
  uint8_t nickLen = plain[idx++];
  if (nickLen > 12 || (uint16_t)(idx + nickLen + 1) > cLen) return false;
  uint8_t copyNick = (uint8_t)min((int)nickLen, (int)(nickOutLen - 1));
  memcpy(nickOut, &plain[idx], copyNick);
  nickOut[copyNick] = '\0';
  idx += nickLen;
  uint8_t msgLen = plain[idx++];
  if ((uint16_t)(idx + msgLen) > cLen) return false;
  uint8_t copyMsg = (uint8_t)min((int)msgLen, (int)(msgOutLen - 1));
  memcpy(msgOut, &plain[idx], copyMsg);
  msgOut[copyMsg] = '\0';
  return true;
}
