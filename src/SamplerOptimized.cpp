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
void SamplerOptimized::Process(int16_t *output)
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

        // posとpos_fをローカル変数にコピーすることでレジスタのみで処理できる
        auto pos = player->pos;
        auto pos_f = player->pos_f;

        uint32_t pitch_u = pitch; // pitchの整数部分
        float pitch_frac = pitch - pitch_u; // pitchの小数部分

        if (sample->adsrEnabled) // adsrEnabledによる場合分けが多いので、まずadsrEnabledで分ける
        {
            float gain = player->adsrGain * masterVolume;

            for (int n = 0; n < SAMPLE_BUFFER_SIZE; n++)
            {
                // 波形を読み込む
                int32_t current_sample = sample->sample[pos];
                int32_t next_sample = sample->sample[pos + 1];
                // 線形補間
                float diff = next_sample - current_sample;
                float val = (float)current_sample + diff * pos_f;

                // 音量を適用し出力とミックス
                data[n] += val * gain;

                // 次のサンプルへ移動
                pos += pitch_u;
                pos_f = pitch_frac;
                if (pos_f >= 1.0f)
                {
                    pos_f -= 1.0f;
                    pos++;
                }

                // ループポイントが設定されている場合はループする
                if (pos >= sample->loopEnd)
                    pos -= (sample->loopEnd - sample->loopStart);
            }
        }
        else
        {
            // TODO: adsrEnabledでないサンプルを用いる時は実装する
        }
        player->pos = pos;
        player->pos_f = pos_f;
    }

    for (uint8_t i = 0; i < SAMPLE_BUFFER_SIZE; i+=4)
    {
        output[i+0] = data[i+0];
        output[i+1] = data[i+1];
        output[i+2] = data[i+2];
        output[i+3] = data[i+3];
    }
}
