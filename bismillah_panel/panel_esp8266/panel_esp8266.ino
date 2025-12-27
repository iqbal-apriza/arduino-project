#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_INA219.h>
#include <BH1750.h>
#include <DHT.h>

#define V_MTR         A0
#define TEMP_PIN      D0
#define DHT_PIN       D3
#define RELAY         D4
#define BTN           D5

#define DHTTYPE       DHT22

#define UPPER_TEMP    40
#define LOWER_TEMP    35

#define R             10
#define Q             0.4

OneWire oneWire(TEMP_PIN);
DallasTemperature temp_sensor(&oneWire);
LiquidCrystal_I2C lcd(0x27, 16, 2);
BH1750 luxmeter;
DHT DHT22_sensor(DHT_PIN, DHTTYPE);
Adafruit_INA219 INA_1(0x40);

DeviceAddress dev_addr;

typedef struct Panel {
  float tegangan;
  float arus;
  float daya;
  float suhu_panel;
} panel;

panel panel_sendiri;

char buff[16];
char num[8];

uint8_t temp6[8] = {0x28, 0x6B, 0x79, 0xB9, 0x00, 0x00, 0x00, 0xB9};

float Xt_c1[3] = {0};
float Pt_c1[3] = {0};
float Kt_c1;

float lux, env_temp, hum;

bool rly_state = 0;
bool btn[2] = {1, 0};
int8_t d_btn;

uint8_t state_panel = 0;

float read_voltage(float val) {
  return 1.71799432e-06 * (val * val * val) - 1.63616114e-04 * (val * val) +  2.50498137e-01 * val - 1.18494463e-01;
}

float read_temp(DeviceAddress deviceaddress) {
  float tempC = temp_sensor.getTempC(deviceaddress);
  return tempC;
}

void kalman(float input, float *Xt, float *Pt, float &Kt) {
  Xt[2] = Xt[0];
  Pt[2] = Pt[0] + Q;
  Kt = Pt[2] / (Pt[2] + R);
  
  Xt[1] = Xt[2] + (Kt * (input - Xt[2]));
  Pt[1] = (1 - Kt) * Pt[2];

  Xt[0] = Xt[1];
  Pt[0] = Pt[1];
}

void render_voltage(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 4, 1, num);
  sprintf(buff, "Teg: %sV", num);
  lcd.print(buff);
}

void render_current(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 4, 1, num);
  sprintf(buff, "Arus: %smA", num);
  lcd.print(buff);
}

void render_power(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 4, 1, num);
  sprintf(buff, "Power: %sW", num);
  lcd.print(buff);
}

void render_temp(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 4, 1, num);
  sprintf(buff, "TempP: %sC", num);
  lcd.print(buff);
}

void render_ir(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 4, 1, num);
  sprintf(buff, "IRR: %sW/m2", num);
  lcd.print(buff);
}

void render_temp_env(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 4, 1, num);
  sprintf(buff, "TempEnv: %sC", num);
  lcd.print(buff);
}

void render_hum(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 4, 1, num);
  sprintf(buff, "Hum: %s%%", num);
  lcd.print(buff);
}

void menu() {
  lcd.setCursor(0, 0);
  lcd.print("Panel Sendiri");
  switch(state_panel) {
    case 0:
      render_voltage(panel_sendiri.tegangan);
      if(d_btn < 0) {
        state_panel++;
        lcd.clear();
      }
      break;
    case 1:
      render_current(panel_sendiri.arus);
      if(d_btn < 0) {
        state_panel++;
        lcd.clear();
      }
      break;
    case 2:
      render_power(panel_sendiri.daya);
      if(d_btn < 0) {
        state_panel++;
        lcd.clear();
      }
      break;
    case 3:
      render_temp(panel_sendiri.suhu_panel);
      if(d_btn < 0) {
        state_panel++;
        lcd.clear();
      }
      break;
    case 4:
      render_ir(lux);
      if(d_btn < 0) {
        state_panel++;
        lcd.clear();
      }
      break;
    case 5:
      render_temp_env(env_temp);
      if(d_btn < 0) {
        state_panel++;
        lcd.clear();
      }
      break;
    case 6:
      render_hum(hum);
      if(d_btn < 0) {
        state_panel = 0;
        lcd.clear();
      }
      break;
  }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(V_MTR, INPUT);
  pinMode(BTN, INPUT_PULLUP);
  pinMode(RELAY, OUTPUT);

  INA_1.begin();
  luxmeter.begin();
  temp_sensor.begin();
  DHT22_sensor.begin();
  
  lcd.init();
  lcd.backlight();
  
  Serial.begin(115200);
}

void loop() {
  // put your main code here, to run repeatedly:
  temp_sensor.requestTemperatures();

  kalman(INA_1.getCurrent_mA(), Xt_c1, Pt_c1, Kt_c1);

  panel_sendiri.tegangan    = read_voltage(analogRead(V_MTR) / 10.0);
  panel_sendiri.arus        = Xt_c1[1];
  panel_sendiri.daya        = panel_sendiri.tegangan * (panel_sendiri.arus / 1000.0);
  panel_sendiri.suhu_panel  = read_temp(temp6);
  
  lux = luxmeter.readLightLevel() / 120.0;
  env_temp = DHT22_sensor.readTemperature();
  hum = DHT22_sensor.readHumidity();
  
  if(panel_sendiri.suhu_panel >= UPPER_TEMP) {
    rly_state = 1;
  } else if(panel_sendiri.suhu_panel < LOWER_TEMP) {
    rly_state = 0;
  }

  digitalWrite(RELAY, !rly_state);

  btn[1] = digitalRead(BTN);
  d_btn = btn[1] - btn[0];
  btn[0] = btn[1];
  
  menu();

  delay(10);
}
