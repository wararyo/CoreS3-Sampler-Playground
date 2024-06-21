#pragma once

#include <list>
#include <deque>
#include <SamplerBase.h>
#include <M5Unified.h>
#include <ml_reverb.h>

class SamplerLegacy : public SamplerBase
{
public:
    struct SamplePlayer
    {
        SamplePlayer(struct Sample *sample, uint8_t noteNo, float volume, float pitchBend)
            : sample{sample}, noteNo{noteNo}, volume{volume}, pitchBend{pitchBend}, createdAt{micros()} {}
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
    class Channel
    {
    public:
        Channel() {}
        Channel(SamplerLegacy *sampler) : sampler{sampler} {}
        void NoteOn(uint8_t noteNo, uint8_t velocity);
        void NoteOff(uint8_t noteNo, uint8_t velocity);
        void PitchBend(int16_t pitchBend);
        void SetTimbre(Timbre *timbre);

    private:
        SamplerLegacy *sampler; // 親サンプラー
        struct PlayingNote
        {
            uint8_t noteNo;
            uint_fast8_t playerId;
        };
        Timbre *timbre;
        float pitchBend = 0.0f;
        std::list<PlayingNote> playingNotes; // このチャンネルで現在再生しているノート
    };

    SamplerLegacy()
    {
        for (uint_fast8_t i = 0; i < CH_COUNT; i++)
            channels[i] = Channel(this);

        revBuffer = (float *)heap_caps_aligned_calloc(16, 1, sizeof(float) * REV_BUFF_SIZE, MALLOC_CAP_INTERNAL);
        Reverb_Setup(revBuffer);
        Reverb_SetLevel(0, 0.2f);
    }
    ~SamplerLegacy()
    {
        free(revBuffer);
    }
    void NoteOn(uint8_t noteNo, uint8_t velocity, uint8_t channel);
    void NoteOff(uint8_t noteNo, uint8_t velocity, uint8_t channel);
    void PitchBend(int16_t pitchBend, uint8_t channel);
    void SetTimbre(uint8_t channel, Timbre *timbre);

    void Process(int16_t *output);

    float masterVolume = 0.4f;

private:
    struct Message
    {
        uint8_t status;
        uint8_t channel;
        uint8_t noteNo;
        uint8_t velocity;
        int16_t pitchBend;
    };
    enum MessageStatus
    {
        NOTE_ON,
        NOTE_OFF,
        PITCH_BEND
    };
    Channel channels[CH_COUNT]; // コンストラクタで初期化する
    SamplePlayer players[MAX_SOUND] = {SamplePlayer()};
    std::deque<Message> messageQueue;
    float *revBuffer;
    float PitchFromNoteNo(float noteNo, float root, float pitchBend);
    void UpdateAdsr(SamplePlayer *player);
};
