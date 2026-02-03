#include <stdbool.h>
#include <stdio.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

#define RADIUS 15
#define GRID_SIZE (2 * RADIUS + 1)
#define ITERATION_COUNT 10000000

static bool screen[GRID_SIZE][GRID_SIZE];

/**
 * Uses Casey Muratori's Efficient DDA Circle Outlines algorithm from
 * https://www.computerenhance.com/p/efficient-dda-circle-outlines
 */
static void draw_circle(bool (*grid)[GRID_SIZE], int r)
{
    int compare = -2 * r + 1;
    int delta_x = 2;
    int delta_y = -4 * r + 4;

    int x = 0;
    int y = r;
    while (x <= y) {
        grid[r + x][r + y] = true;
        grid[r + y][r + x] = true;

        grid[r + x][r - y] = true;
        grid[r + y][r - x] = true;

        grid[r - x][r + y] = true;
        grid[r - y][r + x] = true;

        grid[r - x][r - y] = true;
        grid[r - y][r - x] = true;

        x++;
        compare += delta_x;
        delta_x += 4;

        if (compare > 0) {
            y--;
            compare += delta_y;
            delta_y += 4;
        }
    }
}

static void print_grid(bool (*grid)[GRID_SIZE], int height, int width)
{
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            printf("%s", grid[i][j] ? "██" : "  ");
        }
        printf("\n");
    }
}

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    printf("Drawing a circle %d times\n", ITERATION_COUNT);
    jk_profile_frame_begin();
    for (int i = 0; i < ITERATION_COUNT; i++) {
        draw_circle(screen, RADIUS);
    }
    jk_platform_profile_end_and_print();

    print_grid(screen, GRID_SIZE, GRID_SIZE);

    return 0;
}
