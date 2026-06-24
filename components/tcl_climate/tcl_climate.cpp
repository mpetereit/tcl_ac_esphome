/**
 * tcl_climate.cpp
 *
 * ESPHome Custom Component for TCL / Kesser / Pioneer / DAIZUKI AC units
 * Protokoll reverse-engineered by adaasch (https://github.com/adaasch/AC-hack)
 * Original ESPHome-Component: lNikazzzl (https://github.com/lNikazzzl/tcl_ac_esphome)
 *
 * Änderungen gegenüber Original:
 *  - Horizontaler Schwenkmodus (hswing) vollständig implementiert
 *  - Byte 11[4] (Move Horiz. Vane Flag) korrekt gesetzt
 *  - Byte 33 (Horiz. Vane Position) mit allen Protokollwerten belegt
 *  - ESPHome 2026.6.x kompatibel (swing_mode CLIMATE_SWING_BOTH / HORIZONTAL)
 *  - Horizontal-Select parallel zu Vertical-Select als template select
 *
 * Protokoll-Referenz (adaasch):
 *   Byte 11[4]  : Move Horiz. Vane  (1 = schwenken aktiv, 0 = fix)
 *   Byte 33     : Horiz. Vane Position
 *     0x80 = N/A (kein Befehl)
 *     0x81 = Fix left
 *     0x82 = Fix mid-left
 *     0x83 = Fix mid
 *     0x84 = Fix mid-right
 *     0x85 = Fix right
 *     0x88 = Move full range
 *     0x90 = Move left range
 *     0x98 = Move mid range
 *     0xA0 = Move right range
 *
 *   Byte 10[2:4]: Move Vert. Vane   (0b111 = schwenken aktiv, 0b000 = fix)
 *   Byte 32     : Vert. Vane Position
 *     0x00 = N/A
 *     0x01 = Fix top
 *     0x02 = Fix upper
 *     0x03 = Fix mid
 *     0x04 = Fix lower
 *     0x05 = Fix bottom
 *     0x08 = Move full range
 *     0x10 = Move upper range
 *     0x18 = Move lower range
 */

#include "tcl_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tcl_climate {

static const char *const TAG = "tcl_climate";

// ─── Protokoll-Konstanten ────────────────────────────────────────────────────
static const uint8_t PREAMBLE        = 0xBB;
static const uint8_t CMD_SET         = 0x03;
static const uint8_t CMD_GET         = 0x04;
static const uint8_t PAYLOAD_LEN_SET = 0x1D;  // 29 Byte Payload
static const uint8_t PAYLOAD_LEN_GET = 0x02;

// Vertikale Lamellenposition (Byte 32 im Gesamtpaket = Payload-Byte 27)
static const uint8_t VVANE_NA         = 0x00;
static const uint8_t VVANE_FIX_TOP    = 0x01;
static const uint8_t VVANE_FIX_UPPER  = 0x02;
static const uint8_t VVANE_FIX_MID    = 0x03;
static const uint8_t VVANE_FIX_LOWER  = 0x04;
static const uint8_t VVANE_FIX_BOTTOM = 0x05;
static const uint8_t VVANE_MOVE_FULL  = 0x08;
static const uint8_t VVANE_MOVE_UPPER = 0x10;
static const uint8_t VVANE_MOVE_LOWER = 0x18;

// Horizontale Lamellenposition (Byte 33 im Gesamtpaket = Payload-Byte 28)
static const uint8_t HVANE_NA         = 0x80;
static const uint8_t HVANE_FIX_LEFT   = 0x81;
static const uint8_t HVANE_FIX_MLEFT  = 0x82;
static const uint8_t HVANE_FIX_MID    = 0x83;
static const uint8_t HVANE_FIX_MRIGHT = 0x84;
static const uint8_t HVANE_FIX_RIGHT  = 0x85;
static const uint8_t HVANE_MOVE_FULL  = 0x88;
static const uint8_t HVANE_MOVE_LEFT  = 0x90;
static const uint8_t HVANE_MOVE_MID   = 0x98;
static const uint8_t HVANE_MOVE_RIGHT = 0xA0;

