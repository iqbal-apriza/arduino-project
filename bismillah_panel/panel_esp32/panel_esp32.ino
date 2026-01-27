#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <Adafruit_INA219.h>
#include <BH1750.h>
#include <LiquidCrystal_I2C.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#include "esp_bt.h"
#include "esp_wifi.h"

#define V_MTR1        36
#define V_MTR2        39
#define V_MTR3        34

#define RLY_TEC       18
#define RLY_MIS       5

#define BTN_1         17
#define BTN_2         16

#define TEMP_PIN      26
#define DHT_PIN       4
#define DHTTYPE       DHT22

#define FLOW_PIN      35

#define ALPHA         0.01
#define ALPHA_F       0.5

#define UPPER_TEMP    46
#define LOWER_TEMP    41

#define R             10
#define Q             0.5

#define WIFI_SSID     "PanelBismillah"
#define WIFI_PASS     "bismillah"

const char* URL_VIRA =
"https://script.google.com/macros/s/AKfycbyVjr44kB2X8aRnKXSnZ-eKELAHoRh1UUMpRyH1OFGHpO_VkiGQ5jV94r5YLvUIysv_WQ/exec";

const char* URL_HARLIN =
"https://script.google.com/macros/s/AKfycbyYPdy1E8q-22umrS73s6uS5IKBeqwmqod1KXeX3SHQrDhxWv65l-f0rcpTj6oMrzkRKQ/exec";

const char* URL_AUL =
"https://script.google.com/macros/s/AKfycbw_ED41QKOzKrdC6ibaJC1i7ZWOYWeFekUMfxfy0jfHiYGhe2RyRHAsckbbpPrbgzSF/exec";

const char *serverURL =
"http://192.168.43.50/data";

unsigned long lastSend = 0;
unsigned long last_lux_send = 0;
#define SEND_INTERVAL 60000

IPAddress localIP(192, 168, 43, 51);
IPAddress gateway(192, 168, 43, 1);
IPAddress subnet(255, 255, 255, 0);

IPAddress dns1(8, 8, 8, 8);
IPAddress dns2(8, 8, 4, 4);

OneWire oneWire(TEMP_PIN);
DallasTemperature temp_sensor(&oneWire);
DHT DHT22_sensor(DHT_PIN, DHTTYPE);
BH1750 luxmeter;
LiquidCrystal_I2C lcd(0x27, 16, 2);

DeviceAddress dev_addr;

Adafruit_INA219 INA_TEC(0x40);      // Tdk jumper
Adafruit_INA219 INA_MIST(0x41);     // A0
Adafruit_INA219 INA_BIASA(0x44);    // A1

typedef struct Panel {
  double tegangan;
  float arus;
  float daya;
  float suhu_panel;
  float suhu_tec;
  float suhu_liquid;
  float flowrate;
} panel;

panel panel_biasa;
panel panel_tec;
panel panel_mist;

int adc_val = 0;
int num_devices = 0;

char buff[256];
char num[8];

uint8_t temp_panel_tec[8]   = {0x28, 0x32, 0xC4, 0xB2, 0x00, 0x00, 0x00, 0x36}; // Sensor suhu 2 (P_TEC)
uint8_t temp_panel_biasa[8] = {0x28, 0x20, 0xD7, 0xB3, 0x00, 0x00, 0x00, 0xBE}; // Sensor suhu 3 (P_BIASA)
uint8_t temp_panel_mist[8]  = {0x28, 0xC8, 0x87, 0xB9, 0x00, 0x00, 0x00, 0xB8}; // Sensor suhu 4 (TEC)
uint8_t temp_tec[8]         = {0x28, 0xFE, 0xA4, 0xB2, 0x00, 0x00, 0x00, 0xC5}; // Sensor suhu 1 (P_MIST)
uint8_t temp_liq[8]         = {0x28, 0x28, 0xBF, 0xB2, 0x00, 0x00, 0x00, 0xBF}; // Sensor suhu 5 (RADIATOR)

volatile uint32_t pulse = 0;
unsigned long t1[2] = {0, 0};
unsigned long t2[2] = {0, 0};
uint32_t p[2] = {0, 0};
float dt, dt2, dp;
float flow = 0;
float freq_filter = 0;

