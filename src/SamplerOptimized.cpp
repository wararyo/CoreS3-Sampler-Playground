#include <SamplerOptimized.h>

float SamplerOptimized::PitchFromNoteNo(float noteNo, float root)
{
    float delta = noteNo - root;
    float f = ((pow(2.0f, delta / 12.0f)));
    return f;
}

void SamplerOptimized::UpdateAdsr(SamplerOptimized::SamplePlayer *player)
{
    Sample *sample = player->sample;
    float goal;
    if (player->released)
        player->adsrState = release;

    switch (player->adsrState)
    {
    case attack:
        player->adsrGain += sample->attack * player->volume;
        if (player->adsrGain >= player->volume)
        {
            player->adsrGain = player->volume;
            player->adsrState = decay;
        }
        break;
    case decay:
        goal = sample->sustain * player->volume;
        player->adsrGain = (player->adsrGain - goal) * sample->decay + goal;
        if ((player->adsrGain - sample->sustain) < 0.001f)
        {
            player->adsrState = sustain;
            player->adsrGain = goal;
        }
        break;
    case sustain:
        break;
    case release:
        player->adsrGain *= sample->release;
        if (player->adsrGain < 0.001f)
        {
            player->adsrGain = 0;
            player->playing = false;
        }
        break;
    }
}

void SamplerOptimized::NoteOn(uint8_t noteNo, uint8_t velocity, uint8_t channel)
{
    uint8_t oldestPlayerId = 0;
    for (uint8_t i = 0; i < MAX_SOUND; i++)
    {
        if (players[i].playing == false)
        {
            players[i] = SamplerOptimized::SamplePlayer(sample, noteNo, velocity / 127.0f);
            return;
        }
        else
        {
            if (players[i].createdAt < players[oldestPlayerId].createdAt)
                oldestPlayerId = i;
        }
    }
    // 全てのPlayerが再生中だった時には、最も昔に発音されたPlayerを停止する
    players[oldestPlayerId] = SamplerOptimized::SamplePlayer(sample, noteNo, velocity / 127.0f);
}
void SamplerOptimized::NoteOff(uint8_t noteNo, uint8_t velocity, uint8_t channel)
{
    for (uint8_t i = 0; i < MAX_SOUND; i++)
    {
        if (players[i].playing == true && players[i].noteNo == noteNo)
        {
            players[i].released = true;
        }
    }
}

void SamplerOptimized::SetSample(uint8_t channel, Sample *s)
{
    sample = s;
}

// sampler_process_inner の動作時に必要なデータ類をまとめた構造体
// アセンブリ言語からアクセスするのでメンバの順序を変えないこと
struct sampler_process_inner_work_t {
    const int16_t* src;
    float* dst;
    float pos_f;
    float gain;
    float pitch;
};

extern "C" {
// アセンブリ言語版を呼び出すための関数宣言
// これをコメントアウトすると、アセンブリ言語版は呼ばれなくなり、C/C++版が呼ばれる
    void sampler_process_inner(sampler_process_inner_work_t* work, uint32_t length);
}

// アセンブリ言語版と同様の処理を行うC/C++版の実装
// weak属性を付けることで、アセンブリ言語版があればそちらを使う
__attribute((weak, optimize("-O3")))
void sampler_process_inner(sampler_process_inner_work_t* work, uint32_t length)
{
    const int16_t* s = work->src;
    float* d = work->dst;
    float pos_f = work->pos_f;
    float gain = work->gain;
    float pitch = work->pitch;
    do {
        int32_t s0 = s[0];
        int32_t s1 = s[1];
        // 2点間の差分にpos_fを掛けて補間
        float val = s0 + (s1 - s0) * pos_f;
        // 音量係数gainを掛けたあと波形合成
        d[0] += val * gain;
        // 出力先をひとつ進める
        ++d;
        // pos_fをpitchぶん進める
        pos_f += pitch;
        // pos_fから整数部分を取り出す
        uint32_t intval = pos_f;
        // pos_fから整数部分を引くことで小数部分のみを取り出す
        pos_f -= intval;
        // サンプリング元データ取得位置を進める
        s += intval;
        // 処理したのでlengthを1減らす
    } while (--length);
    // 結果をworkに書き戻す
    work->src = s;
    work->dst = d;
    work->pos_f = pos_f;
}

