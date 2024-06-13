#include <EffectReverb.h>
#include <M5Unified.h>

// 各フィルターの設定
// バッファの後ろは4サンプル以上空けておく必要がある
// バッファは16バイトアラインされている必要がある
struct filter_t
{
    // buffer_end - buffer_start = 遅延の長さ(サンプル数)
    // buffer_end[0] - buffer_end[3]はバッファに含まれないがメモリとして自由に使える領域
    float *buffer_start;
    float *cursor;
    float g; // フィードバックのレベル 一般的にgで表される
    float *buffer_end;
};

struct filter_t combs[4];
struct filter_t allpasses[3];

void EffectReverb::Init()
{
    // 必要な分より少し多めにメモリを確保してしまっていますが許容しています
    size_t size = ((REVERB_DELAY_BASIS_COMB_0 +
                    REVERB_DELAY_BASIS_COMB_1 +
                    REVERB_DELAY_BASIS_COMB_2 +
                    REVERB_DELAY_BASIS_COMB_3 +
                    REVERB_DELAY_BASIS_ALL_0 +
                    REVERB_DELAY_BASIS_ALL_1 +
                    REVERB_DELAY_BASIS_ALL_2 +
                    3 * 8) &
                   ~0b11) *
                  sizeof(float);
#if defined(M5UNIFIED_PC_BUILD)
    memory = (float *)calloc(1, size);
#else
    // DRAMに16バイトアラインされた状態でメモリを確保する (SIMDを使用するには16バイトアラインされている必要がある)
    memory = (float *)heap_caps_aligned_calloc(16, 1, size, MALLOC_CAP_INTERNAL);
#endif

    // 現状、timeは0.11〜1.0のみ対応
    if (time > 1.0) time = 1.0;
    else if (time < 0.11) time = 0.11;

    // 各フィルター構造体の初期化 (バッファは16バイトアラインしておく)
    float *cursor = memory;
    combs[0] = filter_t{cursor, cursor, 0.805f, cursor + ((uint32_t)(time * REVERB_DELAY_BASIS_COMB_0) & ~0b11)};
    cursor += (REVERB_DELAY_BASIS_COMB_0 + 3) & ~0b11;
    combs[1] = filter_t{cursor, cursor, 0.827f, cursor + ((uint32_t)(time * REVERB_DELAY_BASIS_COMB_1) & ~0b11)};
    cursor += (REVERB_DELAY_BASIS_COMB_1 + 3) & ~0b11;
    combs[2] = filter_t{cursor, cursor, 0.783f, cursor + ((uint32_t)(time * REVERB_DELAY_BASIS_COMB_2) & ~0b11)};
    cursor += (REVERB_DELAY_BASIS_COMB_2 + 3) & ~0b11;
    combs[3] = filter_t{cursor, cursor, 0.764f, cursor + ((uint32_t)(time * REVERB_DELAY_BASIS_COMB_3) & ~0b11)};
    cursor += (REVERB_DELAY_BASIS_COMB_3 + 3) & ~0b11;
    allpasses[0] = filter_t{cursor, cursor, 0.7f, cursor + ((uint32_t)(time * REVERB_DELAY_BASIS_ALL_0) & ~0b11)};
    cursor += (REVERB_DELAY_BASIS_ALL_0 + 3) & ~0b11;
    allpasses[1] = filter_t{cursor, cursor, 0.7f, cursor + ((uint32_t)(time * REVERB_DELAY_BASIS_ALL_1) & ~0b11)};
    cursor += (REVERB_DELAY_BASIS_ALL_1 + 3) & ~0b11;
    allpasses[2] = filter_t{cursor, cursor, 0.7f, cursor + ((uint32_t)(time * REVERB_DELAY_BASIS_ALL_2) & ~0b11)};
    cursor += (REVERB_DELAY_BASIS_ALL_2 + 3) & ~0b11;

    M5.Log.printf("memory: %x, comb0: %x-%x, comb1: %x-%x, comb3: %x-%x, comb4: %x-%x",
        memory,
        combs[0].buffer_start, combs[0].buffer_end,
        combs[1].buffer_start, combs[1].buffer_end,
        combs[2].buffer_start, combs[2].buffer_end,
        combs[3].buffer_start, combs[3].buffer_end);
}

