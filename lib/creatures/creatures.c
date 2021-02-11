
#include <stdio.h>
#include <stdint.h>

#include "creatures.h"

// Write out the file with our magic number
FILE *open_file(char *file_name, struct Header header)
{
    FILE *our_file = fopen(file_name, "wb");
    fprintf(our_file, "%.*s", 5, MAGIC_NUMBER);
    fwrite(&header, sizeof(header), 1, our_file);
    return our_file;
}

// Close the file cleanly
void close_file(FILE *file)
{
    fclose(file);
}

// Write out a movement frame to disk
void write_movement_frame(FILE *file, uint8_t *positions, int number_of_servos)
{
    fputc(MOVEMENT_FRAME_TYPE, file);
    fwrite(positions, number_of_servos, 1, file);
}

void write_pause_frame(FILE *file, uint16_t millseconds_to_pause)
{
    struct Pause pause;
    pause.pause_type = (uint8_t)PAUSE_FRAME_TYPE;
    pause.pause_in_milliseconds = millseconds_to_pause;
    printf("size of pause is %ld\n", sizeof(pause));

    fwrite(&pause, sizeof(pause), 1, file);
}

void write_led_control_frame(FILE *file)
{
    // I have no idea yet! :)
}