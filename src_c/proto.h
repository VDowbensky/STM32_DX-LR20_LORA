#ifndef _PROTO_H_
#define _PROTO_H_

bool protoReadVarint(const uint8_t* data, uint8_t len, uint8_t& pos, uint64_t& out);
bool protoSkipField(const uint8_t* data, uint8_t len, uint8_t& pos, uint8_t wireType);

#endif
