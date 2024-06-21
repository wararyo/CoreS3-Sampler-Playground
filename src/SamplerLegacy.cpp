#include <SamplerLegacy.h>
#include <tables.h>

void SamplerLegacy::SetTimbre(uint8_t channel, Timbre *t)
{
    if(channel < CH_COUNT) channels[channel].SetTimbre(t);
}
void SamplerLegacy::Channel::SetTimbre(Timbre *t)
{
    timbre = t;
}

void SamplerLegacy::NoteOn(uint8_t noteNo, uint8_t velocity, uint8_t channel)
{
    if (channel >= CH_COUNT) channel = 0; // 無効なチャンネルの場合は1CHにフォールバック
    velocity &= 0b01111111; // velocityを0-127の範囲に収める
    messageQueue.push_back(Message{MessageStatus::NOTE_ON, channel, noteNo, velocity, 0});
}
void SamplerLegacy::NoteOff(uint8_t noteNo, uint8_t velocity, uint8_t channel)
{
    if (channel >= CH_COUNT) channel = 0; // 無効なチャンネルの場合は1CHにフォールバック
    velocity &= 0b01111111; // velocityを0-127の範囲に収める
    messageQueue.push_back(Message{MessageStatus::NOTE_OFF, channel, noteNo, velocity, 0});
}
void SamplerLegacy::PitchBend(int16_t pitchBend, uint8_t channel)
{
    if (channel >= CH_COUNT) return; // 無効なチャンネルの場合は何もしない
    if (pitchBend < -8192) pitchBend = -8192;
    else if (pitchBend > 8191) pitchBend = 8191;
    messageQueue.push_back(Message{MessageStatus::PITCH_BEND, channel, 0, 0, pitchBend});
}

float SamplerLegacy::PitchFromNoteNo(float noteNo, float root, float pitchBend)
{
    float delta = noteNo - root + pitchBend;
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
        if ((player->adsrGain - goal) < 0.01f)
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

void SamplerLegacy::Channel::NoteOn(uint8_t noteNo, uint8_t velocity)
{
    M5.Log.printf("Channel::NoteOn %d %d\n", noteNo, velocity);
    // 空いているPlayerを探し、そのPlayerにサンプルをセットする
    uint_fast8_t oldestPlayerId = 0;
    for (uint_fast8_t i = 0; i < MAX_SOUND; i++)
    {
        if (sampler->players[i].playing == false)
        {
            sampler->players[i] = SamplerLegacy::SamplePlayer(timbre->GetAppropriateSample(noteNo, velocity), noteNo, velocityTable[velocity], pitchBend);
            playingNotes.push_back(PlayingNote{noteNo, i});
            return;
        }
        else
        {
            if (sampler->players[i].createdAt < sampler->players[oldestPlayerId].createdAt)
                oldestPlayerId = i;
        }
    }
    // 全てのPlayerが再生中だった時には、最も昔に発音されたPlayerを停止する
    sampler->players[oldestPlayerId] = SamplerLegacy::SamplePlayer(timbre->GetAppropriateSample(noteNo, velocity), noteNo, velocityTable[velocity], pitchBend);
    playingNotes.push_back(PlayingNote{noteNo, oldestPlayerId});
}
void SamplerLegacy::Channel::NoteOff(uint8_t noteNo, uint8_t velocity)
{
    // 現在このチャンネルで発音しているノートの中で該当するnoteNoのものの発音を終わらせる
    for (auto itr = playingNotes.begin(); itr != playingNotes.end(); itr++)
    {
        if (itr->noteNo == noteNo)
        {
            SamplePlayer *player = &(sampler->players[itr->playerId]);
            // 発音後に同時発音数制限によって発音が止められていなければ、発音を終わらせる
            // TODO: 本当はユニークID的なものを設けるべきだが、
            //       とりあえずnoteNoが合ってれば高確率で該当の発音でしょうという判断をしています
            if (player->noteNo == noteNo)
                player->released = true;
            playingNotes.erase(itr);
        }
    }
}
void SamplerLegacy::Channel::PitchBend(int16_t b)
{
    pitchBend = b * 12.0f / 8192.0f;
    // 既に発音中のノートに対してピッチベンドを適用する
    for (auto itr = playingNotes.begin(); itr != playingNotes.end(); itr++)
    {
        // TODO: 同時発音数制限によって発音が止められて別の音が流れている場合は動作がおかしくなるので修正すべき
        SamplePlayer *player = &(sampler->players[itr->playerId]);
        player->pitchBend = pitchBend;
    }
}

void SamplerLegacy::Process(int16_t *output)
{
    // キューを処理する
    while (!messageQueue.empty())
    {
        Message message = messageQueue.front();
        switch (message.status)
        {
        case MessageStatus::NOTE_ON:
            channels[message.channel].NoteOn(message.noteNo, message.velocity);
            break;
        case MessageStatus::NOTE_OFF:
            channels[message.channel].NoteOff(message.noteNo, message.velocity);
            break;
        case MessageStatus::PITCH_BEND:
            channels[message.channel].PitchBend(message.pitchBend);
            break;
        }
        messageQueue.pop_front();
    }

    // 波形を生成
    float data[SAMPLE_BUFFER_SIZE] = {0.0f};
    for (uint8_t i = 0; i < MAX_SOUND; i++)
    {
        SamplePlayer *player = &players[i];
        if (player->playing == false)
            continue;
        Sample *sample = player->sample;
        if (player->playing == false)
            continue;

        float pitch = PitchFromNoteNo(player->noteNo, player->sample->root, player->pitchBend);

        if (sample->adsrEnabled) // adsrEnabledによる場合分けが多いので、まずadsrEnabledで分ける
        {
            uint_fast8_t length = SAMPLE_BUFFER_SIZE / ADSR_UPDATE_SAMPLE_COUNT;
            for (int j = 0; j < length; j++)
            {
                UpdateAdsr(player);
                for (int n = j * ADSR_UPDATE_SAMPLE_COUNT; n < (j + 1) * ADSR_UPDATE_SAMPLE_COUNT; n++)
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
        }
        else
        {
            uint_fast8_t length = SAMPLE_BUFFER_SIZE / ADSR_UPDATE_SAMPLE_COUNT;
            for (int j = 0; j < length; j++)
            {
                for (int n = j * ADSR_UPDATE_SAMPLE_COUNT; n < (j + 1) * ADSR_UPDATE_SAMPLE_COUNT; n++)
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
    }

    Reverb_Process(data, SAMPLE_BUFFER_SIZE);

    for (uint8_t i = 0; i < SAMPLE_BUFFER_SIZE; i++)
    {
        output[i] = int16_t(data[i] * masterVolume);
    }
}
