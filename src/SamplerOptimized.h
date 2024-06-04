#pragma once

#include <SamplerBase.h>
#include <Arduino.h>
#include <list>

class SamplerOptimized : public SamplerBase
{
public:
    // 与えられたサンプルを再生する
    class SamplePlayer
    {
    public:
        SamplePlayer(struct Sample *sample, uint8_t noteNo, float volume)
            : sample{sample}, noteNo{noteNo}, volume{volume}, createdAt{micros()}
        {
            UpdatePitch();
            gain = volume;
        }
        SamplePlayer() : sample{nullptr}, noteNo{60}, volume{1.0f}, playing{false}, createdAt{micros()} {}
        struct Sample *sample;
        uint8_t noteNo;
        float pitchBend = 0;
        float volume;
        unsigned long createdAt = 0;
        bool released = false;

        bool playing = true;
        uint32_t pos = 0;
        float pos_f = 0.0f;
        float gain = 0.0f; // volumeとADSR処理により算出される値
        float pitch = 1.0f; // noteNoとpitchBendにより算出される値
        enum SampleAdsr adsrState = SampleAdsr::attack;

        void UpdateGain();
        void UpdatePitch();
    private:
    };

    // MIDI規格のチャンネルに対応する概念
    class Channel
    {
    public:
        Channel() {}
        Channel(SamplerOptimized *sampler) : sampler{sampler} {}
        void NoteOn(uint8_t noteNo, uint8_t velocity);
        void NoteOff(uint8_t noteNo, uint8_t velocity);
        void SetTimbre(Timbre *timbre);

    private:
        SamplerOptimized *sampler; // 親サンプラー
        struct PlayingNote
        {
            uint8_t noteNo;
            uint_fast8_t playerId;
        };
        Timbre *timbre;
        std::list<PlayingNote> playingNotes; // このチャンネルで現在再生しているノート
    };

    SamplerOptimized()
    {
        for (uint_fast8_t i = 0; i < CH_COUNT; i++)
            channels[i] = Channel(this);
    }
    void NoteOn(uint8_t noteNo, uint8_t velocity, uint8_t channel);
    void NoteOff(uint8_t noteNo, uint8_t velocity, uint8_t channel);
    void SetTimbre(uint8_t channel, Timbre *timbre);

    void Process(int16_t *output);

    float masterVolume = 0.25f;

private:
    Channel channels[CH_COUNT]; // コンストラクタで初期化する
    SamplePlayer players[MAX_SOUND] = {SamplePlayer()};
};
