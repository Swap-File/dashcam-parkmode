#include <Arduino.h>
#include <WiFi.h>
#include <driver/rtc_io.h>
#include "conf.h"
#include "email.h"

// RTC stores these persistant config across deep sleep 
RTC_DATA_ATTR bool car_off;
RTC_DATA_ATTR bool car_at_home;

static void hold_pin_off(gpio_num_t pin) {
  if (rtc_gpio_get_level(pin) != 0) {
    Serial.printf("Turning pin %d off.\r\n", pin);
    rtc_gpio_hold_dis(pin);
  } else {
    Serial.printf("Pin %d already off.\r\n", pin);
  }
}

static void hold_pin_on(gpio_num_t pin) {
  if (rtc_gpio_get_level(pin) != 1) {
    Serial.printf("Turning pin %d on.\r\n", pin);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, 1);
    rtc_gpio_hold_en(pin);
  } else {
    Serial.printf("Pin %d already on.\r\n", pin);
  }
}

void sleep_logic(EpromTags *config) {

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    car_off = !car_off;

    if (car_off) {
      // clear at home flag
      Serial.printf("Car stopped, checking for Home AP....\r\n");
      car_at_home = false;

      for (int j = 0; j < config->home_connection_attempts; j++) {
        Serial.printf("Starting Scan #%d\r\n", j);
        WiFi.mode(WIFI_STA);
        WiFi.begin(config->ssid, config->password);
        unsigned long wifi_timeout =
            millis() + config->home_connection_timeout * 1000;
        while (millis() < wifi_timeout) {
          if (WiFi.status() == WL_CONNECTED) {
            car_at_home = true;
            Serial.printf("WiFi Connected in: %lu ms\r\n",
                          wifi_timeout - millis());
            break;
          }
        }

        if (car_at_home && strlen(config->url) > 0)
          email_send(config);

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
      Serial.printf("Dashcam shutdown in %d minutes.\r\n",
                    config->home_shutdown_delay);
      hold_pin_on(INDICATOR_LED);
      esp_sleep_enable_timer_wakeup(config->home_shutdown_delay * 60 * 1000 *
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
