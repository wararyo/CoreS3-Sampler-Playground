#pragma once

#include <stdio.h>

// ADSR更新周期 (サンプル数)
// ※ ADSRの更新周期が短いほど、ADSRの変化が滑らかになりますが、CPU負荷が増加します
#define ADSR_UPDATE_SAMPLE_COUNT 64

// 1回の波形生成処理で出力するサンプル数 (ADSR_UPDATE_SAMPLE_COUNT の倍数であること)
// ※ この数値が大きいほど、波形生成処理の効率が向上しますが、操作してから発音されるまでのレイテンシが増加します
#define SAMPLE_BUFFER_SIZE ( ADSR_UPDATE_SAMPLE_COUNT * 2 )
#define SAMPLE_RATE 48000

#define MAX_SOUND 16 // 最大同時発音数

enum SampleAdsr
{
    attack,
    decay,
    sustain,
    release,
};

struct Sample
{
    const int16_t *sample;
    uint32_t length;
    uint8_t root;
    uint32_t loopStart;
    uint32_t loopEnd;

    bool adsrEnabled;
    float attack;
    float decay;
    float sustain;
    float release;
};

class SamplerBase
{
public:
    virtual void NoteOn(uint8_t noteNo, uint8_t velocity, uint8_t channel)=0;
    virtual void NoteOff(uint8_t noteNo, uint8_t velocity, uint8_t channel)=0;
    virtual void SetSample(uint8_t channel, Sample *sample);

    // SAMPLE_BUFFER_SIZE分の音声処理を進めます
    // outputの配列数はSAMPLE_BUFFER_SIZEと同じかそれ以上である必要があります
    virtual void Process(int16_t *output)=0;
};
