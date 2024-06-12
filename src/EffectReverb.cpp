#include <EffectReverb.h>
#include <M5Unified.h>

// 各フィルターの設定
// バッファの後ろは3サンプル以上空けておく必要がある
struct filter_t
{
    // buffer_end - buffer_start = 遅延の長さ(サンプル数)
    // buffer_end[0] - buffer_end[2]はバッファに含まれないがメモリとして自由に使える領域
    float *buffer_start;
    float *cursor;
    float g; // フィードバックのレベル 一般的にgで表される
    float *buffer_end;
};

struct filter_t combs[4];
struct filter_t allpasses[3];

void EffectReverb::Init()
{
    size_t size = sizeof(float) * (REVERB_DELAY_BASIS_COMB_0 + 3 +
                                   REVERB_DELAY_BASIS_COMB_1 + 3 +
                                   REVERB_DELAY_BASIS_COMB_2 + 3 +
                                   REVERB_DELAY_BASIS_COMB_3 + 3 +
                                   REVERB_DELAY_BASIS_ALL_0 + 3 +
                                   REVERB_DELAY_BASIS_ALL_1 + 3 +
                                   REVERB_DELAY_BASIS_ALL_2 + 3);
#if defined(M5UNIFIED_PC_BUILD)
    memory = (float *)calloc(1, size);
#else
    memory = (float *)heap_caps_calloc(1, size, MALLOC_CAP_INTERNAL);
#endif

    // 現状、timeは0.02〜1.0のみ対応
    if (time > 1.0) time = 1.0;
    else if (time < 0.02) time = 0.02;

    float *cursor = memory;
    combs[0] = filter_t{cursor, cursor, 0.805f, cursor + (uint32_t)(time * REVERB_DELAY_BASIS_COMB_0)};
    cursor += REVERB_DELAY_BASIS_COMB_0 + 3;
    combs[1] = filter_t{cursor, cursor, 0.827f, cursor + (uint32_t)(time * REVERB_DELAY_BASIS_COMB_1)};
    cursor += REVERB_DELAY_BASIS_COMB_1 + 3;
    combs[2] = filter_t{cursor, cursor, 0.783f, cursor + (uint32_t)(time * REVERB_DELAY_BASIS_COMB_2)};
    cursor += REVERB_DELAY_BASIS_COMB_2 + 3;
    combs[3] = filter_t{cursor, cursor, 0.764f, cursor + (uint32_t)(time * REVERB_DELAY_BASIS_COMB_3)};
    cursor += REVERB_DELAY_BASIS_COMB_3 + 3;
    allpasses[0] = filter_t{cursor, cursor, 0.7f, cursor + (uint32_t)(time * REVERB_DELAY_BASIS_ALL_0)};
    cursor += REVERB_DELAY_BASIS_ALL_0 + 3;
    allpasses[1] = filter_t{cursor, cursor, 0.7f, cursor + (uint32_t)(time * REVERB_DELAY_BASIS_ALL_1)};
    cursor += REVERB_DELAY_BASIS_ALL_1 + 3;
    allpasses[2] = filter_t{cursor, cursor, 0.7f, cursor + (uint32_t)(time * REVERB_DELAY_BASIS_ALL_2)};
    cursor += REVERB_DELAY_BASIS_ALL_2 + 3;
}

