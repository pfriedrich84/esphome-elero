#pragma once
#include "elero_rx_metadata.h"
#include <cstdint>

namespace esphome { namespace elero {

// CRC_AUTOFLUSH-safe FIFO ownership. Never consume a length byte while RX can
// still rewrite the FIFO pointers on a CRC failure. Wait for GDO0 packet end,
// freeze RX in verified IDLE, then read exactly one length + body + status at a
// time. A following in-flight packet delays the drain, not just its prefix.
// A race starting reception between the GPIO sample and SIDLE can leave an
// interrupted tail: discard that tail explicitly, never splice it to a future
// packet. Complete preceding packets still get delivered. No RF register tuning.
class RxFifoReader {
 public:
  enum class Result { READY, RECEIVING, RECOVERED, IO_ERROR };
  template<class IO>
  Result drain(IO &io, RxTimeline &timeline, bool keep_idle = false) {
    const uint32_t now = io.now();
    if (!this->pending_) {
      this->anchor_ = timeline.capture(now);
      this->pending_ = true;
    }
    if (io.packet_active()) {
      if (static_cast<uint32_t>(now - this->anchor_.received_at_ms) < 20)
        return Result::RECEIVING;
      // Bounded recovery of stuck GDO/incomplete reception. Still drain any
      // complete predecessor before discarding the interrupted trailing frame.
    }
    if (!io.enter_idle()) { io.resume_rx(); return Result::IO_ERROR; }
    uint8_t available = 0;
    if (!io.rx_bytes(available)) {
      io.resume_rx();  // failed TX preparation must not strand the radio in IDLE
      return Result::IO_ERROR;
    }
    if ((available & 0x80) != 0 || available > 64) {
      io.discard("fifo_overflow");
      this->pending_ = false;
      if (!keep_idle) io.resume_rx();
      return Result::RECOVERED;
    }
    Result result = Result::READY;
    while (available != 0) {  // frozen snapshot <=64 bytes, hence bounded
      uint8_t packet[64]{};
      io.read_fifo(packet, 1);
      --available;
      const uint16_t remainder = static_cast<uint16_t>(packet[0]) + 2;
      if (packet[0] < 17 || remainder > 63 || remainder > available) {
        io.discard(remainder > available ? "interrupted_rx_tail" : "invalid_fifo_length");
        result = Result::RECOVERED;
        break;
      }
      io.read_fifo(packet + 1, static_cast<uint8_t>(remainder));
      available -= remainder;
      auto meta = timeline.capture(now);
      if (this->pending_) {
        meta.epoch = this->anchor_.epoch;
        meta.received_at_ms = this->anchor_.received_at_ms;
      }
      io.packet(packet, static_cast<uint8_t>(remainder + 1), meta);
    }
    this->pending_ = false;
    if (!keep_idle) io.resume_rx();
    return result;
  }
  void reset() { this->pending_ = false; }
 private:
  bool pending_{false};
  RxMetadata anchor_{};
};
}}
