#include <stdio.h>
#include <math.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <Adafruit_INA219.h>
#include <BH1750.h>
#include <LiquidCrystal_I2C.h>

#define V_MTR1      36
#define V_MTR2      39
#define V_MTR3      34

#define RLY_TEC     18
#define RLY_MIS     5

#define BTN_1       17
#define BTN_2       16

#define TEMP_PIN    26
#define DHT_PIN     4
#define DHTTYPE     DHT22

//#define FLOW_PIN    2

#define ALPHA       0.01

#define UPPER_TEMP  40
#define LOWER_TEMP  35

#define R           10
#define Q           0.5

OneWire oneWire(TEMP_PIN);
DallasTemperature temp_sensor(&oneWire);
DHT DHT22_sensor(DHT_PIN, DHTTYPE);
BH1750 luxmeter;
LiquidCrystal_I2C lcd(0x27, 16, 2);

DeviceAddress dev_addr;

Adafruit_INA219 INA_1(0x40);
Adafruit_INA219 INA_2(0x41);
Adafruit_INA219 INA_3(0x44);

Adafruit_INA219 ina1;

typedef struct Panel {
  double tegangan;
  float arus;
  float daya;
  float suhu_panel;
  float suhu_tec;
} panel;

panel panel_biasa;
panel panel_tec;
panel panel_mist;

int adc_val = 0;
int num_devices = 0;

long tm = 0;
long prev_tm = 0;

char buff[256];
char num[8];

uint8_t temp_panel1[8] = {0x28, 0xFE, 0xA4, 0xB2, 0x00, 0x00, 0x00, 0xC5}; // Sensor suhu 1
uint8_t temp_panel2[8] = {0x28, 0x32, 0xC4, 0xB2, 0x00, 0x00, 0x00, 0x36}; // Sensor suhu 2
uint8_t temp_panel3[8] = {0x28, 0x20, 0xD7, 0xB3, 0x00, 0x00, 0x00, 0xBE}; // Sensor suhu 3
uint8_t temp_tec[8]    = {0x28, 0xC8, 0x87, 0xB9, 0x00, 0x00, 0x00, 0xB8}; // Sensor suhu 4

uint8_t temp5[8] = {0x28, 0x28, 0xBF, 0xB2, 0x00, 0x00, 0x00, 0xBF}; // Sensor suhu 5
uint8_t temp6[8] = {0x28, 0x6B, 0x79, 0xB9, 0x00, 0x00, 0x00, 0xB9}; // Sensor suhu 6

float calibrationFactor = 4.5;
volatile byte pulseCount = 0;
float flowRate = 0;
unsigned long oldTime = 0;

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

void printAddress(DeviceAddress deviceAddress) {
  for(uint8_t i = 0; i < 8; i++) {
    Serial.print("0x");
    if(deviceAddress[i] < 0x10) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
    if(i < 7) Serial.print(", ");
  }
  Serial.println("");
}

void pulseCounter() {
  pulseCount++;
}

float read_temp(DeviceAddress deviceaddress) {
  float tempC = temp_sensor.getTempC(deviceaddress);
  return tempC;
}

void read_dht(float &temp, float &hum) {
  temp = DHT22_sensor.readTemperature();
  hum = DHT22_sensor.readHumidity();
}

//double adc_akor(double val) {
//  return 1.80738944e-12 * (val * val * val * val * val) - 1.86503300e-09 * (val * val * val * val) + 6.72609191e-07 * (val * val * val) - 1.01223233e-04 * (val * val) + 1.37882775e-02 * val + 1.65699497e-02;
//}

double read_voltage(double val) {
  return 4.76519633e-12 * (val * val * val * val * val) - 5.55330768e-09 * (val * val * val * val) + 2.16590213e-06 * (val * val * val) - 3.54001049e-04 * (val * val) + 8.62879537e-02 * val + 5.23825095e-01;
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

void render_temp_tec(float val) {
  lcd.setCursor(0, 1);
  dtostrf(val, 4, 1, num);
  sprintf(buff, "TempTEC: %sC", num);
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
      render_voltage((float)panel_biasa.tegangan);
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
      render_panel2();
      if(d_bt1 < 0) {
        state_panel++;
        state_menu = 0;
        lcd.clear();
      }
      break;
    case 2:
      render_panel3();
      if(d_bt1 < 0) {
        state_panel++;
        state_menu = 0;
        lcd.clear();
      }
      break;
    case 3:
      lcd.setCursor(0, 0);
      lcd.print("Irradiance");
      render_ir(lux);
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
        state_panel = 0;
        state_menu = 0;
        lcd.clear();
      }
      break;
  }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(V_MTR1, INPUT);
  pinMode(V_MTR2, INPUT);
  pinMode(V_MTR3, INPUT);

  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);

  pinMode(RLY_TEC, OUTPUT);
  pinMode(RLY_MIS, OUTPUT);
  
