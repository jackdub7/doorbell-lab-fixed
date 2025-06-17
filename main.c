#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "lib/buttons.h"
#include "lib/camera.h"
#include "lib/client.h"
#include "lib/colors.h"
#include "lib/device.h"
#include "lib/display.h"
#include "lib/fonts/fonts.h"
#include "lib/image.h"
#include "lib/log.h"

#define VIEWER_FOLDER "viewer/"
#define MAX_ENTRIES 8
#define MAX_FILE_NAME 100
#define HW_ID "7EA58328B"

enum Mode { MODE_WELCOME, MODE_BROWSER };

typedef struct {
    uint8_t *buf;
    size_t buffer_size;
} ThreadArg;

void intHandler(int dummy) {
    log_info("Exiting...");
    display_exit();
    exit(0);
}

int get_entries(char *folder, char entries[][MAX_FILE_NAME]) {
    DIR *dp = opendir(folder);
    struct dirent *dir;
    int count = 0;

    if (!dp) {
        return 0;
    }

    while ((dir = readdir(dp)) != NULL && count < MAX_ENTRIES) {
        char *dot = strrchr(dir->d_name, '.');
        if (dot && (strcmp(dot, ".bmp") == 0 || strcmp(dot, ".log") == 0)) {
            strncpy(entries[count], dir->d_name, MAX_FILE_NAME);
            entries[count][MAX_FILE_NAME - 1] = '\0';
            count++;
        }
    }

    closedir(dp);
    return count;
}

void draw_menu(char entries[][MAX_FILE_NAME], int num_entries, int selected) {
    display_clear(WHITE);
    for (int i = 0; i < num_entries; i++) {
        uint16_t bg = (i == selected) ? BYU_BLUE : WHITE;
        uint16_t fg = (i == selected) ? BYU_LIGHT_SAND : BLACK;
        display_draw_rectangle(0, i * 20, 127, i * 20 + 20, bg, true, 1);
        display_draw_string(5, i * 20, entries[i], &Font12, bg, fg);
    }
}

void draw_file(char *folder, char *file_name) {
    char path[256];
    snprintf(path, sizeof(path), "%s%s", folder, file_name);
    const char *ext = strrchr(file_name, '.');

    if (ext && strcmp(ext, ".bmp") == 0) {
        display_draw_image(path);
    } else if (ext && strcmp(ext, ".log") == 0) {
        FILE *f = fopen(path, "r");
        if (!f) {
            return;
        }
        char buffer[400] = {0};
        fread(buffer, 1, sizeof(buffer) - 1, f);
        fclose(f);
        display_clear(WHITE);
        display_draw_string(0, 0, buffer, &Font8, WHITE, BLACK);
    }
    delay_ms(2000);
}

void *photo_thread(void *arg) {
    ThreadArg *thread_arg = (ThreadArg *)arg;

    // Save the photo
    camera_save_to_file(thread_arg->buf, thread_arg->buffer_size, VIEWER_FOLDER "doorbell.bmp");

    // Upload the photo
    Config config = {
        .host = "ecen224.byu.edu",
        .port = "2240",
        .hw_id = HW_ID,
        .payload_size = thread_arg->buffer_size,
        .payload = thread_arg->buf,
    };

    int sockfd = client_connect(&config);
    client_send_image(sockfd, &config);
    client_receive_response(sockfd);
    client_close(sockfd);

    free(thread_arg->buf);
    free(thread_arg);

    return NULL;
}

void show_welcome(void) {
    display_clear(WHITE);
    display_draw_string(10, 50, "Welcome!", &Font16, WHITE, BLACK);
}

void show_ding_dong(void) {
    display_clear(WHITE);
    display_draw_string(20, 60, "Ding dong!", &Font16, WHITE, BLACK);
    delay_ms(2000);
    show_welcome();
}

int main(void) {
    sleep(15); // Delay for display conflict
    signal(SIGINT, intHandler);
    log_info("Starting...");

    if (!bcm2835_init()) {
        log_error("BCM2835 init failed!");
        return 1;
    }

    display_init();
    buttons_init();
    show_welcome();

    enum Mode mode = MODE_WELCOME;
    int sequence[3] = {0};
    int seq_index = 0;

    char entries[MAX_ENTRIES][MAX_FILE_NAME];
    int num_entries = get_entries(VIEWER_FOLDER, entries);
    int selected = 0;

    while (1) {
        delay_ms(200);

        if (mode == MODE_WELCOME) {
            if (button_up() == 0) {
                sequence[seq_index++ % 3] = 1;
                while (button_up() == 0) {
                    delay_ms(1);
                }
            } else if (button_down() == 0) {
                sequence[seq_index++ % 3] = 2;
                while (button_down() == 0) {
                    delay_ms(1);
                }
            }

            // Check for Up-Up-Down sequence: 1, 1, 2
            if (sequence[(seq_index - 3 + 3) % 3] == 1 && sequence[(seq_index - 2 + 3) % 3] == 1 &&
                sequence[(seq_index - 1 + 3) % 3] == 2) {
                mode = MODE_BROWSER;
                num_entries = get_entries(VIEWER_FOLDER, entries);
                draw_menu(entries, num_entries, selected);
                continue;
            }

            if (button_center() == 0) {
                size_t size = 128 * 128 * 3 + 54;
                uint8_t *buf = malloc(size);
                if (!buf) {
                    continue;
                }

                camera_capture_data(buf, size);

                ThreadArg *arg = malloc(sizeof(ThreadArg));
                arg->buf = buf;
                arg->buffer_size = size;

                pthread_t thread_id;
                pthread_create(&thread_id, NULL, photo_thread, (void *)arg);
                pthread_detach(thread_id);

                show_ding_dong();
                while (button_center() == 0) {
                    delay_ms(1);
                }
            }

        } else if (mode == MODE_BROWSER) {
            if (button_up() == 0) {
                selected = (selected - 1 + num_entries) % num_entries;
                draw_menu(entries, num_entries, selected);
                while (button_up() == 0) {
                    delay_ms(1);
                }
            }

            if (button_down() == 0) {
                selected = (selected + 1) % num_entries;
                draw_menu(entries, num_entries, selected);
                while (button_down() == 0) {
                    delay_ms(1);
                }
            }

            if (button_right() == 0) {
                draw_file(VIEWER_FOLDER, entries[selected]);
                draw_menu(entries, num_entries, selected);
                while (button_right() == 0) {
                    delay_ms(1);
                }
            }

            if (button_left() == 0) {
                show_welcome();
                mode = MODE_WELCOME;
                while (button_left() == 0) {
                    delay_ms(1);
                }
            }
        }
    }

    return 0;
}
