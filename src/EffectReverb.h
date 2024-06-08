#pragma once

#include <stdio.h>
#include <EffectBase.h>

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
    class CombFilter
    {
    public:
        CombFilter(float *buffer, float g, uint32_t delaySamples) : buffer{buffer}, g{g}, delaySamples{delaySamples} {}
        CombFilter() {}
        float *buffer;
        uint32_t cursor = 0;
        float g; // フィードバックのレベル 一般的にgで表される
        uint32_t delaySamples;

        void Process(const float *input, float *output, uint32_t len);
    };
    class AllpassFilter
    {
    public:
        AllpassFilter(float *buffer, float g, uint32_t delaySamples) : buffer{buffer}, g{g}, delaySamples{delaySamples} {}
        AllpassFilter() {}
        float *buffer;
        uint32_t cursor = 0;
        float g; // フィードバックのレベル 一般的にgで表される
        uint32_t delaySamples;

        void Process(const float *input, float *output, uint32_t len);
    };
    EffectReverb(float level, float time, uint32_t bufferSize) : level{level}, time{time}, bufferSize{bufferSize}
    {
        Init();
    }
    ~EffectReverb()
    {
        free(memory);
    }
    float level = 0.1f; // リバーブの強さ 入力の音量は変化しません(DRY/WETではありません)
    float time = 1.0f;  // リバーブの持続時間
    uint32_t bufferSize;
    void Init();
    void Process(const float *input, float *output);

private:
    float *memory;
    float *combFilterSwaps[4]; // コムフィルターは並列で処理するため、処理結果を一時的に退避させる場所が必要になる
    CombFilter combFilters[4];
    AllpassFilter allpassFilters[3];
};