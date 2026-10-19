#include "proto.h"

bool protoReadVarint(const uint8_t* data, uint8_t len, uint8_t& pos, uint64_t& out) 
{
  out = 0;
  uint8_t shift = 0;
  while (pos < len && shift < 64) 
  {
    uint8_t b = data[pos++];
    out |= ((uint64_t)(b & 0x7F)) << shift;
    if ((b & 0x80) == 0) return true;
    shift += 7;
  }
  return false;
}

bool protoSkipField(const uint8_t* data, uint8_t len, uint8_t& pos, uint8_t wireType) 
{
  uint64_t v = 0;
  switch (wireType) 
  {
    case 0:
    return protoReadVarint(data, len, pos, v);
    
    case 1:
    if (pos + 8 > len) return false;
    pos += 8;
    return true;
    
    case 2:
    if (!protoReadVarint(data, len, pos, v)) return false;
    if (v > (uint64_t)(len - pos)) return false;
    pos += (uint8_t)v;
    return true;
    
    case 5:
    if (pos + 4 > len) return false;
    pos += 4;
    return true;
    
    default:
    return false;
  }
}