#if defined ( __XTENSA__ )

#if __has_include (<sdkconfig.h>)
#include <sdkconfig.h>
#endif

/// 波形合成処理を行う関数。 ループ一回につき4サンプル分の処理を行う。
// void sampler_process_inner(proc_inner_work_t* work, uint32_t length)
// a2 = proc_inner_work_t* work
// a3 = uint32_t length
    .global     sampler_process_inner
    .section    .text
    .align      4
sampler_process_inner:
    entry       sp, 32
    lsi         f4, a2, 8               // f4 に pos_f を設定
    lsi         f14,a2, 16              // f14 に pitch を設定
    l32i        a4, a2, 4               // a4 に出力先アドレスを取得
    add.s       f5, f4, f14             // f5 = pos_f1 = pos_f + pitch
    add.s       f14,f14,f14             // f14 pitch = pitch * 2
    addx4       a15,a3, a4              // a15 に出力先アドレスの終了アドレスを設定
    s32i        a15,a2, 4               // 出力先終了アドレスを結果データに先に書き戻して置く
    srli        a3, a3, 2               // length /= 4 ループ回数を1/4にする (4サンプルずつ処理するため)
    beqz        a3, PROC_INNER1_END     // length が 0 の場合は終了

// ループ開始前の時点で pos_f を4サンプル分作成しておく
    l32i        a8, a2, 0               // a8 に元データのアドレスを取得
    add.s       f6, f4, f14             // f6 = pos_f2 = pos_f + pitch*2
    add.s       f7, f5, f14             // f7 = pos_f3 = pos_f1 + pitch*2
    add.s       f14,f14,f14             // f14 元のpitchの4倍の値になる
    utrunc.s    a13,f5,  0              // a13 = pos_f1 の整数部分を取得
    utrunc.s    a14,f6,  0              // a14 = pos_f2 の整数部分を取得
    utrunc.s    a15,f7,  0              // a15 = pos_f3 の整数部分を取得
    lsi         f15,a2, 12              // f15 に gain を設定
    addx2       a9, a13,a8              // a9 に元データの1つ先のサンプルアドレスを取得
    addx2       a10,a14,a8              // a10 に元データの2つ先のサンプルアドレスを取得
    addx2       a11,a15,a8              // a11 に元データの3つ先のサンプルアドレスを取得
    ufloat.s    f9, a13, 0              // f9  = pos_f1 の整数部分をfloatに変換
    ufloat.s    f10,a14, 0              // f10 = pos_f2 の整数部分をfloatに変換
    ufloat.s    f11,a15, 0              // f11 = pos_f3 の整数部分をfloatに変換

/// ここまでで f4, f5, f6, f7 に pos_f の値が4サンプル分が用意される。
/// また、 a8, a9, a10, a11 に データ取得アドレス 4サンプル分が用意される。
/// pitch は元の値の4倍にして f14 に格納しておく

    loop        a3, PROC_INNER1_LOOP_END // ループ開始

                            //↓ここのコメントは結果が出るまでに必要なCPUサイクル数。
                            // 例えば a3:3c は a3に結果が出るのに3サイクルかかることを示す
    l16si       a12,a8, 0               // a12 に元データ0 を取得
    l16si       a13,a9, 0               // a13 に元データ1 を取得
    l16si       a14,a8, 2   //f14:2c    // a14 に元データ0 の右隣の値を取得
    l16si       a15,a9, 2   //f15:2c    // a15 に元データ1 の右側の値を取得
    sub.s       f5, f5, f9              // f5 = pos_f1 の小数部分を計算
    sub         a14,a14,a12             // a14 = diff0 = 元データ0 と右隣の差分を計算
    sub         a15,a15,a13             // a15 = diff1 = 元データ1 と右隣の差分を計算
    float.s     f8, a12,0   // f8:1c    // f8 = (float)a12 元データ0 を float に変換
    float.s     f9, a13,0   // f9:1c    // f9 = (float)a13 元データ1 を float に変換
    float.s     f12,a14,0   //f12:1c    // f12 = (float)a14 diff0を float に変換
    float.s     f13,a15,0   //f13:1c    // f13 = (float)a15 diff1を float に変換
    madd.s      f8, f12,f4  // f8:3c    // 出力値 f8 += diff0 * pos_f0
    madd.s      f9, f13,f5  // f9:3c    // 出力値 f9 += diff1 * pos_f1

    l16si       a12,a10,0               // a12 に元データ2 を取得
    l16si       a13,a11,0               // a13 に元データ3 を取得
    l16si       a14,a10,2               // a14 に元データ2 の右隣の値を取得
    l16si       a15,a11,2               // a15 に元データ3 の右側の値を取得
    sub.s       f6, f6, f10             // f6 = pos_f2 の小数部分を計算
    sub.s       f7, f7, f11             // f7 = pos_f3 の小数部分を計算
    sub         a14,a14,a12             // a14 = diff2 = 元データ2 と右隣の差分を計算
    sub         a15,a15,a13             // a15 = diff3 = 元データ3 と右隣の差分を計算
    float.s     f10,a12,0   //f10:1c    // f10 = (float)a12 元データ2 を float に変換
    float.s     f11,a13,0   //f11:1c    // f11 = (float)a13 元データ3 を float に変換
    float.s     f12,a14,0   //f12:1c    // f12 = (float)a14 diff2を float に変換
    float.s     f13,a15,0   //f13:1c    // f13 = (float)a15 diff3を float に変換
    madd.s      f10,f12,f6  //f10:3c    // 出力値 f10 += diff2 * pos_f2
    madd.s      f11,f13,f7  //f11:3c    // 出力値 f11 += diff3 * pos_f3

