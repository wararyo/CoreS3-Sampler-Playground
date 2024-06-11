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

    // 現状、timeは0.02〜1.0のみ対応
    if (time > 1.0) time = 1.0;
    else if (time < 0.02) time = 0.02;

    float *cursor = memory;
    combFilters[0] = CombFilter(cursor, 0.805f, (uint32_t)(time * REVERB_DELAY_BASIS_COMB_0));
    cursor += REVERB_DELAY_BASIS_COMB_0;
    combFilters[1] = CombFilter(cursor, 0.827f, (uint32_t)(time * REVERB_DELAY_BASIS_COMB_1));
    cursor += REVERB_DELAY_BASIS_COMB_1;
    combFilters[2] = CombFilter(cursor, 0.783f, (uint32_t)(time * REVERB_DELAY_BASIS_COMB_2));
    cursor += REVERB_DELAY_BASIS_COMB_2;
    combFilters[3] = CombFilter(cursor, 0.764f, (uint32_t)(time * REVERB_DELAY_BASIS_COMB_3));
    cursor += REVERB_DELAY_BASIS_COMB_3;
    allpassFilters[0] = AllpassFilter(cursor, 0.7f, (uint32_t)(time * REVERB_DELAY_BASIS_ALL_0));
    cursor += REVERB_DELAY_BASIS_ALL_0;
    allpassFilters[1] = AllpassFilter(cursor, 0.7f, (uint32_t)(time * REVERB_DELAY_BASIS_ALL_1));
    cursor += REVERB_DELAY_BASIS_ALL_1;
    allpassFilters[2] = AllpassFilter(cursor, 0.7f, (uint32_t)(time * REVERB_DELAY_BASIS_ALL_2));
    cursor += REVERB_DELAY_BASIS_ALL_2;
}

__attribute((optimize("-O3")))
void EffectReverb::CombFilter::Process4(const float *input, float *__restrict__ output)
{
    for (uint_fast8_t i = 0; i < 4; i++)
    {
        const float readback = buffer[cursor];
        const float newValue = readback * g + input[i];
        buffer[cursor] = newValue;
        if (++cursor >= delaySamples) cursor = 0;
        output[i] += readback; // このリバーブではコムフィルターは並列でのみ用いられるので、加算したほうが処理の都合がいい
    }
}

__attribute((optimize("-O3")))
void EffectReverb::AllpassFilter::Process4(const float *input, float *__restrict__ output)
{
    for (uint_fast8_t i = 0; i < 4; i++)
    {
        float readback = buffer[cursor];
        readback += (-g) * input[i];
        const float newValue = readback * g + input[i];
        buffer[cursor] = newValue;
        if (++cursor >= delaySamples) cursor = 0;
        output[i] = readback; // コムフィルターと異なり上書きする
    }
}

__attribute((optimize("-O3")))
void EffectReverb::Process(const float *input, float *__restrict__ output)
{
    float buffer[bufferSize];         // 最初に振幅を下げてここに格納しておく(リバーブの効果は絶対的な振幅に影響されないためOK)
    float multiplier = level * 0.25f; // 0.25fはコムフィルターの平均を取るため

    float processed[bufferSize]; // これが最終的にリバーブ成分になる
    memset(processed, 0, sizeof(float) * bufferSize);

    uint32_t length = bufferSize >> 2; // 1ループで4サンプル処理する

    // 現在処理中の各データブロックの先頭 (4サンプル=1ブロック)
    const float *in = input;
    float *buf = buffer;
    float *pr = processed;
    float *out = output;

    do
    {
        // 入力の振幅を下げてbufferに格納
        buf[0] = in[0] * multiplier;
        buf[1] = in[1] * multiplier;
        buf[2] = in[2] * multiplier;
        buf[3] = in[3] * multiplier;

        // 4つのコムフィルター(並列)
        for (uint_fast8_t f = 0; f < 4; f++)
        {
            combFilters[f].Process4(buf, pr); // 内部でprocessedに加算される
        }
        // コムフィルターの出力は本来足して平均を取るべきだが、最初に0.25を掛けているので単純に足し合わせるだけでOK

        // 3つのオールパスフィルター(直列)
        for (uint_fast8_t f = 0; f < 3; f++)
        {
            allpassFilters[f].Process4(pr, pr); // processedは内部で上書きされる
        }

        // 原音と合わせて出力
        out[0] = pr[0];
        out[1] = pr[1];
        out[2] = pr[2];
        out[3] = pr[3];

        // 最後にカーソルを進める
        in += 4;
        buf += 4;
        pr += 4;
        out += 4;
    } while (--length);
}