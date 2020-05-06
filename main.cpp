#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <driver/rtc_io.h>

// Home Wifi name & password
const char *ssid = "";
const char *password = "";

// Optiona IFTTT (email)
String url = "";
const char *host = "maker.ifttt.com";
const int httpsPort = 443;

// how many times to try connecting
#define HOME_CONNECTION_ATTEMPTS 2

// how many seconds to wait for a connection
#define HOME_CONNECTION_TIME 20

// how many minutes to delay before shutting down at home
#define HOME_SHUTDOWN_DELAY 0.5

// pins things are physically hooked up to (needs RTC support)
#define DASHCAM_PWR_OUTPUT_PIN GPIO_NUM_32
#define DASHCAM_SHUTDOWN_DELAY_LED GPIO_NUM_13
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

  Serial.printf("Connecting to: %s\r\n", host);
  if (!client.connect(host, httpsPort)) {
    Serial.printf("Connection failed\r\n");
    return;
  }

  client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host +
               "\r\n" + "Connection: close\r\n\r\n");
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

void setup() {

  Serial.begin(115200);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {

    car_off = !car_off;

    if (car_off) {

      // clear at home flag
      car_at_home = false;

      Serial.printf("Car stopped, checking for Home AP....\r\n");

      for (int j = 0; j < HOME_CONNECTION_ATTEMPTS; j++) {

        Serial.printf("Starting Scan #%d\r\n", j);

        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);

        unsigned long wifi_timeout = millis() + HOME_CONNECTION_TIME * 1000;

        while (millis() < wifi_timeout) {
          if (WiFi.status() == WL_CONNECTED) {
            car_at_home = true;
            Serial.printf("WiFi Connected in: %lu ms\r\n", wifi_timeout - millis());
            break;
          }
          delay(100);
        }
        if (car_at_home)
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
    hold_pin_off(DASHCAM_SHUTDOWN_DELAY_LED);
    car_at_home = false;

  } else {

    // ext1 wakeup will immediately trigger if we assume wrong
    Serial.printf("Initial Powerup\r\n");
    car_at_home = false;
    car_off = false;
  }

  Serial.flush();

  const uint64_t mask = 1ULL << CAR_PWR_INPUT_PIN;

  if (car_off) {
    // car is off, watch for it to turn on
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ALL_LOW);

    // car is off, if we are home set a timer to shut the dashcam off
    if (car_at_home) {
      Serial.printf("Dashcam shutdown in %f minutes.\r\n", HOME_SHUTDOWN_DELAY);
      hold_pin_on(DASHCAM_SHUTDOWN_DELAY_LED);
      esp_sleep_enable_timer_wakeup(HOME_SHUTDOWN_DELAY * 60 * 1000 * 1000);
    }
  } else {
    // car is on, watch for it to turn off
    hold_pin_off(DASHCAM_SHUTDOWN_DELAY_LED);
    hold_pin_on(DASHCAM_PWR_OUTPUT_PIN);
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_HIGH);
  }

  esp_deep_sleep_start();
}

void loop() {}