float lux, env_temp, hum;

float Xt_c1[3] = {0};
float Pt_c1[3] = {0};

float Xt_c2[3] = {0};
float Pt_c2[3] = {0};

float Xt_c3[3] = {0};
float Pt_c3[3] = {0};

float Kt_c1, Kt_c2, Kt_c3;

bool rly_tec_state = 0;
bool rly_mis_state = 0;

bool btn1[2] = {1, 0};
bool btn2[2] = {1, 0};
int8_t d_bt1, d_bt2;

uint8_t state_menu = 0;
uint8_t state_panel = 0;

void connectWiFi() {
  if(!WiFi.config(localIP, gateway, subnet, dns1, dns2)) {
    Serial.println("Failed to configure static IP");
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting WiFi");
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\n✅ WiFi connected");
  lcd.setCursor(0, 0);
  lcd.print("WiFi connected");

  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G);
}

void printAddress(DeviceAddress deviceAddress) {
  for(uint8_t i = 0; i < 8; i++) {
    Serial.print("0x");
    if(deviceAddress[i] < 0x10) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
    if(i < 7) Serial.print(", ");
  }
  Serial.println("");
}

void pulse_count() {
  pulse++;
}

float read_temp(DeviceAddress deviceaddress) {
  float tempC = temp_sensor.getTempC(deviceaddress);
  return tempC;
}

void read_dht(float &temp, float &hum) {
  temp = DHT22_sensor.readTemperature();
  hum = DHT22_sensor.readHumidity();
}

double read_voltage(double val) {
  return 4.76519633e-12 * (val * val * val * val * val) - 5.55330768e-09 * (val * val * val * val) + 2.16590213e-06 * (val * val * val) - 3.54001049e-04 * (val * val) + 8.62879537e-02 * val + 5.23825095e-01;
}

void kalman(float input, float *Xt, float *Pt, float &Kt, float dt) {
  Xt[2] = Xt[0];
  Pt[2] = Pt[0] + Q * dt;
  Kt = Pt[2] / (Pt[2] + R);
  
  Xt[1] = Xt[2] + (Kt * (input - Xt[2]));
  Pt[1] = (1 - Kt) * Pt[2];

  Xt[0] = Xt[1];
  Pt[0] = Pt[1];
}

void render_voltage(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 6, 2, num);
  sprintf(buff, "Teg: %sV", num);
  lcd.print(buff);
}

void render_current(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 7, 1, num);
  sprintf(buff, "Arus: %smA", num);
  lcd.print(buff);
}

void render_power(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 5, 2, num);
  sprintf(buff, "Power: %sW", num);
  lcd.print(buff);
}

void render_temp(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 4, 1, num);
  sprintf(buff, "TempP: %sC", num);
  lcd.print(buff);
}

void render_temp_tec(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 4, 1, num);
  sprintf(buff, "TempTEC: %sC", num);
  lcd.print(buff);
}

void render_temp_liq(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 4, 1, num);
  sprintf(buff, "TempLiq: %sC", num);
  lcd.print(buff);
}

void render_flow(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 5, 1, num);
  sprintf(buff, "Flow: %smL/s", num);
  lcd.print(buff);
}

void render_lux(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 7, 1, num);
  sprintf(buff, "Lux: %slx", num);
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

void render_wifi() {
  lcd.setCursor(0, 1);
  if(WiFi.status() == WL_CONNECTED) {
    long rssi = WiFi.RSSI();
    sprintf(buff, "%5d dBm", (int)rssi);
    lcd.print(buff);
  } else {
    lcd.print("WiFi lost");
  }
}

void render_panel1() {
  lcd.setCursor(0, 0);
  lcd.print("Panel Biasa");
  switch(state_menu) {
    case 0:
      render_voltage((float)panel_biasa.tegangan);
      if(d_bt2 < 0) {
        state_menu++;
        lcd.clear();
      }
      break;
    case 1:
      render_current(panel_biasa.arus);
      if(d_bt2 < 0) {
        state_menu++;
        lcd.clear();
      }
      break;
    case 2:
      render_power(panel_biasa.daya);
      if(d_bt2 < 0) {
        state_menu++;
        lcd.clear();
      }
      break;
    case 3:
      render_temp(panel_biasa.suhu_panel);
      if(d_bt2 < 0) {
        state_menu = 0;
        lcd.clear();
      }
      break;
  }
}

