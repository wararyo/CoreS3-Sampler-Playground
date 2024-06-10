#include <EffectReverb.h>
#include <M5Unified.h>

void EffectReverb::Init()
{
    size_t size = sizeof(float) * (REVERB_DELAY_BASIS_COMB_0 +
                                   REVERB_DELAY_BASIS_COMB_1 +
                                   REVERB_DELAY_BASIS_COMB_2 +
                                   REVERB_DELAY_BASIS_COMB_3 +
                                   REVERB_DELAY_BASIS_ALL_0 +
                                   REVERB_DELAY_BASIS_ALL_1 +
                                   REVERB_DELAY_BASIS_ALL_2);
#if defined(M5UNIFIED_PC_BUILD)
    memory = (float *)calloc(1, size);
#else
    memory = (float *)heap_caps_calloc(1, size, MALLOC_CAP_INTERNAL);
#endif

    float *cursor = memory;
    // TODO: timeを考慮する
    combFilters[0] = CombFilter(cursor, 0.805f, REVERB_DELAY_BASIS_COMB_0);
    cursor += REVERB_DELAY_BASIS_COMB_0;
    combFilters[1] = CombFilter(cursor, 0.827f, REVERB_DELAY_BASIS_COMB_1);
    cursor += REVERB_DELAY_BASIS_COMB_1;
    combFilters[2] = CombFilter(cursor, 0.783f, REVERB_DELAY_BASIS_COMB_2);
    cursor += REVERB_DELAY_BASIS_COMB_2;
    combFilters[3] = CombFilter(cursor, 0.764f, REVERB_DELAY_BASIS_COMB_3);
    cursor += REVERB_DELAY_BASIS_COMB_3;
    allpassFilters[0] = AllpassFilter(cursor, 0.7f, REVERB_DELAY_BASIS_ALL_0);
    cursor += REVERB_DELAY_BASIS_ALL_0;
    allpassFilters[1] = AllpassFilter(cursor, 0.7f, REVERB_DELAY_BASIS_ALL_1);
    cursor += REVERB_DELAY_BASIS_ALL_1;
    allpassFilters[2] = AllpassFilter(cursor, 0.7f, REVERB_DELAY_BASIS_ALL_2);
    cursor += REVERB_DELAY_BASIS_ALL_2;
}

__attribute((optimize("-O3")))
void EffectReverb::CombFilter::Process(const float *input, float *__restrict__ output, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        const float readback = buffer[cursor];
        const float newValue = readback * g + input[i];
        buffer[cursor] = newValue;
        cursor++;
        if (cursor >= delaySamples) cursor = 0;
        output[i] += readback; // このリバーブではコムフィルターは並列でのみ用いられるので、加算したほうが処理の都合がいい
    }
}

__attribute((optimize("-O3")))
void EffectReverb::AllpassFilter::Process(const float *input, float *__restrict__ output, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        float readback = buffer[cursor];
        readback += (-g) * input[i];
        const float newValue = readback * g + input[i];
        buffer[cursor] = newValue;
        cursor++;
        if (cursor >= delaySamples) cursor = 0;
        output[i] = readback; // コムフィルターと異なり上書きする
    }
}

__attribute((optimize("-O3")))
void EffectReverb::Process(const float *input, float *__restrict__ output)
{
    // 最初に振幅を下げておく(リバーブの効果は絶対的な振幅に影響されないためOK)
    float buffer[bufferSize];
    float multiplier = level * 0.25f; // コムフィルターの平均を取るための0.25f;
    for (uint32_t i = 0; i < bufferSize; i++)
    {
        buffer[i] = input[i] * multiplier;
    }

    float processed[bufferSize]; // これが最終的にリバーブ成分になる
    memset(processed, 0, sizeof(float) * bufferSize);

    // 4つのコムフィルター(並列)
    for (uint_fast8_t i; i < 4; i++)
    {
        combFilters[i].Process(buffer, processed, bufferSize); // 内部でprocessedに加算される
    }
    // コムフィルターの出力は本来足して平均を取るべきだが、最初に0.25を掛けているので単純に足し合わせるだけでOK

    // 3つのオールパスフィルター(直列)
    for (uint_fast8_t i; i < 3; i++)
    {
        allpassFilters[i].Process(processed, processed, bufferSize); // processedは内部で上書きされる
    }

    // 原音と合わせて出力
    for (uint32_t i = 0; i < bufferSize; i++)
    {
        output[i] = input[i] + processed[i];
    }
}