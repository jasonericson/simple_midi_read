#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 

#define SHORT_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"
#define SHORT_TO_BINARY(short)  \
  (short & 0x8000 ? '1' : '0'), \
  (short & 0x4000 ? '1' : '0'), \
  (short & 0x2000 ? '1' : '0'), \
  (short & 0x1000 ? '1' : '0'), \
  (short & 0x0800 ? '1' : '0'), \
  (short & 0x0400 ? '1' : '0'), \
  (short & 0x0200 ? '1' : '0'), \
  (short & 0x0100 ? '1' : '0'), \
  (short & 0x80 ? '1' : '0'), \
  (short & 0x40 ? '1' : '0'), \
  (short & 0x20 ? '1' : '0'), \
  (short & 0x10 ? '1' : '0'), \
  (short & 0x08 ? '1' : '0'), \
  (short & 0x04 ? '1' : '0'), \
  (short & 0x02 ? '1' : '0'), \
  (short & 0x01 ? '1' : '0') 

/* TODO: Check for endian-ness */
static int get_next_int(unsigned char** buffer_read)
{
    int result;

#ifdef SMR_VERBOSE_DEBUGGING
    printf("Getting next int.\n");
    printf("Byte 0: %.2x\n", (*buffer_read)[0] & 0xff);
    printf("Byte 1: %.2x\n", (*buffer_read)[1] & 0xff);
    printf("Byte 2: %.2x\n", (*buffer_read)[2] & 0xff);
    printf("Byte 3: %.2x\n", (*buffer_read)[3] & 0xff);
#endif

    result = (*buffer_read)[3] | ((int)(*buffer_read)[2] << 8) | ((int)(*buffer_read)[1] << 16) | ((int)(*buffer_read)[0] << 24);
    *buffer_read += 4;

    return result;
}

/* TODO: Figure out fixed-width implementation if possible. */
static unsigned short get_next_short(unsigned char** buffer_read)
{
    unsigned short result;

#ifdef SMR_VERBOSE_DEBUGGING
    printf("Getting next short.\n");
    printf("Byte 0: "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY((*buffer_read)[0]));
    printf("Byte 1: "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY((*buffer_read)[1]));
#endif

    result = (*buffer_read)[1] | (*buffer_read)[0] << 8;
    *buffer_read += 2;

    return result;
}

struct smr_midi_data
{
    unsigned short format;
    unsigned short ntracks;
    unsigned short tickdiv;
};

int smr_read_file(char* filename, struct smr_midi_data* result)
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

    int thing = 0;

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
    if (strncmp((char*)buffer_read, "MThd", 4) != 0)
    {
        /* TODO: Separate out verbose logging? */
        printf("MIDI file is missing header identifier 'MThd'.\n");
        return 1;
    }

    /* Check for header chunklen */
    /* TODO: Standardize when buffer pointer advances. */
    buffer_read += 4;
    header_chunklen = get_next_int(&buffer_read);
    if (header_chunklen != 6)
    {
        printf("This MIDI file (%s) is not compatible with this version of the parser.\n", filename);
        /* VERBOSE */
        printf("Expected header chunk size of 6 bytes, got %d.\n", header_chunklen);
        return 1;
    }

    result->format = get_next_short(&buffer_read);
    if (result->format == 0)
    {
        printf("This is a single-track MIDI file.\n");
    }
    else if (result->format == 1)
    {
        printf("This file contains two or more tracks meant to be played in tandem.\n");
    }
    else if (result->format == 2)
    {
        printf("This file contains one or more tracks meant to be played independently.\n");
    }
    else
    {
        printf("Do not recognize MIDI format %hu.\n", result->format);
        /*return 1;*/
    }

    result->ntracks = get_next_short(&buffer_read);
    printf("Number of tracks in this file: %hu.\n", result->ntracks);

    if (buffer_read[0] & (1<<15))
    {
        printf("This file is governed by timecode.\n");
        /* TODO: Handle timecode case. */
        buffer_read += 2;
    }
    else
    {
        printf("This file is governed by metrical timing.\n");
        result->tickdiv = get_next_short(&buffer_read);
        printf("Tickdiv value: %hu\n", result->tickdiv);
    }

    printf("All good so far!\n");
    return 0;
}
