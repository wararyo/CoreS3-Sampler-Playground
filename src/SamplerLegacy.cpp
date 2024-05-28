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

void SamplerLegacy::Process(int16_t *output)
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

        if (sample->adsrEnabled) // adsrEnabledによる場合分けが多いので、まずadsrEnabledで分ける
        {
            for (int n = 0; n < SAMPLE_BUFFER_SIZE; n++)
            {
                // 波形を読み込む&線形補完
                float val = (sample->sample[player->pos + 1] * player->pos_f) + (sample->sample[player->pos] * (1.0f - player->pos_f));
                val *= player->adsrGain;
                data[n] += val;

                // 次のサンプルへ移動
                int32_t pitch_u = pitch;
                player->pos_f += pitch - pitch_u;
                player->pos += pitch_u;
                if (player->pos_f >= 1.0f)
                {
                    player->pos++;
                    player->pos_f--;
                }

                // ループポイントが設定されている場合はループする
                while (player->pos >= sample->loopEnd)
                    player->pos -= (sample->loopEnd - sample->loopStart);
            }
        }
        else
        {
            for (int n = 0; n < SAMPLE_BUFFER_SIZE; n++)
            {
                if (player->pos >= sample->length)
                {
                    player->playing = false;
                    break;
                }
                // 波形を読み込む
                float val = sample->sample[player->pos];
                val *= player->volume;
                data[n] += val;

                // 次のサンプルへ移動
                int32_t pitch_u = pitch;
                player->pos_f += pitch - pitch_u;
                player->pos += pitch_u;
                int posI = player->pos_f;
                player->pos += posI;
                player->pos_f -= posI;
            }
        }
    }

    for (uint8_t i = 0; i < SAMPLE_BUFFER_SIZE; i++)
    {
        output[i] = int16_t(data[i] * masterVolume);
    }
}
