#include "game_audio.hpp"

#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/task.h"

namespace usage_panel::amoled {
namespace {
constexpr char TAG[] = "game_audio";
constexpr int SAMPLE_RATE = 16000;

void tone(esp_codec_dev_handle_t codec, int frequency, int duration_ms, int amplitude)
{
    int16_t samples[256];
    const int total = SAMPLE_RATE * duration_ms / 1000;
    uint32_t phase = 0;
    const uint32_t step = static_cast<uint32_t>(
        (static_cast<uint64_t>(frequency) << 24) / SAMPLE_RATE);
    for (int offset = 0; offset < total; offset += 256) {
        const int count = total - offset < 256 ? total - offset : 256;
        for (int i = 0; i < count; ++i) {
            phase += step;
            const int triangle = static_cast<int>((phase >> 15) & 0x1ff);
            const int signed_triangle = triangle < 256 ? triangle - 128 : 383 - triangle;
            const int fade = (total - offset - i) * 256 / total;
            samples[i] = static_cast<int16_t>(signed_triangle * amplitude * fade / (128 * 256));
        }
        if (esp_codec_dev_write(codec, samples, count * sizeof(int16_t)) != ESP_OK) break;
    }
}

void play_sound(esp_codec_dev_handle_t codec, GameSound sound)
{
    switch (sound) {
    case GameSound::Input: tone(codec, 900, 35, 2600); break;
    case GameSound::Safe:
        tone(codec, 640, 85, 4200);
        tone(codec, 820, 110, 4200);
        break;
    case GameSound::Boom:
        tone(codec, 190, 140, 7200);
        tone(codec, 110, 230, 6200);
        break;
    case GameSound::Result:
        tone(codec, 520, 110, 4500);
        tone(codec, 660, 110, 4500);
        tone(codec, 780, 190, 4500);
        break;
    }
}
}  // namespace

bool GameAudio::begin()
{
    queue_ = xQueueCreate(8, sizeof(GameSound));
    if (!queue_) return false;
    if (xTaskCreate(task_entry, "game_audio", 4096, this, 3, nullptr) == pdPASS)
        return true;
    vQueueDelete(queue_);
    queue_ = nullptr;
    return false;
}

void GameAudio::play(GameSound sound)
{
    if (queue_) xQueueSend(queue_, &sound, 0);
}

void GameAudio::task_entry(void* context)
{
    auto* self = static_cast<GameAudio*>(context);
    esp_codec_dev_handle_t codec = nullptr;
    bool unavailable = false;
    GameSound sound{};
    while (xQueueReceive(self->queue_, &sound, portMAX_DELAY) == pdTRUE) {
        if (unavailable) continue;
        if (!codec) {
            codec = bsp_audio_codec_speaker_init();
            if (!codec) {
                ESP_LOGW(TAG, "speaker initialization failed; game remains silent");
                unavailable = true;
                continue;
            }
            esp_codec_dev_set_out_vol(codec, 55);
            esp_codec_dev_sample_info_t format = {};
            format.sample_rate = SAMPLE_RATE;
            format.channel = 1;
            format.bits_per_sample = 16;
            if (esp_codec_dev_open(codec, &format) != ESP_OK) {
                ESP_LOGW(TAG, "speaker stream failed; game remains silent");
                unavailable = true;
                continue;
            }
        }
        play_sound(codec, sound);
    }
    vTaskDelete(nullptr);
}

}  // namespace usage_panel::amoled
