#if defined ( __XTENSA__ )

#if __has_include (<sdkconfig.h>)
#include <sdkconfig.h>
#endif

// void sampler_process_inner(proc_inner_work_t* work, uint32_t length)
// a2 = proc_inner_work_t* work
// a3 = uint32_t length
    .global     sampler_process_inner
    .section    .text
    .align      4
sampler_process_inner:
    entry       sp, 32
    beqz        a3, PROC_INNER1_END
    l32i        a11,a2, 0               // a11 に元データのアドレスを取得
    l32i        a13,a2, 4               // a13 に出力先アドレスを取得
    lsi         f8, a2, 8               // f8 に pos_f を設定
    lsi         f9, a2, 12              // f9 に gain を設定
    lsi         f10,a2, 16              // f10 に pitch を設定
    addx4       a15,a3, a13             // a15 に出力先アドレスの終了アドレスを設定
    s32i        a15,a2, 4               // 出力先終了アドレスを結果データに先に書き戻して置く
    addi        a13,a13,-4              // 出力先アドレスを 4 バイト戻す(ループ内の最適化の都合)
    add.s       f11,f8, f10             // f11 = posf + pitch 新しい pos_f を計算
    l16si       a15,a11,0               // a15 に元データ[0]を取得
    l16si       a14,a11,2               // a14 に元データ[1]を取得
    // ループ開始前に初回データの準備としてf11,a14,a15を準備しておく

    loop        a3, PROC_INNER1_LOOP_END      // ループ開始
                            //↓ここのコメントは結果が出るまでに必要なCPUサイクル数。
                            // 例えば a12:3c は a12に結果が出るのに3サイクルかかることを示す
    utrunc.s    a12,f11,0   //a12:3c    // a12 = f11 新しい pos_f の整数部分を取得
    sub         a14,a14,a15             // a14 = diff = s[1] - s[0] // 元データの差分を計算
    float.s     f15,a15,0   //f15:1c    // f15 = (float)a15 元データ[0]をfloat値に変換
    float.s     f14,a14,0   //f14:1c    // f14 = diff = (float)a14 差分値を float に変換
    lsi         f13,a13,4               // f13 に既存の合成波形を読出しておく (事前に-4しているので+4オフセット)
    ufloat.s    f12,a12,0   //f12:1c    // f12 = pos_fの整数部分をfloatに変換
    madd.s      f15,f14,f8  //f15:3c    // 出力値 f15 += diff * pitch
    sub.s       f8, f11,f12 // f8:3c    // f8 = 新しいpos_fの小数部分を計算
    addi        a13,a13,4               // 合成波形の出力先を 4 バイト進める
    addx2       a11,a12,a11             // 元データのアドレスを pos_f の整数部分の分進める
    madd.s      f13,f15,f9  //f13:3c    // f13 += 出力値 * gain
    add.s       f11,f8, f10 //f11:3c    // 次回のループで使う新しい pos_f を計算 (f11 = pos_f + pitch)
    l16si       a15,a11,0               // 次回のループで使う元データ s[0] を取得
    l16si       a14,a11,2               // 次回のループで使う元データ s[1] を取得
    ssi         f13,a13,0               // 合成結果の値f13を出力先にストア
PROC_INNER1_LOOP_END:
    s32i        a11,a2, 0
    ssi         f8, a2, 8
PROC_INNER1_END:
    retw

#endif
