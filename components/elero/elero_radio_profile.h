#pragma once

#include "cc1101.h"
#include <cstddef>
#include <cstdint>

namespace esphome {
namespace elero {

struct RadioRegisterValue { uint8_t address; uint8_t value; };

// The existing Elero profile, not RadioLib's bootstrap configuration. Verify
// before SRX: automatic calibration may subsequently change FSCAL registers.
inline constexpr RadioRegisterValue ELERO_RADIO_PROFILE[] = {
    {CC1101_FSCTRL1, 0x08}, {CC1101_FSCTRL0, 0x00},
    {CC1101_MDMCFG4, 0x7B}, {CC1101_MDMCFG3, 0x83}, {CC1101_MDMCFG2, 0x13},
    {CC1101_MDMCFG1, 0x52}, {CC1101_MDMCFG0, 0xF8}, {CC1101_CHANNR, 0x00},
    {CC1101_DEVIATN, 0x43}, {CC1101_FREND1, 0xB6}, {CC1101_FREND0, 0x10},
    {CC1101_MCSM0, 0x18}, {CC1101_MCSM1, 0x3F}, {CC1101_FOCCFG, 0x1D},
    {CC1101_BSCFG, 0x1F}, {CC1101_AGCCTRL2, 0xC7}, {CC1101_AGCCTRL1, 0x00},
    {CC1101_AGCCTRL0, 0xB2}, {CC1101_FSCAL3, 0xEA}, {CC1101_FSCAL2, 0x2A},
    {CC1101_FSCAL1, 0x00}, {CC1101_FSCAL0, 0x1F}, {CC1101_FSTEST, 0x59},
    {CC1101_TEST2, 0x81}, {CC1101_TEST1, 0x35}, {CC1101_TEST0, 0x09},
    {CC1101_IOCFG0, 0x06}, {CC1101_PKTCTRL1, 0x8C}, {CC1101_PKTCTRL0, 0x45},
    {CC1101_ADDR, 0x00}, {CC1101_PKTLEN, 0x3C}, {CC1101_SYNC1, 0xD3}, {CC1101_SYNC0, 0x91},
};
constexpr uint8_t radio_profile_value(uint8_t address) {
  for (const auto &reg : ELERO_RADIO_PROFILE) if (reg.address == address) return reg.value;
  return 0;
}

static_assert(radio_profile_value(CC1101_MCSM1) == 0x3F, "CCA and RXOFF/TXOFF must retain the Elero profile");
static_assert((radio_profile_value(CC1101_MDMCFG1) & 0x80) == 0, "Airtime estimate assumes FEC disabled");
static_assert((radio_profile_value(CC1101_MDMCFG2) & 0x08) == 0, "Airtime estimate assumes Manchester disabled");
static_assert((radio_profile_value(CC1101_MDMCFG2) & 0x03) == 3, "Airtime estimate assumes repeated sync word");

inline constexpr uint8_t ELERO_PATABLE[] = {0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0};

// IO keeps all hardware calls on the owning task; the same procedure is tested
// with a fake register file. The caller must acquire verified IDLE first.
template<class IO> bool configure_radio_profile(IO &io, uint8_t freq2, uint8_t freq1, uint8_t freq0) {
  const RadioRegisterValue frequency[] = {
      {CC1101_FREQ2, freq2}, {CC1101_FREQ1, freq1}, {CC1101_FREQ0, freq0}};
  for (const auto &reg : frequency) io.write_reg(reg.address, reg.value);
  for (const auto &reg : ELERO_RADIO_PROFILE) {
    // CCA and RXOFF stay unchanged. RadioLib explicitly supplies TXOFF=RX.
    io.write_reg(reg.address, reg.address == CC1101_MCSM1 ? reg.value & ~0x03 : reg.value);
  }
  if (!io.enable_rx_after_tx()) return false;
  io.write_burst(CC1101_PATABLE, ELERO_PATABLE, sizeof(ELERO_PATABLE));
  for (const auto &reg : frequency) {
    const uint8_t actual = io.read_reg(reg.address);
    if (actual != reg.value) { io.mismatch(reg.address, reg.value, actual); return false; }
  }
  for (const auto &reg : ELERO_RADIO_PROFILE) {
    const uint8_t actual = io.read_reg(reg.address);
    if (actual != reg.value) { io.mismatch(reg.address, reg.value, actual); return false; }
  }
  uint8_t patable[sizeof(ELERO_PATABLE)];
  io.read_buf(CC1101_PATABLE, patable, sizeof(patable));
  for (size_t i = 0; i < sizeof(patable); ++i) {
    if (patable[i] != ELERO_PATABLE[i]) {
      io.mismatch(CC1101_PATABLE, ELERO_PATABLE[i], patable[i]);
      return false;
    }
  }
  return true;
}

}  // namespace elero
}  // namespace esphome
