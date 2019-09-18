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

static uint8_t get_next_uint8(uint8_t** buffer_read)
{
    uint8_t result;

    result = (*buffer_read)[0];
    *buffer_read += 1;

    return result;
}

static uint32_t get_next_uint24(uint8_t** buffer_read)
{
    uint32_t result;

    result = 0;
    result = (*buffer_read)[2] | ((*buffer_read)[1] << 8) | ((*buffer_read)[0] << 16);
    *buffer_read += 3;

    return result;
}

static uint16_t get_next_uint16(uint8_t** buffer_read)
{
    uint16_t result;

    result = (*buffer_read)[1] | (*buffer_read)[0] << 8;
    *buffer_read += 2;

    return result;
}

/* TODO: Check for endian-ness */
static uint32_t get_next_uint32(uint8_t** buffer_read)
{
    uint32_t result;

    result = (*buffer_read)[3] | ((*buffer_read)[2] << 8) | ((*buffer_read)[1] << 16) | ((*buffer_read)[0] << 24);
    *buffer_read += 4;

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
    SMRE_metrical,
    SMRE_timecode
};

enum smr_event_type
{
    SMRE_midi_note_off = 0x80,
    SMRE_midi_note_on = 0x90,
    SMRE_midi_polyphonic_pressure = 0xA0,
    SMRE_midi_controller = 0xB0,
    SMRE_midi_program_change = 0xC0,
    SMRE_midi_channel_pressure = 0xD0,
    SMRE_midi_pitch_bend = 0xE0,

    SMRE_sysex_single = 0xF0,
    SMRE_sysex_escape = 0xF7,

