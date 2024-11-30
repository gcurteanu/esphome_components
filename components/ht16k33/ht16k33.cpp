#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "ht16k33.h"
#include "font.h"

#ifndef USE_ESP8266
  #define pgm_read_word(s) (*s)
#endif

namespace esphome {
namespace ht16k33_alpha {

static const char *TAG = "ht16k33_alpha";

// First set bit determines command, bits after that are the data.
static const uint8_t DISPLAY_COMMAND_SET_DDRAM_ADDR = 0x00;
static const uint8_t DISPLAY_COMMAND_SYSTEM_SETUP = 0x21;
static const uint8_t DISPLAY_COMMAND_INT_SETUP = 0xA0;
static const uint8_t DISPLAY_COMMAND_DISPLAY_OFF = 0x80;
static const uint8_t DISPLAY_COMMAND_DISPLAY_ON = 0x81;
static const uint8_t DISPLAY_COMMAND_DIMMING = 0xE0;

void HT16K33AlphaDisplay::setup() {
  for (auto *display : this->displays_) {
    display->write_bytes(DISPLAY_COMMAND_SYSTEM_SETUP, nullptr, 0);
    display->write_bytes(DISPLAY_COMMAND_INT_SETUP, nullptr, 0);
    display->write_bytes(DISPLAY_COMMAND_DISPLAY_ON, nullptr, 0);
  }
  this->set_brightness(1);
}

void HT16K33AlphaDisplay::loop() {
  unsigned long now = millis();
  int numc = this->displays_.size() * 16;
  int len = this->buffer_.size();
  if (!this->scroll_ || (len <= numc))
    return;
  if ((this->offset_ == 0) && (now - this->last_scroll_ < this->scroll_delay_))
    return;
  if ((!this->continuous_ && (this->offset_ + numc >= len)) ||
      (this->continuous_ && (this->offset_ > len - 2))) {
    if (this->continuous_ || (now - this->last_scroll_ >= this->scroll_dwell_)) {
      this->offset_ = 0;
      this->last_scroll_ = now;
      this->display_();
    }
  } else if (now - this->last_scroll_ >= this->scroll_speed_) {
    this->offset_ += 2;
    this->last_scroll_ = now;
    this->display_();
  }
}

float HT16K33AlphaDisplay::get_setup_priority() const { return setup_priority::PROCESSOR; }

void HT16K33AlphaDisplay::display_() {
  int numc = this->displays_.size() * 16;
  int len = this->buffer_.size();
  uint8_t data[numc];
  memset(data, 0, numc);
  int pos = this->offset_;
  for (int i = 0; i < (this->digits_ * 2); i++, pos++) {
    if (pos >= len) {
      if (!this->continuous_)
        break;
      pos %= len;
    }
    data[i] = this->buffer_[pos];
  }


  /*ESP_LOGD("dsp", "chars=%d", (int)numc);
  for(int i=0;i<numc;i++) {
    ESP_LOGD("dsp", "byte[%02d]=%04X", (int)i, (int)data[i]);
  }*/
  pos = 0;
  for (auto *display : this->displays_) {
    display->write_bytes(DISPLAY_COMMAND_SET_DDRAM_ADDR, data + pos, 16);
    pos += 16;
  }
}

void HT16K33AlphaDisplay::update() {
  int prev_fill = this->buffer_.size();
  this->buffer_.clear();
  this->call_writer();
  int numc = this->displays_.size() * 16;
  int len = this->buffer_.size();
  if ((this->scroll_ && (prev_fill != len) && !this->continuous_) || (len <= numc)) {
    this->last_scroll_ = millis();
    this->offset_ = 0;
  }
  this->display_();
}

void HT16K33AlphaDisplay::set_brightness(float level) {
  int val = (int) round(level * 16);
  if (val < 0)
    val = 0;
  if (val > 16)
    val = 16;
  this->brightness_ = val;
  for (auto *display : this->displays_) {
    if (val == 0) {
      display->write_bytes(DISPLAY_COMMAND_DISPLAY_OFF, nullptr, 0);
    } else {
      display->write_bytes(DISPLAY_COMMAND_DIMMING + (val - 1), nullptr, 0);
      display->write_bytes(DISPLAY_COMMAND_DISPLAY_ON, nullptr, 0);
    }
  }
}

float HT16K33AlphaDisplay::get_brightness() {
  return this->brightness_ / 16.0;
}

void HT16K33AlphaDisplay::print(const char *str) {
  uint16_t fontc = 0;
  while (*str != '\0') {
    uint8_t c = *reinterpret_cast<const uint8_t *>(str++);
    if (c > 127)
      fontc = 0;
    else
      fontc = pgm_read_word(&alphafonttable[c]);
    c = *reinterpret_cast<const uint8_t *>(str);
    if (c == '.') {
      fontc |= 0x4000;
      str++;
    }
    this->buffer_.push_back(fontc & 0xff);
    this->buffer_.push_back(fontc >> 8);
  }
}

void HT16K33AlphaDisplay::print(const std::string &str) { this->print(str.c_str()); }

void HT16K33AlphaDisplay::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0) {
    if (this->reverse_) {
      char r_buffer[64];
      for (int i=0; i<ret; i++) {
        r_buffer[i] = buffer[ret-1-i];
      }
      r_buffer[ret] = 0x00;
      this->print(r_buffer);
    } else {
      this->print(buffer);
    }
  }
}

#ifdef USE_TIME
void HT16K33AlphaDisplay::strftime(const char *format, ESPTime time) {
  char buffer[64];
  size_t ret = time.strftime(buffer, sizeof(buffer), format);
  if (ret > 0)
    this->print(buffer);
}
#endif

}  // namespace ht16k33_alpha
}  // namespace esphome