__attribute((optimize("-O3")))
void comb_filter_process(const float *input, float *output, struct filter_t *comb, size_t len)
{
    float *buffer_start = comb->buffer_start;
    float *cursor = comb->cursor;
    float g = comb->g;
    float *buffer_end = comb->buffer_end;

    do
    {
        // バッファ終端までの容量で処理できるループ数を計算
        // 4の倍数にするために+3して 下位2bitを捨てる
        size_t remain = (buffer_end - cursor + 3) & ~0b11;
        // バッファ先頭からスタートする場合はバッファを終端まで使い切らない
        // これによりバッファ終端と先頭を繋ぐ処理をループ後のみにまとめることができる
        if (remain > 4 && (cursor - buffer_start) < 4)
        {
            remain -= 4;
        }
        if (len < remain)
        {
            remain = len;
        }
        len -= remain;

        // コムフィルター処理
        // 一度に4サンプル処理するのでループ回数を1/4にする
        remain >>= 2;
#if CONFIG_IDF_TARGET_ESP32S3
        // ESP32S3の場合はSIMD命令を使って高速化
        __asm__ (
        // f0 | f1 -- f4 | f5 -- f8 | f9 -- f12 |
        //  g | readback | outValue |  inValue  | 
        "   wfr             f0, %4                  \n" // f0 = g
        "   beqz.n          %0, .COMB_LOOP_END      \n"
        "   loop            %0, .COMB_LOOP_END      \n" // remain回ループ
        "   lsi             f1, %3, 0               \n" // readback0 = cursor[0]
        "   lsi             f2, %3, 4               \n" // readback1 = cursor[1]
        "   lsi             f3, %3, 8               \n" // readback2 = cursor[2]
        "   lsi             f4, %3, 12              \n" // readback3 = cursor[3]
        "   lsi             f5, %2, 0               \n" // outValue0 = output[0]
        "   lsi             f6, %2, 4               \n" // outValue1 = output[1]
        "   lsi             f7, %2, 8               \n" // outValue2 = output[2]
        "   lsi             f8, %2, 12              \n" // outValue3 = output[3]
        "   lsi             f9, %1, 0               \n" // inValue0 = input[0]
        "   lsi             f10, %1, 4              \n" // inValue1 = input[1]
        "   lsi             f11, %1, 8              \n" // inValue2 = input[2]
        "   lsi             f12, %1, 12             \n" // inValue3 = input[3]
        "   add.s           f5, f1, f5              \n" // outValue0 += readback0
        "   add.s           f6, f2, f6              \n" // outValue1 += readback1
        "   add.s           f7, f3, f7              \n" // outValue2 += readback2
        "   add.s           f8, f4, f8              \n" // outValue3 += readback3
        "   madd.s          f9, f0, f1              \n" // inValue0 += g * readback0
        "   madd.s          f10, f0, f2             \n" // inValue1 += g * readback1
        "   madd.s          f11, f0, f3             \n" // inValue2 += g * readback2
        "   madd.s          f12, f0, f4             \n" // inValue3 += g * readback3
        "   ssi             f5, %2, 0               \n" // output[0] = outValue0
        "   ssi             f6, %2, 4               \n" // output[1] = outValue1
        "   ssi             f7, %2, 8               \n" // output[2] = outValue2
        "   ssi             f8, %2, 12              \n" // output[3] = outValue3
        "   ssi             f9, %3, 0               \n" // cursor[0] = inValue0
        "   ssi             f10, %3, 4              \n" // cursor[1] = inValue1
        "   ssi             f11, %3, 8              \n" // cursor[2] = inValue2
        "   ssi             f12, %3, 12             \n" // cursor[3] = inValue3
        "   addi            %1, %1, 16              \n" // input += 4
        "   addi            %3, %3, 16              \n" // cursor += 4
        "   addi            %2, %2, 16              \n" // output += 4
        ".COMB_LOOP_END:                            \n"
        : // output-list            // アセンブリ言語からC/C++への受渡し
        : // input-list             // C/C++からアセンブリ言語への受渡し
            "r" ( remain ),         // %0 = remain
            "r" ( input ),          // %1 = input
            "r" ( output ),         // %2 = output
            "r" ( cursor ),         // %3 = cursor
            "r" ( g )               // %4 = g
        : // clobber-list           // 値を書き換えたレジスタの申告
            "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12"
        );
#else
        for (size_t i = 0; i < remain; i++)
        {
            const float readback0 = cursor[0];
            const float readback1 = cursor[1];
            const float readback2 = cursor[2];
            const float readback3 = cursor[3];
            float outValue0 = output[0];
            float outValue1 = output[1];
            float outValue2 = output[2];
            float outValue3 = output[3];
            outValue0 += readback0; // このリバーブではコムフィルターは並列でのみ用いられるので、加算したほうが処理の都合がいい
            outValue1 += readback1;
            outValue2 += readback2;
            outValue3 += readback3;
            const float inValue0 = input[0];
            const float inValue1 = input[1];
            const float inValue2 = input[2];
            const float inValue3 = input[3];
            output[0] = outValue0;
            output[1] = outValue1;
            output[2] = outValue2;
            output[3] = outValue3;
            cursor[0] = readback0 * g + inValue0;
            cursor[1] = readback1 * g + inValue1;
            cursor[2] = readback2 * g + inValue2;
            cursor[3] = readback3 * g + inValue3;
            input += 4;
            cursor += 4;
            output += 4;
        }
#endif

        // バッファ終端と先頭をノイズなくつなげるための処理
        if (cursor >= buffer_end)
        {
            cursor -= buffer_end - buffer_start;
            buffer_start[0] = buffer_end[0];
            buffer_start[1] = buffer_end[1];
            buffer_start[2] = buffer_end[2];
            // buffer_start[3] = buffer_end[3]; // SIMDを使う場合は実質的にここも行われる
        }
        else
        {
            // バッファの先頭を使った直後に通る処理 (ここを通る回数はそれほど多くない)
            buffer_end[0] = buffer_start[0];
            buffer_end[1] = buffer_start[1];
            buffer_end[2] = buffer_start[2];
            // buffer_end[3] = buffer_start[3]; // SIMDを使う場合は実質的にここも行われる
        }
    } while (len);
    comb->cursor = cursor;
}

