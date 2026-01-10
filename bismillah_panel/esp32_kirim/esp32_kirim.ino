#include <WiFi.h>
#include <HTTPClient.h>

const char *ssid = "YUDAPASHA";
const char *pass = "yuda543203";

const char *serverURL = "http://192.168.100.50/data";

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  WiFi.begin(ssid, pass);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi");
}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long tm = millis();
  float value = 10 * sin(tm / 1000.0);

  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String postData = "value=" + String(value, 2);

    http.begin(serverURL);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int code = http.POST(postData);
    Serial.println("Sent: " + postData + " | HTTP: " + String(code));

    http.end();
  }

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  delay(500);
}
