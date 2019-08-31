#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "profiler_macos.h"

static int32_t compare_next_string(uint8_t** buffer_read, const char* to_compare)
{
    int32_t result;
    int32_t string_length;

    string_length = strlen(to_compare);
    result = strncmp((char*)*buffer_read, to_compare, string_length);
    *buffer_read += string_length;

    return result;
}

static uint8_t get_next_byte(uint8_t** buffer_read)
{
    uint8_t result;

    result = (*buffer_read)[0];
    *buffer_read += 1;

    return result;
}

/* TODO: Check for endian-ness */
static uint32_t get_next_int(uint8_t** buffer_read)
{
    uint32_t result;

    result = (*buffer_read)[3] | ((*buffer_read)[2] << 8) | ((*buffer_read)[1] << 16) | ((*buffer_read)[0] << 24);
    *buffer_read += 4;

    return result;
}

static uint32_t get_next_int24(uint8_t** buffer_read)
{
    uint32_t result;

    result = 0;
    result = (*buffer_read)[2] | ((*buffer_read)[1] << 8) | ((*buffer_read)[0] << 16);
    *buffer_read += 3;

    return result;
}

/* TODO: Figure out fixed-width implementation if possible. */
static uint16_t get_next_short(uint8_t** buffer_read)
{
    uint16_t result;

    result = (*buffer_read)[1] | (*buffer_read)[0] << 8;
    *buffer_read += 2;

    return result;
}

static uint32_t get_next_variable_length_int(uint8_t** buffer_read)
{
    uint32_t result;

    result = 0;
    do
    {
        result = (result << 7) | (uint32_t)(**buffer_read & 0x7F);
    } while (*(*buffer_read)++ & 0x80);

    return result;
}

enum smr_time_type
{
    SMR_TT_metrical,
    SMR_TT_timecode
};

enum smr_event_type
{
    Midi_NoteOff = 0x80,
    Midi_NoteOn = 0x90,
    Midi_PolyphonicPressure = 0xA0,
    Midi_Controller = 0xB0,
    Midi_ProgramChange = 0xC0,
    Midi_ChannelPressure = 0xD0,
    Midi_PitchBend = 0xE0,

    SysEx_Single = 0xF0,
    SysEx_Escape = 0xF7,

    Meta_SequenceNumber = 0xFF00,
    Meta_Text = 0xFF01,
    Meta_Copyright = 0xFF02,
    Meta_TrackName = 0xFF03,
    Meta_InstrumentName = 0xFF04,
    Meta_Lyric = 0xFF05,
    Meta_Marker = 0xFF06,
    Meta_CuePoint = 0xFF07,
    Meta_ProgramName = 0xFF08,
    Meta_DeviceName = 0xFF09,
    Meta_MidiChannelPrefix = 0xFF20,
    Meta_MidiPort = 0xFF21,
    Meta_EndOfTrack = 0xFF2F,
    Meta_Tempo = 0xFF51,
    Meta_SmpteOffset = 0xFF54,
    Meta_TimeSignature = 0xFF58,
    Meta_KeySignature = 0xFF59,
    Meta_SequencerSpecificEvent = 0xFF7F
};

struct smr_event
{
    uint32_t delta_time;
    enum smr_event_type event_type;

    struct
    {
        union
        {
            uint32_t length;

            struct
            {
                union
                {
                    uint8_t note;
                    uint8_t controller;
                    uint8_t program;
                    uint8_t pp;
                    uint8_t hr;
                    uint8_t nn;
                    uint8_t sf;
                };

                union
                {
                    uint8_t velocity;
                    uint8_t pressure;
                    uint8_t value;
                    uint8_t mn;
                    uint8_t dd;
                    uint8_t mi;
                };

                union
                {
                    uint8_t channel;
                    uint8_t se;
                    uint8_t cc;
                };

                union
                {
                    uint8_t fr;
                    uint8_t bb;
                };
            };

            uint16_t pitch_bend;
            uint16_t ss_ss;
        };

        union
        {
            uint8_t* message;
            uint8_t* bytes;
            uint8_t* data;
            char* text;

            uint32_t tempo;

            uint8_t ff;
        };
    };
};

struct smr_track_data
{
    uint32_t nevents;
    struct smr_event* events;
};