// コムフィルター処理を4サンプル進める
__attribute((optimize("-O3")))
inline void comb_filter_process4(const float *input, float *__restrict__ output, struct filter_t *comb)
{
    float *buffer_start = comb->buffer_start;
    float *cursor = comb->cursor;
    float g = comb->g;
    float *buffer_end = comb->buffer_end;

    // bufferのループ処理をforループ内で行う代わりにその前後で行う
    bool should_loop = (cursor + 4) >= buffer_end;
    if (should_loop)
    {
        // バッファの始まりの部分をバッファの直後にコピーしておく
        buffer_end[0] = buffer_start[0];
        buffer_end[1] = buffer_start[1];
        buffer_end[2] = buffer_start[2];
    }

    // 実際のコムフィルター処理
#if CONFIG_IDF_TARGET_ESP32S3
    // ESP32S3の場合はSIMD命令を使って高速化
    __asm__ (
    "   lsi             f1, %0, 0               \n" // f1 = input[0]
    "   lsi             f9, %2, 0               \n" // f9 = cursor[0]
    "   wfr             f0, %3                  \n" // f0 = g
    "   madd.s          f1, f0, f9              \n" // f1 += g * cursor[0]
    "   lsi             f8, %2, 4               \n" // f8 = cursor[1]
    "   lsi             f7, %2, 8               \n" // f7 = cursor[2]
    "   lsi             f6, %2, 12              \n" // f6 = cursor[3]
    "   ssi             f1, %2, 0               \n" // cursor[0] = f1
    "   lsi             f1, %0, 4               \n" // f1 = input[1]
    "   lsi             f5, %1, 0               \n" // f5 = output[0]
    "   madd.s          f1, f0, f8              \n" // f1 += g * cursor[1]
    "   lsi             f4, %1, 4               \n" // f4 = output[1]
    "   lsi             f3, %1, 8               \n" // f3 = output[2]
    "   add.s           f5, f5, f9              \n" // f5 += cursor[0]
    "   ssi             f1, %2, 4               \n" // cursor[1] = f1
    "   lsi             f2, %0, 8               \n" // f2 = input[2]
    "   lsi             f1, %1, 12              \n" // f1 = output[3]
    "   madd.s          f2, f0, f7              \n" // f2 += g * cursor[2]
    "   add.s           f4, f4, f8              \n" // f4 += cursor[1]
    "   add.s           f3, f3, f7              \n" // f3 += cursor[2]
    "   add.s           f1, f1, f6              \n" // f1 += cursor[3]
    "   ssi             f2, %2, 8               \n" // cursor[2] = f2
    "   lsi             f2, %0, 12              \n" // f2 = input[3]
    "   ssi             f5, %1, 0               \n" // output[0] = f5
    "   madd.s          f2, f6, f0              \n" // f2 += cursor[3] * g
    "   ssi             f4, %1, 4               \n" // output[1] = f4
    "   ssi             f3, %1, 8               \n" // output[2] = f3
    "   ssi             f1, %1, 12              \n" // output[3] = f1
    "   ssi             f2, %2, 12              \n" // cursor[3] = f2
    : // output-list            // アセンブリ言語からC/C++への受渡し
    : // input-list             // C/C++からアセンブリ言語への受渡し
        "r" ( input ),          // %0 = input
        "r" ( output ),         // %1 = output
        "r" ( cursor ),         // %2 = cursor
        "r" ( g )               // %3 = g
    : // clobber-list           // 値を書き換えたレジスタの申告
        "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9"
    );
    cursor += 4;
#else
    for (uint_fast8_t i = 0; i < 4; i++)
    {
        const float readback = *cursor;
        const float newValue = readback * g + input[i];
        *cursor = newValue;
        cursor++;
        output[i] += readback; // このリバーブではコムフィルターは並列でのみ用いられるので、加算したほうが処理の都合がいい
    }
#endif

    if (should_loop)
    {
        // コピーしておいたバッファの一部分を元の場所に戻す
        buffer_start[0] = buffer_end[0];
        buffer_start[1] = buffer_end[1];
        buffer_start[2] = buffer_end[2];
        // ループ
        cursor -= buffer_end - buffer_start;
    }
    comb->cursor = cursor;
}

