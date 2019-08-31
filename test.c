#include <string.h>

/*#define SMR_VERBOSE_DEBUGGING*/
#include "simple_midi_read.h"

int main()
{
    struct smr_event event;
    struct smr_midi_data result;

    event.delta_time = 127;
    event.event_type = Midi_NoteOff;

    event.length = 0x08040201;
    event.message = (uint8_t*) 0x8040201008040201;

    printf("length: %d\n", event.length);

    printf("note: 0x%.2x\n", event.note & 0xFF);
    printf("controller: 0x%.2x\n", event.controller & 0xFF);
    printf("pp: 0x%.2x\n", event.pp & 0xFF);
    printf("hr: 0x%.2x\n", event.hr & 0xFF);
    printf("nn: 0x%.2x\n", event.nn & 0xFF);
    printf("sf: 0x%.2x\n", event.sf & 0xFF);
    printf("velocity: 0x%.2x\n", event.velocity & 0xFF);
    printf("pressure: 0x%.2x\n", event.pressure & 0xFF);
    printf("value: 0x%.2x\n", event.value & 0xFF);
    printf("mn: 0x%.2x\n", event.mn & 0xFF);
    printf("dd: 0x%.2x\n", event.dd & 0xFF);
    printf("mi: 0x%.2x\n", event.mi & 0xFF);
    printf("se: 0x%.2x\n", event.se & 0xFF);
    printf("cc: 0x%.2x\n", event.cc & 0xFF);
    printf("fr: 0x%.2x\n", event.fr & 0xFF);
    printf("bb: 0x%.2x\n", event.bb & 0xFF);
    printf("pitch_bend: 0x%.4x\n", event.pitch_bend & 0xFFFF);
    printf("ss_ss: 0x%.4x\n", event.ss_ss & 0xFFFF);
    printf("message: 0x%p\n", (void*)event.message);
    printf("bytes: 0x%p\n", (void*)event.bytes);
    printf("text: 0x%p\n", (void*)event.text);
    printf("ff: 0x%.2x\n", event.ff & 0xFF);

    return smr_read_file("c_scale.mid", &result);
}