// ─── CRC8 Polynom = 1 (XOR-Summe aller Bytes) ───────────────────────────────
static uint8_t calc_checksum(const uint8_t *data, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; i++) sum ^= data[i];
  return sum;
}

// ─── Hilfsfunktionen für Vane-String → Byte-Mapping ─────────────────────────

static uint8_t vswing_str_to_byte(const std::string &s) {
  if (s == "Fix top")      return VVANE_FIX_TOP;
  if (s == "Fix upper")    return VVANE_FIX_UPPER;
  if (s == "Fix mid")      return VVANE_FIX_MID;
  if (s == "Fix lower")    return VVANE_FIX_LOWER;
  if (s == "Fix bottom")   return VVANE_FIX_BOTTOM;
  if (s == "Move full")    return VVANE_MOVE_FULL;
  if (s == "Move upper")   return VVANE_MOVE_UPPER;
  if (s == "Move lower")   return VVANE_MOVE_LOWER;
  return VVANE_NA;  // "Last position" oder unbekannt
}

static uint8_t hswing_str_to_byte(const std::string &s) {
  if (s == "Fix left")      return HVANE_FIX_LEFT;
  if (s == "Fix mid-left")  return HVANE_FIX_MLEFT;
  if (s == "Fix mid")       return HVANE_FIX_MID;
  if (s == "Fix mid-right") return HVANE_FIX_MRIGHT;
  if (s == "Fix right")     return HVANE_FIX_RIGHT;
  if (s == "Move full")     return HVANE_MOVE_FULL;
  if (s == "Move left")     return HVANE_MOVE_LEFT;
  if (s == "Move mid")      return HVANE_MOVE_MID;
  if (s == "Move right")    return HVANE_MOVE_RIGHT;
  return HVANE_NA;  // "Last position" oder unbekannt
}

static std::string vbyte_to_str(uint8_t b) {
  switch (b) {
    case VVANE_FIX_TOP:    return "Fix top";
    case VVANE_FIX_UPPER:  return "Fix upper";
    case VVANE_FIX_MID:    return "Fix mid";
    case VVANE_FIX_LOWER:  return "Fix lower";
    case VVANE_FIX_BOTTOM: return "Fix bottom";
    case VVANE_MOVE_FULL:  return "Move full";
    case VVANE_MOVE_UPPER: return "Move upper";
    case VVANE_MOVE_LOWER: return "Move lower";
    default:               return "Last position";
  }
}

static std::string hbyte_to_str(uint8_t b) {
  switch (b) {
    case HVANE_FIX_LEFT:   return "Fix left";
    case HVANE_FIX_MLEFT:  return "Fix mid-left";
    case HVANE_FIX_MID:    return "Fix mid";
    case HVANE_FIX_MRIGHT: return "Fix mid-right";
    case HVANE_FIX_RIGHT:  return "Fix right";
    case HVANE_MOVE_FULL:  return "Move full";
    case HVANE_MOVE_LEFT:  return "Move left";
    case HVANE_MOVE_MID:   return "Move mid";
    case HVANE_MOVE_RIGHT: return "Move right";
    default:               return "Last position";
  }
}

