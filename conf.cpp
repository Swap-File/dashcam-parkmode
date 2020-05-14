#include <Arduino.h>
#include <Preferences.h>
#include "conf.h"

void conf_print(EpromTags *config) {
  Serial.printf("WiFi SSID: %s\r\n", config->ssid);
  Serial.printf("WiFi Password: %s\r\n", config->password);
  Serial.printf("Host: %s\r\n", config->host);
  Serial.printf("URL: %s\r\n", config->url);
  Serial.printf("Port %d\r\n", config->httpsPort);
  Serial.printf("Home Connection Attemps (retries): %d\r\n",
                config->home_connection_attempts);
  Serial.printf("Home Connection Timeout (seconds): %d\r\n",
                config->home_connection_timeout);
  Serial.printf("Home Shutdown Delay (minutes): %d\r\n",
                config->home_shutdown_delay);
}

void conf_init(EpromTags *config) {
  
  config->init = 0x00;

  strncpy(config->ssid, "", sizeof(config->ssid));
  strncpy(config->password, "", sizeof(config->password));
  strncpy(config->host, "", sizeof(config->host));
  strncpy(config->url, "", sizeof(config->url));

  config->httpsPort = 443;
  config->home_connection_attempts = 2;
  config->home_connection_timeout = 20;
  config->home_shutdown_delay = 1;
}

void conf_get(EpromTags *config) {
  Preferences prefs;
  prefs.begin("conf");
  prefs.getBytes("conf", config->raw, sizeof(EpromTags));
  prefs.end();
}

void conf_put(EpromTags *config) {
      Preferences prefs;
      prefs.begin("conf");
      prefs.putBytes("conf", config->raw, sizeof(EpromTags));
      prefs.end();
}