// オールパスフィルター処理を4サンプル進める
__attribute((optimize("-O3")))
inline void allpass_filter_process4(const float *input, float *__restrict__ output, struct filter_t *allpass)
{
    float *buffer_start = allpass->buffer_start;
    float *cursor = allpass->cursor;
    float g = allpass->g;
    float *buffer_end = allpass->buffer_end;

    // bufferのループ処理をforループ内で行う代わりにその前後で行う
    bool should_loop = (cursor + 4) >= buffer_end;
    if (should_loop)
    {
        // バッファの始まりの部分をバッファの直後にコピーしておく
        buffer_end[0] = buffer_start[0];
        buffer_end[1] = buffer_start[1];
        buffer_end[2] = buffer_start[2];
    }

    // 実際のオールパスフィルター処理
#if CONFIG_IDF_TARGET_ESP32S3
    // ESP32S3の場合はSIMD命令を使って高速化
    __asm__ (
    "   lsi             f1, %0, 0               \n" // f1 = input[0]
    "   lsi             f5, %2, 0               \n" // f5 = cursor[0]
    "   wfr             f0, %3                  \n" // f0 = g
    "   msub.s          f5, f0, f1              \n" // f5 -= g * input[0]
    "   lsi             f4, %2, 4               \n" // f4 = cursor[1]
    "   lsi             f3, %2, 8               \n" // f3 = cursor[2]
    "   lsi             f2, %2, 12              \n" // f2 = cursor[3]
    "   madd.s          f1, f0, f5              \n" // f1 += g * f5
    "   ssi             f5, %1, 0               \n" // output[0] = f5
    "   ssi             f1, %2, 0               \n" // cursor[0] = f1
    "   lsi             f1, %0, 4               \n" // f1 = input[1]
    "   msub.s          f4, f0, f1              \n" // f4 -= g * input[1]
    "   madd.s          f1, f0, f4              \n" // f1 += g * f4
    "   ssi             f4, %1, 4               \n" // output[1] = f4
    "   ssi             f1, %2, 4               \n" // cursor[1] = f1
    "   lsi             f1, %0, 8               \n" // f1 = input[2]
    "   msub.s          f3, f0, f1              \n" // f3 -= g * input[2]
    "   madd.s          f1, f0, f3              \n" // f1 += g * f3
    "   ssi             f3, %1, 8               \n" // output[2] = f3
    "   ssi             f1, %2, 8               \n" // cursor[2] = f1
    "   lsi             f1, %0, 12              \n" // f1 = input[3]
    "   msub.s          f2, f1, f0              \n" // f2 -= input[3] * g
    "   madd.s          f1, f0, f2              \n" // f1 += g * f2
    "   ssi             f2, %1, 12              \n" // output[3] = f2
    "   ssi             f1, %2, 12              \n" // cursor[3] = f1
    : // output-list            // アセンブリ言語からC/C++への受渡し
    : // input-list             // C/C++からアセンブリ言語への受渡し
        "r" ( input ),          // %0 = input
        "r" ( output ),         // %1 = output
        "r" ( cursor ),         // %2 = cursor
        "r" ( g )               // %3 = g
    : // clobber-list           // 値を書き換えたレジスタの申告
        "f0", "f1", "f2", "f3", "f4", "f5"
    );
    cursor += 4;
#else
    for (uint_fast8_t i = 0; i < 4; i++)
    {
        float readback = *cursor;
        readback += (-g) * input[i];
        const float newValue = readback * g + input[i];
        *cursor = newValue;
        cursor++;
        output[i] = readback; // コムフィルターと異なり上書きする
    }
#endif

    if (should_loop)
    {
        // コピーしておいたバッファの一部分を元の場所に戻す
        buffer_start[0] = buffer_end[0];
        buffer_start[1] = buffer_end[1];
        buffer_start[2] = buffer_end[2];
        // ループ
        cursor -= buffer_end - buffer_start;
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
            comb_filter_process4(buf, pr, &combs[f]); // 内部でprocessedに加算される
        }
        // コムフィルターの出力は本来足して平均を取るべきだが、最初に0.25を掛けているので単純に足し合わせるだけでOK

        // 3つのオールパスフィルター(直列)
        for (uint_fast8_t f = 0; f < 3; f++)
        {
            allpass_filter_process4(pr, pr, &allpasses[f]); // processedは内部で上書きされる
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