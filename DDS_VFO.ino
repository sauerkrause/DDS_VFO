#include <AD9850.h>
#include <RotaryEncoder.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int W_CLK_PIN = 8;
const int FQ_UD_PIN = 9;
const int DATA_PIN = 10;
const int RESET_PIN = 11;
const int ROTARY_BUTTON_PIN = A1;
const long M = 1000000;
const long K = 1000;
double trim_freq = 125*M;
long freq = 1.8*M;
long old_freq = 0;
int phase = 0;
RotaryEncoder r = RotaryEncoder(2, 3);
LiquidCrystal_I2C lcd(0x27, 16, 2);
int rotary_button_state = 0;
long freq_steps[] = {500, 1000, 2500, 5000};
unsigned long freq_step_n = 1;
long freq_step = freq_steps[freq_step_n];

void setup() {
  Serial.begin(9600);
  DDS.begin(W_CLK_PIN, FQ_UD_PIN, DATA_PIN, RESET_PIN);
  DDS.calibrate(trim_freq);
  pinMode(ROTARY_BUTTON_PIN, INPUT);
  lcd.init();
  lcd.backlight();
  lcd.home();
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  DDS.setfreq(freq, phase);
  draw_lcd();
  delay(2000);
}

void loop() {
  int state = digitalRead(ROTARY_BUTTON_PIN);
  if (state == LOW) {
    freq_step_n = (freq_step_n + 1) % 4;
    freq_step = freq_steps[freq_step_n];
    draw_lcd();
  }
  if (old_freq != freq) {
    DDS.setfreq(freq, phase);
    Serial.println(freq);
    old_freq = freq;
    draw_lcd();
  }
  delay(200);
}

void draw_lcd() {
  long mhz = freq / M;
  long decimal = freq % M;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("VFO");
  lcd.setCursor(7, 0);
  lcd.print("S: ");
  lcd.print(freq_step);
  lcd.print("Hz");
  lcd.setCursor(0, 1);
  lcd.print(mhz);
  lcd.print(".");
  if (decimal) {
    lcd.print(decimal);
  } else {
    lcd.print("000000");
  }
  lcd.print(" MHz");
}

ISR(PCINT2_vect) {
  r.tick();
  RotaryEncoder::Direction result = r.getDirection();
  switch (result) {
  case RotaryEncoder::Direction::CLOCKWISE:
    freq = freq - freq_step;
    break;
  case RotaryEncoder::Direction::COUNTERCLOCKWISE:
    freq = freq + freq_step;
    break;
  }
}