#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "conf.h"

void email_send(EpromTags * config) {
  uint32_t start_time = millis();

  Serial.print("Device IP: ");
  Serial.println(WiFi.localIP());

  WiFiClientSecure client;

  Serial.printf("Connecting to: %s\r\n", config->host);
  if (!client.connect(config->host, config->httpsPort)) {
    Serial.printf("Connection failed\r\n");
    return;
  }

  client.print(String("GET ") + config->url + " HTTP/1.1\r\n" +
               "Host: " + config->host + "\r\n" + "Connection: close\r\n\r\n");
  Serial.printf("Request sent\r\n");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r")
      break;
  }

  String line = client.readStringUntil('\n');
  Serial.println(line);
  Serial.printf("HTTPS GET took: %lu ms\r\n", millis() - start_time);
}

