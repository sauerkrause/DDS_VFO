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

const long M = 1000000;
const long K = 1000;
double trim_freq = 125*M;
int ptt = 0;
int old_ptt = 0;
unsigned long freq = 1.8*M;
long old_freq = 0;
int phase = 0;
RotaryEncoder r = RotaryEncoder(2, 3);
LiquidCrystal_I2C lcd(0x27, 16, 2);
int rotary_button_state = 0;
long freq_steps[] = {1, 10, 100, 500, 1000, 2500, 5000, 10000, 100000, 1000000};
char* freq_step_strs[] = {"1Hz", "10Hz", "100Hz", "500Hz", "1kHz", "2.5kHz", "5kHz", "10kHz", "100kHz", "1MHz"};
unsigned long freq_step_n = 4;
long freq_step = freq_steps[freq_step_n];

uint8_t need_refresh = 0;

uint16_t step_addr = 0;
uint16_t freq_addr = 1;

void setup() {
  Serial.begin(9600);
  DDS.begin(W_CLK_PIN, FQ_UD_PIN, DATA_PIN, RESET_PIN);
  DDS.calibrate(trim_freq);
  pinMode(PTT_PIN, INPUT);
  pinMode(ROTARY_BUTTON_PIN, INPUT);
  lcd.init();
  lcd.backlight();
  lcd.home();
  // get the step from eeprom
  uint8_t b = EEPROM.read(step_addr);
  if (b != 0xFF) {
    freq_step_n = b;
    freq_step = freq_steps[freq_step_n];
  }
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  DDS.setfreq(freq, phase);
  draw_lcd();
  delay(2000);
}

void loop() {
  need_refresh = 0;
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
  delay(200);
}

void draw_lcd() {
  unsigned long mhz = freq / M;
  unsigned long khz = freq % M / K;
  unsigned long hz = freq % M - khz * K;
  char hz_buffer[4];
  char khz_buffer[4];
  snprintf(hz_buffer, 4, "%03d", hz);
  snprintf(khz_buffer, 4, "%03d", khz);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("VFO ");
  lcd.print(ptt ? "* " : "  ");
  lcd.print("S:");
  lcd.print(freq_step_strs[freq_step_n]);
  lcd.setCursor(0, 1);
  lcd.print(mhz);
  lcd.print(".");
  lcd.print(khz_buffer);
  lcd.print(" ");
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