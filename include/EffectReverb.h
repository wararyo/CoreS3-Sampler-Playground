#pragma once

#include <stdio.h>
#include <EffectBase.h>

#if defined(M5UNIFIED_PC_BUILD)
#include <cstdlib>
#endif

#if defined ( ARDUINO )
#include <Arduino.h>
#elif defined ( ESP_PLATFORM )
// TODO ここ書く
#endif

#define REVERB_DELAY_BASIS_COMB_0 3460
#define REVERB_DELAY_BASIS_COMB_1 2988
#define REVERB_DELAY_BASIS_COMB_2 3882
#define REVERB_DELAY_BASIS_COMB_3 4312
#define REVERB_DELAY_BASIS_ALL_0 480
#define REVERB_DELAY_BASIS_ALL_1 161
#define REVERB_DELAY_BASIS_ALL_2 46

// シュレーダーのリバーブ
class EffectReverb : public EffectBase
{
public:
    EffectReverb(float level, float time, uint32_t bufferSize, uint32_t sampleRate) : level{level}, time{time}, bufferSize{bufferSize}, sampleRate{sampleRate}
    {
        Init();
    }
    ~EffectReverb()
    {
        free(memory);
    }
    float level = 0.05f; // リバーブの強さ 入力の音量は変化しません(DRY/WETではありません)
    float time = 1.0f;  // リバーブの持続時間
    uint32_t bufferSize; // Processで渡されるinputおよびoutputの長さ　必ず4の倍数である必要がある
    uint32_t sampleRate;
    void Init();
    void Process(const float *input, float *output);

private:
    float *memory;
};