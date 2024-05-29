#include <SamplerLegacy.h>

float SamplerLegacy::PitchFromNoteNo(float noteNo, float root)
{
    float delta = noteNo - root;
    float f = ((pow(2.0f, delta / 12.0f)));
    return f;
}

void SamplerLegacy::UpdateAdsr(SamplerLegacy::SamplePlayer *player)
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

void SamplerLegacy::NoteOn(uint8_t noteNo, uint8_t velocity, uint8_t channel)
{
    uint8_t oldestPlayerId = 0;
    for (uint8_t i = 0; i < MAX_SOUND; i++)
    {
        if (players[i].playing == false)
        {
            players[i] = SamplerLegacy::SamplePlayer(sample, noteNo, velocity / 127.0f);
            return;
        }
        else
        {
            if (players[i].createdAt < players[oldestPlayerId].createdAt)
                oldestPlayerId = i;
        }
    }
    // 全てのPlayerが再生中だった時には、最も昔に発音されたPlayerを停止する
    players[oldestPlayerId] = SamplerLegacy::SamplePlayer(sample, noteNo, velocity / 127.0f);
}
void SamplerLegacy::NoteOff(uint8_t noteNo, uint8_t velocity, uint8_t channel)
{
    for (uint8_t i = 0; i < MAX_SOUND; i++)
    {
        if (players[i].playing == true && players[i].noteNo == noteNo)
        {
            players[i].released = true;
        }
    }
}

void SamplerLegacy::SetSample(uint8_t channel, Sample *s)
{
    sample = s;
}

__attribute((optimize("-O3")))
void SamplerLegacy::Process(int16_t* __restrict__ output)
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

        if (pitch < 1.0f)
        { // pitchが1未満の場合は線形補間処理を行う。
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
        } else {
            // pitchが1以上の場合は補間処理の効果がそれほど高くないと思われるので処理を省いて高速化する
            // ループの残り回数をremainに保持しておく
            uint32_t remain = SAMPLE_BUFFER_SIZE;
            auto d = data;
            do {
                // loopEndに到達するまで何個サンプル出力できるか求める
                int32_t length = 1 + ((loopEnd - pos) / pitch);
                // ループの残り回数を考慮してlengthを調整
                length = remain < length ? remain : length;
                // ループ残り回数をカウントダウン
                remain -= length;
                if (length & 3)
                { // 後で4個単位でサンプル処理するので、先に端数分のループを処理する
                    int l = length & 3;
                    do {
                        // 補間処理を省略して波形合成
                        d[0] += src[pos] * gain;
                        d += 1;
                        pos_f += pitch;
                        uint32_t intval = pos_f;
                        pos += intval;
                        pos_f -= intval;
                    } while (--l);
                }
                // lengthを右2ビットシフトしてループ回数を 1/4にしておく
                length >>= 2;
                if (length)
                { // 4個単位でサンプル処理を行う
                    do {
                        auto s = &src[pos];
                        float pf1 = pos_f + pitch;
                        float pf2 = pf1 + pitch;
                        float pf3 = pf2 + pitch;
                        float d0 = d[0] + s[0            ] * gain;
                        float d1 = d[1] + s[(uint32_t)pf1] * gain;
                        float d2 = d[2] + s[(uint32_t)pf2] * gain;
                        float d3 = d[3] + s[(uint32_t)pf3] * gain;
                        pos_f = pf3 + pitch;
                        uint32_t intval = (uint32_t)pos_f;
                        pos += intval;
                        pos_f -= intval;
                        d[0] = d0;
                        d[1] = d1;
                        d[2] = d2;
                        d[3] = d3;
                        d += 4;
                    } while (--length);
                }

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
