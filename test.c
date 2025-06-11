#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lib/buttons.h"
#include "lib/colors.h"
#include "lib/device.h"
#include "lib/display.h"
#include "lib/fonts/fonts.h"
#include "lib/log.h"

#define VIEWER_FOLDER "viewer/"
#define MAX_ENTRIES 8
#define MAX_TEXT_SIZE 400
#define MAX_FILE_NAME 100

// Colors — Feel free to change these to fit your preference
#define BACKGROUND_COLOR WHITE
#define FONT_COLOR BLACK
#define SELECTED_BG_COLOR BYU_BLUE
#define SELECTED_FONT_COLOR BYU_LIGHT_SAND

#define NUM_ENTRIES 5
#define MAX_ENTRY_LENGTH 20

char entries[NUM_ENTRIES][MAX_ENTRY_LENGTH] = {"Clear", "Hello", "Chars", "Stars", "Flag"};

// Makes sure to deinitialize everything before program close
void intHandler(int dummy) {
    log_info("Exiting...");

    // Resets the Screen
    display_clear(BLACK);
    display_draw_string(5, 5, "Goodbye!", &Font16, BLACK, WHITE);

    // Releases control of the display
    display_exit();
    exit(0);
}

// clearScreen
//
//
// Clears the screen to white.
void clearScreen() {
    display_clear(WHITE);
}

// drawHelloWorld
//
// Draws "Hello World!" on the display 10 times
// Make each line a different color
void drawHelloWorld() {
    display_clear(BACKGROUND_COLOR);
    uint16_t colors[] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, ORANGE, PURPLE, PINK, BLACK};
    for (int i = 0; i < 10; i++) {
        display_draw_string(0, i * 10, "Hello World!", &Font12, BACKGROUND_COLOR, colors[i % 10]);
    }
}

// drawChars
//
// Draw chars of different values, sizes, colors, and locations onto the screen
void drawChars() {
    display_clear(BACKGROUND_COLOR);

    char chars[] = {'A', 'B', 'C', 'D', 'E', '@', '7', '*', '#', '!'};
    void* fonts[] = {&Font8, &Font12, &Font16, &Font20, &Font24,
                     &Font8, &Font12, &Font16, &Font20, &Font24};
    uint16_t fg_colors[] = {WHITE, BLACK, BLUE, RED, GREEN, CYAN, MAGENTA, YELLOW, ORANGE, PURPLE};
    uint16_t bg_colors[] = {RED, GREEN, YELLOW, BLACK, BLUE, WHITE, ORANGE, CYAN, BLACK, MAGENTA};

    for (int i = 0; i < 10; i++) {
        int x = 10 + i * 12;
        int y = 10 + i * 12;
        display_draw_char(x, y, chars[i], fonts[i], fg_colors[i], bg_colors[i]);
    }
}

void draw_star(int x, int y, uint16_t color) {
    int points[10][2] = {
        { x,     y - 5 },   // Top point
        { x + 2, y - 1 },   // Upper-right inner
        { x + 5, y - 1 },   // Right point
        { x + 3, y + 1 },   // Lower-right inner
        { x + 4, y + 5 },   // Bottom right
        { x,     y + 3 },   // Bottom center
        { x - 4, y + 5 },   // Bottom left
        { x - 3, y + 1 },   // Lower-left inner
        { x - 5, y - 1 },   // Left point
        { x - 2, y - 1 }    // Upper-left inner
    };

    // Draw lines between the 10 points to form the star
    for (int i = 0; i < 10; i++) {
        int next = (i + 1) % 10;
        display_draw_line(points[i][0], points[i][1],
                          points[next][0], points[next][1],
                          color, 1);
    }
}

// drawStars
//
// Draw at least 20 stars on the screen
// TIP: Make a function that uses display_draw_# functions to make the shape of the star,
// Then call that function at multiple locations
void drawStars() {
    display_clear(BACKGROUND_COLOR);

    for (int i = 0; i < 20; i++) {
        int x = (i % 5) * 25 + 5;  // 5 columns, 25 pixels apart
        int y = (i / 5) * 20 + 5;  // 4 rows, 20 pixels apart
        draw_star(x, y, YELLOW);
    }
}

// drawFlag 
//
// Use at least 5 other display_draw functions to make any flag you want.
// It must have at least 3 colors, and use 3 unique display_draw functions
void drawFlag() {
    display_clear(BLACK);  // Clear with visible background

    int stripe_width = 100;
    int stripe_height = 30;

    // Top-left corner position
    int x1 = 5;  // 5px from left edge
    int y1 = 5;  // 5px from top edge

    // Draw vertical flag: Red (top), White (middle), Blue (bottom)
    display_draw_rectangle(x1, y1, x1 + stripe_width, y1 + stripe_height, RED, true, 1);
    display_draw_rectangle(x1, y1 + stripe_height, x1 + stripe_width, y1 + 2 * stripe_height, WHITE, true, 1);
    display_draw_rectangle(x1, y1 + 2 * stripe_height, x1 + stripe_width, y1 + 3 * stripe_height, BLUE, true, 1);
}

// draw_menu
//
// Draw the menu of the screen. It uses the entries array to create the menu, with the num_entries
// specifying how many entries are in the entries array. The selected parameter is the item in the
// menu that is selected and should be highlighted. Use BACKGROUND_COLOR, FONT_COLOR,
// SELECTED_BG_COLOR, and SELECTED_FONT_COLOR to help specify the colors of the background, font,
// select bar color, and selected text color.
void drawMenu(char entries[][MAX_ENTRY_LENGTH], int num_entries, int selected) {
    display_clear(BACKGROUND_COLOR);

    for (int i = 0; i < num_entries; i++) {
        uint16_t bg = (i == selected) ? SELECTED_BG_COLOR : BACKGROUND_COLOR;
        uint16_t fg = (i == selected) ? SELECTED_FONT_COLOR : FONT_COLOR;

        // Use rectangle as a fill background
        display_draw_rectangle(0, i * 20, 127, i * 20 + 20, bg, true, 1);

        // Draw the text over it
        display_draw_string(5, i * 20, entries[i], &Font12, bg, fg);
    }
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
    
    int selected = 0;
    drawMenu(entries, NUM_ENTRIES, selected);

    while (true) {
        delay_ms(200);

        if (button_up() == 0) {
            selected = (selected - 1 + NUM_ENTRIES) % NUM_ENTRIES;
            drawMenu(entries, NUM_ENTRIES, selected);
            while (button_up() == 0) delay_ms(1);
        }

        if (button_down() == 0) {
            selected = (selected + 1) % NUM_ENTRIES;
            drawMenu(entries, NUM_ENTRIES, selected);
            while (button_down() == 0) delay_ms(1);
        }

        if (button_right() == 0) {
            switch (selected) {
                case 0: clearScreen(); break;
                case 1: drawHelloWorld(); break;
                case 2: drawChars(); break;
                case 3: drawStars(); break;
                case 4: drawFlag(); break;
            }
            delay_ms(2000);
            drawMenu(entries, NUM_ENTRIES, selected);
            while (button_right() == 0) delay_ms(1);
        }

    }

    return 0;
}
