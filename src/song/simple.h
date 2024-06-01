#pragma once

#include <MidiMessage.h>

typedef struct MidiMessage m;

const MidiMessage song[] = {
m{0,0x90,60,127},
m{0,0x90,64,127},
m{0,0x90,67,127},
m{48000,0x80,60,127},
m{48000,0x80,64,127},
m{48000,0x80,67,127},
m{96000,0xFF,0x2F,0x00}
};