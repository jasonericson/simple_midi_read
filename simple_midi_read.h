#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
            char* text;

            uint8_t ff;
        };
    };
};

struct smr_track_data
{
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
};

int smr_read_file(char* filename, struct smr_midi_data* file_data)
{
    FILE* file_ptr;
    long int file_size;
    uint8_t* full_buffer;
    uint8_t* buffer_read;
    uint32_t header_chunklen;
    int32_t i;

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
    full_buffer = (uint8_t*) malloc(file_size + 1);
    fread(full_buffer, sizeof(uint8_t), file_size, file_ptr);
    fclose(file_ptr);
    buffer_read = full_buffer;

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
        printf("This MIDI file (%s) is not compatible with this version of the parser.\n", filename);
        /* VERBOSE */
        printf("Expected header chunk size of 6 bytes, got %d.\n", header_chunklen);
        return 1;
    }

    file_data->format = get_next_short(&buffer_read);
    if (file_data->format == 0)
    {
        printf("This is a single-track MIDI file.\n");
    }
    else if (file_data->format == 1)
    {
        printf("This file contains two or more tracks meant to be played in tandem.\n");
    }
    else if (file_data->format == 2)
    {
        printf("This file contains one or more tracks meant to be played independently.\n");
    }
    else
    {
        printf("Do not recognize MIDI format %hu.\n", file_data->format);
        /*return 1;*/
    }

    file_data->ntracks = get_next_short(&buffer_read);
    printf("Number of tracks in this file: %hu.\n", file_data->ntracks);

    if (buffer_read[0] & (1<<15))
    {
        /* TODO: Test timecode */
        file_data->time_type = SMR_TT_timecode;
        printf("This file is governed by timecode.\n");
        /* FPS comes in as a negative value for some reason, this flips it to positive. */
        file_data->fps = 0 - get_next_byte(&buffer_read);
        file_data->subframe_resolution = get_next_byte(&buffer_read);
        printf("FPS value: %d\n", file_data->fps);
        printf("Sub-frame resolution: %d\n", file_data->subframe_resolution);
    }
    else
    {
        file_data->time_type = SMR_TT_metrical;
        printf("This file is governed by metrical timing.\n");
        file_data->tickdiv = get_next_short(&buffer_read);
        printf("Tickdiv value: %hu\n", file_data->tickdiv);
    }

    /*file_data->tracks = (struct smr_track_data*) malloc(file_data->ntracks * sizeof(struct smr_track_data));*/

    /* Add all tracks. */
    for (i = 0; i < file_data->ntracks; ++i)
    {
        uint32_t track_chunklen;
        uint8_t* track_start;
        uint32_t event_num;

        if (compare_next_string(&buffer_read, "MTrk") != 0)
        {
            printf("Did not find an expected track header.\n");
            return 1;
        }

        track_chunklen = get_next_int(&buffer_read);
        track_start = buffer_read;

        printf("- Track %d:\n", i);

        event_num = 0;
        /* TODO: Maybe ignore track length and just look for End of Track event? */
        while (buffer_read - track_start < track_chunklen)
        {
            uint32_t delta_time;
            uint8_t status_byte;
            uint8_t status_byte_top;
            enum smr_event_type event_type;
            uint32_t event_chunklen;

            delta_time = get_next_variable_length_int(&buffer_read);
            printf("--- Event %d | Delta Time = %u | Type = ", event_num, delta_time);
            status_byte = get_next_byte(&buffer_read);
            status_byte_top = status_byte & 0xF0;
            if (status_byte_top >= 0x80 & status_byte_top < 0xF0)
            {
                /* MIDI event */
                event_type = status_byte_top;
                /* Bottom nibble = channel */

                switch (event_type)
                {
                    case Midi_NoteOff:
                        printf("MIDI - Note Off\n");
                        event_chunklen = 2;
                        break;
                    case Midi_NoteOn:
                        printf("MIDI - Note On\n");
                        event_chunklen = 2;
                        break;
                    case Midi_PolyphonicPressure:
                        printf("MIDI - Polyphonic Pressure\n");
                        event_chunklen = 2;
                        break;
                    case Midi_Controller:
                        printf("MIDI - Controller\n");
                        event_chunklen = 2;
                        break;
                    case Midi_PitchBend:
                        printf("MIDI - Pitch Bend\n");
                        event_chunklen = 2;
                        break;

                    case Midi_ProgramChange:
                        printf("MIDI - Program Change\n");
                        event_chunklen = 1;
                        break;
                    case Midi_ChannelPressure:
                        printf("MIDI - Channel Pressure\n");
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

                switch (event_type)
                {
                    case SysEx_Single:
                        printf("SysEx - Single\n");
                        break;
                    case SysEx_Escape:
                        printf("SysEx - Escape\n");
                        break;
                    default:
                        /* Can't happen. */
                        break;
                }
            }
            else if (status_byte == 0xFF)
            {
                uint8_t meta_event_type;

                meta_event_type = get_next_byte(&buffer_read);
                event_type = meta_event_type | (status_byte << 8);
                switch (event_type)
                {
                    case Meta_SequenceNumber:
                        printf("Meta - Sequence Number\n");
                        break;
                    case Meta_Text:
                        printf("Meta - Text\n");
                        break;
                    case Meta_Copyright:
                        printf("Meta - Copyright\n");
                        break;
                    case Meta_TrackName:
                        printf("Meta - Track Name\n");
                        break;
                    case Meta_InstrumentName:
                        printf("Meta - Instrument Name\n");
                        break;
                    case Meta_Lyric:
                        printf("Meta - Lyric\n");
                        break;
                    case Meta_Marker:
                        printf("Meta - Marker\n");
                        break;
                    case Meta_CuePoint:
                        printf("Meta - Cue Point\n");
                        break;
                    case Meta_ProgramName:
                        printf("Meta - Program Name\n");
                        break;
                    case Meta_DeviceName:
                        printf("Meta - Device Name\n");
                        break;
                    case Meta_MidiChannelPrefix:
                        printf("Meta - MIDI Channel Prefix\n");
                        break;
                    case Meta_MidiPort:
                        printf("Meta - MIDI Port\n");
                        break;
                    case Meta_EndOfTrack:
                        printf("Meta - End of Track\n");
                        break;
                    case Meta_Tempo:
                        printf("Meta - Tempo\n");
                        break;
                    case Meta_SmpteOffset:
                        printf("Meta - SMPTE Offset\n");
                        break;
                    case Meta_TimeSignature:
                        printf("Meta - Time Signature\n");
                        break;
                    case Meta_KeySignature:
                        printf("Meta - Key Signature\n");
                        break;
                    case Meta_SequencerSpecificEvent:
                        printf("Meta - Sequencer Specific Event\n");
                        break;
                    default:
                        printf("\nDo not recognize meta event type %0.2x.\n", meta_event_type & 0xFF);
                        return 1;
                }

                event_chunklen = get_next_variable_length_int(&buffer_read);
            }
            else
            {
                printf("\nDo not recognize status byte %0.2x.\n", status_byte & 0xFF);
                return 1;
            }

            buffer_read += event_chunklen;
            event_num += 1;
        }
    }

    printf("\nFinished reading file!\n");
    return 0;
}
