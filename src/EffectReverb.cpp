#include <EffectReverb.h>
#include <M5Unified.h>
#include <cmath>
#if defined(M5UNIFIED_PC_BUILD)
#define M_PI 3.14159265359
#else
#include <bits/stdc++.h>
#endif 

// コム/オールパスフィルターの設定
// バッファの後ろは4サンプル以上空けておく必要がある
// バッファは16バイトアラインされている必要がある
struct feedback_filter_t
{
    // buffer_end - buffer_start = 遅延の長さ(サンプル数)
    // buffer_end[0] - buffer_end[3]はバッファに含まれないがメモリとして自由に使える領域
    float *buffer_start;
    float *cursor;
    float g; // フィードバックのレベル 一般的にgで表される
    float *buffer_end;
};

// 双二次フィルターの設定
struct biquad_filter_t
{
    float in2; // 2つ前の入力
    float in1; // 1つ前の入力
    float out2; // 2つ前の出力
    float out1; // 1つ前の出力

    // 各入力/出力に掛ける係数
    float f_in; // 入力
    float f_in1; // 1つ前の入力
    float f_in2; // 2つ前の入力
    float f_out1; // 1つ前の出力
    float f_out2; // 2つ前の出力
};

biquad_filter_t setup_bandpass_filter(uint32_t sample_rate, float cutoff_freq, float band_width_octave)
{
    float omega = 2.0f * M_PI * cutoff_freq / sample_rate;
    float alpha = std::sin(omega) * std::sinh(log(2.0f) / 2.0 * band_width_octave * omega / std::sin(omega));
    
    float a0 =  1.0f + alpha;
    float a1 = -2.0f * std::cos(omega);
    float a2 =  1.0f - alpha;
    float b0 =  alpha;
    float b1 =  0.0f;
    float b2 = -alpha;

    return biquad_filter_t{0.0f, 0.0f, 0.0f, 0.0f, b0/a0, b1/a0, b2/a0, a1/a0, a2/a0};
}

struct feedback_filter_t combs[4];
struct feedback_filter_t allpasses[3];
struct biquad_filter_t bandpass;

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
                    7 * 8) &
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
    combs[0] = feedback_filter_t{cursor, cursor, 0.805f, cursor + ((uint32_t)(time * REVERB_DELAY_BASIS_COMB_0) & ~0b11)};
    cursor += (REVERB_DELAY_BASIS_COMB_0 + 7) & ~0b11;
    combs[1] = feedback_filter_t{cursor, cursor, 0.827f, cursor + ((uint32_t)(time * REVERB_DELAY_BASIS_COMB_1) & ~0b11)};
    cursor += (REVERB_DELAY_BASIS_COMB_1 + 7) & ~0b11;
    combs[2] = feedback_filter_t{cursor, cursor, 0.783f, cursor + ((uint32_t)(time * REVERB_DELAY_BASIS_COMB_2) & ~0b11)};
    cursor += (REVERB_DELAY_BASIS_COMB_2 + 7) & ~0b11;
    combs[3] = feedback_filter_t{cursor, cursor, 0.764f, cursor + ((uint32_t)(time * REVERB_DELAY_BASIS_COMB_3) & ~0b11)};
    cursor += (REVERB_DELAY_BASIS_COMB_3 + 7) & ~0b11;
    allpasses[0] = feedback_filter_t{cursor, cursor, 0.7f, cursor + ((uint32_t)(time * REVERB_DELAY_BASIS_ALL_0) & ~0b11)};
    cursor += (REVERB_DELAY_BASIS_ALL_0 + 7) & ~0b11;
    allpasses[1] = feedback_filter_t{cursor, cursor, 0.7f, cursor + ((uint32_t)(time * REVERB_DELAY_BASIS_ALL_1) & ~0b11)};
    cursor += (REVERB_DELAY_BASIS_ALL_1 + 7) & ~0b11;
    allpasses[2] = feedback_filter_t{cursor, cursor, 0.7f, cursor + ((uint32_t)(time * REVERB_DELAY_BASIS_ALL_2) & ~0b11)};
    cursor += (REVERB_DELAY_BASIS_ALL_2 + 7) & ~0b11;

    bandpass = setup_bandpass_filter(sampleRate, 2000.0f, 1.0f);
}

__attribute((noinline, optimize("-O3")))
void comb_filter_process(const float *input, float *output, struct feedback_filter_t *comb, size_t len)
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
        // f0 |   f1 - f4   |   f5 - f8   |  f9 - f12  |
        //  g | readback3-0 | outValue3-0 | inValue3-0 | 
        "   wfr             f0, %4                    \n" // f0 = g
        "   beqz.n          %0, REVERB_COMB_LOOP_END  \n"
        "   loop            %0, REVERB_COMB_LOOP_END  \n" // remain回ループ
        "   ee.ldf.128.ip   f1, f2, f3, f4, %3, 0     \n" // readback3-0 = cursor[3-0]
        "   ee.ldf.128.ip   f5, f6, f7, f8, %2, 0     \n" // outValue3-0 = output[3-0]
        "   ee.ldf.128.ip   f9, f10, f11, f12, %1, 16 \n" // inValue3-0 = input[3-0]; input += 4
        "   add.s           f8, f4, f8                \n" // outValue0 += readback0
        "   add.s           f7, f3, f7                \n" // outValue1 += readback1
        "   add.s           f6, f2, f6                \n" // outValue2 += readback2
        "   add.s           f5, f1, f5                \n" // outValue3 += readback3
        "   madd.s          f12, f0, f4               \n" // inValue0 += g * readback0
        "   madd.s          f11, f0, f3               \n" // inValue1 += g * readback1
        "   madd.s          f10, f0, f2               \n" // inValue2 += g * readback2
        "   madd.s          f9, f0, f1                \n" // inValue3 += g * readback3
        "   ee.stf.128.ip   f5, f6, f7, f8, %2, 16    \n" // output[3-0] = outValue3-0; output += 4
        "   ee.stf.128.ip   f9, f10, f11, f12, %3, 16 \n" // cursor[3-0] = inValue3-0; cursor += 4
        "REVERB_COMB_LOOP_END:                        \n"
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
        }
        else
        {
            // バッファの先頭を使った直後に通る処理 (ここを通る回数はそれほど多くない)
            buffer_end[0] = buffer_start[0];
            buffer_end[1] = buffer_start[1];
            buffer_end[2] = buffer_start[2];
        }
    } while (len);
    comb->cursor = cursor;
}

