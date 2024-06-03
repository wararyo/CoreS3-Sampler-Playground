#include <SamplerBase.h>

Sample *Timbre::GetAppropriateSample(uint8_t noteNo, uint8_t velocity)
{
    for (MappedSample ms : samples)
    {
        if (ms.lowerNoteNo <= noteNo && noteNo <= ms.upperNoteNo && ms.lowerVelocity <= velocity && velocity <= ms.upperVelocity)
            return ms.sample;
    }
    return nullptr;
}
