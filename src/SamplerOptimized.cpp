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
        if ((player->adsrGain - sample->sustain) < 0.01f)
        {
            player->adsrState = sustain;
            player->adsrGain = goal;
        }
        break;
    case sustain:
        break;
    case release:
        player->adsrGain *= sample->release;
        if (player->adsrGain < 0.01f)
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

__attribute((optimize("-O3")))
void SamplerOptimized::Process(int16_t* __restrict__ output)
{
    float data[SAMPLE_BUFFER_SIZE] = {0.0f};

    // 波形を生成
    for (uint8_t i = 0; i < MAX_SOUND; i++)
    {
        SamplePlayer *player = &players[i];
        if (player->playing == false)
            continue;
        Sample *sample = player->sample;
        if (sample->adsrEnabled)
            UpdateAdsr(player);
        if (player->playing == false)
            continue;

        float pitch = PitchFromNoteNo(player->noteNo, player->sample->root);

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
        auto pos = player->pos;
        float pos_f = player->pos_f;
        auto src = sample->sample;
/*
        // pitchが充分に低い場合 (0.5以下ぐらい？？) 、同一サンプル点から2回以上連続して線形補間処理ができるので、
        // それに合わせてより軽量になりそうな処理を試す。効果がなさそうならこの分岐内の処理は省いてもよい
        if (pitch <= 0.5f)
        {
            uint32_t n = 0;
            int32_t current_sample = src[pos];
            int32_t next_sample = src[pos + 1];
            float diff = next_sample - current_sample;
            do
            {
                // 現在の値をpos_fに応じて線形補間
                float val = (float)current_sample + diff * pos_f;
                // gainを掛けて波形合成
                data[n] += val * gain;
                pos_f += pitch;
                if (pos_f >= 1.0f) {
                    // pitchが1未満の条件下にあるため、pos_f+pitchは2未満であることが保証される。
                    pos_f-=1.0f;
                    if (++pos >= loopEnd) {
                        pos += loopBack;
                        if (loopBack == 0)
                        {   // ループポイントが設定されていない場合は再生を停止する
                            player->playing = false;
                            break;
                        }
                    }
                    current_sample = next_sample;
                    next_sample = src[pos + 1];
                    diff = next_sample - current_sample;
                }
            } while (++n < SAMPLE_BUFFER_SIZE);
        } else
//*/
        { // pitchの状況に関わらず補間処理による波形生成を行う

            // ループの残り回数をremainに保持しておく
            uint32_t remain = SAMPLE_BUFFER_SIZE;
            auto d = data;
            do {
                // loopEndに到達するまでに何個サンプル出力できるか求める
                int32_t length = 1 + ((loopEnd - pos) / pitch);
                // ループの残り回数を考慮してlengthを調整
                length = remain < length ? remain : length;
                // ループ残り回数から今回処理する分を引いておく
                remain -= length;
                auto s = &src[pos];
                // 処理回数が奇数の場合は先に1つ処理しておく
                if (length & 1) {
                    // サンプリング元データを2点読み込む
                    int32_t s0 = s[0];
                    int32_t s1 = s[1];
                    // 2点間の差分にpos_fを掛けて補間
                    auto val = s0 + (s1 - s0) * pos_f;
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
                    --length;
                }
                // 残りの処理回数は2の倍数であるので、回数を 1/2 にしておく
                length >>= 1;
                if (length)
                { // 1ループあたり2個のサンプルを処理する。(レジスタの使用効率を上げるため)
                    do {
                        int32_t s0 = s[0];
                        int32_t s1 = s[1];
                        auto val = s0 + (s1 - s0) * pos_f;
                        d[0] += val * gain;
                        pos_f += pitch;
                        uint32_t intval = pos_f;
                        pos_f -= intval;
                        s += intval;

                        s0 = s[0];
                        s1 = s[1];
                        val = s0 + (s1 - s0) * pos_f;
                        d[1] += val * gain;
                        pos_f += pitch;
                        intval = pos_f;
                        pos_f -= intval;
                        s += intval;
                        d += 2;
                    } while (--length);
                }
                // posの計算は最後にループ外で行う
                pos = s - src;
                if (pos >= loopEnd) {
                    if (loopBack == 0)
                    {   // ループポイントが設定されていない場合は再生を停止する
                        player->playing = false;
                        break;
                    }
                    while (pos >= loopEnd) {
                        pos += loopBack;
                    }
                }
            } while (remain != 0);
        }
        // ループを終えた後でposとpos_fをplayerに書き戻しておく
        player->pos = pos;
        player->pos_f = pos_f;
    }

    {
        auto o = output;
        auto d = data;
        for (int i = 0; i < SAMPLE_BUFFER_SIZE>>3; i++)
        { // 事前にマスターボリュームを適用しているので単純に代入するのみでよい
            o[0] = d[0];
            o[1] = d[1];
            o[2] = d[2];
            o[3] = d[3];
            o[4] = d[4];
            o[5] = d[5];
            o[6] = d[6];
            o[7] = d[7];
            o += 8;
            d += 8;
        }
    }
}
