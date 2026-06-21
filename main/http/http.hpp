#pragma once

#include "config.hpp"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include "wifi/wifi.hpp"
#include <array>
#include <cstring>

class HttpClient {
public:
  static HttpClient &instance();
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
    std::array<char, kMaxHttpResponseBody> body = {0};
    size_t size = 0;
    int response_code = 0;
    HttpError error = HttpError::REQUEST_OK;
    bool overflow = false;
    bool ok() const {
      return error == REQUEST_OK && 200 <= response_code &&
             response_code < 300 && !overflow;
    }
  };

  struct HttpRequest {
    std::array<char, 256> url = {0};
    HttpMethod method = HttpMethod::HTTP_METHOD_GET;
    std::array<char, 1024> body;
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
  static constexpr int kMaxQueueSize = 10;      // max jobs on the queue
  static constexpr int kStackBytes = 1024 * 16; // stack size in bytes
  static esp_err_t event_handler(esp_http_client_event *);

  StaticQueue_t queue_memory_;
  std::array<HttpJob, kMaxQueueSize> queue_buffer_;
  QueueHandle_t queue_ = nullptr;

  static constexpr char kTaskName[] = "HttpClient";
  StaticTask_t task_memory_;
  std::array<StackType_t, kStackBytes> stack_;
  TaskHandle_t task_ = nullptr;

  esp_http_client_handle_t esp_http_client_;
};