    SMRE_meta_sequence_number = 0xFF00,
    SMRE_meta_text = 0xFF01,
    SMRE_meta_copyright = 0xFF02,
    SMRE_meta_track_name = 0xFF03,
    SMRE_meta_instrument_name = 0xFF04,
    SMRE_meta_lyric = 0xFF05,
    SMRE_meta_marker = 0xFF06,
    SMRE_meta_cue_point = 0xFF07,
    SMRE_meta_program_name = 0xFF08,
    SMRE_meta_device_name = 0xFF09,
    SMRE_meta_midi_channel_prefix = 0xFF20,
    SMRE_meta_midi_port = 0xFF21,
    SMRE_meta_end_of_track = 0xFF2F,
    SMRE_meta_tempo = 0xFF51,
    SMRE_meta_smpte_offset = 0xFF54,
    SMRE_meta_time_signature = 0xFF58,
    SMRE_meta_key_signature = 0xFF59,
    SMRE_meta_sequencer_specific_event = 0xFF7F
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
                /* I typically avoid multiple declarations on a single line, but
                   this all takes up far too much vertical space otherwise. */
                union { uint8_t note, controller, program, pp, hr, nn, sf; };
                union { uint8_t velocity, pressure, value, mn, dd, mi; };
                union { uint8_t channel, se, cc; };
                union { uint8_t fr, bb; };
            };
            uint16_t pitch_bend, ss_ss;
        };
        union
        {
            uint8_t *message, *bytes, *data;
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
    header_chunklen = get_next_uint32(&buffer_read);
    if (header_chunklen != 6)
    {
        printf("This MIDI file is not compatible with this version of the parser.\n");
        /* VERBOSE */
        printf("Expected header chunk size of 6 bytes, got %d.\n", header_chunklen);
        return 1;
    }

    file_data->format = get_next_uint16(&buffer_read);
    if (file_data->format > 2)
    {
        printf("Do not recognize MIDI format %hu.\n", file_data->format);
        /*return 1;*/
    }

    file_data->ntracks = get_next_uint16(&buffer_read);

    if (buffer_read[0] & (1<<15))
    {
        /* TODO: Test timecode */
        file_data->time_type = SMRE_timecode;
        /* FPS comes in as a negative value for some reason, this flips it to positive. */
        file_data->fps = 0 - get_next_uint8(&buffer_read);
        file_data->subframe_resolution = get_next_uint8(&buffer_read);
    }
    else
    {
        file_data->time_type = SMRE_metrical;
        file_data->tickdiv = get_next_uint16(&buffer_read);
    }

    total_alloc_size = file_data->ntracks * sizeof(struct smr_track_data);
    all_tracks_start = buffer_read;
    total_num_events = 0;

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

        track_chunklen = get_next_uint32(&buffer_read);
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
            status_byte = get_next_uint8(&buffer_read);

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
                    case SMRE_midi_note_off:
                    case SMRE_midi_note_on:
                    case SMRE_midi_polyphonic_pressure:
                    case SMRE_midi_controller:
                    case SMRE_midi_pitch_bend:
                        event_chunklen = 2;
                        break;

                    case SMRE_midi_program_change:
                    case SMRE_midi_channel_pressure:
                        event_chunklen = 1;
                        break;
                    default:
                        /* Can't happen. */
                        event_chunklen = 0; /* <- Appeasing compiler warnings. */
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

                meta_event_type = get_next_uint8(&buffer_read);
                event_type = meta_event_type | (status_byte << 8);
                event_chunklen = get_next_variable_length_int(&buffer_read);

                switch (event_type)
                {
                    case SMRE_meta_text:
                    case SMRE_meta_copyright:
                    case SMRE_meta_track_name:
                    case SMRE_meta_instrument_name:
                    case SMRE_meta_lyric:
                    case SMRE_meta_marker:
                    case SMRE_meta_cue_point:
                    case SMRE_meta_program_name:
                    case SMRE_meta_device_name:
                        /* Allocating 1 extra byte for text, to add null terminator. */
                        total_alloc_size += event_chunklen + 1;
                        break;
                    case SMRE_meta_sequencer_specific_event:
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

    total_alloc_size += total_num_events * sizeof(struct smr_event);
    file_data->_mem_block = (uint8_t*)malloc(total_alloc_size);
    mem_ptr = file_data->_mem_block;

    file_data->tracks = (struct smr_track_data*)mem_ptr;
    mem_ptr = (uint8_t*)(file_data->tracks + file_data->ntracks);
    event_ptr = (struct smr_event*)mem_ptr;
    mem_ptr = (uint8_t*)(event_ptr + total_num_events);
    /* Rewind back to beginning of all track data. */
    buffer_read = all_tracks_start;

    for (i = 0; i < file_data->ntracks; ++i)
    {
        struct smr_track_data track_data;
        uint32_t track_chunklen;
        uint8_t* track_start;
        uint8_t last_status_byte;

        /* Skip reading track header. */
        buffer_read += 4;

        track_chunklen = get_next_uint32(&buffer_read);
        track_start = buffer_read;
        track_data.events = event_ptr;

        track_data.nevents = 0;

        while (buffer_read - track_start < track_chunklen)
        {
            struct smr_event event;
            uint8_t status_byte;
            uint8_t status_byte_top;

            event.delta_time = get_next_variable_length_int(&buffer_read);
            status_byte = get_next_uint8(&buffer_read);

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
                    case SMRE_midi_note_off:
                    case SMRE_midi_note_on:
                        event.note = get_next_uint8(&buffer_read);
                        event.velocity = get_next_uint8(&buffer_read);
                        break;
                    case SMRE_midi_polyphonic_pressure:
                        event.note = get_next_uint8(&buffer_read);
                        event.pressure = get_next_uint8(&buffer_read);
                        break;
                    case SMRE_midi_controller:
                        event.controller = get_next_uint8(&buffer_read);
                        event.value = get_next_uint8(&buffer_read);
                        break;
                    case SMRE_midi_program_change:
                        event.program = get_next_uint8(&buffer_read);
                        break;
                    case SMRE_midi_channel_pressure:
                        event.pressure = get_next_uint8(&buffer_read);
                        break;
                    case SMRE_midi_pitch_bend:
                        /* TODO: Test this, I don't know that this how the pitch bend value is supposed to be read in. */
                        event.pitch_bend = get_next_uint16(&buffer_read);
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
                    event.message[message_index] = get_next_uint8(&buffer_read);
                }

                mem_ptr += event.length;
            }
            else if (status_byte == 0xFF)
            {
                uint8_t meta_event_type;

                meta_event_type = get_next_uint8(&buffer_read);
                event.event_type = meta_event_type | (status_byte << 8);
                event.length = get_next_variable_length_int(&buffer_read);

                switch (event.event_type)
                {
                    case SMRE_meta_sequence_number:
                        event.ss_ss = get_next_uint16(&buffer_read);
                        break;
                    case SMRE_meta_text:
                    case SMRE_meta_copyright:
                    case SMRE_meta_track_name:
                    case SMRE_meta_instrument_name:
                    case SMRE_meta_lyric:
                    case SMRE_meta_marker:
                    case SMRE_meta_cue_point:
                    case SMRE_meta_program_name:
                    case SMRE_meta_device_name:
                    {
                        uint32_t text_index;

                        event.text = (char*)mem_ptr;

                        for (text_index = 0; text_index < event.length; ++text_index)
                        {
                            event.text[text_index] = (char)get_next_uint8(&buffer_read);
                        }

                        /* Add null terminator. */
                        event.text[event.length] = 0;
                        mem_ptr += event.length + 1;
                        break;
                    }
                    case SMRE_meta_midi_channel_prefix:
                        event.cc = get_next_uint8(&buffer_read);
                        break;
                    case SMRE_meta_midi_port:
                        event.pp = get_next_uint8(&buffer_read);
                        break;
                    case SMRE_meta_end_of_track:
                        /* Empty event. */
                        break;
                    case SMRE_meta_tempo:
                        event.tempo = get_next_uint24(&buffer_read);
                        break;
                    case SMRE_meta_smpte_offset:
                        event.hr = get_next_uint8(&buffer_read);
                        event.mn = get_next_uint8(&buffer_read);
                        event.se = get_next_uint8(&buffer_read);
                        event.fr = get_next_uint8(&buffer_read);
                        event.ff = get_next_uint8(&buffer_read);
                        break;
                    case SMRE_meta_time_signature:
                        event.nn = get_next_uint8(&buffer_read);
                        event.dd = get_next_uint8(&buffer_read);
                        event.cc = get_next_uint8(&buffer_read);
                        event.bb = get_next_uint8(&buffer_read);
                        break;
                    case SMRE_meta_key_signature:
                        event.sf = get_next_uint8(&buffer_read);
                        event.mi = get_next_uint8(&buffer_read);
                        break;
                    case SMRE_meta_sequencer_specific_event:
                    {
                        uint32_t data_index;

                        event.data = (uint8_t*)mem_ptr;

                        for (data_index = 0; data_index < event.length; ++data_index)
                        {
                            event.data[data_index] = (uint8_t) get_next_uint8(&buffer_read);
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