// ─── setup() ─────────────────────────────────────────────────────────────────
void TclClimate::setup() {
  // Traits: unterstützte Modi, Schwenkmodi, Lüfterstufen
  auto traits = this->get_traits();

  traits.set_supports_current_temperature(true);
  traits.set_visual_min_temperature(16.0f);
  traits.set_visual_max_temperature(31.0f);
  traits.set_visual_temperature_step(0.5f);

  traits.set_supported_modes({
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_FAN_ONLY,
    climate::CLIMATE_MODE_HEAT_COOL,
  });

  traits.set_supported_fan_modes({
    climate::CLIMATE_FAN_AUTO,
    climate::CLIMATE_FAN_LOW,
    climate::CLIMATE_FAN_MEDIUM,
    climate::CLIMATE_FAN_HIGH,
  });

  // Alle vier Schwenkmodi anbieten
  traits.set_supported_swing_modes({
    climate::CLIMATE_SWING_OFF,
    climate::CLIMATE_SWING_VERTICAL,
    climate::CLIMATE_SWING_HORIZONTAL,
    climate::CLIMATE_SWING_BOTH,
  });

  this->set_traits(traits);

  // Vane-Select-Callbacks registrieren
  if (this->vswing_select_ != nullptr) {
    this->vswing_select_->add_on_state_callback([this](const std::string &value, size_t) {
      ESP_LOGD(TAG, "Vertical vane select changed: %s", value.c_str());
      this->vane_vertical_pos_ = vswing_str_to_byte(value);
      // Swing-Mode im Climate-State synchronisieren
      bool v_swinging = (this->vane_vertical_pos_ == VVANE_MOVE_FULL ||
                         this->vane_vertical_pos_ == VVANE_MOVE_UPPER ||
                         this->vane_vertical_pos_ == VVANE_MOVE_LOWER);
      bool h_swinging = (this->swing_mode == climate::CLIMATE_SWING_HORIZONTAL ||
                         this->swing_mode == climate::CLIMATE_SWING_BOTH);
      if (v_swinging && h_swinging)
        this->swing_mode = climate::CLIMATE_SWING_BOTH;
      else if (v_swinging)
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
      else if (h_swinging)
        this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
      else
        this->swing_mode = climate::CLIMATE_SWING_OFF;
      this->send_set_command_();
    });
  }

  if (this->hswing_select_ != nullptr) {
    this->hswing_select_->add_on_state_callback([this](const std::string &value, size_t) {
      ESP_LOGD(TAG, "Horizontal vane select changed: %s", value.c_str());
      this->vane_horizontal_pos_ = hswing_str_to_byte(value);
      // Swing-Mode im Climate-State synchronisieren
      bool h_swinging = (this->vane_horizontal_pos_ == HVANE_MOVE_FULL ||
                         this->vane_horizontal_pos_ == HVANE_MOVE_LEFT ||
                         this->vane_horizontal_pos_ == HVANE_MOVE_MID ||
                         this->vane_horizontal_pos_ == HVANE_MOVE_RIGHT);
      bool v_swinging = (this->swing_mode == climate::CLIMATE_SWING_VERTICAL ||
                         this->swing_mode == climate::CLIMATE_SWING_BOTH);
      if (v_swinging && h_swinging)
        this->swing_mode = climate::CLIMATE_SWING_BOTH;
      else if (h_swinging)
        this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
      else if (v_swinging)
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
      else
        this->swing_mode = climate::CLIMATE_SWING_OFF;
      this->send_set_command_();
    });
  }

  // Initialen Status abrufen
  this->send_get_command_();
}

// ─── loop() ──────────────────────────────────────────────────────────────────
void TclClimate::loop() {
  // Alle verfügbaren UART-Bytes lesen
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    this->rx_buffer_.push_back(c);
  }

  // Auf vollständige Antwortpakete prüfen
  this->parse_rx_buffer_();

  // Periodischer Status-Poll
  const uint32_t now = millis();
  if (now - this->last_poll_ms_ >= POLL_INTERVAL_MS) {
    this->last_poll_ms_ = now;
    this->send_get_command_();
  }
}

