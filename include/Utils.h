#pragma once

#if defined(ESP_PLATFORM)
#include <esp_log.h>
#define LOGE(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
#define LOGI(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
#define LOGD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
#elif defined(ARDUINO)
#include <Arduino.h>
#define LOGE(tag, format, ...) Serial.printf("[%s] " format, tag, ##__VA_ARGS__)
#define LOGI(tag, format, ...) Serial.printf("[%s] " format, tag, ##__VA_ARGS__)
#define LOGD(tag, format, ...) Serial.printf("[%s] " format, tag, ##__VA_ARGS__)
#else
#include <cstdio>
#define LOGE(tag, format, ...) printf("[%s] " format, tag, ##__VA_ARGS__)
#define LOGI(tag, format, ...) printf("[%s] " format, tag, ##__VA_ARGS__)
#define LOGD(tag, format, ...) printf("[%s] " format, tag, ##__VA_ARGS__)
#endif

namespace capsule
{
namespace sampler
{

// ArduinoのmicrosをArduino以外の環境でも使用するための関数
unsigned long micros();

}
}
