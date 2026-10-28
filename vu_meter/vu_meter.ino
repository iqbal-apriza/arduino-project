#include "stdio.h"
#include "stdlib.h"
#include <LiquidCrystal_I2C.h>

#define N 8

int num_sigL = 0, num_sigR = 0;
char buff_l[16], buff_r[16];
float audio_l, audio_r;

LiquidCrystal_I2C lcd(0x20, 16, 2);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("L");
  lcd.setCursor(0, 1);
  lcd.print("R");
}

void loop() {
  // put your main code here, to run repeatedly:
  float peak_l = 0, peak_r = 0;

  for(int i = 0; i < N; i++) {
    audio_l = abs(((float)analogRead(A0) - (float)511.0) / ((float)511));
    audio_r = abs(((float)analogRead(A1) - (float)511.0) / ((float)511));
    // Serial.println(audio_l);
    if(audio_l > peak_l) peak_l = audio_l;
    if(audio_r > peak_r) peak_r = audio_r;
  }


  // audio_l = abs((float)analogRead(A0) / ((float)1023));
  // audio_r = abs((float)analogRead(A1) / ((float)1023));

  peak_l = 20 * log10(fmax(peak_l, 1e-6));
  peak_r = 20 * log10(fmax(peak_r, 1e-6));

  num_sigL = constrain((peak_l + 60) * 0.25, 0, 15);
  num_sigR = constrain((peak_r + 60) * 0.25, 0, 15);

  for(int i = 0; i < 15; i++) {
    buff_l[i] = (i < num_sigL) ? 0xFF : ' ';
    buff_r[i] = (i < num_sigR) ? 0xFF : ' ';
  }
  buff_l[15] = "\0";
  buff_r[15] = "\0";

  lcd.setCursor(1, 0);
  lcd.printstr(buff_l);
  lcd.setCursor(1, 1);
  lcd.printstr(buff_r);
}