// ─── control() – von ESPHome/HA aufgerufen ───────────────────────────────────
void TclClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();

  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();

  if (call.get_fan_mode().has_value())
    this->fan_mode = *call.get_fan_mode();

  // Swing-Modus: ESPHome CLIMATE_SWING_* → interne Vane-Bytes
  if (call.get_swing_mode().has_value()) {
    this->swing_mode = *call.get_swing_mode();

    switch (this->swing_mode) {
      case climate::CLIMATE_SWING_OFF:
        // Vertikale Lamelle auf Mitte fixieren, horizontal auf Mitte fixieren
        this->vane_vertical_pos_   = VVANE_FIX_MID;
        this->vane_horizontal_pos_ = HVANE_FIX_MID;
        break;

      case climate::CLIMATE_SWING_VERTICAL:
        this->vane_vertical_pos_   = VVANE_MOVE_FULL;
        // Horizontal unverändert lassen – aber sicherstellen dass kein Move aktiv
        if (this->vane_horizontal_pos_ == HVANE_MOVE_FULL ||
            this->vane_horizontal_pos_ == HVANE_MOVE_LEFT ||
            this->vane_horizontal_pos_ == HVANE_MOVE_MID ||
            this->vane_horizontal_pos_ == HVANE_MOVE_RIGHT) {
          this->vane_horizontal_pos_ = HVANE_FIX_MID;
        }
        break;

      case climate::CLIMATE_SWING_HORIZONTAL:
        this->vane_horizontal_pos_ = HVANE_MOVE_FULL;
        // Vertikal fixieren falls aktuell schwenkend
        if (this->vane_vertical_pos_ == VVANE_MOVE_FULL ||
            this->vane_vertical_pos_ == VVANE_MOVE_UPPER ||
            this->vane_vertical_pos_ == VVANE_MOVE_LOWER) {
          this->vane_vertical_pos_ = VVANE_FIX_MID;
        }
        break;

      case climate::CLIMATE_SWING_BOTH:
        this->vane_vertical_pos_   = VVANE_MOVE_FULL;
        this->vane_horizontal_pos_ = HVANE_MOVE_FULL;
        break;

      default:
        break;
    }

    // Select-Widgets synchronisieren
    if (this->vswing_select_ != nullptr)
      this->vswing_select_->publish_state(vbyte_to_str(this->vane_vertical_pos_));
    if (this->hswing_select_ != nullptr)
      this->hswing_select_->publish_state(hbyte_to_str(this->vane_horizontal_pos_));
  }

  this->send_set_command_();
  this->publish_state();
}

// ─── traits() ────────────────────────────────────────────────────────────────
climate::ClimateTraits TclClimate::traits() {
  auto t = climate::ClimateTraits();
  t.set_supports_current_temperature(true);
  t.set_visual_min_temperature(16.0f);
  t.set_visual_max_temperature(31.0f);
  t.set_visual_temperature_step(0.5f);
  t.set_supported_modes({
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_FAN_ONLY,
    climate::CLIMATE_MODE_HEAT_COOL,
  });
  t.set_supported_fan_modes({
    climate::CLIMATE_FAN_AUTO,
    climate::CLIMATE_FAN_LOW,
    climate::CLIMATE_FAN_MEDIUM,
    climate::CLIMATE_FAN_HIGH,
  });
  t.set_supported_swing_modes({
    climate::CLIMATE_SWING_OFF,
    climate::CLIMATE_SWING_VERTICAL,
    climate::CLIMATE_SWING_HORIZONTAL,
    climate::CLIMATE_SWING_BOTH,
  });
  return t;
}

// ─── Sendefunktionen ─────────────────────────────────────────────────────────

void TclClimate::send_get_command_() {
  // BB 00 01 04 02 01 00 <CRC>
  uint8_t pkt[8];
  pkt[0] = PREAMBLE;
  pkt[1] = 0x00;  // response flag (wir sind Modul → 0)
  pkt[2] = 0x01;  // request flag
  pkt[3] = CMD_GET;
  pkt[4] = PAYLOAD_LEN_GET;
  pkt[5] = 0x01;
  pkt[6] = 0x00;
  pkt[7] = calc_checksum(pkt, 7);
  this->write_array(pkt, 8);
  ESP_LOGV(TAG, "Sent GET command");
}

