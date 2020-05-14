#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <driver/rtc_io.h>
#include "EEPROM.h"

#define INIT_VALUE 0xA1

// data that gets saved to eeprom
typedef struct {
  char init;

  char ssid[32];
  char password[32];

  char host[32];
  char url[100];
  int httpsPort;

  int home_connection_attempts; // retries
  int home_connection_timeout;  // seconds
  int home_shutdown_delay;      // minutes
} EpromTags;

EpromTags config;

// pins things are physically hooked up to (pins must have RTC support)
#define DASHCAM_PWR_OUTPUT_PIN GPIO_NUM_32
#define INDICATOR_LED GPIO_NUM_13
#define CAR_PWR_INPUT_PIN GPIO_NUM_33

// data stored in RTC to survive deep sleep
RTC_DATA_ATTR bool car_off;
RTC_DATA_ATTR bool car_at_home;

void hold_pin_off(gpio_num_t pin) {
  if (rtc_gpio_get_level(pin) != 0) {
    Serial.printf("Turning pin %d off.\r\n", pin);
    rtc_gpio_hold_dis(pin);
  } else {
    Serial.printf("Pin %d already off.\r\n", pin);
  }
}

void hold_pin_on(gpio_num_t pin) {
  if (rtc_gpio_get_level(pin) != 1) {
    Serial.printf("Turning pin %d on.\r\n", pin);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, 1);
    rtc_gpio_hold_en(pin);
  } else {
    Serial.printf("Pin %d already on.\r\n", pin);
  }
}

