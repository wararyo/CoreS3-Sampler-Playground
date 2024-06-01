#pragma once

#include <SamplerBase.h>
#include <Arduino.h>
#include <list>

class SamplerOptimized : public SamplerBase
{
public:
    struct SamplePlayer
    {
        SamplePlayer(struct Sample *sample, uint8_t noteNo, float volume)
            : sample{sample}, noteNo{noteNo}, volume{volume}, createdAt{micros()}
            {
                float delta = noteNo - sample->root;
                pitch = ((pow(2.0f, delta / 12.0f)));
            }
        SamplePlayer() : sample{nullptr}, noteNo{60}, volume{1.0f}, playing{false}, createdAt{micros()} {}
        struct Sample *sample;
        uint8_t noteNo;
        float pitchBend = 0;
        float volume;
        unsigned long createdAt = 0;
        uint32_t pos = 0;
        float pos_f = 0.0f;
        bool playing = true;
        bool released = false;
        float adsrGain = 0.0f;
        float pitch = 1.0f;
        enum SampleAdsr adsrState = SampleAdsr::attack;
    };
    class Channel
    {
    public:
        Channel() {}
        Channel(SamplerOptimized *sampler) : sampler{sampler} {}
        void NoteOn(uint8_t noteNo, uint8_t velocity);
        void NoteOff(uint8_t noteNo, uint8_t velocity);
        void SetSample(Sample *sample);

    private:
        SamplerOptimized *sampler;
        struct PlayingNote
        {
            uint8_t noteNo;
            uint_fast8_t playerId;
        };
        struct Sample *sample;               // TODO: 複数のサンプルをセットする
        std::list<PlayingNote> playingNotes; // このチャンネルで現在再生しているノート
    };

    SamplerOptimized()
    {
        for (uint_fast8_t i = 0; i < CH_COUNT; i++)
            channels[i] = Channel(this);
    }
    void NoteOn(uint8_t noteNo, uint8_t velocity, uint8_t channel);
    void NoteOff(uint8_t noteNo, uint8_t velocity, uint8_t channel);
    void SetSample(uint8_t channel, Sample *sample);

    void Process(int16_t *output);

    float masterVolume = 0.25f;

private:
    Channel channels[CH_COUNT]; // コンストラクタで初期化する
    SamplePlayer players[MAX_SOUND] = {SamplePlayer()};

    float PitchFromNoteNo(float noteNo, float root);
    void UpdateAdsr(SamplePlayer *player);
};
