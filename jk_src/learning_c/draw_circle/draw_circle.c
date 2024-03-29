#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define RADIUS 15
#define GRID_SIZE (2 * RADIUS + 1)

int int_sqrt(int n)
{
    return (int)sqrt((double)n);
}

void draw_circle(bool (*grid)[GRID_SIZE], int r)
{
    int prev_y = 0;
    int r_squared = r * r;
    for (int x = 0; x < r + 1; x++) {
        int x_minus_r = x - r;
        int x_minus_r_squared = x_minus_r * x_minus_r;
        int y = int_sqrt(r_squared - x_minus_r_squared);

        int y_diff = y - prev_y;
        for (int i = 0; i < (y_diff - 1); i++) {
            int fill_y = prev_y + i + 1;
            grid[r + fill_y][x] = true;
            grid[r - fill_y][x] = true;
            grid[r + fill_y][2 * r - x] = true;
            grid[r - fill_y][2 * r - x] = true;
        }
        prev_y = y;

        grid[r + y][x] = true;
        grid[r - y][x] = true;
        grid[r + y][2 * r - x] = true;
        grid[r - y][2 * r - x] = true;
    }
}

void print_grid(bool (*grid)[GRID_SIZE], int height, int width)
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

    draw_circle(grid, RADIUS);

    print_grid(grid, GRID_SIZE, GRID_SIZE);

    return 0;
}
