#pragma once

/**
 * tcl_climate.h
 *
 * ESPHome Custom Component für TCL / Kesser / Pioneer / DAIZUKI Klimaanlagen
 * Angepasst für ESPHome 2026.6.x mit horizontalem Schwenkmodus (hswing)
 */

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/select/select.h"

#include <vector>

namespace esphome {
namespace tcl_climate {

// Abfrage-Intervall in ms (alle 5 Sekunden Status holen)
static const uint32_t POLL_INTERVAL_MS = 5000;

class TclClimate : public climate::Climate, public uart::UARTDevice, public Component {
 public:
  // ── ESPHome-Lifecycle ──────────────────────────────────────────────────────
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // ── Climate-Interface ─────────────────────────────────────────────────────
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;

  // ── Select-Widget-Setter (werden aus YAML per set_xxx aufgerufen) ─────────

  /** Vertikale Lamellensteuerung (template select im YAML) */
  void set_vswing_select(select::Select *sel) { this->vswing_select_ = sel; }

  /** Horizontale Lamellensteuerung (template select im YAML) */
  void set_hswing_select(select::Select *sel) { this->hswing_select_ = sel; }

 protected:
  // ── Interne Methoden ──────────────────────────────────────────────────────
  void send_get_command_();
  void send_set_command_();
  void parse_rx_buffer_();
  void handle_status_response_(const uint8_t *data, size_t len);

  // ── State ─────────────────────────────────────────────────────────────────
  std::vector<uint8_t> rx_buffer_;
  uint32_t             last_poll_ms_{0};

  // Aktuelle Lamellenposition (Protokoll-Bytes)
  uint8_t vane_vertical_pos_   {0x03};  // default: Fix mid (vertikal)
  uint8_t vane_horizontal_pos_ {0x83};  // default: Fix mid (horizontal)

  // Optional: Select-Widgets für fein-granulare Steuerung
  select::Select *vswing_select_{nullptr};
  select::Select *hswing_select_{nullptr};
};

}  // namespace tcl_climate
}  // namespace esphome
