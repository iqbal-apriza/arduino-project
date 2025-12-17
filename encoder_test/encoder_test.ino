#define DATA_A    3
#define DATA_B    12
#define DATA_Z    7

long pulse = 0;
int pulse_now = 0;
int prev_pulse = 0;

void enc_calc() {
  if(digitalRead(DATA_B) == 1) {
    pulse++;
  } else {
    pulse--;
  }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(DATA_A, INPUT_PULLUP);
  pinMode(DATA_B, INPUT_PULLUP);
  pinMode(DATA_Z, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(DATA_A), enc_calc, FALLING);
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
//  Serial.print(digitalRead(DATA_A));
//  Serial.print(" | ");
//  Serial.print(digitalRead(DATA_B));
//  Serial.print(" | ");
//  Serial.println(digitalRead(DATA_Z));
  int pulse_now = pulse;
  float d_pulse = pulse_now - prev_pulse;
//  Serial.print(pulse_now);
  Serial.print(" | ");
  Serial.println(d_pulse);
  prev_pulse = pulse_now;
}