__attribute((optimize("-O3")))
void allpass_filter_process(const float *input, float *output, struct filter_t *allpass, size_t len)
{
    float *buffer_start = allpass->buffer_start;
    float *cursor = allpass->cursor;
    float g = allpass->g;
    float *buffer_end = allpass->buffer_end;

    do
    {
        // バッファ終端までの容量で処理できるループ数を計算
        // 4の倍数にするために+3して 下位2bitを捨てる
        size_t remain = (buffer_end - cursor + 3) & ~3;
        // バッファ先頭からスタートする場合はバッファを終端まで使い切らない
        // これによりバッファ終端と先頭を繋ぐ処理をループ後のみにまとめることができる
        if (remain > 4 && (cursor - buffer_start) < 4)
        {
            remain -= 4;
        }
        if (len < remain)
        {
            remain = len;
        }
        len -= remain;
        
        // オールパスフィルター処理
        // 一度に4サンプル処理するのでループ回数を1/4にする
        remain >>= 2;
#if CONFIG_IDF_TARGET_ESP32S3
        // ESP32S3の場合はSIMD命令を使って高速化
        __asm__ (
        // f0 | f1 -- f4 | f5 -- f8 |
        //  g | readback | newValue |
        "   wfr             f0, %4                  \n" // f0 = g
        "   beqz.n          %0, .ALLPASS_LOOP_END   \n"
        "   loop            %0, .ALLPASS_LOOP_END   \n" // remain回ループ
        "   lsi             f5, %1, 0               \n" // newValue0 = input[0]
        "   lsi             f6, %1, 4               \n" // newValue1 = input[1]
        "   lsi             f7, %1, 8               \n" // newValue2 = input[2]
        "   lsi             f8, %1, 12              \n" // newValue3 = input[3]
        "   lsi             f1, %3, 0               \n" // readback0 = cursor[0]
        "   lsi             f2, %3, 4               \n" // readback1 = cursor[1]
        "   lsi             f3, %3, 8               \n" // readback2 = cursor[2]
        "   lsi             f4, %3, 12              \n" // readback3 = cursor[3]
        "   msub.s          f1, f0, f5              \n" // readback0 -= g * newValue0
        "   msub.s          f2, f0, f6              \n" // readback1 -= g * newValue1
        "   msub.s          f3, f0, f7              \n" // readback2 -= g * newValue2
        "   msub.s          f4, f0, f8              \n" // readback3 -= g * newValue3
        "   madd.s          f5, f0, f1              \n" // newValue0 += g * readback0
        "   madd.s          f6, f0, f2              \n" // newValue1 += g * readback1
        "   madd.s          f7, f0, f3              \n" // newValue2 += g * readback2
        "   madd.s          f8, f0, f4              \n" // newValue3 += g * readback3
        "   ssi             f5, %3, 0               \n" // cursor[0] = newValue0
        "   ssi             f6, %3, 4               \n" // cursor[1] = newValue1
        "   ssi             f7, %3, 8               \n" // cursor[2] = newValue2
        "   ssi             f8, %3, 12              \n" // cursor[3] = newValue3
        "   ssi             f1, %2, 0               \n" // output[0] = readback0
        "   ssi             f2, %2, 4               \n" // output[1] = readback1
        "   ssi             f3, %2, 8               \n" // output[2] = readback2
        "   ssi             f4, %2, 12              \n" // output[3] = readback3
        "   addi            %1, %1, 16              \n" // input += 4
        "   addi            %3, %3, 16              \n" // cursor += 4
        "   addi            %2, %2, 16              \n" // output += 4
        ".ALLPASS_LOOP_END:                         \n"
        : // output-list            // アセンブリ言語からC/C++への受渡し
        : // input-list             // C/C++からアセンブリ言語への受渡し
            "r" ( remain ),         // %0 = remain
            "r" ( input ),          // %1 = input
            "r" ( output ),         // %2 = output
            "r" ( cursor ),         // %3 = cursor
            "r" ( g )               // %4 = g
        : // clobber-list           // 値を書き換えたレジスタの申告
            "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8"
        );
#else
        for (size_t i = 0; i < remain; i++)
        {
            float readback0 = cursor[0];
            float readback1 = cursor[1];
            float readback2 = cursor[2];
            float readback3 = cursor[3];
            float newValue0 = input[0];
            float newValue1 = input[1];
            float newValue2 = input[2];
            float newValue3 = input[3];
            readback0 += (-g) * newValue0;
            readback1 += (-g) * newValue1;
            readback2 += (-g) * newValue2;
            readback3 += (-g) * newValue3;
            newValue0 += readback0 * g;
            newValue1 += readback1 * g;
            newValue2 += readback2 * g;
            newValue3 += readback3 * g;
            cursor[0] = newValue0;
            cursor[1] = newValue1;
            cursor[2] = newValue2;
            cursor[3] = newValue3;
            output[0] = readback0;
            output[1] = readback1;
            output[2] = readback2;
            output[3] = readback3;
            input += 4;
            cursor += 4;
            output += 4;
        }
#endif

        // バッファ終端と先頭をノイズなくつなげるための処理
        if (cursor >= buffer_end)
        {
            cursor -= buffer_end - buffer_start;
            buffer_start[0] = buffer_end[0];
            buffer_start[1] = buffer_end[1];
            buffer_start[2] = buffer_end[2];
            // buffer_start[3] = buffer_end[3]; // SIMDを使う場合は実質的にここも行われる
        }
        else
        {
            // バッファの先頭を使った直後に通る処理 (ここを通る回数はそれほど多くない)
            buffer_end[0] = buffer_start[0];
            buffer_end[1] = buffer_start[1];
            buffer_end[2] = buffer_start[2];
            // buffer_end[3] = buffer_start[3]; // SIMDを使う場合は実質的にここも行われる
        }
    } while (len);
    allpass->cursor = cursor;
}