void send_email() {
  uint32_t start_time = millis();

  Serial.print("Device IP: ");
  Serial.println(WiFi.localIP());

  WiFiClientSecure client;

  Serial.printf("Connecting to: %s\r\n", config.host);
  if (!client.connect(config.host, config.httpsPort)) {
    Serial.printf("Connection failed\r\n");
    return;
  }

  client.print(String("GET ") + config.url + " HTTP/1.1\r\n" +
               "Host: " + config.host + "\r\n" + "Connection: close\r\n\r\n");
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

void print_config(void) {
  Serial.printf("WiFi SSID: %s\r\n", config.ssid);
  Serial.printf("WiFi Password: %s\r\n", config.password);
  Serial.printf("Host: %s\r\n", config.host);
  Serial.printf("URL: %s\r\n", config.url);
  Serial.printf("Port %d\r\n", config.httpsPort);
  Serial.printf("Home Connection Attemps (retries): %d\r\n",
                config.home_connection_attempts);
  Serial.printf("Home Connection Timeout (seconds): %d\r\n",
                config.home_connection_timeout);
  Serial.printf("Home Shutdown Delay (minutes): %d\r\n",
                config.home_shutdown_delay);
}

int load_string(char *location, int size) {
  int i = 0;
  while (1) {
    if (Serial.available() > 0) {
      location[i] = Serial.read();
      Serial.write(location[i]);
      if (location[i] == '\n' || location[i] == '\r')
        break;
      if (i < size - 1 && isspace(location[i]) == false)
        i++;
    }
  }
  location[i] = '\0';
  Serial.println("");
  int temp;
  sscanf(location, "%d", &temp);
  return temp;
}

void first_boot() {
  Serial.printf("Initial Powerup\r\n");
  car_at_home = false;
  car_off = false;

  pinMode(INDICATOR_LED, OUTPUT);

  uint32_t start_time = millis() + 10000;
  while (config.init != INIT_VALUE || millis() < start_time) {
    digitalWrite(INDICATOR_LED, 1);
    delay(100);
    digitalWrite(INDICATOR_LED, 0);
    delay(100);
    if (config.init != INIT_VALUE)
      Serial.println("No configuration set.");
    else
      Serial.printf("Booting in %lu seconds...\r\n",
                    (start_time - millis()) / 1000);

    Serial.println("Press Any Key to Configure.");

    if (Serial.peek() > 0) {
      Serial.println("");
      Serial.println("Starting Configuration");
      digitalWrite(INDICATOR_LED, 1);
      delay(100); // delay to capture any straggling characters
      while (Serial.peek() > 0)
        Serial.read(); // empty buffer

      if (config.init == INIT_VALUE)
        print_config();

      Serial.println("");
      Serial.println("Please enter new WiFi SSID:");
      load_string(config.ssid, sizeof(config.ssid));

      Serial.println("Please enter new WiFi Password:");
      load_string(config.password, sizeof(config.password));

      Serial.println(
          "Please enter new ifttt host (enter for none)(maker.ifttt.com):");
      load_string(config.host, sizeof(config.host));

      Serial.println(
          "Please enter new ifttt url (enter for "
          "none)(/trigger/NAME/with/key/CODE):");
      load_string(config.url, sizeof(config.url));

      char temp[100];
      Serial.println("Please enter new ifttt port (enter for none)(443):");
      config.httpsPort = load_string(temp, sizeof(temp));

      Serial.println("Please enter new Home Connection Attempts (Retries)(2):");
      config.home_connection_attempts = load_string(temp, sizeof(temp));

      Serial.println("Please enter new Home Connection Timeout (Seconds)(20):");
      config.home_connection_timeout = load_string(temp, sizeof(temp));

      Serial.println("Please enter new Home Shutdown Delay (Minutes)(1):");
      config.home_shutdown_delay = load_string(temp, sizeof(temp));

      config.init = INIT_VALUE;
      EEPROM.put(0, config);

      Serial.println("");
      print_config();
      Serial.println("");

      Serial.println("Press enter to continue:");
      load_string(temp, sizeof(temp));

      EEPROM.commit();
    }
  }
  digitalWrite(INDICATOR_LED, 0);
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(sizeof(EpromTags));
  EEPROM.get(0, config);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    car_off = !car_off;

    if (car_off) {
      // clear at home flag
      Serial.printf("Car stopped, checking for Home AP....\r\n");
      car_at_home = false;

      for (int j = 0; j < config.home_connection_attempts; j++) {
        Serial.printf("Starting Scan #%d\r\n", j);
        WiFi.mode(WIFI_STA);
        WiFi.begin(config.ssid, config.password);
        unsigned long wifi_timeout =
            millis() + config.home_connection_timeout * 1000;
        while (millis() < wifi_timeout) {
          if (WiFi.status() == WL_CONNECTED) {
            car_at_home = true;
            Serial.printf("WiFi Connected in: %lu ms\r\n",
                          wifi_timeout - millis());
            break;
          }
        }

        if (car_at_home && strlen(config.url) > 0)
          send_email();

        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);

        // don't scan again if we are already home
        if (car_at_home)
          break;
      }

      if (!car_at_home)
        Serial.printf("Leaving dashcam on...\r\n");
    } else {
      Serial.printf("Car turned on...\r\n");
    }

  } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    // turn the camera off and clear the flag
    hold_pin_off(DASHCAM_PWR_OUTPUT_PIN);
    hold_pin_off(INDICATOR_LED);
    car_at_home = false;
  } else {
    first_boot();
  }

  Serial.flush();
  const uint64_t mask = 1ULL << CAR_PWR_INPUT_PIN;

  if (car_off) {
    // car is off, watch for it to turn on
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ALL_LOW);
    // car is off, if we are home set a timer to shut the dashcam off
    if (car_at_home) {
      Serial.printf("Dashcam shutdown in %d minutes.\r\n",
                    config.home_shutdown_delay);
      hold_pin_on(INDICATOR_LED);
      esp_sleep_enable_timer_wakeup(config.home_shutdown_delay * 60 * 1000 *
                                    1000);
    }
  } else {
    // car is on, watch for it to turn off
    hold_pin_off(INDICATOR_LED);
    hold_pin_on(DASHCAM_PWR_OUTPUT_PIN);
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_HIGH);
  }

  esp_deep_sleep_start();
}

void loop() {}
