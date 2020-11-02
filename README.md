# Implementation
simple_midi_read is a single header library in the style of [stb](https://github.com/nothings/stb). To add to your code, put simple_midi_read.h somewhere in your codebase, and include it anywhere you need to use it. In exactly *one* of the files that you include it in, add

    #define SMR_IMPLEMENTATION
before the include. So your code in that file should look like:

    #include ...
    #include ...
    #define SMR_IMPLEMENTATION
    #include "simple_midi_read.h"
The header file will then build along with the rest of your code.
# Usage
To read a MIDI file:

    struct smr_midi_data midi_data;
    int result;
    result = smr_read_file("path/to/midi_file.mid", &midi_data);
    if (result != 0)
    {
        print("Error!");
    }
**Important!** The library does not manage the memory of `smr_midi_data`, so to free that memory, you must call:

    smr_free_midi_data(&midi_data);
An `smr_midi_data` struct is laid out much like a MIDI file itself, minus the byte packing and variable length types (except strings). [Here's a good resource](http://www.somascape.org/midi/tech/mfile.html) to familiarize yourself with the MIDI file spec. (Take note especially of the **Delta times** section, which explains how time works in a MIDI file.)

The `smr_midi_data` struct contains an array of tracks (`smr_midi_data.tracks`, of length `smr_midi_data.ntracks`), and each track contains an array of events (`smr_track_data.events`, of length `smr_track_data.nevents`). Every event in the array is an `smr_event`, but you can get the type of event using the enum `smr_event.event_type`. From there, `smr_event` uses unions to give you access to any relevant variable to that type of event. For instance, if the event is type `SMRE_midi_note_on`, you can get the event's `smr_event.note` and `smr_event.velocity`; if the event is type `SMRE_midi_controller`, you can get the event's `smr_event.controller` and `smr_event.value`. More commenting of these events in the code is needed, but for now you can refer to the [MIDI file spec](http://www.somascape.org/midi/tech/mfile.html), as well as [test.c](https://github.com/jasonericson/simple_midi_read/blob/master/test.c), to get an understanding of how these different events work.
# Miscellaneous
 - This library requires C11 or later to compile, to take advantage of anonymous structs and unions (which is critical to how I've structured `smr_event` and `smr_midi_data`). Without that, you would also need C99 for the fixed-size types (`uint32_t`, etc.). If you require an older version of C, and/or have ideas on how to better structure those aspects of the code, I'm open to hearing it.
 - There are currently two allocations that happen when loading a file - one to load the raw file data into memory, and one to create the block of memory for storing the `smr_midi_data` track array, event array, and strings (`smr_midi_data._mem_block`). It's on my to-do list to offer the user a way to define their own `malloc` replacement.
 - If you would rather read from a memory block instead of a file, you can call `smr_read_byte_array` directly. This will skip the allocation in `smr_read_file` and bring your total number of allocations to 1.