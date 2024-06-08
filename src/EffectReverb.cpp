#include <EffectReverb.h>
#include <Arduino.h>

void EffectReverb::Init()
{
    size_t size = REVERB_DELAY_BASIS_COMB_0 +
                  REVERB_DELAY_BASIS_COMB_1 +
                  REVERB_DELAY_BASIS_COMB_2 +
                  REVERB_DELAY_BASIS_COMB_3 +
                  REVERB_DELAY_BASIS_ALL_0 +
                  REVERB_DELAY_BASIS_ALL_1 +
                  REVERB_DELAY_BASIS_ALL_2 + 
                  (bufferSize * 4);
    memory = (float *)heap_caps_calloc(1, size, MALLOC_CAP_INTERNAL);

    uint32_t offset = 0;
    // TODO: timeを考慮する
    combFilters[0] = CombFilter(memory + offset, 0.805f, REVERB_DELAY_BASIS_COMB_0);
    offset += REVERB_DELAY_BASIS_COMB_0;
    combFilters[1] = CombFilter(memory + offset, 0.827f, REVERB_DELAY_BASIS_COMB_1);
    offset += REVERB_DELAY_BASIS_COMB_1;
    combFilters[2] = CombFilter(memory + offset, 0.783f, REVERB_DELAY_BASIS_COMB_2);
    offset += REVERB_DELAY_BASIS_COMB_2;
    combFilters[3] = CombFilter(memory + offset, 0.764f, REVERB_DELAY_BASIS_COMB_3);
    offset += REVERB_DELAY_BASIS_COMB_3;
    for (uint_fast8_t i; i < 4; i++)
    {
        combFilterSwaps[i] = memory + offset;
        offset += bufferSize;
    }
}

__attribute((optimize("-O3")))
void EffectReverb::CombFilter::Process(const float *input, float* __restrict__ output, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        const float readback = buffer[cursor];
        const float newValue = readback * g + input[i];
        buffer[cursor] = newValue;
        if (cursor < delaySamples) cursor++;
        else cursor = 0;
        output[i] = readback;
    }
}

__attribute((optimize("-O3")))
void EffectReverb::Process(const float *input, float* __restrict__ output)
{
    // コムフィルターのテスト
    combFilters[0].Process(input, output, bufferSize);
}