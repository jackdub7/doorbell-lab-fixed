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
#define MAX_TEXT_SIZE 400
#define MAX_FILE_NAME 100
#define HW_ID "7EA58328B"

// Colors
#define BACKGROUND_COLOR WHITE
#define FONT_COLOR BLACK
#define SELECTED_BG_COLOR BYU_BLUE
#define SELECTED_FONT_COLOR BYU_LIGHT_SAND

// Status state
enum StatusState { STATUS_NONE, STATUS_SENDING, STATUS_SENT };

volatile enum StatusState status_state = STATUS_NONE;
volatile time_t sent_time = 0;
pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;

// Thread argument struct
typedef struct {
    uint8_t *buf;
    size_t buffer_size;
} ThreadArg;

// Function prototypes
void draw_status_bar(void);

void intHandler(int dummy) {
    log_info("Exiting...");
    display_exit();
    exit(0);
}

int get_entries(char *folder, char entries[][MAX_FILE_NAME]) {
    DIR *dp;
    struct dirent *dir;
    int count = 0;

    dp = opendir(folder);
    if (!dp) {
        log_error("Failed to open folder: %s", folder);
        return 0;
    }

    while ((dir = readdir(dp)) != NULL && count < MAX_ENTRIES) {
        char *dot = strrchr(dir->d_name, '.');
        if (!dot) {
            continue;
        }

        if (strcmp(dot, ".bmp") == 0 || strcmp(dot, ".log") == 0) {
            strncpy(entries[count], dir->d_name, MAX_FILE_NAME);
            entries[count][MAX_FILE_NAME - 1] = '\0';
            count++;
        }
    }

    closedir(dp);
    return count;
}

void draw_menu(char entries[][MAX_FILE_NAME], int num_entries, int selected) {
    display_clear(BACKGROUND_COLOR);

    for (int i = 0; i < num_entries; i++) {
        uint16_t bg = (i == selected) ? SELECTED_BG_COLOR : BACKGROUND_COLOR;
        uint16_t fg = (i == selected) ? SELECTED_FONT_COLOR : FONT_COLOR;

        display_draw_rectangle(0, i * 20, 127, i * 20 + 20, bg, true, 1);
        display_draw_string(5, i * 20, entries[i], &Font12, bg, fg);
    }
}

void draw_file(char *folder, char *file_name) {
    char path[200];
    snprintf(path, sizeof(path), "%s%s", folder, file_name);

    const char *ext = strrchr(file_name, '.');
    if (ext && strcmp(ext, ".bmp") == 0) {
        display_draw_image(path);
    } else if (ext && strcmp(ext, ".log") == 0) {
        FILE *f = fopen(path, "r");
        if (!f) {
            log_error("Failed to open file: %s", path);
            return;
        }
        char buffer[MAX_TEXT_SIZE + 1] = {0};
        fread(buffer, 1, MAX_TEXT_SIZE, f);
        fclose(f);

        display_clear(BACKGROUND_COLOR);
        display_draw_string(0, 0, buffer, &Font8, BACKGROUND_COLOR, FONT_COLOR);
    }

    draw_status_bar();
    delay_ms(2000);
}

void *send_image(void *arg) {
    ThreadArg *thread_arg = (ThreadArg *)arg;
    status_state = STATUS_SENDING;
    uint8_t *buf = thread_arg->buf;
    size_t buffer_size = thread_arg->buffer_size;

    Config config = {
        .host = "ecen224.byu.edu",
        .port = "2240",
        .hw_id = HW_ID,
        .payload_size = buffer_size,
        .payload = buf,
    };

    log_info("Sending image: payload_size=%zu", buffer_size);

    int sockfd = client_connect(&config);
    client_send_image(sockfd, &config);
    client_receive_response(sockfd);
    client_close(sockfd);

    status_state = STATUS_SENT;

    sent_time = time(NULL);

    delay_ms(2000);

    free(buf);
    free(thread_arg);
    status_state = STATUS_NONE;

    return NULL;
}

void draw_status_bar(void) {
    if (status_state == STATUS_NONE) {
        return;
    }

    int bar_height = 12;
    int y_start = 128 - bar_height;

    uint16_t color = (status_state == STATUS_SENDING) ? BYU_BLUE : GREEN;
    const char *text = (status_state == STATUS_SENDING) ? "Sending..." : "Sent!";

    display_draw_rectangle(0, y_start, 127, 127, BACKGROUND_COLOR, true, 1);

    display_draw_rectangle(0, y_start, 127, 127, color, true, 1);

    display_draw_string(3, y_start + 2, text, &Font8, color, WHITE);
}

