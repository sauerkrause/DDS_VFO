#include <AD9850.h>
#include <RotaryEncoder.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

const int W_CLK_PIN = 8;
const int FQ_UD_PIN = 9;
const int DATA_PIN = 10;
const int RESET_PIN = 11;
const int PTT_PIN = A0;
const int ROTARY_BUTTON_PIN = A1;
const uint8_t BUTTON_PINS[] = {12, 4, 5, 6, 7};
uint8_t button_states[] = {1, 1, 1, 1, 1};

const long M = 1000000;
const long K = 1000;
double trim_freq = 125*M;
int ptt = 0;
int old_ptt = 0;
unsigned long freq = 14233000;
long old_freq = 0;
int phase = 0;
RotaryEncoder r = RotaryEncoder(2, 3);
LiquidCrystal_I2C lcd(0x27, 16, 2);
int rotary_button_state = 0;
unsigned long freq_steps[] = {1, 10, 100, 500, 1000, 2500, 5000, 10000, 100000, 1000000};
char* freq_step_strs[] = {"1Hz", "10Hz", "100Hz", "500Hz", "1kHz", "2.5kHz", "5kHz", "10kHz", "100kHz", "1MHz"};
long freq_step_n = 4;
long freq_step = freq_steps[freq_step_n];
uint8_t bands[] = {160, 80, 60, 40, 30, 20, 17, 15, 12, 10};
unsigned long band_edges[sizeof(bands)*2];
int8_t current_band = -1;

byte plus_minus_sym[] = {
  B00100,
  B00100,
  B11111,
  B00100,
  B00100,
  B00000,
  B11111
};

uint8_t need_refresh = 0;

uint16_t step_addr = 0;
uint16_t freq_addr = 1;

void setup() {
  Serial.begin(9600);
  DDS.begin(W_CLK_PIN, FQ_UD_PIN, DATA_PIN, RESET_PIN);
  DDS.calibrate(trim_freq);
  pinMode(PTT_PIN, INPUT);
  pinMode(ROTARY_BUTTON_PIN, INPUT);
  for (size_t i = 0; i < sizeof(BUTTON_PINS); ++i) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }
  lcd.init();
  lcd.backlight();
  lcd.home();
  lcd.createChar(0, plus_minus_sym);
  init_band_edges();
  determine_band();
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  DDS.setfreq(freq, phase);
  draw_lcd();
  delay(200);
}

void init_band_edges() {
  for(size_t i = 0; i < sizeof(bands); ++i) {
    int eeprom_addr = i << 3;
    EEPROM.get(eeprom_addr, band_edges[i << 1]);
    EEPROM.get(eeprom_addr + 4, band_edges[(i << 1) + 1]);
  }
}

void determine_band() {
  current_band = -1;
  for (size_t i = 0; i < sizeof(bands); ++i) {
    long band_bottom = band_edges[i<<1];
    long band_top = band_edges[(i<<1)+1];
    if (freq > band_top) {
      continue;
    } else if (freq <= band_top && freq >= band_bottom) {
      current_band = i;
      break;
    } else if (freq < band_bottom) {
      break;
    }
  }
}

void handle_button(unsigned int button) {
    switch(button) {
      case 0:
        break;
      case 1:
        band_down();
        break;
      case 2:
        band_up();
        break;
      case 3:
        freq_step_n = (freq_step_n - 1);
        if (freq_step_n < 0)
          freq_step_n = (sizeof(freq_steps) >> 2) - 1;
        freq_step = freq_steps[freq_step_n];
        break;
      case 4:
        freq_step_n = (freq_step_n + 1) % (sizeof(freq_steps) >> 2);
        freq_step = freq_steps[freq_step_n];
        break;
    }
    need_refresh = true;
}

void band_down() {
  long last_freq = 0;
  for (size_t i = 0; i < sizeof(band_edges) >> 2; ++i) {
    if (freq > band_edges[i]) {
      last_freq = band_edges[i];
    } else {
      break;
    }
  }
  freq = last_freq;
}

void band_up() {
  long last_freq = 30*M;
  for (size_t i = (sizeof(band_edges) >> 2) - 1; i >= 0; --i) {
    if (freq < band_edges[i]) {
      last_freq = band_edges[i];
    } else {
      break;
    }
  }
  freq = last_freq;
}

void loop() {
  need_refresh = 0;
  const size_t num_buttons = sizeof(BUTTON_PINS);
  for (size_t i = 0; i < num_buttons; ++i) {
    int state = digitalRead(BUTTON_PINS[i]);
    if (state == LOW){
      button_states[i] = state;
      handle_button(i);
    }
  }
  int state = digitalRead(ROTARY_BUTTON_PIN);
  ptt = digitalRead(PTT_PIN);
  if (state == LOW) {
    freq_step_n = (freq_step_n + 1) % (sizeof(freq_steps)/sizeof(long));
    freq_step = freq_steps[freq_step_n];
    need_refresh = 1;
    EEPROM.update(step_addr, freq_step_n);
  }
  if (old_freq != freq) {
    DDS.setfreq(freq, phase);
    determine_band();
    old_freq = freq;
    need_refresh = 1;
  }
  if (ptt != old_ptt) {
    old_ptt = ptt;
    need_refresh = 1;
  }
  if (need_refresh) {
    draw_lcd();
  }
  delay(139);
}

void draw_lcd() {
  unsigned long mhz = freq / M;
  unsigned long khz = freq % M / K;
  unsigned long hz = freq % M - khz * K;
  char hz_buffer[4];
  char khz_buffer[4];
  char line_buffer[17];
  char band_str[5] = "--m\0";
  if (current_band >= 0) {
    snprintf(band_str, 5, "%dm", bands[current_band]);
  }
  snprintf(hz_buffer, 4, "%03d", hz);
  snprintf(khz_buffer, 4, "%03d", khz);
  lcd.clear();
  lcd.setCursor(0, 0);
  snprintf(line_buffer, 17, "%s %s +%s", band_str, ptt ? "*" : " ", freq_step_strs[freq_step_n]);
  lcd.print(line_buffer);
  for (size_t i = 0; i < 17; ++i) {
    if (line_buffer[i] == '+') {
      lcd.setCursor(i, 0);
      lcd.write(0);
    }
  }
  lcd.setCursor(0, 1);
  lcd.print(mhz);
  lcd.print('.');
  lcd.print(khz_buffer);
  lcd.print(' ');
  lcd.print(hz_buffer);
  lcd.print(" MHz");
}

ISR(PCINT2_vect) {
  r.tick();
  RotaryEncoder::Direction result = r.getDirection();
  switch (result) {
  case RotaryEncoder::Direction::CLOCKWISE:
    if (ptt != HIGH) 
      freq = freq - freq_step;
    break;
  case RotaryEncoder::Direction::COUNTERCLOCKWISE:
    if (ptt != HIGH)
      freq = freq + freq_step;
    break;
  }
}