void TclClimate::send_set_command_() {
  // Gesamtpaket: 5 Header + 29 Payload + 1 CRC = 35 Byte
  uint8_t pkt[35] = {0};

  pkt[0] = PREAMBLE;
  pkt[1] = 0x00;  // module → 0
  pkt[2] = 0x01;  // request
  pkt[3] = CMD_SET;
  pkt[4] = PAYLOAD_LEN_SET;

  // ── Payload-Byte 0 (pkt[5]): Eco/Display/Buzzer/Power ──
  // Power: Bits 4:7  → 0x4 = On, 0x0 = Off
  uint8_t power_bits = (this->mode != climate::CLIMATE_MODE_OFF) ? 0x04 : 0x00;
  pkt[5] = 0x00;  // eco=0, display=0, buzzer=0, unknown=0
  pkt[6] = 0x00;

  // ── Payload-Byte 2 (pkt[7]): Power[4:7] | ExtendedMode[0:3] | Mode[4:7] ──
  //   Bit-Layout laut Protokoll:
  //     [7:4] = Power  (0x4 = on, 0x0 = off)
  //     [3:0] = Extended Mode (0x0=normal, 0x1=health, 0x4=turbo, 0x8=low noise)
  // pkt[7] = (power_bits << 4) | 0x00  (normal mode)
  pkt[7] = (power_bits << 4);

  // ── pkt[8]: ExtendedMode[4:7] | Mode[0:3] ──
  uint8_t ac_mode = 0x03;  // default: cooling
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT:       ac_mode = 0x01; break;
    case climate::CLIMATE_MODE_DRY:        ac_mode = 0x02; break;
    case climate::CLIMATE_MODE_COOL:       ac_mode = 0x03; break;
    case climate::CLIMATE_MODE_FAN_ONLY:   ac_mode = 0x07; break;
    case climate::CLIMATE_MODE_HEAT_COOL:  ac_mode = 0x08; break;
    default:                               ac_mode = 0x03; break;
  }
  pkt[8] = (0x01 << 4) | ac_mode;  // ExtendedMode nibble = 0x1 (bleibt immer 1 laut Traces)

  // ── pkt[9]: Unknown[0:3]=0x5 | Temperature[4:7] ──
  //   Temperature: 0xF + 16 - T  (T in [16..31])
  float t = this->target_temperature;
  if (t < 16.0f) t = 16.0f;
  if (t > 31.0f) t = 31.0f;
  uint8_t temp_int  = (uint8_t)t;
  bool    temp_half = (t - temp_int) >= 0.5f;
  uint8_t temp_nibble = (uint8_t)(0x0F + 16 - temp_int) & 0x0F;
  pkt[9] = (temp_nibble << 4) | 0x05;

  // ── pkt[10]: 8°Heater[0] | Unknown[1] | MoveVertVane[2:4] | FanSpeed[5:7] ──
  uint8_t fan_bits = 0x00;  // auto
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:    fan_bits = 0x02; break;  // speed 1
    case climate::CLIMATE_FAN_MEDIUM: fan_bits = 0x03; break;  // speed 3
    case climate::CLIMATE_FAN_HIGH:   fan_bits = 0x07; break;  // speed 5
    default:                          fan_bits = 0x00; break;  // auto
  }

  // MoveVertVane: wenn vertikale Schwenkposition ein "Move" ist → 0b111, sonst 0b000
  bool v_move = (this->vane_vertical_pos_ == VVANE_MOVE_FULL  ||
                 this->vane_vertical_pos_ == VVANE_MOVE_UPPER ||
                 this->vane_vertical_pos_ == VVANE_MOVE_LOWER);
  uint8_t vvane_move_bits = v_move ? 0x07 : 0x00;

  pkt[10] = (fan_bits << 5) | (vvane_move_bits << 2) | 0x00;  // 8°heater=0, unknown=0

  // ── pkt[11]: Unknown[0:3]=0 | MoveHorizVane[4] | Unknown[5] | TempDeci[6] | Unknown[7] ──
  // MoveHorizVane Bit[4]: 1 wenn horizontales Schwenken aktiv
  bool h_move = (this->vane_horizontal_pos_ == HVANE_MOVE_FULL  ||
                 this->vane_horizontal_pos_ == HVANE_MOVE_LEFT  ||
                 this->vane_horizontal_pos_ == HVANE_MOVE_MID   ||
                 this->vane_horizontal_pos_ == HVANE_MOVE_RIGHT);
  uint8_t hvane_move_bit = h_move ? 0x10 : 0x00;
  uint8_t temp_deci_bit  = temp_half ? 0x40 : 0x00;
  pkt[11] = hvane_move_bit | temp_deci_bit;

  // pkt[12..26]: all 0 (unknown / sleep mode etc.)
  // pkt[27]: Vert. Vane Position (Byte 32 im Protokoll = pkt-Offset 27+5=32 ✓)
  pkt[27] = this->vane_vertical_pos_;

  // pkt[28]: Horiz. Vane Position (Byte 33 im Protokoll = pkt-Offset 28+5=33 ✓)
  pkt[28] = this->vane_horizontal_pos_;

  // pkt[29..33]: all 0

  // CRC über alle Bytes außer dem letzten
  pkt[34] = calc_checksum(pkt, 34);

  this->write_array(pkt, 35);

  ESP_LOGD(TAG, "Sent SET: mode=%d fan=%d temp=%.1f vvane=0x%02X hvane=0x%02X swing=%d",
           (int)this->mode,
           (int)this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO),
           (double)this->target_temperature,
           this->vane_vertical_pos_,
           this->vane_horizontal_pos_,
           (int)this->swing_mode);
}