//  pinMode(FLOW_PIN, INPUT);
//  digitalWrite(FLOW_PIN, HIGH);
  
  temp_sensor.begin();
  DHT22_sensor.begin();
  
  INA_1.begin();
  INA_2.begin();
  INA_3.begin();

  ina1.begin();
  
  luxmeter.begin();
  lcd.init();
  lcd.backlight();
  
  num_devices = temp_sensor.getDeviceCount();

//  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);
  Serial.begin(115200);
}

void loop() {
  // put your main code here, to run repeatedly:  
  temp_sensor.requestTemperatures();

  kalman(INA_1.getCurrent_mA(), Xt_c1, Pt_c1, Kt_c1);
  kalman(INA_2.getCurrent_mA(), Xt_c2, Pt_c2, Kt_c2);
  kalman(INA_3.getCurrent_mA(), Xt_c3, Pt_c3, Kt_c3);
  
  panel_biasa.tegangan    = read_voltage(analogRead(V_MTR1) / 10.0);
  panel_biasa.arus        = Xt_c1[1];
  panel_biasa.daya        = panel_biasa.tegangan * (panel_biasa.arus / 1000.0);
  panel_biasa.suhu_panel  = read_temp(temp_panel1);
  panel_biasa.suhu_tec    = 0;
  
  panel_tec.tegangan      = read_voltage(analogRead(V_MTR2) / 10.0);
  panel_tec.arus          = Xt_c2[1];
  panel_tec.daya          = panel_tec.tegangan * (panel_tec.arus / 1000.0);
  panel_tec.suhu_panel    = read_temp(temp_panel2);
  panel_tec.suhu_tec      = read_temp(temp_tec);
  
  panel_mist.tegangan     = read_voltage(analogRead(V_MTR3) / 10.0);
  panel_mist.arus         = Xt_c3[1];
  panel_mist.daya         = panel_mist.tegangan * (panel_mist.arus / 1000.0);
  panel_mist.suhu_panel   = read_temp(temp_panel3);
  panel_mist.suhu_tec     = 0;

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

  delay(100);
  
//  if((millis() - oldTime) > 1000) {
//    detachInterrupt(digitalPinToInterrupt(FLOW_PIN));
//    flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
//    oldTime = millis();
//
//    Serial.print("Flow rate: ");
//    Serial.print(flowRate);
//    Serial.println(" L/min");
//  }
  
//  float DHT22_Hum = DHT22_sensor.readHumidity();
//  float DHT22_TempC = DHT22_sensor.readTemperature();
//  float current_mA = ina219.getCurrent_mA();

//  Serial.println(lux);

//  Serial.print("Humidity: ");
//  Serial.print(DHT22_Hum);
//  Serial.print(" % , ");
//  Serial.print("Temperature: "); 
//  Serial.print(DHT22_TempC);
//  Serial.println(" °C");
  
//  Serial.println("Printing address...");
//  for(int i = 0; i < num_devices; i++) {
//    Serial.print("Sensor ");
//    Serial.print(i + 1);
//    Serial.print(" : ");
//    temp_sensor.getAddress(dev_addr, i);
//    printAddress(dev_addr);
//  }

//  sensors.requestTemperatures();
//  Serial.print("Celsius temperature: ");
//  Serial.println(sensors.getTempCByIndex(0));
//  delay(1000);
  
//  adc_val = analogRead(ADC_PIN);
//  Serial.println(adc_val);
//  digitalWrite(RELAY, HIGH);
//  delay(1000);
//  digitalWrite(RELAY, LOW);
//  delay(1000);
//  prev_tm = tm;
}
