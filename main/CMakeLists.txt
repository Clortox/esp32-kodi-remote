file(GLOB_RECURSE SRCS "*.cpp")

idf_component_register(
  SRCS ${SRCS}
  INCLUDE_DIRS "."
  REQUIRES nvs_flash esp_wifi esp_event esp_http_client
)