struct smr_midi_data
{
    uint16_t format;
    uint16_t ntracks;
    enum smr_time_type time_type;
    union
    {
        uint16_t tickdiv;
        struct
        {
            uint8_t fps;
            uint8_t subframe_resolution;
        };
    };
    struct smr_track_data* tracks;
    uint8_t* _mem_block;
};

int smr_read_byte_array(uint8_t* buffer, struct smr_midi_data* file_data)
{
    uint8_t* buffer_read;
    uint32_t header_chunklen;
    int32_t i;
    uint64_t total_alloc_size;
    uint8_t* mem_ptr;
    struct smr_event* event_ptr;
    uint8_t* all_tracks_start;
    uint32_t total_num_events;

    buffer_read = buffer;

    /* Check for header identifier */
    if (compare_next_string(&buffer_read, "MThd") != 0)
    {
        /* TODO: Separate out verbose logging? */
        printf("MIDI file is missing header identifier 'MThd'.\n");
        return 1;
    }

    /* Check for header chunklen */
    /* TODO: Standardize when buffer pointer advances. */
    header_chunklen = get_next_int(&buffer_read);
    if (header_chunklen != 6)
    {
        printf("This MIDI file is not compatible with this version of the parser.\n");
        /* VERBOSE */
        printf("Expected header chunk size of 6 bytes, got %d.\n", header_chunklen);
        return 1;
    }

    file_data->format = get_next_short(&buffer_read);
    if (file_data->format > 2)
    {
        printf("Do not recognize MIDI format %hu.\n", file_data->format);
        /*return 1;*/
    }

    file_data->ntracks = get_next_short(&buffer_read);

    if (buffer_read[0] & (1<<15))
    {
        /* TODO: Test timecode */
        file_data->time_type = SMR_TT_timecode;
        /* FPS comes in as a negative value for some reason, this flips it to positive. */
        file_data->fps = 0 - get_next_byte(&buffer_read);
        file_data->subframe_resolution = get_next_byte(&buffer_read);
    }
    else
    {
        file_data->time_type = SMR_TT_metrical;
        file_data->tickdiv = get_next_short(&buffer_read);
    }

    total_alloc_size = file_data->ntracks * sizeof(struct smr_track_data);
    all_tracks_start = buffer_read;
    total_num_events = 0;

    printf("First pass\n");
    START_TIMER()
    /* Add all tracks. */
    for (i = 0; i < file_data->ntracks; ++i)
    {
        uint32_t track_chunklen;
        uint8_t* track_start;
        uint32_t track_num_events;
        uint8_t last_status_byte;

        if (compare_next_string(&buffer_read, "MTrk") != 0)
        {
            printf("Did not find an expected track header.\n");
            return 1;
        }

        track_chunklen = get_next_int(&buffer_read);
        track_start = buffer_read;

        track_num_events = 0;
        last_status_byte = 0xFF;
        /* TODO: Maybe ignore track length and just look for End of Track event? */
        while (buffer_read - track_start < track_chunklen)
        {
            uint32_t delta_time;
            uint8_t status_byte, status_byte_top;
            enum smr_event_type event_type;
            uint32_t event_chunklen;

            delta_time = get_next_variable_length_int(&buffer_read);
            status_byte = get_next_byte(&buffer_read);

            /* Check for running status. */
            if (status_byte < 0x80)
            {
                if (last_status_byte >= 0xF0)
                {
                    printf("Currently not supporting running status for non-MIDI events.");
                    return 1;
                }

                status_byte = last_status_byte;
                /* Back up buffer so that value can be read again. */
                buffer_read -= 1;
            }

            last_status_byte = status_byte;

            status_byte_top = status_byte & 0xF0;
            if (status_byte_top >= 0x80 & status_byte_top < 0xF0)
            {
                /* MIDI event */
                event_type = status_byte_top;
                /* Bottom nibble = channel */

                switch (event_type)
                {
                    case Midi_NoteOff:
                    case Midi_NoteOn:
                    case Midi_PolyphonicPressure:
                    case Midi_Controller:
                    case Midi_PitchBend:
                        event_chunklen = 2;
                        break;

                    case Midi_ProgramChange:
                    case Midi_ChannelPressure:
                        event_chunklen = 1;
                        break;
                    default:
                        /* Can't happen. */
                        break;
                }
            }
            else if (status_byte == 0xF0 || status_byte == 0xF7)
            {
                /* SysEx event */
                event_type = status_byte;
                event_chunklen = get_next_variable_length_int(&buffer_read);

                total_alloc_size += event_chunklen;
            }
            else if (status_byte == 0xFF)
            {
                uint8_t meta_event_type;

                meta_event_type = get_next_byte(&buffer_read);
                event_type = meta_event_type | (status_byte << 8);
                event_chunklen = get_next_variable_length_int(&buffer_read);

                switch (event_type)
                {
                    case Meta_Text:
                    case Meta_Copyright:
                    case Meta_TrackName:
                    case Meta_InstrumentName:
                    case Meta_Lyric:
                    case Meta_Marker:
                    case Meta_CuePoint:
                    case Meta_ProgramName:
                    case Meta_DeviceName:
                        /* Allocating 1 extra byte for text, to add null terminator. */
                        total_alloc_size += event_chunklen + 1;
                        break;
                    case Meta_SequencerSpecificEvent:
                        total_alloc_size += event_chunklen;
                        break;
                    default:
                        /* All other meta events don't need extra allocation space. */
                        break;
                }
            }
            else
            {
                printf("\nDo not recognize status byte %0.2x.\n", status_byte & 0xFF);
                return 1;
            }

            buffer_read += event_chunklen;
            track_num_events += 1;
        }

        total_num_events += track_num_events;
    }

    END_TIMER()

    total_alloc_size += total_num_events * sizeof(struct smr_event);
    file_data->_mem_block = (uint8_t*)malloc(total_alloc_size);
    mem_ptr = file_data->_mem_block;

    file_data->tracks = (struct smr_track_data*)mem_ptr;
    mem_ptr = (uint8_t*)(file_data->tracks + file_data->ntracks);
    event_ptr = (struct smr_event*)mem_ptr;
    mem_ptr = (uint8_t*)(event_ptr + total_num_events);
    /* Rewind back to beginning of all track data. */
    buffer_read = all_tracks_start;

    printf("Second pass\n");
    START_TIMER()
    for (i = 0; i < file_data->ntracks; ++i)
    {
        struct smr_track_data track_data;
        uint32_t track_chunklen;
        uint8_t* track_start;
        uint8_t last_status_byte;

        /* Skip reading track header. */
        buffer_read += 4;

        track_chunklen = get_next_int(&buffer_read);
        track_start = buffer_read;
        track_data.events = event_ptr;

        track_data.nevents = 0;

        while (buffer_read - track_start < track_chunklen)
        {
            struct smr_event event;
            uint8_t status_byte;
            uint8_t status_byte_top;
            uint32_t event_chunklen;

            event.delta_time = get_next_variable_length_int(&buffer_read);
            status_byte = get_next_byte(&buffer_read);

            /* Check for running status. */
            if (status_byte < 0x80)
            {
                status_byte = last_status_byte;
                buffer_read -= 1;
            }

            last_status_byte = status_byte;

            status_byte_top = status_byte & 0xF0;
            if (status_byte_top >= 0x80 & status_byte_top < 0xF0)
            {
                /* MIDI event */
                event.event_type = status_byte_top;
                /* Getting bottom nibble as channel. */
                event.channel = status_byte & 0x0F;

                switch (event.event_type)
                {
                    case Midi_NoteOff:
                    case Midi_NoteOn:
                        event.note = get_next_byte(&buffer_read);
                        event.velocity = get_next_byte(&buffer_read);
                        break;
                    case Midi_PolyphonicPressure:
                        event.note = get_next_byte(&buffer_read);
                        event.pressure = get_next_byte(&buffer_read);
                        break;
                    case Midi_Controller:
                        event.controller = get_next_byte(&buffer_read);
                        event.value = get_next_byte(&buffer_read);
                        break;
                    case Midi_ProgramChange:
                        event.program = get_next_byte(&buffer_read);
                        break;
                    case Midi_ChannelPressure:
                        event.pressure = get_next_byte(&buffer_read);
                        break;
                    case Midi_PitchBend:
                        /* TODO: Test this, I don't know that this how the pitch bend value is supposed to be read in. */
                        event.pitch_bend = get_next_short(&buffer_read);
                        break;
                    default:
                        /* Can't happen. */
                        break;
                }
            }
            else if (status_byte == 0xF0 || status_byte == 0xF7)
            {
                uint32_t message_index;

                /* SysEx event */
                event.event_type = status_byte;
                event.length = get_next_variable_length_int(&buffer_read);
                event.message = (uint8_t*)mem_ptr;

                for (message_index = 0; message_index < event.length; ++message_index)
                {
                    event.message[message_index] = get_next_byte(&buffer_read);
                }

                mem_ptr += event.length;
            }
            else if (status_byte == 0xFF)
            {
                uint8_t meta_event_type;

                meta_event_type = get_next_byte(&buffer_read);
                event.event_type = meta_event_type | (status_byte << 8);
                event.length = get_next_variable_length_int(&buffer_read);

                switch (event.event_type)
                {
                    case Meta_SequenceNumber:
                        event.ss_ss = get_next_short(&buffer_read);
                        break;
                    case Meta_Text:
                    case Meta_Copyright:
                    case Meta_TrackName:
                    case Meta_InstrumentName:
                    case Meta_Lyric:
                    case Meta_Marker:
                    case Meta_CuePoint:
                    case Meta_ProgramName:
                    case Meta_DeviceName:
                    {
                        uint32_t text_index;

                        event.text = (char*)mem_ptr;

                        for (text_index = 0; text_index < event.length; ++text_index)
                        {
                            event.text[text_index] = (char)get_next_byte(&buffer_read);
                        }

                        /* Add null terminator. */
                        event.text[event.length] = 0;
                        mem_ptr += event.length + 1;
                        break;
                    }
                    case Meta_MidiChannelPrefix:
                        event.cc = get_next_byte(&buffer_read);
                        break;
                    case Meta_MidiPort:
                        event.pp = get_next_byte(&buffer_read);
                        break;
                    case Meta_EndOfTrack:
                        /* Empty event. */
                        break;
                    case Meta_Tempo:
                        event.tempo = get_next_int24(&buffer_read);
                        break;
                    case Meta_SmpteOffset:
                        event.hr = get_next_byte(&buffer_read);
                        event.mn = get_next_byte(&buffer_read);
                        event.se = get_next_byte(&buffer_read);
                        event.fr = get_next_byte(&buffer_read);
                        event.ff = get_next_byte(&buffer_read);
                        break;
                    case Meta_TimeSignature:
                        event.nn = get_next_byte(&buffer_read);
                        event.dd = get_next_byte(&buffer_read);
                        event.cc = get_next_byte(&buffer_read);
                        event.bb = get_next_byte(&buffer_read);
                        break;
                    case Meta_KeySignature:
                        event.sf = get_next_byte(&buffer_read);
                        event.mi = get_next_byte(&buffer_read);
                        break;
                    case Meta_SequencerSpecificEvent:
                    {
                        uint32_t data_index;

                        event.data = (uint8_t*)mem_ptr;

                        for (data_index = 0; data_index < event.length; ++data_index)
                        {
                            event.data[data_index] = (uint8_t) get_next_byte(&buffer_read);
                        }

                        mem_ptr += event.length;
                        break;
                    }
                    default:
                        /* Can't happen. */
                        break;
                }
            }
            else
            {
                /* Running status */
                printf("\nDo not recognize status byte %0.2x.\n", status_byte & 0xFF);
                return 1;
            }

            track_data.nevents += 1;
            *event_ptr = event;
            event_ptr += 1;
        }

        file_data->tracks[i] = track_data;
    }

    END_TIMER()

    return 0;
}

int smr_read_file(char* filename, struct smr_midi_data* file_data)
{
    FILE* file_ptr;
    long int file_size;
    uint8_t* buffer;
    int return_code;

    file_ptr = fopen(filename, "rb");
    if (!file_ptr)
    {
        printf("Unable to open file!\n");
        return 1;
    }

    /* Get file size */
    fseek(file_ptr, 0L, SEEK_END);
    file_size = ftell(file_ptr);
    fseek(file_ptr, 0L, SEEK_SET);

    /* Read entire file */
    buffer = (uint8_t*) malloc(file_size + 1);
    fread(buffer, sizeof(uint8_t), file_size, file_ptr);
    fclose(file_ptr);

    return_code = smr_read_byte_array(buffer, file_data);

    free(buffer);

    return return_code;
}

int smr_free_midi_data(struct smr_midi_data* midi_data)
{
    free(midi_data->_mem_block);

    return 0;
}