// ESP32S3はSIMD命令が使用可能なので分岐する
#if CONFIG_IDF_TARGET_ESP32S3
    ee.ldf.128.ip   f3,f2,f1,f0, a4, 0  // f0~f3 に既存の合成波形を読出しておく
#else
    lsi         f0, a4, 0               // f0 に既存の合成波形を読出しておく
    lsi         f1, a4, 4               // f1 に既存の合成波形を読出しておく
    lsi         f2, a4, 8               // f2 に既存の合成波形を読出しておく
    lsi         f3, a4, 12              // f3 に既存の合成波形を読出しておく
#endif

    madd.s      f0, f8, f15 // f0:3c    // f0 += 出力値 * gain
    madd.s      f1, f9, f15 // f1:3c    // f1 += 出力値 * gain
    madd.s      f2,f10, f15 // f2:3c    // f2 += 出力値 * gain
    madd.s      f3,f11, f15 // f3:3c    // f3 += 出力値 * gain
    add.s       f4, f4, f14 // f4:3c    // f4 = pos_f  += pitch*4
    add.s       f5, f5, f14 // f5:3c    // f5 = pos_f1 += pitch*4
    add.s       f6, f6, f14 // f6:3c    // f6 = pos_f2 += pitch*4
    add.s       f7, f7, f14 // f7:3c    // f7 = pos_f3 += pitch*4

// ESP32S3はSIMD命令が使用可能なので分岐する
#if CONFIG_IDF_TARGET_ESP32S3
    ee.stf.128.ip f3,f2,f1,f0, a4, 16   // f0~f3 の値を出力先にストアしa4を16進める
#else
    ssi         f0, a4, 0               // 合成結果の値 f0 を出力先にストア
    ssi         f1, a4, 4               // 合成結果の値 f1 を出力先にストア
    ssi         f2, a4, 8               // 合成結果の値 f2 を出力先にストア
    ssi         f3, a4, 12              // 合成結果の値 f3 を出力先にストア
    addi        a4, a4, 16              // 出力先アドレスを a4 を 16 バイト進める
#endif

    utrunc.s    a12,f4,  0  //a12:3c    // a12 = pos_f の整数部分を取得
    utrunc.s    a13,f5,  0  //a13:3c    // a13 = pos_f1 の整数部分を取得
    utrunc.s    a14,f6,  0  //a14:3c    // a14 = pos_f2 の整数部分を取得
    utrunc.s    a15,f7,  0  //a15:3c    // a15 = pos_f3 の整数部分を取得
    addx2       a8, a12,a8              // a8 サンプルアドレスを進める
    addx2       a9, a13,a9              // a9 サンプルアドレスを進める
    addx2       a10,a14,a10             // a10 サンプルアドレスを進める
    addx2       a11,a15,a11             // a11 サンプルアドレスを進める
    ufloat.s    f8, a12, 0  // f8:1c    // f8  = pos_f の整数部分をfloatに変換
    ufloat.s    f9, a13, 0  // f9:1c    // f9  = pos_f1 の整数部分をfloatに変換
    ufloat.s    f10,a14, 0  //f10:1c    // f10 = pos_f2 の整数部分をfloatに変換
    ufloat.s    f11,a15, 0  //f11:1c    // f11 = pos_f3 の整数部分をfloatに変換
    sub.s       f4, f4, f8              // f4 = pos_f の小数部分を計算

PROC_INNER1_LOOP_END:
    s32i        a8, a2, 0
    ssi         f4, a2, 8
PROC_INNER1_END:
    retw

#endif
