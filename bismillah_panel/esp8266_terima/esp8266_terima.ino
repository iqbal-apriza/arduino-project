#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char *ssid = "YUDAPASHA";
const char *pass = "yuda543203";

IPAddress localIP(192, 168, 100, 50);
IPAddress gateway(192, 168, 100, 1);
IPAddress subnet(255, 255, 255, 0);

IPAddress dns1(8, 8, 8, 8);
IPAddress dns2(8, 8, 4, 4);

ESP8266WebServer server(80);

void handleData() {
  if(server.hasArg("value")) {
    String value = server.arg("value");
    Serial.println("Received from ESP32: " + value);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "No value");
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  if(!WiFi.config(localIP, gateway, subnet, dns1, dns2)) {
    Serial.println("Failed to configure static IP");
  }

  WiFi.begin(ssid, pass);

  Serial.print("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("ESP8266 IP: ");
  Serial.println(WiFi.localIP());

  server.on("/data", handleData);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();
}
