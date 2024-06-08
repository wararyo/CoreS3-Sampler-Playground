#pragma once

class EffectBase
{
public:
    uint32_t bufferSize = 128; // 一度に処理するサンプル数 Processに渡すinputのサイズはこれでなければならない
    void Process(const float *input, float *output);
};