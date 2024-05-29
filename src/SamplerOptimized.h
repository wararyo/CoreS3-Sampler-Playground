#pragma once

#include <SamplerBase.h>
#include <Arduino.h>

class SamplerOptimized : public SamplerBase
{
public:
    struct SamplePlayer
    {
        SamplePlayer(struct Sample *sample, uint8_t noteNo, float volume)
            : sample{sample}, noteNo{noteNo}, volume{volume}, createdAt{micros()} {}
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
        enum SampleAdsr adsrState = SampleAdsr::attack;
    };
    void NoteOn(uint8_t noteNo, uint8_t velocity, uint8_t channel);
    void NoteOff(uint8_t noteNo, uint8_t velocity, uint8_t channel);
    void SetSample(uint8_t channel, Sample *sample);

    void Process(int16_t *output);

    float masterVolume = 0.25f;

private:
    struct Sample *sample;

    SamplePlayer players[MAX_SOUND] = {SamplePlayer()};
    float PitchFromNoteNo(float noteNo, float root);
    void UpdateAdsr(SamplePlayer *player);
};
