#include <EffectReverb.h>
#include <M5Unified.h>

struct comb_t
{
    float *buffer_start;
    float *cursor;
    float g; // フィードバックのレベル 一般的にgで表される
    float *buffer_end; // buffer_end - buffer_start = 遅延の長さ(サンプル数)
};
struct allpass_t
{
    float *buffer_start;
    float *cursor;
    float g; // フィードバックのレベル 一般的にgで表される
    float *buffer_end; // buffer_end - buffer_start = 遅延の長さ(サンプル数)
};

struct comb_t combs[4];
struct allpass_t allpasses[3];

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
    combs[0] = comb_t{cursor, cursor, 0.805f, cursor + (uint32_t)(time * REVERB_DELAY_BASIS_COMB_0)};
    cursor += REVERB_DELAY_BASIS_COMB_0;
    combs[1] = comb_t{cursor, cursor, 0.827f, cursor + (uint32_t)(time * REVERB_DELAY_BASIS_COMB_1)};
    cursor += REVERB_DELAY_BASIS_COMB_1;
    combs[2] = comb_t{cursor, cursor, 0.783f, cursor + (uint32_t)(time * REVERB_DELAY_BASIS_COMB_2)};
    cursor += REVERB_DELAY_BASIS_COMB_2;
    combs[3] = comb_t{cursor, cursor, 0.764f, cursor + (uint32_t)(time * REVERB_DELAY_BASIS_COMB_3)};
    cursor += REVERB_DELAY_BASIS_COMB_3;
    allpasses[0] = allpass_t{cursor, cursor, 0.7f, cursor + (uint32_t)(time * REVERB_DELAY_BASIS_ALL_0)};
    cursor += REVERB_DELAY_BASIS_ALL_0;
    allpasses[1] = allpass_t{cursor, cursor, 0.7f, cursor + (uint32_t)(time * REVERB_DELAY_BASIS_ALL_1)};
    cursor += REVERB_DELAY_BASIS_ALL_1;
    allpasses[2] = allpass_t{cursor, cursor, 0.7f, cursor + (uint32_t)(time * REVERB_DELAY_BASIS_ALL_2)};
    cursor += REVERB_DELAY_BASIS_ALL_2;
}

extern "C"
{
    void comb_filter_process4_work(const float *input, float *__restrict__ output, struct comb_t *comb);
    void allpass_filter_process4_work(const float *input, float *__restrict__ output, struct allpass_t *allpass);
}

// コムフィルター処理を4サンプル進める
__attribute((weak, optimize("-O3")))
void comb_filter_process4_work(const float *input, float *__restrict__ output, struct comb_t *comb)
{
    float *buffer_start = comb->buffer_start;
    float *cursor = comb->cursor;
    float g = comb->g;
    float *buffer_end = comb->buffer_end;
    for (uint_fast8_t i = 0; i < 4; i++)
    {
        const float readback = *cursor;
        const float newValue = readback * g + input[i];
        *cursor = newValue;
        if (++cursor >= buffer_end) cursor = buffer_start;
        output[i] += readback; // このリバーブではコムフィルターは並列でのみ用いられるので、加算したほうが処理の都合がいい
    }
    comb->cursor = cursor;
}

// オールパスフィルター処理を4サンプル進める
__attribute((weak, optimize("-O3")))
void allpass_filter_process4_work(const float *input, float *__restrict__ output, struct allpass_t *allpass)
{
    float *buffer_start = allpass->buffer_start;
    float *cursor = allpass->cursor;
    float g = allpass->g;
    float *buffer_end = allpass->buffer_end;
    for (uint_fast8_t i = 0; i < 4; i++)
    {
        float readback = *cursor;
        readback += (-g) * input[i];
        const float newValue = readback * g + input[i];
        *cursor = newValue;
        if (++cursor >= buffer_end) cursor = buffer_start;
        output[i] = readback; // コムフィルターと異なり上書きする
    }
    allpass->cursor = cursor;
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
            comb_filter_process4_work(buf, pr, &combs[f]); // 内部でprocessedに加算される
        }
        // コムフィルターの出力は本来足して平均を取るべきだが、最初に0.25を掛けているので単純に足し合わせるだけでOK

        // 3つのオールパスフィルター(直列)
        for (uint_fast8_t f = 0; f < 3; f++)
        {
            allpass_filter_process4_work(pr, pr, &allpasses[f]); // processedは内部で上書きされる
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