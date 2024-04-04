#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/profile/profile.h>
// #jk_build dependencies_end

#define RADIUS 15
#define GRID_SIZE (2 * RADIUS + 1)

static int int_sqrt(int n)
{
    return (int)sqrt((double)n);
}

static void draw_circle(bool (*grid)[GRID_SIZE], int r)
{
    int y = r;
    for (int x = 0; x <= y; x++) {
        y = int_sqrt(r * r - x * x);

        grid[r + y][r + x] = true;
        grid[r + x][r + y] = true;

        grid[r + y][r - x] = true;
        grid[r + x][r - y] = true;

        grid[r - y][r + x] = true;
        grid[r - x][r + y] = true;

        grid[r - y][r - x] = true;
        grid[r - x][r - y] = true;
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

int main(void)
{
    bool grid[GRID_SIZE][GRID_SIZE];
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            grid[i][j] = false;
        }
    }

    jk_profile_begin();
    for (int i = 0; i < 10000000; i++) {
        draw_circle(grid, RADIUS);
    }
    jk_profile_end_and_print();

    print_grid(grid, GRID_SIZE, GRID_SIZE);

    return 0;
}
