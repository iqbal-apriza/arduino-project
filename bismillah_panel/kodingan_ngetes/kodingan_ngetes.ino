#include <stdio.h>
#include <math.h>
#include <Adafruit_INA219.h>
#include <BH1750.h>

#define V_PIN     36

//Adafruit_INA219 INA_1;
BH1750 luxmeter;

float value = 0;
float alpha = 0.2;

float sensor, output;
float Xt_C1[3] = {0};
float Pt_C1[3] = {0};
float Kt, R, Q;

float read_volt(int val) {
  return ((float)(val / 4096.0) * 4.1) / (0.122);
}

float read_volt2(double val) {
  return val / (0.128205128205);
}

double adc_calc(int val) {
  return 2.74796313e-12 * (val * val * val) - 4.95153221e-08 * (val * val) + 9.36722739e-04 * (val) + 4.92313549e-02;
}

double adc2_calc(double val) {
  return 1.91647006e-12 * (val * val * val * val * val) - 1.97319613e-09 * (val * val * val * val) + 7.15691209e-07 * (val * val * val) - 1.10838629e-04 * (val * val) + 1.49329544e-02 * val + 5.50448920e-03;
}

double adc_akor(double val) {
  return 1.80738944e-12 * (val * val * val * val * val) - 1.86503300e-09 * (val * val * val * val) + 6.72609191e-07 * (val * val * val) - 1.01223233e-04 * (val * val) + 1.37882775e-02 * val + 1.65699497e-02;
}

float adc_linear(int val) {
  return 0.000773 * val + 0.139679;
}

void kalman(float input, float *Xt, float *Pt) {
  Xt[2] = Xt[0];
  Pt[2] = Pt[0] + Q;
  Kt = Pt[2] / (Pt[2] + R);
  Xt[1] = Xt[2] + (Kt * (input - Xt[2]));
  Pt[1] = (1 - Kt) * Pt[2];

  Xt[0] = Xt[1];
  Pt[0] = Pt[1];
}

void setup() {
  // put your setup code here, to run once:
//  pinMode(V_PIN, INPUT);
//  INA_1.begin();
  luxmeter.begin();
  Serial.begin(115200);
  R = 10;
  Q = 0.5;
}

void loop() {
  // put your main code here, to run repeatedly:

//  float current = INA_1.getCurrent_mA();
  float lux = luxmeter.readLightLevel();
//  kalman(current, Xt_C1, Pt_C1);

//  value = alpha * adc + (1 - alpha) * value;
//  double voltage = adc_akor((double)value / 10.0);
//  double voltage2 = read_volt2(voltage);

//  Serial.print(voltage);
//  Serial.print(" | ");value
//  Serial.print(voltage2);
//  Serial.print(" | ");
  Serial.println(lux);
//  delay(50);
}
