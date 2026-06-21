#pragma once

#include "config.hpp"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include "wifi/wifi.hpp"
#include <array>
#include <memory>

class HttpClient {
public:
  HttpClient &instance();
  HttpClient(const HttpClient &) = delete;
  HttpClient &operator=(const HttpClient &) = delete;

  typedef esp_http_client_method_t HttpMethod;

  enum HttpError {
    REQUEST_OK,
    HTTP_CLIENT_REQUEST_FAILED,
    WIFI_NOT_CONECTED,
    JOB_TIMEOUT,
    RESPONSE_OVERFLOW
  };

  struct HttpResponse {
    int response_code;
    HttpError error;
    bool overflow = false;
    bool ok() const {
      return error == REQUEST_OK && 200 <= response_code &&
             response_code < 300 && !overflow;
    }
    std::array<char, kMaxHttpResponseBody> body = {0};
  };

  struct HttpRequest {
    std::array<char, 256> url = {0};
    HttpMethod method;
    std::array<char, 2048> body;
  };

  struct HttpJob {
    const HttpRequest *request;
    HttpResponse *response;
    TaskHandle_t task;
  };

  /**
   * Public contract for other elements looking to perform an http requst
   */
  HttpResponse request(const HttpRequest &request, TickType_t timeout);

private:
  HttpClient();

  [[noreturn]] void task_run();
  [[noreturn]] static void task_handler(void *);

  void perform_request(HttpJob &job);

  static constexpr int kDefaultTimeout = 5000; // ms
  static constexpr int kMaxRedirect =
      3; // max number of http redirects to follow
  static constexpr int kMaxQueueSize = 10; // max jobs on the queue
  static constexpr int kStackWords = 2000; // stack size in bytes
  static esp_err_t event_handler(esp_http_client_event *);

  StaticQueue_t queue_memory_;
  std::array<HttpJob, kMaxQueueSize> queue_buffer_;
  QueueHandle_t queue_ = nullptr;

  static constexpr char kTaskName[] = "HttpClient";
  StaticTask_t task_memory_;
  std::array<StackType_t, kStackWords> stack_;
  TaskHandle_t task_ = nullptr;

  esp_http_client_handle_t esp_http_client_;
};
