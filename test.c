#include <string.h>

/*#define SMR_VERBOSE_DEBUGGING*/
#include "simple_midi_read.h"

int main()
{
    struct smr_event event;

    event.delta_time = 127;
    event.event_type = Midi_NoteOff;

    event.length = 0x08040201;

    printf("length: %d\n", event.length);

    printf("note: %.2x\n", event.note & 0xFF);
    printf("controller: %.2x\n", event.controller & 0xFF);
    printf("velocity: %.2x\n", event.velocity & 0xFF);
    printf("pressure: %.2x\n", event.pressure & 0xFF);
    printf("value: %.2x\n", event.value & 0xFF);

    /*struct smr_midi_data result;

    return smr_read_file("c_scale.mid", &result);*/
}
