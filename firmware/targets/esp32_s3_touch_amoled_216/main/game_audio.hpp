#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace usage_panel::amoled {

enum class GameSound : uint8_t { Input, Safe, Boom, Result };

class GameAudio {
public:
    bool begin();
    void play(GameSound sound);

private:
    static void task_entry(void* context);
    QueueHandle_t queue_ = nullptr;
};

}  // namespace usage_panel::amoled
