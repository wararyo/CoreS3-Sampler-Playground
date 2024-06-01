#pragma once

struct MidiMessage
{
    uint32_t time;
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
};