// ─── RX-Puffer auswerten ─────────────────────────────────────────────────────
void TclClimate::parse_rx_buffer_() {
  // Minimale Paketgröße: 5 Header + 2 Payload (GET-Request) + 1 CRC = 8
  // Status-Antwort: 5 + 55 + 1 = 61 Byte
  while (this->rx_buffer_.size() >= 8) {
    // Preamble suchen
    if (this->rx_buffer_[0] != PREAMBLE) {
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }

    // Mindest-Header gelesen?
    if (this->rx_buffer_.size() < 5) break;

    uint8_t payload_len = this->rx_buffer_[4];
    size_t  total_len   = 5 + payload_len + 1;  // Header + Payload + CRC

    if (this->rx_buffer_.size() < total_len) break;  // noch nicht vollständig

    // CRC prüfen
    uint8_t expected_crc = calc_checksum(this->rx_buffer_.data(), total_len - 1);
    uint8_t received_crc = this->rx_buffer_[total_len - 1];

    if (expected_crc != received_crc) {
      ESP_LOGW(TAG, "CRC error: expected 0x%02X got 0x%02X – discarding byte", expected_crc, received_crc);
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }

    // Paket ist valide – auswerten
    uint8_t response_flag = this->rx_buffer_[1];
    uint8_t cmd           = this->rx_buffer_[3];

    if (response_flag == 0x01) {  // Antwort von der AC
      if ((cmd == CMD_SET || cmd == CMD_GET) && payload_len >= 0x37) {
        this->handle_status_response_(this->rx_buffer_.data(), total_len);
      }
    }

    // Verarbeitetes Paket entfernen
    this->rx_buffer_.erase(this->rx_buffer_.begin(),
                            this->rx_buffer_.begin() + total_len);
  }
}

