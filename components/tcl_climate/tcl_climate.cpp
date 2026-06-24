#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "tcl_climate.h"

namespace esphome {
namespace tcl_climate {

void TCLClimate::set_current_temperature(float current_temperature) {
  if (this->current_temperature == current_temperature) return;
  this->is_changed = true;
  this->current_temperature = current_temperature;
}

void TCLClimate::set_custom_fan_mode(esphome::StringRef fan_mode) {
  std::string fm(fan_mode.data(), fan_mode.size());
  if (this->custom_fan_mode == fm) return;
  this->is_changed = true;
  this->custom_fan_mode = fm;
}

void TCLClimate::set_mode(esphome::climate::ClimateMode mode) {
  if (this->mode == mode) return;
  this->is_changed = true;
  this->mode = mode;
}

void TCLClimate::set_target_temperature(float target_temperature) {
  if (this->target_temperature == target_temperature) return;
  this->is_changed = true;
  this->target_temperature = target_temperature;
}

void TCLClimate::build_set_cmd(get_cmd_resp_t *get_cmd_resp) {
    memcpy(m_set_cmd.raw, set_cmd_base, sizeof(m_set_cmd.raw));

    m_set_cmd.data.power = get_cmd_resp->data.power;
    m_set_cmd.data.off_timer_en = 0;
    m_set_cmd.data.on_timer_en = 0;
    m_set_cmd.data.beep = 1;
    m_set_cmd.data.disp = 1;
    m_set_cmd.data.eco = 0;

    switch (get_cmd_resp->data.mode) {
      case 0x01:
        m_set_cmd.data.mode = 0x03;
        break;
      case 0x03:
        m_set_cmd.data.mode = 0x02;
        break;
      case 0x02:
        m_set_cmd.data.mode = 0x07;
        break;
      case 0x04:
        m_set_cmd.data.mode = 0x01;
        break;
      case 0x05:
        m_set_cmd.data.mode = 0x08;
        break;
    }

    m_set_cmd.data.turbo = get_cmd_resp->data.turbo;
    m_set_cmd.data.mute = get_cmd_resp->data.mute;
    m_set_cmd.data.temp = 15 - get_cmd_resp->data.temp;

    switch (get_cmd_resp->data.fan) {
      case 0x00:
        m_set_cmd.data.fan = 0x00;
        break;
      case 0x01:
        m_set_cmd.data.fan = 0x02;
        break;
      case 0x04:
        m_set_cmd.data.fan = 0x06;
        break;
      case 0x02:
        m_set_cmd.data.fan = 0x03;
        break;
      case 0x05:
        m_set_cmd.data.fan = 0x07;
        break;
      case 0x03:
        m_set_cmd.data.fan = 0x05;
        break;
    }

    m_set_cmd.data.half_degree = 0;

    // --- START PATCH: Swing Mode für Kesser Modul 32001-000140 ---
    if (get_cmd_resp->data.vswing == 1) {
        m_set_cmd.data.vswing = 0x07;
    } else {
        m_set_cmd.data.vswing = 0x00;
    }

    if (get_cmd_resp->data.hswing == 1) {
        m_set_cmd.data.hswing = 0x01;
    } else {
        m_set_cmd.data.hswing = 0x00;
    }
    // --- END PATCH ---

    // XOR Prüfsummen-Berechnung
    uint8_t xor_byte = 0;
    for (size_t i = 0; i < sizeof(m_set_cmd.raw) - 1; i++) {
        xor_byte ^= m_set_cmd.raw[i];
    }
    m_set_cmd.raw[sizeof(m_set_cmd.raw) - 1] = xor_byte;
}

void TCLClimate::setup() {
  set_update_interval(900);
  this->set_supported_custom_fan_modes({"Turbo", "Mute", "Automatic", "1", "2", "3", "4", "5"});
}

void TCLClimate::control(const climate::ClimateCall &call) {

    if (call.get_mode().has_value()) {
      climate::ClimateMode climate_mode = *call.get_mode();

      get_cmd_resp_t get_cmd_resp = {0};
      memcpy(get_cmd_resp.raw, m_get_cmd_resp.raw, sizeof(get_cmd_resp.raw));

      if (call.get_swing_mode().has_value()) {
        auto swing_mode = *call.get_swing_mode();
        get_cmd_resp_t get_cmd_resp = {0};
        memcpy(get_cmd_resp.raw, m_get_cmd_resp.raw, sizeof(get_cmd_resp.raw));
      
        switch (swing_mode) {
          case climate::CLIMATE_SWING_OFF:
            get_cmd_resp.data.vswing = 0;
            get_cmd_resp.data.hswing = 0;
            break;
          case climate::CLIMATE_SWING_VERTICAL:
            get_cmd_resp.data.vswing = 1;
            get_cmd_resp.data.hswing = 0;
            break;
          case climate::CLIMATE_SWING_HORIZONTAL:
            get_cmd_resp.data.vswing = 0;
            get_cmd_resp.data.hswing = 1;
            break;
          case climate::CLIMATE_SWING_BOTH:
            get_cmd_resp.data.vswing = 1;
            get_cmd_resp.data.hswing = 1;
            break;
        }
      
        build_set_cmd(&get_cmd_resp);
        ready_to_send_set_cmd_flag = true;
      }
      
      if (climate_mode == climate::CLIMATE_MODE_OFF) {
        get_cmd_resp.data.power = 0x00;
      } else {
        get_cmd_resp.data.power = 0x01;
        switch (climate_mode) {
          case climate::CLIMATE_MODE_COOL:
            get_cmd_resp.data.mode = 0x01;
            break;
          case climate::CLIMATE_MODE_DRY:
            get_cmd_resp.data.mode = 0x03;
            break;
          case climate::CLIMATE_MODE_FAN_ONLY:
            get_cmd_resp.data.mode = 0x02;
            break;
          case climate::CLIMATE_MODE_HEAT:
          case climate::CLIMATE_MODE_HEAT_COOL:
            get_cmd_resp.data.mode = 0x04;
            break;
          case climate::CLIMATE_MODE_AUTO:
            get_cmd_resp.data.mode = 0x05;
            break;
          case climate::CLIMATE_MODE_OFF:
            break;
        }
      }

      build_set_cmd(&get_cmd_resp);
      ready_to_send_set_cmd_flag = true;
    }

    if (call.get_target_temperature().has_value()) {
      float temp = *call.get_target_temperature();
      
      get_cmd_resp_t get_cmd_resp = {0};
      memcpy(get_cmd_resp.raw, m_get_cmd_resp.raw, sizeof(get_cmd_resp.raw));

      get_cmd_resp.data.temp = uint8_t(temp) - 16;

      build_set_cmd(&get_cmd_resp);
      ready_to_send_set_cmd_flag = true;
    }

    esphome::StringRef custom_fan_mode = call.get_custom_fan_mode();
    if (!custom_fan_mode.empty()) {
      get_cmd_resp_t get_cmd_resp = {0};
      memcpy(get_cmd_resp.raw, m_get_cmd_resp.raw, sizeof(get_cmd_resp.raw));

      get_cmd_resp.data.turbo = 0x00;
      get_cmd_resp.data.mute = 0x00;
      if (custom_fan_mode == "Turbo") { 
        get_cmd_resp.data.fan = 0x03;
        get_cmd_resp.data.turbo = 0x01;
      } else if (custom_fan_mode == "Mute") {
        get_cmd_resp.data.fan = 0x01;
        get_cmd_resp.data.mute = 0x01;
      } else if (custom_fan_mode == "Automatic") get_cmd_resp.data.fan = 0x00;
      else if (custom_fan_mode == "1") get_cmd_resp.data.fan = 0x01;
      else if (custom_fan_mode == "2") get_cmd_resp.data.fan = 0x04;
      else if (custom_fan_mode == "3") get_cmd_resp.data.fan = 0x02;
      else if (custom_fan_mode == "4") get_cmd_resp.data.fan = 0x05;
      else if (custom_fan_mode == "5") get_cmd_resp.data.fan = 0x03;

      build_set_cmd(&get_cmd_resp);
      ready_to_send_set_cmd_flag = true;
    }
}

climate::ClimateTraits TCLClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supported_modes({
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_FAN_ONLY,
    climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_AUTO
  });
  traits.set_supported_swing_modes({
    climate::CLIMATE_SWING_OFF,
    climate::CLIMATE_SWING_VERTICAL,
    climate::CLIMATE_SWING_HORIZONTAL,
    climate::CLIMATE_SWING_BOTH
  });  
  traits.set_visual_min_temperature(16.0);
  traits.set_visual_max_temperature(31.0);
  traits.set_visual_target_temperature_step(1.0);
  return traits;
}

void TCLClimate::update() {
    uint8_t req_cmd[] = {0xBB, 0x00, 0x01, 0x04, 0x02, 0x01, 0x00, 0xBD};

    if (ready_to_send_set_cmd_flag) {
        ESP_LOGW("TCL", "Sending data");
        ready_to_send_set_cmd_flag = false;
        write_array(m_set_cmd.raw, sizeof(m_set_cmd.raw));
    }
    else write_array(req_cmd, sizeof(req_cmd));
}

int TCLClimate::read_data_line(int readch, uint8_t *buffer, int len) {
    static int pos = 0;
    static bool wait_len = false;
    static int skipch = 0;

    if (readch >= 0) {
      if (readch == 0xBB && skipch == 0 && !wait_len) {
        pos = 0;
        skipch = 3; 
        wait_len = true;
        if (pos < len-1) buffer[pos++] = readch;
      } else if (skipch == 0 && wait_len) {
        if (pos < len-1) buffer[pos++] = readch;
        skipch = readch + 1; 
        ESP_LOGW("TCL", "len: %d", readch);
        wait_len = false;
      } else if (skipch > 0) {
        if (pos < len-1) buffer[pos++] = readch;
        if (--skipch == 0 && !wait_len) return pos;
      }
    }
    return -1;
}

void TCLClimate::set_swing_mode(climate::ClimateSwingMode swing_mode) {
  if (this->swing_mode == swing_mode) return;
  this->is_changed = true;
  this->swing_mode = swing_mode;
}

bool TCLClimate::is_valid_xor(uint8_t *buffer, int len) {
    uint8_t xor_byte = 0;
    for (int i = 0; i < len - 1; i++) xor_byte ^= buffer[i];
    if (xor_byte == buffer[len - 1]) return true;
    else {
      ESP_LOGW("TCL", "No valid xor crc %02X (calculated %02X)", buffer[len], xor_byte);
      return false;
    }
}

void TCLClimate::print_hex_str(uint8_t *buffer, int len) {
    char str[250] = {0};
    char *pstr = str;
    if (len * 2 > sizeof(str)) ESP_LOGE("TCL", "too long byte data");

    for (int i = 0; i < len; i++) {
      pstr += sprintf(pstr, "%02X ", buffer[i]);
    }

    ESP_LOGW("TCL", "%s", str);
}

void TCLClimate::loop() {
    const int max_line_length = 100;
    static uint8_t buffer[max_line_length];
    
    while (available()) {
      int len = read_data_line(read(), buffer, max_line_length);
      if(len == sizeof(m_get_cmd_resp) && buffer[3] == 0x04) {
        memcpy(m_get_cmd_resp.raw, buffer, len);
        print_hex_str(buffer, len);
        if (is_valid_xor(buffer, len)) {
          float curr_temp = (((buffer[17] << 8) | buffer[18]) / 374 - 32) / 1.8;
          this->is_changed = false;

          if (m_get_cmd_resp.data.power == 0x00) this->set_mode(climate::CLIMATE_MODE_OFF);
          else if (m_get_cmd_resp.data.mode == 0x01) this->set_mode(climate::CLIMATE_MODE_COOL);
          else if (m_get_cmd_resp.data.mode == 0x03) this->set_mode(climate::CLIMATE_MODE_DRY);
          else if (m_get_cmd_resp.data.mode == 0x02) this->set_mode(climate::CLIMATE_MODE_FAN_ONLY);
          else if (m_get_cmd_resp.data.mode == 0x04) this->set_mode(climate::CLIMATE_MODE_HEAT);
          else if (m_get_cmd_resp.data.mode == 0x05) this->set_mode(climate::CLIMATE_MODE_AUTO);

          delay(4);

          if (m_get_cmd_resp.data.turbo) this->set_custom_fan_mode("Turbo");
          else if (m_get_cmd_resp.data.mute) this->set_custom_fan_mode("Mute");
          else if (m_get_cmd_resp.data.fan == 0x00) this->set_custom_fan_mode("Automatic");
          else if (m_get_cmd_resp.data.fan == 0x01) this->set_custom_fan_mode("1");
          else if (m_get_cmd_resp.data.fan == 0x04) this->set_custom_fan_mode("2");
          else if (m_get_cmd_resp.data.fan == 0x02) this->set_custom_fan_mode("3");
          else if (m_get_cmd_resp.data.fan == 0x05) this->set_custom_fan_mode("4");
          else if (m_get_cmd_resp.data.fan == 0x03) this->set_custom_fan_mode("5");

          if (!m_get_cmd_resp.data.vswing && !m_get_cmd_resp.data.hswing)
            this->set_swing_mode(climate::CLIMATE_SWING_OFF);
          else if (m_get_cmd_resp.data.vswing && !m_get_cmd_resp.data.hswing)
            this->set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
          else if (!m_get_cmd_resp.data.vswing && m_get_cmd_resp.data.hswing)
            this->set_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);
          else
            this->set_swing_mode(climate::CLIMATE_SWING_BOTH);

          this->set_target_temperature(float(m_get_cmd_resp.data.temp + 16));
          this->set_current_temperature(curr_temp);
          if (this->is_changed)
            this->publish_state();
        }
      }
    }
}

}  // namespace tcl_climate
}  // namespace esphome