void render_panel2() {
  lcd.setCursor(0, 0);
  lcd.print("Panel TEC");
  switch(state_menu) {
    case 0:
      render_voltage((float)panel_tec.tegangan);
      if(d_bt2 < 0) {
        state_menu++;
        lcd.clear();
      }
      break;
    case 1:
      render_current(panel_tec.arus);
      if(d_bt2 < 0) {
        state_menu++;
        lcd.clear();
      }
      break;
    case 2:
      render_power(panel_tec.daya);
      if(d_bt2 < 0) {
        state_menu++;
        lcd.clear();
      }
      break;
    case 3:
      render_temp(panel_tec.suhu_panel);
      if(d_bt2 < 0) {
        state_menu++;
        lcd.clear();
      }
      break;
    case 4:
      render_temp_tec(panel_tec.suhu_tec);
      if(d_bt2 < 0) {
        state_menu++;
        lcd.clear();
      }
      break;
    case 5:
      render_temp_liq(panel_tec.suhu_liquid);
      if(d_bt2 < 0) {
        state_menu++;
        lcd.clear();
      }
      break;
    case 6:
      render_flow(panel_tec.flowrate);
      if(d_bt2 < 0) {
        state_menu = 0;
        lcd.clear();
      }
      break;
  }
}

void render_panel3() {
  lcd.setCursor(0, 0);
  lcd.print("Panel Mist");
  switch(state_menu) {
    case 0:
      render_voltage((float)panel_mist.tegangan);
      if(d_bt2 < 0) {
        state_menu++;
        lcd.clear();
      }
      break;
    case 1:
      render_current(panel_mist.arus);
      if(d_bt2 < 0) {
        state_menu++;
        lcd.clear();
      }
      break;
    case 2:
      render_power(panel_mist.daya);
      if(d_bt2 < 0) {
        state_menu++;
        lcd.clear();
      }
      break;
    case 3:
      render_temp(panel_mist.suhu_panel);
      if(d_bt2 < 0) {
        state_menu = 0;
        lcd.clear();
      }
      break;
  }
}

void menu() {
  switch(state_panel) {
    case 0:
      render_panel1();
      if(d_bt1 < 0) {
        state_panel++;
        state_menu = 0;
        lcd.clear();
      }
      break;
    case 1:
      render_panel3();
      if(d_bt1 < 0) {
        state_panel++;
        state_menu = 0;
        lcd.clear();
      }
      break;
    case 2:
      render_panel2();
      if(d_bt1 < 0) {
        state_panel++;
        state_menu = 0;
        lcd.clear();
      }
      break;
    case 3:
      lcd.setCursor(0, 0);
      lcd.print("Illuminance");
      render_lux(lux);
      if(d_bt1 < 0) {
        state_panel++;
        state_menu = 0;
        lcd.clear();
      }
      break;
    case 4:
      lcd.setCursor(0, 0);
      lcd.print("Temp Lingkungan");
      render_temp_env(env_temp);
      if(d_bt1 < 0) {
        state_panel++;
        state_menu = 0;
        lcd.clear();
      }
      break;
    case 5:
      lcd.setCursor(0, 0);
      lcd.print("Kelembaban");
      render_hum(hum);
      if(d_bt1 < 0) {
        state_panel++;
        state_menu = 0;
        lcd.clear();
      }
      break;
    case 6:
      lcd.setCursor(0, 0);
      lcd.print("WiFi Strength");
      render_wifi();
      if(d_bt1 < 0) {
        state_panel = 0;
        state_menu = 0;
        lcd.clear();
      }
      break;
  }
}