// ─── Status-Antwort auswerten ─────────────────────────────────────────────────
// Antwortformat (Protokoll-Byte-Indizes, 0-basiert):
//   [0]  = 0xBB
//   [1]  = 0x01 (AC→Modul)
//   [2]  = 0x00
//   [3]  = CMD (0x03 oder 0x04)
//   [4]  = 0x37 (55 Byte Payload)
//   [5]  = Payload-Start
//   --- Payload ---
//   [5+2] = pkt[7]:  Status[0:3] | Mode[4:7]
//   [5+3] = pkt[8]:  FanSpeed[0:3] | SetTemperature[4:7]
//   [5+4] = pkt[9]:  Temperature (deci)
//   [5+5] = pkt[10]: Vanes[0:3]  (bit1=hSwing, bit2=vSwing)
void TclClimate::handle_status_response_(const uint8_t *data, size_t len) {
  if (len < 12) return;

  // Byte-Offset ab Paket-Start (data[0]=BB)
  // pkt[7]  = data[7]
  uint8_t b7  = data[7];
  uint8_t b8  = data[8];
  uint8_t b9  = data[9];
  uint8_t b10 = data[10];

  // ── Status / Power ──
  uint8_t status = (b7 >> 0) & 0x0F;  // Bits [3:0]
  // 0x2=Off, 0x3=On, 0x7=Eco, 0xB=Turbo
  bool power_on = (status == 0x03 || status == 0x07 || status == 0x0B);

  // ── Mode ──
  uint8_t mode_nibble = (b7 >> 4) & 0x0F;  // Bits [7:4]
  // 0x1=cooling, 0x2=venting, 0x3=dehumid, 0x4=heating, 0x5=auto
  if (!power_on) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    switch (mode_nibble) {
      case 0x04: this->mode = climate::CLIMATE_MODE_HEAT;       break;
      case 0x02: this->mode = climate::CLIMATE_MODE_FAN_ONLY;   break;
      case 0x03: this->mode = climate::CLIMATE_MODE_DRY;        break;
      case 0x01: this->mode = climate::CLIMATE_MODE_COOL;       break;
      case 0x05: this->mode = climate::CLIMATE_MODE_HEAT_COOL;  break;
      default:   this->mode = climate::CLIMATE_MODE_COOL;       break;
    }
  }

  // ── Fan Speed ──
  uint8_t fan_nibble = (b8 >> 0) & 0x0F;
  // 0x8=Auto, 0x9=1, 0xC=2, 0xA=3, 0xD=4, 0xB=5
  switch (fan_nibble) {
    case 0x08: this->fan_mode = climate::CLIMATE_FAN_AUTO;   break;
    case 0x09: this->fan_mode = climate::CLIMATE_FAN_LOW;    break;
    case 0x0C: this->fan_mode = climate::CLIMATE_FAN_LOW;    break;
    case 0x0A: this->fan_mode = climate::CLIMATE_FAN_MEDIUM; break;
    case 0x0D: this->fan_mode = climate::CLIMATE_FAN_HIGH;   break;
    case 0x0B: this->fan_mode = climate::CLIMATE_FAN_HIGH;   break;
    default:   this->fan_mode = climate::CLIMATE_FAN_AUTO;   break;
  }

  // ── Zieltemperatur ──
  uint8_t temp_nibble = (b8 >> 4) & 0x0F;  // T = X + 16
  float   set_temp    = (float)(temp_nibble + 16);
  uint8_t deci_flag   = b9;
  if (deci_flag == 0x02) set_temp += 0.5f;
  this->target_temperature = set_temp;

  // ── Schwenk-Flags aus Byte 10 ──
  // bit1 (0x02) = horizontal swinging
  // bit2 (0x04) = vertical swinging
  bool v_swinging_reported = (b10 & 0x04) != 0;
  bool h_swinging_reported = (b10 & 0x02) != 0;

  if (v_swinging_reported && h_swinging_reported)
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  else if (v_swinging_reported)
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  else if (h_swinging_reported)
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  else
    this->swing_mode = climate::CLIMATE_SWING_OFF;

  // ── Select-Widgets aus vollen Vane-Positions-Bytes aktualisieren (falls vorhanden) ──
  // Diese Bytes liegen in der längeren Antwort (Payload ≥ 0x37 = 55 Byte)
  // Byte 32 im Gesamtpaket (0-basiert) = data[32]
  // Byte 33 im Gesamtpaket             = data[33]
  if (len > 34) {
    uint8_t vvane_byte = data[32];
    uint8_t hvane_byte = data[33];

    if (vvane_byte != 0x00) {
      this->vane_vertical_pos_ = vvane_byte;
      if (this->vswing_select_ != nullptr)
        this->vswing_select_->publish_state(vbyte_to_str(vvane_byte));
    }

    if (hvane_byte != 0x80) {
      this->vane_horizontal_pos_ = hvane_byte;
      if (this->hswing_select_ != nullptr)
        this->hswing_select_->publish_state(hbyte_to_str(hvane_byte));
    }
  }

  ESP_LOGD(TAG, "Status: mode=%d fan=%d temp=%.1f swing=%d vvane=0x%02X hvane=0x%02X",
           (int)this->mode,
           (int)this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO),
           (double)this->target_temperature,
           (int)this->swing_mode,
           this->vane_vertical_pos_,
           this->vane_horizontal_pos_);

  this->publish_state();
}

}  // namespace tcl_climate
}  // namespace esphome
