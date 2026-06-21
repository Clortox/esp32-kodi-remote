#include "wifi.hpp"
#include "config.hpp"
#include "esp_event_base.h"
#include "esp_log.h"
#include "esp_netif_types.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include <cstring>

static constexpr char TAG[] = "Wifi";

Wifi &Wifi::instance() {
  static Wifi wifi_ = Wifi();
  return wifi_;
};

Wifi::Wifi() {
  ESP_LOGI(TAG, "Init wifi singleton");

  wifi_event_group_ = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());

  esp_netif_create_default_wifi_sta();

  wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, this, &instance_any_id_));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, this, &instance_got_ip_));

  wifi_config_ = {};
  wifi_config_.sta.threshold.authmode = kAuthMode;
  memcpy(wifi_config_.sta.ssid, kWifiSsid, sizeof(kWifiSsid));
  memcpy(wifi_config_.sta.password, kWifiPassword, sizeof(kWifiPassword));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_));
}

esp_err_t Wifi::start() {
  esp_err_t error = esp_wifi_start();

  EventBits_t bits =
      xEventGroupWaitBits(wifi_event_group_, kConnectedBit | kFailBit, pdFALSE,
                          pdFALSE, portMAX_DELAY);

  if (bits & kConnectedBit) {
    ESP_LOGI(TAG, "Connected to wifi");
  } else if (bits & kFailBit) {
    ESP_LOGE(TAG, "Failed to connect");
  } else {
    ESP_LOGE(TAG, "Unknown event on wifi start! %lu", bits);
  }

  return error;
}

void Wifi::on_event(esp_event_base_t event_base, int32_t event_id, void *data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {

    ESP_LOGI(TAG, "Wifi started, connecting...");

    esp_wifi_connect();

  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (retry_count_ < kMaxRetryCount) {
      esp_wifi_connect();

      retry_count_++;

      ESP_LOGI(TAG, "Connection failed, retrying...");
    } else {

      xEventGroupSetBits(wifi_event_group_, kFailBit);
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *ip_event = (ip_event_got_ip_t *)data;

    ESP_LOGI(TAG, "Obtained IP " IPSTR, IP2STR(&ip_event->ip_info.ip));

    xEventGroupSetBits(wifi_event_group_, kConnectedBit);
  }
}

void Wifi::event_handler(void *arg, esp_event_base_t event_base,
                         int32_t event_id, void *data) {

  static_cast<Wifi *>(arg)->on_event(event_base, event_id, data);
}