void sendToSheet(const char* url, const char* sheet) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) return;
  }


  WiFiClientSecure client;
  client.setInsecure();


  HTTPClient http;
  http.setTimeout(10000);
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<1024> doc;

  // ================= BASE =================
  doc["sheet"]            = sheet;
  doc["radiasi"]          = lux;
  doc["suhu"]             = env_temp;
  doc["kelembaban"]       = hum;
  doc["status_tec"]       = rly_tec_state ? "Aktif" : "Non-Aktif";
  doc["status_mis"]       = rly_mis_state ? "Aktif" : "Non-Aktif";

  // ================= AULIA =================
  if (strcmp(sheet, "AULIA_32") == 0) {
    // ---- TP (Panel Biasa) ----
    doc["tp_suhu"]        = panel_biasa.suhu_panel;
    doc["tp_arus"]        = panel_biasa.arus / 1000.0;
    doc["tp_tegangan"]    = panel_biasa.tegangan;
    doc["tp_pout"]        = panel_biasa.daya;

    // ---- MC (Mist) ----
    doc["mc_suhu"]        = panel_mist.suhu_panel;
    doc["mc_arus"]        = panel_mist.arus / 1000.0;
    doc["mc_tegangan"]    = panel_mist.tegangan;
    doc["mc_pout"]        = panel_mist.daya;

    // ---- LC (TEC) ----
    doc["lc_suhu"]        = panel_tec.suhu_panel;
    doc["lc_arus"]        = panel_tec.arus / 1000.0;
    doc["lc_tegangan"]    = panel_tec.tegangan;
    doc["lc_pout"]        = panel_tec.daya;
  }

  // ================= VIRA =================
  if (strcmp(sheet, "VIRA_32") == 0) {
    doc["tp_suhu"]        = panel_biasa.suhu_panel;
    doc["tp_arus"]        = panel_biasa.arus / 1000.0;
    doc["tp_tegangan"]    = panel_biasa.tegangan;
    doc["tp_pout"]        = panel_biasa.daya;
  }

  // ================= HARLIN =================
  if (strcmp(sheet, "HARLIN_32") == 0) {
    // ---- TPR ----
    doc["tpr_suhu"]       = panel_biasa.suhu_panel;
    doc["tpr_arus"]       = panel_biasa.arus / 1000.0;
    doc["tpr_tegangan"]   = panel_biasa.tegangan;
    doc["tpr_pout"]       = panel_biasa.daya;

    // ---- LCR ----
    doc["lcr_suhu_p"]     = panel_tec.suhu_panel;
    doc["lcr_suhu_t"]     = panel_tec.suhu_tec;
    doc["lcr_suhu_l"]     = panel_tec.suhu_liquid;
    doc["lcr_flow"]       = panel_tec.flowrate / 1000000.0;
    doc["lcr_tegangan"]   = panel_tec.tegangan;
    doc["lcr_arus"]       = panel_tec.arus / 1000.0;
    doc["lcr_pout"]       = panel_tec.daya;
  }

  String payload;
  serializeJson(doc, payload);

  int code = http.POST(payload);

  Serial.println("📤 " + String(sheet));
  Serial.println(payload);
  Serial.println("HTTP: " + String(code));

  http.end();
}


void setup() {
  // put your setup code here, to run once:
  esp_bt_controller_disable();
  esp_bt_controller_deinit();

  pinMode(V_MTR1, INPUT);
  pinMode(V_MTR2, INPUT);
  pinMode(V_MTR3, INPUT);

  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);

  pinMode(RLY_TEC, OUTPUT);
  pinMode(RLY_MIS, OUTPUT);

  digitalWrite(RLY_TEC, HIGH);
  digitalWrite(RLY_MIS, HIGH);
  
  pinMode(FLOW_PIN, INPUT);
  
  temp_sensor.begin();
  DHT22_sensor.begin();
  
  INA_TEC.begin();
  INA_BIASA.begin();
  INA_MIST.begin();
  
  luxmeter.begin();
  lcd.init();
  lcd.backlight();
  
  num_devices = temp_sensor.getDeviceCount();

  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulse_count, RISING);
  Serial.begin(115200);
  delay(200);

  connectWiFi();
  lcd.clear();
}

