#include <string.h>

/*#define SMR_VERBOSE_DEBUGGING*/
#include "simple_midi_read.h"

int main()
{
    struct smr_midi_data result;

    return smr_read_file("c_scale.mid", &result);
}
