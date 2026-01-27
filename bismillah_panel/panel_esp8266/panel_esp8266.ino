#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_INA219.h>
#include <BH1750.h>
#include <DHT.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>

#define V_MTR         A0
#define TEMP_PIN      D0
#define DHT_PIN       D3
#define RELAY         D4
#define BTN           D5

#define DHTTYPE       DHT22

#define UPPER_TEMP    46
#define LOWER_TEMP    41

#define R             10
#define Q             0.4

#define WIFI_SSID     "PanelBismillah"
#define WIFI_PASS     "bismillah"

const char* URL_VIRA =
"https://script.google.com/macros/s/AKfycbyVjr44kB2X8aRnKXSnZ-eKELAHoRh1UUMpRyH1OFGHpO_VkiGQ5jV94r5YLvUIysv_WQ/exec";

const char* URL_HARLIN =
"https://script.google.com/macros/s/AKfycbyYPdy1E8q-22umrS73s6uS5IKBeqwmqod1KXeX3SHQrDhxWv65l-f0rcpTj6oMrzkRKQ/exec";

unsigned long lastSend = 0;
#define SEND_INTERVAL 60000

IPAddress localIP(192, 168, 43, 50);
IPAddress gateway(192, 168, 43, 1);
IPAddress subnet(255, 255, 255, 0);

IPAddress dns1(8, 8, 8, 8);
IPAddress dns2(8, 8, 4, 4);

ESP8266WebServer server(80);

OneWire oneWire(TEMP_PIN);
DallasTemperature temp_sensor(&oneWire);
LiquidCrystal_I2C lcd(0x27, 16, 2);
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

float lux = 0;
float env_temp, hum;

bool rly_state = 0;
bool btn[2] = {1, 0};
int8_t d_btn;

unsigned long t[2] = {0};

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
  lcd.print("Wifi Connected ");

  server.on("/data", handleData);
  server.begin();
  Serial.println("HTTP server started");

  lcd.setCursor(0, 0);
  lcd.print("HTTP server");
  lcd.setCursor(0, 1);
  lcd.print("started");
}

float read_voltage(float val) {
  return 1.71799432e-06 * (val * val * val) - 1.63616114e-04 * (val * val) +  2.50498137e-01 * val - 1.18494463e-01;
}

float read_temp(DeviceAddress deviceaddress) {
  float tempC = temp_sensor.getTempC(deviceaddress);
  return tempC;
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

void menu() {
  switch(state_panel) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Panel Gabungan");
      render_voltage(panel_sendiri.tegangan);
      if(d_btn < 0) {
        state_panel++;
        lcd.clear();
      }
      break;
    case 1:
      lcd.setCursor(0, 0);
      lcd.print("Panel Gabungan");
      render_current(panel_sendiri.arus);
      if(d_btn < 0) {
        state_panel++;
        lcd.clear();
      }
      break;
    case 2:
      lcd.setCursor(0, 0);
      lcd.print("Panel Gabungan");
      render_power(panel_sendiri.daya);
      if(d_btn < 0) {
        state_panel++;
        lcd.clear();
      }
      break;
    case 3:
      lcd.setCursor(0, 0);
      lcd.print("Panel Gabungan");
      render_temp(panel_sendiri.suhu_panel);
      if(d_btn < 0) {
        state_panel++;
        lcd.clear();
      }
      break;
    case 4:
      lcd.setCursor(0, 0);
      lcd.print("Panel Gabungan");
      render_lux(lux);
      if(d_btn < 0) {
        state_panel++;
        lcd.clear();
      }
      break;
    case 5:
      lcd.setCursor(0, 0);
      lcd.print("Panel Gabungan");
      render_temp_env(env_temp);
      if(d_btn < 0) {
        state_panel++;
        lcd.clear();
      }
      break;
    case 6:
      lcd.setCursor(0, 0);
      lcd.print("Panel Gabungan");
      render_hum(hum);
      if(d_btn < 0) {
        state_panel++;
        lcd.clear();
      }
      break;
    case 7:
      lcd.setCursor(0, 0);
      lcd.print("WiFi Strength");
      render_wifi();
      if(d_btn < 0) {
        state_panel = 0;
        lcd.clear();
      }
      break;
  }
}

void sendToSheet(const char* url, const char* sheetName) {
  if (WiFi.status() != WL_CONNECTED) return;

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure(); 

  HTTPClient http;
  http.setTimeout(10000);
  http.begin(*client, url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<512> doc;

  doc["sheet"]        = sheetName;
  doc["radiasi"]      = lux;
  doc["suhu"]         = env_temp;
  doc["kelembaban"]   = hum;
  doc["status"]       = rly_state ? "Aktif" : "Non-Aktif";

  doc["rf_suhu"]      = panel_sendiri.suhu_panel;
  doc["rf_arus"]      = panel_sendiri.arus / 1000.0;
  doc["rf_tegangan"]  = panel_sendiri.tegangan;
  doc["rf_pout"]      = panel_sendiri.daya;

  String payload;
  serializeJson(doc, payload);

  int code = http.POST(payload);

  Serial.println("📤 " + String(sheetName));
  Serial.println(payload);
  Serial.print("HTTP: ");
  Serial.println(code);

  http.end();
}

void handleData() {
  if(server.hasArg("value")) {
    String value = server.arg("value");
    lux = value.toFloat();
    Serial.println("Received from ESP32: " + value);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "No value");
  }
}

void setup() {
  pinMode(V_MTR, INPUT);
  pinMode(BTN, INPUT_PULLUP);
  pinMode(RELAY, OUTPUT);

  digitalWrite(RELAY, HIGH);

  INA_1.begin();
  temp_sensor.begin();
  DHT22_sensor.begin();
  
  lcd.init();
  lcd.backlight();

  Serial.begin(115200);
  delay(200);

  connectWiFi();
  lcd.clear();
}

void loop() {
  server.handleClient();
  
  t[1] = millis();
  float dt = ((float)t[1] - (float)t[0]) / 1000.0;
  
  temp_sensor.requestTemperatures();

  kalman(INA_1.getCurrent_mA(), Xt_c1, Pt_c1, Kt_c1, dt);

  panel_sendiri.tegangan    = read_voltage(analogRead(V_MTR) / 10.0);
  panel_sendiri.arus        = max(Xt_c1[1], 0.0f);
  panel_sendiri.daya        = panel_sendiri.tegangan * (panel_sendiri.arus / 1000.0);
  panel_sendiri.suhu_panel  = read_temp(temp6);
  
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

  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Send to");

    lcd.setCursor(0, 1);
    lcd.print("VIRA_8266");
    sendToSheet(URL_VIRA,   "VIRA_8266");

    lcd.setCursor(0, 1);
    lcd.print("HARLIN_8266");
    sendToSheet(URL_HARLIN, "HARLIN_8266");

    lcd.clear();
  } 

  delay(10);
  t[0] = t[1];
}
