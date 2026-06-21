#include "http.hpp"

static constexpr char TAG[] = "HttpClient";

////////////////////////////////////////////////////////////////////////////////
// Init of singleton ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

HttpClient::HttpClient() {

  ESP_LOGI(TAG, "Init HttpClient");

  uint8_t *raw_queue_buffer = reinterpret_cast<uint8_t *>(queue_buffer_.data());

  static_assert(std::is_trivially_copyable_v<HttpJob>,
                "HttpJob must be capable of being memcpy'd");

  queue_ = xQueueCreateStatic(kMaxQueueSize, sizeof(HttpClient::HttpJob),
                              raw_queue_buffer, &queue_memory_);

  esp_http_client_config_t config = {};
  config.event_handler = event_handler;
  config.timeout_ms = kDefaultTimeout;
  config.max_redirection_count = kMaxRedirect;
  config.skip_cert_common_name_check = true;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.url = "http://example.com";

  esp_http_client_ = esp_http_client_init(&config);

  task_ = xTaskCreateStatic(task_handler, kTaskName, kStackBytes, this, 0,
                            stack_.data(), &task_memory_);
  ESP_LOGI(TAG, "HttpClient created");
}

HttpClient &HttpClient::instance() {
  static HttpClient client = HttpClient();
  return client;
}

////////////////////////////////////////////////////////////////////////////////
// Public Contract /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

HttpClient::HttpResponse HttpClient::request(const HttpRequest &request,
                                             TickType_t timeout) {

  HttpResponse response;

  HttpJob job = {.request = &request,
                 .response = &response,
                 .task = xTaskGetCurrentTaskHandle()};

  if (xQueueSendToBack(queue_, &job, timeout) != pdTRUE) {

    ESP_LOGI(TAG, "Http Request failed");
    response.response_code = -1;
    response.error = HttpError::JOB_TIMEOUT;

  } else {
    ulTaskNotifyTake(pdTRUE, timeout);
  }

  ESP_LOGI(TAG, "Http client has prepared response for caller");

  return response;
}

////////////////////////////////////////////////////////////////////////////////
// esp32 http client wrapper ///////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void HttpClient::perform_request(HttpJob &job) {

  ESP_LOGI(TAG, "Prepare to make request");
  ESP_LOGI(TAG, "URL: %s", job.request->url.data());

  esp_http_client_set_url(esp_http_client_, job.request->url.data());
  esp_http_client_set_method(esp_http_client_, job.request->method);

  if (job.request->method == HttpMethod::HTTP_METHOD_POST) {
    esp_http_client_set_post_field(esp_http_client_, job.request->body.data(),
                                   job.request->body.size());
  }

  esp_http_client_set_user_data(esp_http_client_, &job);

  ESP_LOGD(TAG, "Perform HTTP Request for %s", job.request->url.data());

  esp_err_t error = esp_http_client_perform(esp_http_client_);

  if (error == ESP_OK) {
    job.response->response_code =
        esp_http_client_get_status_code(esp_http_client_);
    job.response->error = HttpError::REQUEST_OK;
  } else {
    job.response->response_code = -1;
    job.response->error = HttpError::HTTP_CLIENT_REQUEST_FAILED;

    ESP_LOGI(TAG, "Failed to perform http request with error %d", error);
  }
}

esp_err_t HttpClient::event_handler(esp_http_client_event *event) {
  esp_http_client_event_id_t event_id = event->event_id;

  HttpJob *job = static_cast<HttpJob *>(event->user_data);

  ESP_LOGI(TAG, "Obtained an event %d", event_id);

  if (event_id == HTTP_EVENT_ERROR) {

  } else if (event_id == HTTP_EVENT_ON_DATA) {
    ESP_LOGI(TAG, "Http Client obtained %d bytes from host", event->data_len);

    auto response = job->response;
    size_t room = response->body.max_size() - response->size;
    size_t length_to_copy = std::min(room, (size_t)event->data_len);

    if (length_to_copy != (size_t)event->data_len) {
      job->response->overflow = true;
    }

    std::memcpy(response->body.data() + response->size, event->data,
                length_to_copy);

    response->size += length_to_copy;

  } else if (event_id == HTTP_EVENT_ON_FINISH) {
    ESP_LOGD(TAG, "Http Client requests completed");
  }

  return ESP_OK;
}

////////////////////////////////////////////////////////////////////////////////
// FreeRTOS Task handler ///////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void HttpClient::task_run() {
  HttpJob job;
  while (true) {
    xQueueReceive(queue_, &job, portMAX_DELAY);

    if (!Wifi::instance().is_connected()) {
      ESP_LOGI(TAG, "Wifi is not connected, exit fast");
      job.response->response_code = -1;
      job.response->error = HttpError::WIFI_NOT_CONECTED;
    } else {
      perform_request(job);
    }

    ESP_LOGI(TAG, "Completed job, notify sender");

    xTaskNotifyGive(job.task);
  }
}

void HttpClient::task_handler(void *self) {
  static_cast<HttpClient *>(self)->task_run();
}
