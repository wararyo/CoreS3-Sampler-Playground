#pragma once

#include <stdint.h>

namespace capsule
{
namespace sampler
{
class EffectBase
{
public:
    uint32_t bufferSize = 128; // 一度に処理するサンプル数 Processに渡すinputのサイズはこれでなければならない
    virtual void Process(const float *input, float *output);
};

}
}
