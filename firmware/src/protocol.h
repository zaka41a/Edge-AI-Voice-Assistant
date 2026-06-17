#ifndef EDGE_AI_PROTOCOL_H
#define EDGE_AI_PROTOCOL_H

#include <cstdint>
#include <cstddef>

namespace proto {

enum Type : uint8_t {
    AUDIO_IN   = 0x01,
    AUDIO_OUT  = 0x02,
    TEXT       = 0x03,
    MOOD       = 0x04,
    SENSOR_REQ = 0x05,
    SENSOR_RSP = 0x06,
    CMD        = 0x07,
    ACK        = 0x08,
    LOG        = 0x09,
};

enum Mood : uint8_t { MOOD_NEUTRAL = 0, MOOD_HAPPY, MOOD_SAD, MOOD_ANGRY, MOOD_CURIOUS };

enum DState : uint8_t { ST_IDLE = 0, ST_LISTENING, ST_THINKING, ST_SPEAKING };

enum Cmd : uint8_t { CMD_STATE = 0x01, CMD_LED_RGB = 0x02 };

constexpr uint8_t  SYNC0       = 0xAA;
constexpr uint8_t  SYNC1       = 0x55;
constexpr uint16_t MAX_PAYLOAD = 1024;
constexpr size_t   OVERHEAD    = 8;

inline uint16_t crc16(const uint8_t *data, size_t n, uint16_t crc = 0xFFFF) {
    for (size_t i = 0; i < n; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

inline size_t encode(uint8_t type, uint8_t seq,
                     const uint8_t *payload, uint16_t len, uint8_t *out) {
    if (len > MAX_PAYLOAD) return 0;
    size_t i = 0;
    out[i++] = SYNC0;
    out[i++] = SYNC1;
    out[i++] = type;
    out[i++] = static_cast<uint8_t>(len & 0xFF);
    out[i++] = static_cast<uint8_t>(len >> 8);
    out[i++] = seq;
    for (uint16_t k = 0; k < len; ++k) out[i++] = payload[k];

    const uint16_t c = crc16(out + 2, static_cast<size_t>(4 + len));
    out[i++] = static_cast<uint8_t>(c & 0xFF);
    out[i++] = static_cast<uint8_t>(c >> 8);
    return i;
}

struct Frame {
    uint8_t  type;
    uint8_t  seq;
    uint16_t len;
    uint8_t  payload[MAX_PAYLOAD];
};

class Decoder {
public:
    bool feed(uint8_t b, Frame &out);
    uint32_t dropped() const { return _dropped; }

private:
    enum St { S_SYNC0, S_SYNC1, S_TYPE, S_LEN_LO, S_LEN_HI, S_SEQ,
              S_PAY, S_CRC_LO, S_CRC_HI };
    St       _st  = S_SYNC0;
    uint8_t  _type = 0, _seq = 0;
    uint16_t _len = 0, _idx = 0, _crc_rx = 0;
    uint8_t  _buf[MAX_PAYLOAD];
    uint32_t _dropped = 0;
};

inline bool Decoder::feed(uint8_t b, Frame &out) {
    switch (_st) {
    case S_SYNC0:
        if (b == SYNC0) _st = S_SYNC1; else ++_dropped;
        break;
    case S_SYNC1:
        if      (b == SYNC1) _st = S_TYPE;
        else if (b == SYNC0) {  }
        else { ++_dropped; _st = S_SYNC0; }
        break;
    case S_TYPE:   _type = b;                 _st = S_LEN_LO; break;
    case S_LEN_LO: _len  = b;                 _st = S_LEN_HI; break;
    case S_LEN_HI:
        _len |= static_cast<uint16_t>(b) << 8;
        if (_len > MAX_PAYLOAD) { ++_dropped; _st = S_SYNC0; }
        else { _idx = 0; _st = S_SEQ; }
        break;
    case S_SEQ:    _seq = b; _st = _len ? S_PAY : S_CRC_LO;  break;
    case S_PAY:
        _buf[_idx++] = b;
        if (_idx >= _len) _st = S_CRC_LO;
        break;
    case S_CRC_LO: _crc_rx = b;                _st = S_CRC_HI; break;
    case S_CRC_HI: {
        _crc_rx |= static_cast<uint16_t>(b) << 8;
        const uint8_t hdr[4] = { _type,
                                 static_cast<uint8_t>(_len & 0xFF),
                                 static_cast<uint8_t>(_len >> 8), _seq };
        uint16_t c = crc16(hdr, 4);
        c = crc16(_buf, _len, c);
        _st = S_SYNC0;
        if (c == _crc_rx) {
            out.type = _type; out.seq = _seq; out.len = _len;
            for (uint16_t k = 0; k < _len; ++k) out.payload[k] = _buf[k];
            return true;
        }
        ++_dropped;
        break;
    }
    }
    return false;
}

}

#endif
