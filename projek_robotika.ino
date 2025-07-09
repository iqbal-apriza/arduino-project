#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <stdio.h>

Servo servo1;
LiquidCrystal_I2C lcd(0x27,  16, 2);

int sen1 = 2;
int sen2 = 3;
int relay = 4;
int servo = 5;
int s0 = 8;
int s1 = 9;
int s2 = 10;
int s3 = 11;
int out = 12;
long time;
long prev_time = 0;
long time_now = 0;
char tmp[3];

bool sen1_cond, sen2_cond, relay_cond;
bool detector = false;

int red, green, blue;
int color;  // 0 = Unknown ; 1 = Red ; 2 = Blue ; 3 = Green ; 4 = White

void setup() {
  // put your setup code here, to run once:
  pinMode(sen1, INPUT);     // Infrared 1
  pinMode(sen2, INPUT);     // Infrared 2
  pinMode(servo, OUTPUT);   // Servo
  pinMode(relay, OUTPUT);   // Relay
  pinMode(s0, OUTPUT);
  pinMode(s1, OUTPUT);
  pinMode(s2, OUTPUT);
  pinMode(s3, OUTPUT);
  pinMode(out, INPUT);
  // Serial.begin(9600);
  digitalWrite(s0, HIGH);
  digitalWrite(s1, HIGH);
  servo1.attach(5);
  servo1.write(92);
  lcd.init();
  lcd.backlight();
}

void loop() {
  time = millis();
  color = GetColors();
  sen1_cond = digitalRead(sen1);
  sen2_cond = digitalRead(sen2);

  if(!(sen1_cond & 1)) {
    time_now = time;
    servo1.write(45);
  }

  if(time - time_now >= 5000) {
    servo1.write(95);
  }

  // if(color == 1 || color == 2) {
  //   if(sen2_cond == 0) {
  //     digitalWrite(relay, LOW);
  //     if()
  //   } else if(send2_cond == 1) {
  //     digitalWrite(relay, HIGH);
  //   }
  // } else {
  //   digitalWrite(relay, HIGH);
  // }

  // if(!detector) {
  //   if(color == 1 || color == 2) {
  //     detector = true;
  //   }
  // }

  if(color == 1 || color == 2 || color == 3) {
    detector = true;
  }

  if(!(sen2_cond & 1) && detector) {
    relay_cond = 1;
  }

  if(sen2_cond) {
    relay_cond = 0;
  }

  if(relay_cond == 1 && sen2_cond == 0) {
    detector = false;
  }

  digitalWrite(relay, relay_cond);

  lcd.setCursor(0, 0);
  sprintf(tmp, "%03d", red);
  lcd.print(tmp);
  lcd.setCursor(0, 1);
  sprintf(tmp, "%03d", green);
  lcd.print(tmp);
  lcd.setCursor(5, 0);
  sprintf(tmp, "%03d", blue);
  lcd.print(tmp);
  lcd.setCursor(5, 1);
  switch (color) {
    case 0:
      lcd.print("Unknown");
      break;
    case 1:
      lcd.print("Red    ");
      break;
    case 2:
      lcd.print("Blue   ");
      break;
    case 3:
      lcd.print("Green  ");
      break;
    case 4:
      lcd.print("White  ");
      break;
    default:
      lcd.print("       ");
  }
  lcd.setCursor(9, 0);
  lcd.print(detector);

  // Serial.println(color);
  // Serial.print("Red = ");
  // Serial.print(red);
  // Serial.print(" Green = ");
  // Serial.print(green);
  // Serial.print(" Blue = ");
  // Serial.println(blue);
  // Serial.print(" ");
  // Serial.println(time - time_now);
  // delay(50);
}

int GetColors() {
  digitalWrite(s2, LOW);

  digitalWrite(s3, LOW);
  red = pulseIn(out, digitalRead(out) == HIGH ? LOW : HIGH);
  delay(20);

  digitalWrite(s3, HIGH);
  blue = pulseIn(out, digitalRead(out) == HIGH ? LOW : HIGH);
  delay(20);

  digitalWrite(s2, HIGH);
  green = pulseIn(out, digitalRead(out) == HIGH ? LOW : HIGH);
  delay(20);

  // if(red <= 15 && green <= 15 && blue <= 15) {
  //   return 4;  // White
  // } else if (red < blue && red <= green && red < 23) {
  //   return 1;  // Red
  // } else if (blue < green && blue < red && blue < 20) {
  //   return 2;  // Blue
  // } else if (green < red && green - blue <= 5) {
  //   return 3;  // Green
  // } else {
  //   return 0;  // Unknown
  // }

  if(red >= 10 && red <= 22 && green >= 27 && green <= 53 && blue >= 20 && blue <= 41) {
    return 1; // Red
  } else if(red >= 10 && red <= 29 && green >= 5 && green <= 16 && blue >= 3 && blue <= 12) {
    return 2; // Blue
  } else if(red >= 12 && red <= 17 && green >= 4 && green <= 13 && blue >= 13 && blue <= 20) {
    return 3; // Green
  } else {
    return 0; // Unknown
  }

}
