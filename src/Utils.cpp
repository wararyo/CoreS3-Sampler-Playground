#include <Utils.h>

#if defined(ARDUINO)
#include <Arduino.h>
#elif defined(ESP_PLATFORM)
#include <esp_timer.h>
#elif defined(SDL_h_)
#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL_main.h>
#include <SDL2/SDL.h>
#elif __has_include(<SDL.h>)
#include <SDL_main.h>
#include <SDL.h>
#endif
#endif

namespace capsule
{
namespace sampler
{

unsigned long micros()
{
#if defined(ARDUINO)
    return ::micros();
#elif defined(ESP_PLATFORM)
    return (unsigned long)esp_timer_get_time();
#elif defined(SDL_h_)
    return SDL_GetPerformanceCounter() / (SDL_GetPerformanceFrequency() / (1000 * 1000));
#endif
}

}
}