__attribute((optimize("-O3")))
void SamplerOptimized::Process(int16_t* __restrict__ output)
{
    float data[SAMPLE_BUFFER_SIZE] = {0.0f};

    // 波形を生成
    for (uint_fast8_t i = 0; i < MAX_SOUND; i++)
    {
        SamplePlayer *player = &players[i];
        if (player->playing == false)
            continue;

        for (uint_fast8_t j = 0; j < SAMPLE_BUFFER_SIZE / ADSR_UPDATE_SAMPLE_COUNT; j++)
        {
            Sample *sample = player->sample;
            if (sample->adsrEnabled)
                UpdateAdsr(player);
            if (player->playing == false)
                break;

            float pitch = player->pitch;

            int32_t loopEnd = sample->length;
            int32_t loopBack = 0;
            float gain = player->volume;
            // adsrEnabledが有効の場合はループポイントとadsrGainを使用する。
            if (sample->adsrEnabled) {
                loopEnd = sample->loopEnd;
                loopBack = sample->loopStart - loopEnd;
                gain = player->adsrGain;
            }

            // gainにマスターボリュームを適用しておく
            gain *= masterVolume;

            // playerのメンバであるposとpos_fをローカル変数にコピーしておく。
            uint32_t pos = player->pos;
            float pos_f = player->pos_f;
            auto src = sample->sample;

            // ループの残り回数をremainに保持しておく
            uint32_t remain = ADSR_UPDATE_SAMPLE_COUNT;
            sampler_process_inner_work_t work = {&src[pos], &data[j * ADSR_UPDATE_SAMPLE_COUNT], pos_f, gain, pitch};

            do {
                // loopEndに到達するまでに何個サンプル出力できるか求める
                int32_t length = 1 + ((loopEnd - pos) / pitch);
                // ループの残り回数を考慮してlengthを調整
                length = remain < length ? remain : length;
                // ループ残り回数から今回処理する分を引いておく
                remain -= length;

                // 波形処理関数を呼び出す。
                sampler_process_inner(&work, length);

                // 現在のサンプル位置に基づいてposがどこまで進んだか求める
                pos = work.src - src;
                if (pos >= loopEnd) {
                    if (loopBack == 0)
                    {   // ループポイントが設定されていない場合は再生を停止する
                        player->playing = false;
                        break;
                    }
                    do {
                        pos += loopBack;
                    } while (pos >= loopEnd);
                }
            } while (remain != 0);
            // ループを終えた後でposとpos_fをplayerに書き戻しておく
            player->pos = pos;
            player->pos_f = work.pos_f;
        }
    }

    { // 生成した波形をint16_tに変換して出力先に書き込む
#if CONFIG_IDF_TARGET_ESP32S3
        // ESP32S3の場合はSIMD命令を使って高速化
        __asm__ (
        "   loop            %2, LOOP_END            \n"     // ループ開始
        "   ee.ldf.128.ip   f11,f10,f9, f8, %1, 16  \n"     // 元データ float 4個 読み、a3 アドレスを 16 加算
        "   ee.ldf.128.ip   f15,f14,f13,f12,%1, 16  \n"     // 元データ float 4個 読み、a3 アドレスを 16 加算
        "   trunc.s         a12,f8, 0               \n"     // float 4個を int32_t 4個に変換
        "   trunc.s         a13,f9, 0               \n"     //
        "   trunc.s         a14,f10,0               \n"     //
        "   trunc.s         a15,f11,0               \n"     //
        "   clamps          a12,a12,15              \n"     // int32_t 4個の値が int16_t の範囲に収まるよう桁溢れを防止
        "   clamps          a13,a13,15              \n"     //
        "   clamps          a14,a14,15              \n"     //
        "   clamps          a15,a15,15              \n"     //
        "   s16i            a12,%0, 0               \n"     // 出力先に int16_t 4個書き込み
        "   s16i            a13,%0, 2               \n"     //
        "   s16i            a14,%0, 4               \n"     //
        "   s16i            a15,%0, 6               \n"     //
        "   trunc.s         a12,f12,0               \n"     // float 4個を int32_t 4個に変換
        "   trunc.s         a13,f13,0               \n"     //
        "   trunc.s         a14,f14,0               \n"     //
        "   trunc.s         a15,f15,0               \n"     //
        "   clamps          a12,a12,15              \n"     // int32_t 4個の値が int16_t の範囲に収まるよう桁溢れを防止
        "   clamps          a13,a13,15              \n"     //
        "   clamps          a14,a14,15              \n"     //
        "   clamps          a15,a15,15              \n"     //
        "   s16i            a12,%0,  8              \n"     // 出力先に int16_t 4個書き込み
        "   s16i            a13,%0, 10              \n"     //
        "   s16i            a14,%0, 12              \n"     //
        "   s16i            a15,%0, 14              \n"     //
        "   addi            %0, %0, 16              \n"     //
        "LOOP_END:                                  \n"     //
        : // output-list 使用せず    // アセンブリ言語からC/C++への受渡しは無し
        : // input-list             // C/C++からアセンブリ言語への受渡し
            "r" ( output ),         //  %0 に変数 output の値を指定
            "r" ( data ),           //  %1 に変数 data の値を指定
            "r" ( SAMPLE_BUFFER_SIZE>>3 )  // %2 にバッファ長 / 8 の値を設定
        : // clobber-list           //  値を書き換えたレジスタの申告
            "f8","f9","f10","f11","f12","f13","f14","f15",
            "a12","a13","a14","a15" //  書き変えたレジスタをコンパイラに知らせる
        );
#else
        auto o = output;
        auto d = data;
        for (int i = 0; i < SAMPLE_BUFFER_SIZE>>2; i++)
        { // 1ループあたりの処理回数を増やすことで処理効率を上げる
            int32_t d0 = d[0];
            int32_t d1 = d[1];
            int32_t d2 = d[2];
            int32_t d3 = d[3];
            o[0] = d0 > INT16_MAX ? INT16_MAX : d0 < INT16_MIN ? INT16_MIN : d0;
            o[1] = d1 > INT16_MAX ? INT16_MAX : d1 < INT16_MIN ? INT16_MIN : d1;
            o[2] = d2 > INT16_MAX ? INT16_MAX : d2 < INT16_MIN ? INT16_MIN : d2;
            o[3] = d3 > INT16_MAX ? INT16_MAX : d3 < INT16_MIN ? INT16_MIN : d3;
            o += 4;
            d += 4;
        }
#endif
    }
}
