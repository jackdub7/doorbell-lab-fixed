#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "camera.h"
#include "log.h"

void camera_capture_data(uint8_t *buf, size_t bufsize) {
    char tmp_file_name[16] = "ecen-224-XXXXXX";
    char *command = "libcamera-still -n --immediate -e bmp --width 128 --height 128 -o ";

    // Create the temporary file
    int fp = mkstemp(tmp_file_name);

    // Create whole command
    ssize_t size = snprintf(NULL, 0, "%s%s", command, tmp_file_name);
    char *full_command = malloc(size + 1);
    snprintf(full_command, size + 1, "%s%s", command, tmp_file_name);

    // Run the command
    system(full_command);

    // Copy the data out of the file
    read(fp, buf, bufsize);

    // Clean up
    close(fp);
    remove(tmp_file_name);
    free(full_command);
}

void camera_save_to_file(uint8_t *buf, size_t bufsize, char *filename) {
    // Ensure the folder exists
    char folder[256];
    strncpy(folder, filename, sizeof(folder));
    folder[sizeof(folder) - 1] = '\0';

    // Chop off the filename to isolate the directory (e.g., "viewer/")
    char *last_slash = strrchr(folder, '/');
    if (last_slash) {
        *last_slash = '\0'; // Truncate to just the folder path
        struct stat st = {0};
        if (stat(folder, &st) == -1) {
            mkdir(folder, 0755);
        }
    }

    // Now write the file
    FILE *f = fopen(filename, "wb");
    if (!f) {
        log_error("Failed to open %s for writing", filename);
        return;
    }

    fwrite(buf, 1, bufsize, f);
    fclose(f);
    log_info("Saved photo to %s\n", filename);
}
