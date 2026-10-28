#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define HEADER1         0xFE
#define HEADER2         0xFD
#define PAYLOAD_SIZE    6


struct SysData {
  int cpu;
  int ram;
  int gpu;
  float temp;
  int test;
};


class PythonSerial {
public:
  uint8_t payload[PAYLOAD_SIZE];
  bool packet_ready;

  PythonSerial() {}

  void connect() {
    while(!Serial);
    
    while(Serial.available() && Serial.read());
    while(Serial.available() == 0) {
      Serial.write(0xAA);
      delay(500);
    }
    while(Serial.available() && Serial.read());

    reset_rx();
  }

  void request_data() {
    Serial.write(0xAB);
  }

  void read_serial() {
    time_ = millis();
    if(time_ - prev_time_ > 500)
      if(rx_state_ != FIND_H1) reset_rx();

    while(Serial.available()) {
      int v = Serial.read();
      if(v < 0) break;
      uint8_t b = (uint8_t)v;
      prev_time_ = millis();

      switch(rx_state_) {
        case FIND_H1:
          if(b == HEADER1)
            rx_state_ = FIND_H2;

          break;

        case FIND_H2:
          if(b == HEADER2)
            rx_state_ = READ_PAYLOAD;

          else if(b == HEADER1)
            rx_state_ = FIND_H2;

          else
            rx_state_ = FIND_H1;

          break;

        case READ_PAYLOAD:
          payload[payload_idx_++] = b;
          checksum_ += b;

          if(payload_idx_ >= PAYLOAD_SIZE)
            rx_state_ = CHECKSUM;

          break;

        case CHECKSUM:
          uint8_t checksum_rx = b;
          uint8_t checksum_pl = (uint8_t)((~checksum_) & 0xFF);

          if(checksum_rx == checksum_pl)
            packet_ready = true;
            
          rx_state_ = FIND_H1;
          break;
      }
    }
  }

  void reset_serial() {
    reset_rx();
  }

  SysData parse_serial() {
    SysData res;
    res.cpu = payload[0];
    res.ram = payload[1];
    res.gpu = payload[2];

    uint32_t temp_int = ((uint32_t)payload[3] & 0xFFUL) |
                         (((uint32_t)payload[4] & 0xFFUL) << 8) |
                         (((uint32_t)payload[5] & 0xFFUL) << 16);
    
    res.temp = (float)temp_int / 1000.0f;

    return res;
  }
  
private:
  enum RxState {
    FIND_H1, FIND_H2, READ_CPU, READ_PAYLOAD, CHECKSUM, PARSE
  };

  RxState rx_state_;
  unsigned long time_, prev_time_;
  int payload_idx_;
  uint16_t checksum_;

  SysData res;

  void reset_rx() {
    rx_state_ = FIND_H1;
    packet_ready = false;
    payload_idx_ = 0;
    checksum_ = 0;
    prev_time_ = millis();
  }
};


void render_cpu(int val, LiquidCrystal_I2C &lcd) {
  char buff[5];
  snprintf(buff, sizeof(buff), "%-3d%%", val);

  lcd.setCursor(2, 0);
  lcd.print(buff);
}

void render_ram(int val, LiquidCrystal_I2C &lcd) {
  char buff[5];
  snprintf(buff, sizeof(buff), "%-3d%%", val);

  lcd.setCursor(2, 1);
  lcd.print(buff);
}

void render_gpu(int val, LiquidCrystal_I2C &lcd) {
  char buff[5];
  snprintf(buff, sizeof(buff), "%-3d%%", val);

  lcd.setCursor(10, 0);
  lcd.print(buff);
}

void render_temp(float val, LiquidCrystal_I2C &lcd) {
  char buff[7];
  char num[4];

  dtostrf(val, 4, 1, num);
  snprintf(buff, sizeof(buff), "%sC", num);

  lcd.setCursor(10, 1);
  lcd.print(buff);
}


PythonSerial py;
LiquidCrystal_I2C lcd(0x27, 16, 2);

byte cpu_icon[] = {0x0A, 0x1F, 0x11, 0x11, 0x11, 0x1F, 0x0A, 0x00};
byte ram_icon[] = {0x1F, 0x15, 0x15, 0x15, 0x1F, 0x0A, 0x1F, 0x00};
byte gpu_icon[] = {0x1F, 0x15, 0x1F, 0x15, 0x1F, 0x0A, 0x1B, 0x11};
byte temp_icon[] = {0x04, 0x04, 0x04, 0x04, 0x0E, 0x0E, 0x0E, 0x04};


void setup() {
  // put your setup code here, to run once:
  Wire.begin();
  Wire.setWireTimeout(3000, true);

  lcd.init();
  lcd.backlight();

  lcd.createChar(0, cpu_icon);
  lcd.setCursor(0, 0);
  lcd.write(byte(0));

  lcd.createChar(1, ram_icon);
  lcd.setCursor(0, 1);
  lcd.write(byte(1));

  lcd.createChar(2, gpu_icon);
  lcd.setCursor(8, 0);
  lcd.write(byte(2));

  lcd.createChar(3, temp_icon);
  lcd.setCursor(8, 1);
  lcd.write(byte(3));
  
  Serial.begin(115200);
  py.connect();
}

void loop() {
  // put your main code here, to run repeatedly:
  py.request_data();
  delay(50);
  py.read_serial();

  char buff[15];
  
  if(py.packet_ready) {
    SysData stats = py.parse_serial();
    render_cpu(stats.cpu, lcd);
    render_ram(stats.ram, lcd);
    render_gpu(stats.gpu, lcd);
    render_temp(stats.temp, lcd);

//    uint32_t temp_int =
//    ((uint32_t)py.payload[3] & 0xFFUL) |
//    (((uint32_t)py.payload[4] & 0xFFUL) << 8) |
//    (((uint32_t)py.payload[5] & 0xFFUL) << 16);
//
//    lcd.setCursor(0, 1);
//    lcd.print(temp_int);
//
//    lcd.setCursor(0, 0);
//    sprintf(buff, "%x", py.payload[3]);
//    lcd.print(buff);
//
//    lcd.setCursor(3, 0);
//    sprintf(buff, "%x", py.payload[4]);
//    lcd.print(buff);
//
//    lcd.setCursor(6, 0);
//    sprintf(buff, "%x", py.payload[5]);
//    lcd.print(buff);

    py.reset_serial();
  }

  delay(1000);
}
