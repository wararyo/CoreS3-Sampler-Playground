#if defined ( __XTENSA__ )

#if __has_include (<sdkconfig.h>)
#include <sdkconfig.h>
#endif

// void sampler_process_inner(proc_inner_work_t* result, uint32_t length, float gain, float pitch)
// a2 = sampler_process_inner の引数
// a3 = uint32_t length
// a4 = float gain
// a5 = float pitch
    .global     sampler_process_inner
    .section    .text
    .align      4
sampler_process_inner:
    entry       sp, 32
    beqz        a3, FUNC1_END
    wfr         f4, a4                  // f4 に gain を設定
    wfr         f5, a5                  // f5 に pitch を設定
    l32i        a9, a2, 4               // a13 に出力先アドレスを取得
    l32i        a8, a2, 0               // a8 に元データのアドレスを取得
    lsi         f0, a2, 8               // f0 に pos_f を設定
    addx4       a13, a3, a9             // a15 に出力先アドレスの終了アドレスを設定
    s32i        a13, a2, 4              // 出力先終了アドレスを結果データに先に書き戻して置く
    addi        a9, a9, -4              // 出力先アドレスを 4 バイト戻す(ループ内の最適化の都合)
    add.s       f3, f0, f5              // f3 = posf + pitch 新しい pos_f を計算
    l16si       a15, a8, 0              // a15 に元データ[0]を取得
    l16si       a14, a8, 2              // a14 に元データ[1]を取得
    // ループ開始前に初回データの準備としてf3,a14,a15を準備しておく

    loop        a3, FUNC1_LOOP_END      // ループ開始
                              //↓ここのコメントは結果が出るまでに必要なCPUサイクル数。
                              // 例えば a12:3c は a12に結果が出るのに3サイクルかかることを示す
    utrunc.s    a12, f3, 0    //a12:3c  // a12 = f3 新しい pos_f の整数部分を取得
    sub         a14, a14, a15           // a14 = diff = s[1] - s[0] // 元データの差分を計算
    float.s     f15, a15, 0   //f15:1c  // f15 = (float)a15 元データ[0]をfloat値に変換
    float.s     f14, a14, 0   //f14:1c  // f14 = diff = (float)a14 差分値を float に変換
    lsi         f13, a9, 4              // f13 に既存の合成波形を読出しておく (事前に-4しているので+4オフセット)
    ufloat.s    f12, a12, 0   //f12:1c  // f12 = pos_fの整数部分をfloatに変換
    madd.s      f15, f14, f0  //f15:3c  // 出力値 f15 += diff * pitch
    sub.s       f0, f3, f12   // f0:3c  // f0 = 新しいpos_fの小数部分を計算
    addi        a9, a9, 4               // 合成波形の出力先を 4 バイト進める
    addx2       a8, a12, a8             // 元データのアドレスを pos_f の整数部分の分進める
    madd.s      f13, f15, f4  //f13:3c  // f13 += 出力値 * gain
    add.s       f3, f0, f5    // f3:3c  // 次回のループで使う新しい pos_f を計算 (f3 = pos_f + pitch)
    l16si       a15, a8, 0              // 次回のループで使う元データ s[0] を取得
    l16si       a14, a8, 2              // 次回のループで使う元データ s[1] を取得
    ssi         f13, a9, 0              // 合成結果の値f13を出力先にストア
FUNC1_LOOP_END:
    s32i        a8, a2, 0
    ssi         f0, a2, 8
FUNC1_END:
    retw

#endif
