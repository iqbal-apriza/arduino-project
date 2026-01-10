#define R       5
#define Q       1

#define ALPHA   0.5

volatile uint32_t pulse = 0;
int p[2] = {0};
unsigned long t1[2] = {0};
unsigned long t2[2] = {0};
float dp, dt, dt2, T;
float flow;
float freq = 0;

float Xt_f[3] = {0};
float Pt_f[3] = {0};
float Kt_f = 0;

float value = 0;

void pulse_count() {
  pulse++;
}

//float read_flow() {
//  float freq = 1000000 / T;     // Freq Hz
//  float flow = freq / 7.5;      // L/min
//  return flow / 60000;          // m3/s
//}

void kalman(float input, float *Xt, float *Pt, float &Kt, float dt) {
  Xt[2] = Xt[0];
  Pt[2] = Pt[0] + Q * dt;
  Kt = Pt[2] / (Pt[2] + R);
  
  Xt[1] = Xt[2] + (Kt * (input - Xt[2]));
  Pt[1] = (1 - Kt) * Pt[2];

  Xt[0] = Xt[1];
  Pt[0] = Pt[1];
}

void setup() {
  // put your setup code here, to run once:
  attachInterrupt(digitalPinToInterrupt(2), pulse_count, RISING);
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  t1[1] = millis();
  t2[1] = millis();
  dt = (t1[1] - t1[0]) / 1000.0;
  dt2 = (t2[1] - t2[0]) / 1000.0;

  if(dt >= 0.5) {
    noInterrupts();
    p[1] = pulse;
    interrupts();

    dp = p[1] - p[0];
    p[0] = p[1];

    freq = dp / dt;
    flow = (freq / 7.5) * (1000 / 60);

    t1[0] = t1[1];
  }

  value = (ALPHA * value) + ((1 - ALPHA) * freq);
  float flow_baru = (value / 7.5) * (1000 / 60);

  Serial.print(value);
  Serial.print(" | ");
  Serial.print(flow_baru);
  Serial.print(" | ");
  Serial.println(flow);
//  Serial.print
//  Serial.println(((1000000.0 / T) / 7.5) * (1000 / 60));
  delay(500);

  t2[0] = t2[1];
}
