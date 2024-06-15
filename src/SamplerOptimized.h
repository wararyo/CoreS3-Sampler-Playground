#pragma once

#include <list>
#include <deque>
#include <M5Unified.h>
#include <SamplerBase.h>
#include <EffectReverb.h>

class SamplerOptimized : public SamplerBase
{
public:
    // 与えられたサンプルを再生する
    class SamplePlayer
    {
    public:
        SamplePlayer(struct Sample *sample, uint8_t noteNo, float volume, float pitchBend)
            : sample{sample}, noteNo{noteNo}, volume{volume}, pitchBend{pitchBend}, createdAt{M5.micros()}
        {
            UpdatePitch();
            gain = volume;
        }
        SamplePlayer() : sample{nullptr}, noteNo{60}, volume{1.0f}, playing{false}, createdAt{M5.micros()} {}
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
        void PitchBend(int16_t pitchBend);
        void SetTimbre(Timbre *timbre);

    private:
        SamplerOptimized *sampler; // 親サンプラー
        struct PlayingNote
        {
            uint8_t noteNo;
            uint_fast8_t playerId;
        };
        Timbre *timbre;
        float pitchBend = 0.0f;
        std::list<PlayingNote> playingNotes; // このチャンネルで現在再生しているノート
    };

    SamplerOptimized()
    {
        for (uint_fast8_t i = 0; i < CH_COUNT; i++)
            channels[i] = Channel(this);
    }
    void NoteOn(uint8_t noteNo, uint8_t velocity, uint8_t channel);
    void NoteOff(uint8_t noteNo, uint8_t velocity, uint8_t channel);
    void PitchBend(int16_t pitchBend, uint8_t channel);
    void SetTimbre(uint8_t channel, Timbre *timbre);

    void Process(int16_t *output);

    float masterVolume = 0.4f;

private:
    // 各メッセージのキューイングに使用する
    // MIDIのメッセージとは互換性がない
    // TODO: noteNo/velocityとpitchBendは同時に使われることがないのに両方メモリを占有しているのどうにかならないか…？
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
    // 受け取ったNoteOn/NoteOff/PitchBendなどは一旦キューに入れておき、Processのタイミングで処理する
    // これにより、Processを別スレッドで動かすことができる
    // TODO: messageQueue自体の排他制御は必要ない？
    std::deque<Message> messageQueue;

    EffectReverb reverb = EffectReverb(0.4f, 0.5f, SAMPLE_BUFFER_SIZE, SAMPLE_RATE);
};
