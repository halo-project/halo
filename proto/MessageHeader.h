#pragma once

#include <cinttypes>
#include <arpa/inet.h>

namespace halo {
namespace hdr {

// NOTE: changing any of the existing numbers here breaks backwards compatibility.
enum MessageKind {
  None = 0,
  RawSample = 1
};

typedef uint64_t MessageHeader;

// std::static_assert(sizeof(enum MessageKind) <= sizeof(uint32_t));

inline void setMessageKind(MessageHeader &Hdr, enum MessageKind Kind) {
  Hdr = (Hdr & 0x00000000FFFFFFFF) | ((uint64_t)Kind) << 32;
}

inline enum MessageKind getMessageKind(const MessageHeader &Hdr) {
  return (enum MessageKind) (Hdr >> 32);
}

inline void setPayloadSize(MessageHeader &Hdr, uint32_t Size) {
  Hdr = (Hdr & 0xFFFFFFFF00000000) | ((uint64_t)Size);
}

inline uint32_t getPayloadSize(const MessageHeader &Hdr) {
  return (uint32_t) (Hdr & 0x00000000FFFFFFFF);
}

inline void decode(MessageHeader &Hdr) {
  uint64_t x = Hdr;
  // convert from NET to HOST byte ordering
  #if __LITTLE_ENDIAN__
    // flip bottom half and put in front
    x = ((uint64_t) ntohl(x & 0xFFFFFFFF)) << 32;
    // flip top half and put in back.
    x |= ntohl(x >> 32);
  #endif
  Hdr = x;
}

inline void encode(MessageHeader &Hdr) {
  uint64_t x = Hdr;
  // convert from HOST to NET byte ordering
  #if __LITTLE_ENDIAN__
    // flip bottom half and put in front
    x = ((uint64_t) htonl(x & 0xFFFFFFFF)) << 32;
    // flip top half and put in back.
    x |= htonl(x >> 32);
  #endif
  Hdr = x;
}

} // namespace hdr
} // namespace halo
