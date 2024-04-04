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
#define MAX_ROOT 32

static int int_sqrt_rec(int n, int min, int max)
{
    if (min > max) {
        fprintf(stderr, "int_sqrt: Square root not found\n");
        exit(1);
    }
    int root = min + (max - min) / 2;
    int diff = root * root - n;
    int next_diff = (root + 1) * (root + 1) - n;
    if (diff <= 0 && next_diff > 0) { // Found the root because root^2 <= n and (root + 1)^2 > n
        return -diff < next_diff ? root : root + 1;
    } else if (diff < 0) { // Root is too low
        return int_sqrt_rec(n, root + 1, max);
    } else { // Root is too high
        return int_sqrt_rec(n, min, root - 1);
    }
}

static int int_sqrt(int n)
{
    return int_sqrt_rec(n, 0, MAX_ROOT);
}

static void draw_circle(bool (*grid)[GRID_SIZE], int r)
{
    int y = r;
    int r_squared = r * r;
    for (int x = 0; x <= y; x++) {
        y = int_sqrt(r_squared - x * x);

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
