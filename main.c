#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lib/buttons.h"
#include "lib/camera.h"
#include "lib/client.h"
#include "lib/colors.h"
#include "lib/device.h"
#include "lib/display.h" // Make sure this has the required function declarations
#include "lib/fonts/fonts.h"
#include "lib/image.h"
#include "lib/log.h"

#define VIEWER_FOLDER "viewer/"
#define MAX_ENTRIES 8
#define MAX_TEXT_SIZE 400
#define MAX_FILE_NAME 100

#define BACKGROUND_COLOR WHITE
#define FONT_COLOR BLACK
#define SELECTED_BG_COLOR BYU_BLUE
#define SELECTED_FONT_COLOR BYU_LIGHT_SAND

enum StatusState { STATUS_NONE, STATUS_SENDING, STATUS_SENT };
volatile enum StatusState status_state = STATUS_NONE;

typedef struct {
    char filename[MAX_FILE_NAME];
} ThreadArg;

void intHandler(int sig) {
    (void)sig;
    log_info("Exiting...");
    display_exit();
    exit(0);
}

static int get_entries(const char *folder, char entries[MAX_ENTRIES][MAX_FILE_NAME]) {
    DIR *dp = opendir(folder);
    if (!dp) {
        return 0;
    }

    int count = 0;
    struct dirent *entry;

    while (count < MAX_ENTRIES && (entry = readdir(dp)) != NULL) {
        if (strstr(entry->d_name, ".bmp") || strstr(entry->d_name, ".log")) {
            snprintf(entries[count], MAX_FILE_NAME, "%s", entry->d_name);
            ++count;
        }
    }

    closedir(dp);
    return count;
}

static void draw_menu(char entries[MAX_ENTRIES][MAX_FILE_NAME], int num, int selected) {
    display_fill_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, BACKGROUND_COLOR);

    for (int i = 0; i < num; ++i) {
        const uint16_t fg = (i == selected) ? SELECTED_FONT_COLOR : FONT_COLOR;
        const uint16_t bg = (i == selected) ? SELECTED_BG_COLOR : BACKGROUND_COLOR;
        display_draw_text(10, i * 20, entries[i], Font20, fg, bg);
    }

    const char *msg = (status_state == STATUS_SENDING) ? "Sending..."
                      : (status_state == STATUS_SENT)  ? "Sent!"
                                                       : "";
    display_draw_status_bar(msg);
}

static void *send_image_thread(void *varg) {
    ThreadArg *arg = (ThreadArg *)varg;
    status_state = STATUS_SENDING;

    char path[256];
    snprintf(path, sizeof(path), "%s%s", VIEWER_FOLDER, arg->filename);
    log_info("Thread started, pushing %s", path);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        log_error("Failed to open %s", path);
        status_state = STATUS_NONE;
        free(arg);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size_t fsize = ftell(fp);
    rewind(fp);

    uint8_t *buf = malloc(fsize);
    if (!buf) {
        log_error("Out of memory (%zu bytes)", fsize);
        fclose(fp);
        status_state = STATUS_NONE;
        free(arg);
        return NULL;
    }

    fread(buf, 1, fsize, fp);
    fclose(fp);

    const char *hw_id = "7EA58328B";
    size_t hwlen = strlen(hw_id);

    Config cfg = {
        .host = "ecen224.byu.edu",
        .port = "2240",
        .hw_id = hw_id,
        .payload_size = hwlen + fsize,
    };

    cfg.payload = malloc(cfg.payload_size);
    if (!cfg.payload) {
        log_error("Out of memory (payload)");
        free(buf);
        status_state = STATUS_NONE;
        free(arg);
        return NULL;
    }

    memcpy(cfg.payload, hw_id, hwlen);
    memcpy(cfg.payload + hwlen, buf, fsize);

    int sockfd = client_connect(&cfg);
    client_send_image(sockfd, &cfg);
    client_receive_response(sockfd);
    client_close(sockfd);

    free(cfg.payload);
    free(buf);
    free(arg);

    status_state = STATUS_SENT;
    delay_ms(2000);
    status_state = STATUS_NONE;

    return NULL;
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
        struct dirent *e;
        while ((e = readdir(dp)) != NULL) {
            if (strcmp(e->d_name, "doorbell.bmp") == 0) {
                char pth[256];
                snprintf(pth, sizeof(pth), "%s%s", VIEWER_FOLDER, e->d_name);
                remove(pth);
            }
        }
        closedir(dp);
    }

    char entries[MAX_ENTRIES][MAX_FILE_NAME];
    int num_entries = get_entries(VIEWER_FOLDER, entries);
    int sel = 0;

    draw_menu(entries, num_entries, sel);

    while (1) {
        delay_ms(200);

        if (button_up() == 0) {
            sel = (sel - 1 + num_entries) % num_entries;
            draw_menu(entries, num_entries, sel);
        } else if (button_down() == 0) {
            sel = (sel + 1) % num_entries;
            draw_menu(entries, num_entries, sel);
        } else if (button_center() == 0) {
            const char *fname = entries[sel];
            if (strstr(fname, ".bmp")) {
                char pth[256];
                snprintf(pth, sizeof(pth), "%s%s", VIEWER_FOLDER, fname);

                FILE *fp = fopen(pth, "rb");
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    size_t fsize = ftell(fp);
                    rewind(fp);

                    uint8_t *buf = malloc(fsize);
                    if (buf) {
                        fread(buf, 1, fsize, fp);
                        fclose(fp);

                        Bitmap bmp;
                        if (create_bmp(&bmp, buf) == 0) {
                            display_draw_image_data(bmp.pxl_data, bmp.img_width, bmp.img_height);
                            delay_ms(2000);
                        }
                        free(buf);
                    } else {
                        fclose(fp);
                    }
                }
            }

            ThreadArg *targ = malloc(sizeof(ThreadArg));
            if (!targ) {
                continue;
            }
            snprintf(targ->filename, MAX_FILE_NAME, "%s", entries[sel]);

            pthread_t tid;
            pthread_create(&tid, NULL, send_image_thread, targ);
            draw_menu(entries, num_entries, sel);
        }
    }
    return 0;
}