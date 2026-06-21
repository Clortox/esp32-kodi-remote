#pragma once

#include "config.hpp"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"

class Wifi {
public:
  static Wifi &instance();
  Wifi(const Wifi &) = delete;
  Wifi &operator=(const Wifi &) = delete;

  esp_err_t start();

  bool is_connected() const;

private:
  Wifi();

  static void event_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *data);

  void on_event(esp_event_base_t event_base, int32_t event_id, void *data);

  static constexpr EventBits_t kConnectedBit = BIT0;
  static constexpr EventBits_t kFailBit = BIT1;
  static constexpr wifi_auth_mode_t kAuthMode = WIFI_AUTH_WPA2_PSK;
  static constexpr int kMaxRetryCount = 3;

  int retry_count_ = 0;

  wifi_config_t wifi_config_;

  esp_event_handler_instance_t instance_any_id_;
  esp_event_handler_instance_t instance_got_ip_;

  EventGroupHandle_t wifi_event_group_;
};
