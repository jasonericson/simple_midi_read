#include <stdio.h>
#include <stdlib.h>

int read_midi_file(char* filename)
{
    FILE* file_ptr;
    file_ptr = fopen(filename, "rb");
    if (!file_ptr)
    {
        printf("Unable to open file!");
        return 1;
    }

    // Get file size
    fseek(file_ptr, 0L, SEEK_END);
    long int file_size = ftell(file_ptr);
    fseek(file_ptr, 0L, SEEK_SET);

    // Read entire file
    char* in_buffer;
    in_buffer = (char*) malloc(file_size + 1);
    fread(in_buffer, sizeof(char), file_size, file_ptr);

    for (int i = 0; i < file_size; ++i)
    {
        printf("%.2x", in_buffer[i] & 0xff);
        if (i % 16 == 15)
        {
            printf("\n");
        }
        else if (i % 2 == 1)
        {
            printf(" ");
        }
    }
}
