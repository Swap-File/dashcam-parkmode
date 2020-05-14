#ifndef _CONF_H
#define _CONF_H

// pins things are physically hooked up to (pins must have RTC support)
#define DASHCAM_PWR_OUTPUT_PIN GPIO_NUM_32
#define INDICATOR_LED GPIO_NUM_13
#define CAR_PWR_INPUT_PIN GPIO_NUM_33
#define CONFIG_PIN GPIO_NUM_27

// special value saved to eeprom so we know if we have a config or not
#define INIT_VALUE 0xA1

// data that gets saved to eeprom
typedef struct {
  union {
    struct {
      uint8_t init;

      char ssid[32];
      char password[32];

      char host[32];
      char url[100];
      uint32_t httpsPort;

      uint32_t home_connection_attempts; // retries
      uint32_t home_connection_timeout;  // seconds
      uint32_t home_shutdown_delay;      // minutes
    };
    uint8_t raw[256];  //oversized, because lazy
  };
} EpromTags;

void conf_print(EpromTags *config);
void conf_init(EpromTags *config);
void conf_get(EpromTags *config);
void conf_put(EpromTags *config);

#endif