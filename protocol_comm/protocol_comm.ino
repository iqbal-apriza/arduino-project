#define POT           A0

#define HEADER1       0x49
#define HEADER2       0x4E
#define HEADER3       0x41
#define MAX_PAYLOAD   64
#define TIMEOUT       200

float dat1, dat2;

enum RxState {FIND_H1, FIND_H2, FIND_H3, DATA_LEN, PAYLOAD, CHECKSUM};
RxState rx_state = FIND_H1;

uint8_t lenByte = 0;
uint8_t payload[MAX_PAYLOAD];
uint8_t sum = 0;
size_t payloadPos = 0;

bool packet_ready = false;
uint16_t last_millis = 0;

void send_data(float data1, float data2) {
  uint8_t TxData[13];

  union {
    float f;
    uint8_t b[4];
  } conv1, conv2;

  conv1.f = data1;
  conv2.f = data2;

  TxData[0] = HEADER1;
  TxData[1] = HEADER2;
  TxData[2] = HEADER3;

  TxData[3] = 8;

  TxData[4] = conv1.b[0];
  TxData[5] = conv1.b[1];
  TxData[6] = conv1.b[2];
  TxData[7] = conv1.b[3];

  TxData[8] = conv2.b[0];
  TxData[9] = conv2.b[1];
  TxData[10] = conv2.b[2];
  TxData[11] = conv2.b[3];

  TxData[12] = 0;
  for(int i = 3; i < 12; i++) {
    TxData[12] += TxData[i];
  }
  TxData[12] = ~TxData[12];

  Serial.write(TxData, sizeof(TxData));
}

void reset_rx() {
  rx_state = FIND_H1;
  payloadPos = 0;
  lenByte = 0;
  sum = 0;
  packet_ready = false;
  last_millis = millis();
}

void read_serial() {
  if(millis() - last_millis > TIMEOUT) {
    if(rx_state != FIND_H1) {
      reset_rx();
    }
  }

  while(Serial.available()) {
    int v = Serial.read();
    if(v < 0) {
      break;
    }
    uint8_t b = (uint8_t)v;
    last_millis = millis();

    switch(rx_state) {
      case FIND_H1:
        if(b == HEADER1) {
          rx_state = FIND_H2;
        }
        break;
      case FIND_H2:
        if(b == HEADER2) {
          rx_state = FIND_H3;
        } else if(b == HEADER1) {
          rx_state = FIND_H2;
        } else {
          rx_state = FIND_H1;
        }
        break;
      case FIND_H3:
        if(b == HEADER3) {
          rx_state = DATA_LEN;
        } else if(b == HEADER1) {
          rx_state = FIND_H2;
        } else {
          rx_state = FIND_H1;
        }
        break;
      case DATA_LEN:
        lenByte = b;
        if(lenByte == 0 || lenByte > MAX_PAYLOAD) {
          rx_state = FIND_H1;
        } else {
          payloadPos = 0;
          sum = 0;
          rx_state = PAYLOAD;
        }
        break;
      case PAYLOAD:
        payload[payloadPos++] = b;
        sum = (uint8_t)((sum + b) & 0xFF);
        if(payloadPos >= lenByte) {
          rx_state = CHECKSUM;
        }
        break;
      case CHECKSUM:
        uint8_t checksum_rx = b;
        uint8_t checksum_pay = (uint8_t)((~sum) & 0xFF);
        if(checksum_rx == checksum_pay) {
          packet_ready = true;
        }
        rx_state = FIND_H1;
        break;
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(POT, INPUT);
  Serial.begin(115200);
}

void loop() {
  // put your main code here, to run repeatedly:
  int value = analogRead(POT);
  dat1 = value * 2.3;
  dat2 = value * 3.3;
  send_data(dat1, dat2);
//  Serial.print(dat1);
//  Serial.print(" | ");
//  Serial.println(dat2);
  delay(10);
}