__attribute((noinline, optimize("-O3")))
void allpass_filter_process(const float *input, float *output, struct feedback_filter_t *allpass, size_t len)
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
        // f0 |   f1 - f4   |   f5 - f8   |
        //  g | readback3-0 | newValue3-0 |
        "   wfr             f0, %4                      \n" // f0 = g
        "   beqz.n          %0, REVERB_ALLPASS_LOOP_END \n"
        "   loop            %0, REVERB_ALLPASS_LOOP_END \n" // remain回ループ
        "   ee.ldf.128.ip   f5, f6, f7, f8, %1, 16      \n" // newValue3-0 = input[3-0]; input += 4
        "   ee.ldf.128.ip   f1, f2, f3, f4, %3, 0       \n" // readback3-0 = cursor[3-0]
        "   msub.s          f4, f0, f8                  \n" // readback0 -= g * newValue0
        "   msub.s          f3, f0, f7                  \n" // readback1 -= g * newValue1
        "   msub.s          f2, f0, f6                  \n" // readback2 -= g * newValue2
        "   msub.s          f1, f0, f5                  \n" // readback3 -= g * newValue3
        "   madd.s          f8, f0, f4                  \n" // newValue0 += g * readback0
        "   madd.s          f7, f0, f3                  \n" // newValue1 += g * readback1
        "   madd.s          f6, f0, f2                  \n" // newValue2 += g * readback2
        "   madd.s          f5, f0, f1                  \n" // newValue3 += g * readback3
        "   ee.stf.128.ip   f5, f6, f7, f8, %3, 16      \n" // cursor[3-0] = newValue3-0; cursor += 4
        "   ee.stf.128.ip   f1, f2, f3, f4, %2, 16      \n" // output[3-0] = readback3-0; output += 4
        "REVERB_ALLPASS_LOOP_END:                       \n"
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
        }
        else
        {
            // バッファの先頭を使った直後に通る処理 (ここを通る回数はそれほど多くない)
            buffer_end[0] = buffer_start[0];
            buffer_end[1] = buffer_start[1];
            buffer_end[2] = buffer_start[2];
        }
    } while (len);
    allpass->cursor = cursor;
}

__attribute((noinline, optimize("-O3")))
void bandpass_filter_process(const float *input, float *output, struct biquad_filter_t *bandpass, size_t len)
{
    float in_m1 = bandpass->in1;
    float in_m2 = bandpass->in2;
    float out_m1 = bandpass->out1;
    float out_m2 = bandpass->out2;
    float f_in = bandpass->f_in;
    float f_in1 = bandpass->f_in1;
    float f_in2 = bandpass->f_in2;
    float f_out1 = bandpass->f_out1;
    float f_out2 = bandpass->f_out2;

    // バンドパスフィルター処理
    // 1ループで4サンプルを処理する
    const float *in = input;
    float *out = output;
    len >>= 2;
    do
    {
        float in_0 = in[0];
        float in_1 = in[1];
        float in_2 = in[2];
        float in_3 = in[3];
        float out_0 = f_in * in_0 + f_in1 * in_m1 + f_in2 * in_m2 - f_out1 * out_m1 - f_out2 * out_m2;
        float out_1 = f_in * in_1 + f_in1 * in_0 + f_in2 * in_m1 - f_out1 * out_0 - f_out2 * out_m1;
        float out_2 = f_in * in_2 + f_in1 * in_1 + f_in2 * in_0 - f_out1 * out_1 - f_out2 * out_0;
        float out_3 = f_in * in_3 + f_in1 * in_2 + f_in2 * in_1 - f_out1 * out_2 - f_out2 * out_1;
        in_m2  = in_2;        // 2つ前の入力信号を更新
        in_m1  = in_3;        // 1つ前の入力信号を更新
        out_m2 = out_2;       // 2つ前の出力信号を更新
        out_m1 = out_3;       // 1つ前の出力信号を更新
        out[0] = out_0;
        out[1] = out_1;
        out[2] = out_2;
        out[3] = out_3;
        in += 4;
        out += 4;
    } while (--len);
    
    // 次回処理に向けて直前の入力/出力を保存しておく
    bandpass->in1 = in_m1;
    bandpass->in2 = in_m2;
    bandpass->out1 = out_m1;
    bandpass->out2 = out_m2;
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

    { // 入力の振幅を下げてbufferに格納
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
    }

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

    bandpass_filter_process(processed, processed, &bandpass, bufferSize);

    { // 原音と合わせて出力
        uint32_t length = bufferSize >> 2; // 1ループで4サンプル処理する
        const float *in = input;
        float *pr = processed;
        float *out = output;
        do
        {
            out[0] = in[0] + pr[0];
            out[1] = in[1] + pr[1];
            out[2] = in[2] + pr[2];
            out[3] = in[3] + pr[3];
            in += 4;
            pr += 4;
            out += 4;
        } while (--length);
    }
}
