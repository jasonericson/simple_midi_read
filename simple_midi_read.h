#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compare_next_string(unsigned char** buffer_read, const char* to_compare)
{
    int result;
    int string_length;

    string_length = strlen(to_compare);
    result = strncmp((char*)*buffer_read, to_compare, string_length);
    *buffer_read += string_length;

    return result;
}

static unsigned char get_next_char(unsigned char** buffer_read)
{
    unsigned char result;

    result = (*buffer_read)[0];
    *buffer_read += 1;

    return result;
}

/* TODO: Check for endian-ness */
static int get_next_int(unsigned char** buffer_read)
{
    int result;

    result = (*buffer_read)[3] | ((*buffer_read)[2] << 8) | ((*buffer_read)[1] << 16) | ((*buffer_read)[0] << 24);
    *buffer_read += 4;

    return result;
}

/* TODO: Figure out fixed-width implementation if possible. */
static unsigned short get_next_short(unsigned char** buffer_read)
{
    unsigned short result;

    result = (*buffer_read)[1] | (*buffer_read)[0] << 8;
    *buffer_read += 2;

    return result;
}

static unsigned int get_next_variable_length_int(unsigned char** buffer_read)
{
    unsigned int result;

    result = 0;
    do
    {
        result = (result << 7) | (unsigned int)(**buffer_read & 0x7F);
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

    Meta = 0xFF
};

struct smr_event_data
{
    unsigned int delta_time;
    enum smr_event_type event_type;
};

struct smr_track_data
{
    struct smr_event_data* events;
};

struct smr_midi_data
{
    unsigned short format;
    unsigned short ntracks;
    unsigned short tickdiv;
    struct smr_track_data* tracks;
};

int smr_read_file(char* filename, struct smr_midi_data* file_data)
{
    FILE* file_ptr;
    long int file_size;
    unsigned char* full_buffer;
    unsigned char* buffer_read;
    int i;
    int header_chunklen;

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
    full_buffer = (unsigned char*) malloc(file_size + 1);
    fread(full_buffer, sizeof(unsigned char), file_size, file_ptr);
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
        int track_chunklen;
        unsigned char* track_start;

        if (compare_next_string(&buffer_read, "MTrk") != 0)
        {
            printf("Did not find an expected track header.\n");
            return 1;
        }

        track_chunklen = get_next_int(&buffer_read);
        track_start = buffer_read;

        /* TODO: Maybe ignore track length and just look for End of Track event? */
        while (buffer_read - track_start < track_chunklen)
        {
            int delta_time;
            enum smr_event_type event_type;
            int event_chunklen;

            delta_time = get_next_variable_length_int(&buffer_read);
            event_type = get_next_char(&buffer_read);

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
                case SysEx_Single:
                case SysEx_Escape:
                    event_chunklen = get_next_variable_length_int(&buffer_read);
                    break;
                case Meta:
                    get_next_char(&buffer_read);
                    event_chunklen = get_next_variable_length_int(&buffer_read);
                    break;
            }

            buffer_read += event_chunklen;
        }
    }

    printf("All good so far!\n");
    return 0;
}
