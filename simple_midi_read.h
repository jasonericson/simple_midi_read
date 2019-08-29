#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TODO: Check for endian-ness */
int get_next_int(char* buffer_read)
{
    int result;

    result = buffer_read[3] | ((int)buffer_read[2] << 8) | ((int)buffer_read[1] << 16) | ((int)buffer_read[0] << 24);
    buffer_read += 4;

    return result;
}

int read_midi_file(char* filename)
{
    FILE* file_ptr;
    long int file_size;
    char* full_buffer;
    char* buffer_read;
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
    full_buffer = (char*) malloc(file_size + 1);
    fread(full_buffer, sizeof(char), file_size, file_ptr);
    fclose(file_ptr);
    buffer_read = full_buffer;

    /* Check for header identifier */
    if (strncmp(buffer_read, "MThd", 4) != 0)
    {
        /* TODO: Separate out verbose logging? */
        printf("MIDI file is missing header identifier 'MThd'.\n");
        return 1;
    }

    /* Check for header chunklen */
    /* TODO: Standardize when buffer pointer advances. */
    buffer_read += 4;
    header_chunklen = get_next_int(buffer_read);
    if (header_chunklen != 6)
    {
        printf("This MIDI file (%s) is not compatible with this version of the parser.\n", filename);
        /* VERBOSE */
        printf("Expected header chunk size of 6 bytes, got %d.", header_chunklen);
        return 1;
    }

    printf("All good so far!\n");
    return 0;
}
