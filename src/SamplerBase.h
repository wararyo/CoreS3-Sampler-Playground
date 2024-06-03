#pragma once

#include <stdio.h>
#include <vector>

// ADSR更新周期 (サンプル数)
// ※ ADSRの更新周期が短いほど、ADSRの変化が滑らかになりますが、CPU負荷が増加します
#define ADSR_UPDATE_SAMPLE_COUNT 64

// 1回の波形生成処理で出力するサンプル数 (ADSR_UPDATE_SAMPLE_COUNT の倍数であること)
// ※ この数値が大きいほど、波形生成処理の効率が向上しますが、操作してから発音されるまでのレイテンシが増加します
#define SAMPLE_BUFFER_SIZE ( ADSR_UPDATE_SAMPLE_COUNT * 2 )
#define SAMPLE_RATE 48000

#define MAX_SOUND 16 // 最大同時発音数
#define CH_COUNT 16 // サンプラーはMIDIと同様に16個のチャンネルを持つ

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

// MIDI規格のプログラムに対応する概念
// いわゆる音色(おんしょく)
// サンプルの集合からなり、ノートナンバーとベロシティを指定するとサンプルがただ一つ定まる
// 横軸がノートナンバー、縦軸がベロシティの平面に長方形をプロットしていくイメージ
class Timbre
{
public:
    struct MappedSample
    {
        Sample *sample;
        uint8_t lowerNoteNo;   // このサンプルが選ばれるノートナンバーの下限(自身を含む)
        uint8_t upperNoteNo;   // このサンプルが選ばれるノートナンバーの上限(自身を含む)
        uint8_t lowerVelocity; // このサンプルが選ばれるベロシティの下限(自身を含む)
        uint8_t upperVelocity; // このサンプルが選ばれるベロシティの上限(自身を含む)
    };
    Timbre(std::vector<MappedSample> samples) : samples{samples} {}
    // 指定したノートナンバーとベロシティが範囲に含まれているサンプルを返す
    // 該当するサンプルがない場合はnullptrを返す
    Sample *GetAppropriateSample(uint8_t noteNo, uint8_t velocity);
private:
    // サンプルの集合
    // 下記の制約をすべて満たしているものとします
    // * 任意の2つを取り出したとき、それらのノートナンバーの範囲が完全に一致しているか、全く重複していないかのどちらかである
    // * 同じlowerNoteNoを持つ任意の2つを取り出したとき、それらのベロシティの範囲が重複していない
    // * lowerNoteNoの低い順に並んでおり、同じlowerNoteNoを持つ項目はlowerVelocityの低い順に並んでいる
    std::vector<MappedSample> samples;
};

class SamplerBase
{
public:
    virtual void NoteOn(uint8_t noteNo, uint8_t velocity, uint8_t channel)=0;
    virtual void NoteOff(uint8_t noteNo, uint8_t velocity, uint8_t channel)=0;
    virtual void SetTimbre(uint8_t channel, Timbre *timbre);

    // SAMPLE_BUFFER_SIZE分の音声処理を進めます
    // outputの配列数はSAMPLE_BUFFER_SIZEと同じかそれ以上である必要があります
    virtual void Process(int16_t *output)=0;
};