__attribute((optimize("-O3")))
void EffectReverb::Process(const float *input, float *__restrict__ output)
{
    // 最初に振幅を下げてここに格納しておく(リバーブの効果は絶対的な振幅に影響されないためOK)
    // SIMDのために16バイトアラインメント指定を入れておく
    float buffer[bufferSize] __attribute__((aligned(16)));
    float multiplier = level * 0.25f; // 0.25fはコムフィルターの平均を取るため

#if defined(M5UNIFIED_PC_BUILD)
    float processed[bufferSize]; // これが最終的にリバーブ成分になる
    memset(processed, 0, sizeof(float) * bufferSize);
#else
    float processed[bufferSize] __attribute__((aligned(16))) = {0.0f}; // これが最終的にリバーブ成分になる
#endif

    // 入力の振幅を下げてbufferに格納
    uint32_t length = bufferSize >> 2; // 1ループで4サンプル処理する
    const float *in = input;
    float *buf = buffer;
    do
    {
        buf[0] = in[0] * multiplier;
        buf[1] = in[1] * multiplier;
        buf[2] = in[2] * multiplier;
        buf[3] = in[3] * multiplier;
        in += 4;
        buf += 4;
    } while (--length);

    // 4つのコムフィルター(並列)
    for (uint_fast8_t f = 0; f < 4; f++)
    {
        // 内部でprocessedに加算される
        // コムフィルターの出力は本来足して平均を取るべきだが、最初に0.25を掛けているので単純に足し合わせるだけでOK
        comb_filter_process(buffer, processed, &combs[f], bufferSize);
    }

    // 3つのオールパスフィルター(直列)
    for (uint_fast8_t f = 0; f < 3; f++)
    {
        allpass_filter_process(processed, processed, &allpasses[f], bufferSize); // processedは内部で上書きされる
    }

    // 原音と合わせて出力 (今は動作確認のためリバーブ成分のみ)
    length = bufferSize >> 2; // 1ループで4サンプル処理する
    float *pr = processed;
    float *out = output;
    do
    {
        out[0] = pr[0];
        out[1] = pr[1];
        out[2] = pr[2];
        out[3] = pr[3];
        pr += 4;
        out += 4;
    } while (--length);
}