int main(void) {
    signal(SIGINT, intHandler);

    log_info("Starting...");

    if (!bcm2835_init()) {
        log_error("BCM2835 init failed!");
        return 1;
    }

    display_init();
    buttons_init();

    DIR *dp = opendir(VIEWER_FOLDER);
    if (dp) {
        struct dirent *entry;
        while ((entry = readdir(dp)) != NULL) {
            if (strcmp(entry->d_name, "doorbell.bmp") == 0) {
                char path[300];
                snprintf(path, sizeof(path), "%s%s", VIEWER_FOLDER, entry->d_name);
                log_info("Removing old file: %s", path);
                remove(path);
            }
        }
        closedir(dp);
    }

    char entries[MAX_ENTRIES][MAX_FILE_NAME];
    int num_entries = get_entries(VIEWER_FOLDER, entries);
    int selected = 0;

    draw_menu(entries, num_entries, selected);
    draw_status_bar();

    while (true) {
        delay_ms(200);

        if (button_up() == 0) {
            selected = (selected - 1 + num_entries) % num_entries;
            draw_menu(entries, num_entries, selected);
            draw_status_bar();
            while (button_up() == 0) {
                delay_ms(1);
            }
        }

        if (button_down() == 0) {
            selected = (selected + 1) % num_entries;
            draw_menu(entries, num_entries, selected);
            draw_status_bar();
            while (button_down() == 0) {
                delay_ms(1);
            }
        }

        if (button_right() == 0) {
            draw_file(VIEWER_FOLDER, entries[selected]);
            draw_menu(entries, num_entries, selected);
            draw_status_bar();
            while (button_right() == 0) {
                delay_ms(1);
            }
        }

        if (button_center() == 0) {
            display_clear(BACKGROUND_COLOR);
            display_draw_string(5, 60, "Say Cheese!", &Font12, BACKGROUND_COLOR, FONT_COLOR);
            draw_status_bar();

            size_t buffer_size = 128 * 128 * 3 + 54;
            uint8_t *buf = malloc(buffer_size);
            if (!buf) {
                log_error("Failed to allocate buffer for image");
                continue;
            }

            camera_capture_data(buf, buffer_size);
            camera_save_to_file(buf, buffer_size, VIEWER_FOLDER "doorbell.bmp");

            Bitmap *bmp = malloc(sizeof(Bitmap));
            if (!bmp) {
                log_error("Failed to allocate Bitmap struct");
                free(buf);
                continue;
            }

            if (create_bmp(bmp, buf) != 0) {
                log_error("Failed to parse BMP data");
                free(buf);
                free(bmp);
                continue;
            }

            bool viewing = true;
            bool image_changed = true;

            while (viewing) {
                delay_ms(100);

                if (image_changed) {
                    display_draw_image_data(bmp->pxl_data, bmp->img_width, bmp->img_height);
                    reset_pixel_data(bmp);
                    draw_status_bar();
                    image_changed = false;
                }

                if (button_right() == 0) {
                    remove_color_channel(RED_CHANNEL, bmp);
                    image_changed = true;
                } else if (button_left() == 0) {
                    remove_color_channel(BLUE_CHANNEL, bmp);
                    image_changed = true;
                } else if (button_up() == 0) {
                    remove_color_channel(GREEN_CHANNEL, bmp);
                    image_changed = true;
                } else if (button_down() == 0) {
                    or_filter(bmp);
                    image_changed = true;
                } else if (button_center() == 0) {
                    viewing = false;

                    size_t hw_id_len = 9;

                    size_t total_payload_size = hw_id_len + IMG_SIZE;

                    uint8_t *payload = malloc(total_payload_size);
                    memcpy(payload, HW_ID, hw_id_len);
                    memcpy(payload + hw_id_len, buf, IMG_SIZE);

                    log_info("HW_ID bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                             payload[0], payload[1], payload[2], payload[3], payload[4], payload[5],
                             payload[6], payload[7], payload[8]);

                    pthread_t thread_id;
                    ThreadArg *thread_arg = malloc(sizeof(ThreadArg));
                    thread_arg->buf = payload;
                    thread_arg->buffer_size = total_payload_size;

                    pthread_create(&thread_id, NULL, send_image, (void *)thread_arg);
                    pthread_detach(thread_id);

                    while (button_center() == 0) {
                        delay_ms(1);
                    }
                }

                while (button_right() == 0 || button_left() == 0 || button_up() == 0 ||
                       button_down() == 0) {
                    delay_ms(1);
                }
            }

            destroy_bmp(bmp);
            free(bmp);

            num_entries = get_entries(VIEWER_FOLDER, entries);
            draw_menu(entries, num_entries, selected);
            draw_status_bar();

            while (button_center() == 0) {
                delay_ms(1);
            }
        }

        pthread_mutex_lock(&status_mutex);
        if (status_state == STATUS_SENT) {
            time_t current_time = time(NULL);
            if (current_time - sent_time >= 2) {
                status_state = STATUS_NONE;
            }
        }
        pthread_mutex_unlock(&status_mutex);

        draw_status_bar();
    }

    return 0;
}
// back to start again my friend