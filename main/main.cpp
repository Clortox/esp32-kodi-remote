#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "http/http.hpp"
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

void httpTask(void *data) {
  while (true) {
    HttpClient::HttpRequest request = {};
    request.url = {"http://10.0.0.172:8000/response.json"};
    request.method = HttpClient::HttpMethod::HTTP_METHOD_GET;

    ESP_LOGI(TAG, "Prepare http request");

    HttpClient::HttpResponse response =
        HttpClient::instance().request(request, pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Response code %d", response.response_code);
    ESP_LOGI(TAG, "Response side %d", response.size);
    ESP_LOGI(TAG, "Response overflow %d", response.overflow);
    ESP_LOGI(TAG, "Response body %s", response.body.data());

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
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

  TaskHandle_t *handle = nullptr;
  xTaskCreate(httpTask, "HttpRequest", 1024 * 8, NULL, 1, handle);

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "Heartbeat");
  }
}