void loop() {
  // put your main code here, to run repeatedly:
  long rssi2 = WiFi.RSSI();
  Serial.print(WiFi.status());
  Serial.print(" | ");
  Serial.println(rssi2);

//  Serial.println(WiFi.localIP());
  
  t1[1] = millis();
  t2[1] = millis();
  dt = ((float)t1[1] - (float)t1[0]) / 1000.0;
  dt2 = ((float)t2[1] - (float)t2[0]) / 1000.0;
  
  float freq = 0;
  if(dt >= 0.5) {
    noInterrupts();
    p[1] = pulse;
    interrupts();

    dp = p[1] - p[0];
    freq = dp / dt;

    t1[0] = t1[1];
    p[0] = p[1];
  }

  temp_sensor.requestTemperatures();

  kalman(INA_TEC.getCurrent_mA(), Xt_c1, Pt_c1, Kt_c1, dt2);
  kalman(INA_MIST.getCurrent_mA(), Xt_c2, Pt_c2, Kt_c2, dt2);
  kalman(INA_BIASA.getCurrent_mA(), Xt_c3, Pt_c3, Kt_c3, dt2);

  freq_filter = (ALPHA_F * freq_filter) + ((1 - ALPHA_F) * freq);
  flow = (freq_filter / 7.5) * (1000 / 60);

  panel_tec.tegangan        = read_voltage(analogRead(V_MTR1) / 10.0);
  panel_tec.arus            = max(Xt_c1[1], 0.0f);
  panel_tec.daya            = panel_tec.tegangan * (panel_tec.arus / 1000.0);
  panel_tec.suhu_panel      = read_temp(temp_panel_tec);
  panel_tec.suhu_tec        = read_temp(temp_tec);
  panel_tec.suhu_liquid     = read_temp(temp_liq);
  panel_tec.flowrate        = flow;

  panel_mist.tegangan       = read_voltage(analogRead(V_MTR2) / 10.0);
  panel_mist.arus           = max(Xt_c2[1], 0.0f);
  panel_mist.daya           = panel_mist.tegangan * (panel_mist.arus / 1000.0);
  panel_mist.suhu_panel     = read_temp(temp_panel_mist);

  panel_biasa.tegangan      = read_voltage(analogRead(V_MTR3) / 10.0);
  panel_biasa.arus          = max(Xt_c3[1], 0.0f);
  panel_biasa.daya          = panel_biasa.tegangan * (panel_biasa.arus / 1000.0);
  panel_biasa.suhu_panel    = read_temp(temp_panel_biasa);

  lux = luxmeter.readLightLevel();
  env_temp = DHT22_sensor.readTemperature();
  hum = DHT22_sensor.readHumidity();

  if(panel_tec.suhu_panel >= UPPER_TEMP) {
    rly_tec_state = 1;
  } else if(panel_tec.suhu_panel < LOWER_TEMP) {
    rly_tec_state = 0;
  }

  if(panel_mist.suhu_panel >= UPPER_TEMP) {
    rly_mis_state = 1;
  } else if(panel_mist.suhu_panel < LOWER_TEMP) {
    rly_mis_state = 0;
  }

  digitalWrite(RLY_TEC, !rly_tec_state);
  digitalWrite(RLY_MIS, !rly_mis_state);

  btn1[1] = digitalRead(BTN_1);
  d_bt1   = btn1[1] - btn1[0];
  btn1[0] = btn1[1];

  btn2[1] = digitalRead(BTN_2);
  d_bt2   = btn2[1] - btn2[0];
  btn2[0] = btn2[1];

  menu();

  if (millis() - last_lux_send >= 1000 && WiFi.status() == WL_CONNECTED) {
    last_lux_send = millis();
    HTTPClient http;
    http.setTimeout(1500);

    String postData = "value=" + String(lux, 1);

    http.begin(serverURL);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int code = http.POST(postData);
    Serial.println("Sent: " + postData + " | HTTP: " + String(code));

    http.end();
  }

  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Send to");

    lcd.setCursor(0, 1);
    lcd.print("VIRA_32");
    sendToSheet(URL_VIRA,   "VIRA_32");

    lcd.setCursor(0, 1);
    lcd.print("HARLIN_32");
    sendToSheet(URL_HARLIN, "HARLIN_32");

    lcd.setCursor(0, 1);
    lcd.print("AULIA_32 ");
    sendToSheet(URL_AUL,    "AULIA_32");

    lcd.clear();
  }

  delay(10);
  t2[0] = t2[1];
}
