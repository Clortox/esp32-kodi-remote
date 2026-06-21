#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "wifi/wifi.hpp"
#include <inttypes.h>
#include <stdio.h>

static inline constexpr char TAG[] = "main";

void banner() {
  ESP_LOGI(TAG, "=======================");
  ESP_LOGI(TAG, "Hello from ESP Remote");
  ESP_LOGI(TAG, "Author: Tyler Perkins (hello@clortox.com)");
  ESP_LOGI(TAG, "For use with Kodi and my specific sound system");
  ESP_LOGI(TAG, "=======================");
}

extern "C" void app_main() {
  banner();

  // dirty init nvs
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

  Wifi &instance = Wifi::instance();
  instance.start();
}
