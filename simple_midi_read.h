#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char smr_byte;
typedef unsigned short smr_short;
typedef unsigned int smr_int;
typedef char smr_char;
typedef short smr_s_short;
typedef int smr_s_int;

static int compare_next_string(smr_byte** buffer_read, const char* to_compare)
{
    int result;
    int string_length;

    string_length = strlen(to_compare);
    result = strncmp((char*)*buffer_read, to_compare, string_length);
    *buffer_read += string_length;

    return result;
}

static smr_byte get_next_byte(smr_byte** buffer_read)
{
    smr_byte result;

    result = (*buffer_read)[0];
    *buffer_read += 1;

    return result;
}

/* TODO: Check for endian-ness */
static smr_int get_next_int(smr_byte** buffer_read)
{
    smr_int result;

    result = (*buffer_read)[3] | ((*buffer_read)[2] << 8) | ((*buffer_read)[1] << 16) | ((*buffer_read)[0] << 24);
    *buffer_read += 4;

    return result;
}

/* TODO: Figure out fixed-width implementation if possible. */
static smr_short get_next_short(smr_byte** buffer_read)
{
    smr_short result;

    result = (*buffer_read)[1] | (*buffer_read)[0] << 8;
    *buffer_read += 2;

    return result;
}

static smr_int get_next_variable_length_int(smr_byte** buffer_read)
{
    smr_int result;

    result = 0;
    do
    {
        result = (result << 7) | (smr_int)(**buffer_read & 0x7F);
    } while (*(*buffer_read)++ & 0x80);

    return result;
}

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

struct smr_event_data
{
    smr_int delta_time;
    enum smr_event_type event_type;
};

struct smr_track_data
{
    struct smr_event_data* events;
};

struct smr_midi_data
{
    smr_short format;
    smr_short ntracks;
    smr_short tickdiv;
    struct smr_track_data* tracks;
};

int smr_read_file(char* filename, struct smr_midi_data* file_data)
{
    FILE* file_ptr;
    long int file_size;
    smr_byte* full_buffer;
    smr_byte* buffer_read;
    smr_int header_chunklen;
    int i;

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
    full_buffer = (smr_byte*) malloc(file_size + 1);
    fread(full_buffer, sizeof(smr_byte), file_size, file_ptr);
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
        printf("This file is governed by timecode.\n");
        /* TODO: Handle timecode case. */
        buffer_read += 2;
    }
    else
    {
        printf("This file is governed by metrical timing.\n");
        file_data->tickdiv = get_next_short(&buffer_read);
        printf("Tickdiv value: %hu\n", file_data->tickdiv);
    }

    /*file_data->tracks = (struct smr_track_data*) malloc(file_data->ntracks * sizeof(struct smr_track_data));*/

    /* Add all tracks. */
    for (i = 0; i < file_data->ntracks; ++i)
    {
        smr_int track_chunklen;
        smr_byte* track_start;
        int event_num;

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
            smr_int delta_time;
            smr_byte status_byte;
            smr_byte status_byte_top;
            enum smr_event_type event_type;
            smr_int event_chunklen;

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
                }
            }
            else if (status_byte == 0xFF)
            {
                smr_byte meta_event_type;

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
