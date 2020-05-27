#include <Arduino.h>
#include "conf.h"
#include "sleep.h"
#include "server.h"

/* 
Espressif IDF 1.9.0 (! THIS IS OLDER THAN CURRENT !)
Dependency Graph
|-- <ESP Async WebServer> 1.2.3
|   |-- <AsyncTCP> 1.1.1
|   |-- <FS> 1.0
|   |-- <WiFi> 1.0
|-- <AsyncTCP> 1.1.1
|-- <WiFi> 1.0
|-- <DNSServer> 1.1.0
|   |-- <WiFi> 1.0
*/

// EEPROM stores the persistant config across power cycles
EpromTags config;

void setup() {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  Serial.begin(115200);

  conf_get(&config);

  pinMode(CONFIG_PIN, INPUT_PULLUP);
  pinMode(INDICATOR_LED, OUTPUT);

  if (digitalRead(CONFIG_PIN) == 0) {
    Serial.printf("Starting Configuration...\r\n");

    // this will never return
    server_start(&config);

  } else {

    // this will never exit if entered
    while (config.init != INIT_VALUE) {
      Serial.println("No configuration set, please connect config jumper and reboot.");
      // lazy fast blink that doubles as a delay between serial messages
      digitalWrite(INDICATOR_LED, 1);
      delay(100);
      digitalWrite(INDICATOR_LED, 0);
      delay(100);
    }

    // this will never return
    sleep_logic(&config);
  }
}

void loop() {
}