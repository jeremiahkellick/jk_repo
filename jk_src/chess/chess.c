// #jk_build linker_arguments User32.lib Gdi32.lib

#include <stdint.h>
#include <windows.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

typedef struct Color {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t padding[1];
} Color;
_STATIC_ASSERT(sizeof(Color) == 4);

typedef struct Bitmap {
    Color *memory;
    int width;
    int height;
} Bitmap;

static uint32_t running;
static int time;
static Bitmap window_bitmap;

void update_dimensions(Bitmap *bitmap, HWND window)
{
    RECT rect;
    GetClientRect(window, &rect);
    bitmap->width = rect.right - rect.left;
    bitmap->height = rect.bottom - rect.top;
}

int mod(int a, int b)
{
    int result = a % b;
    return result < 0 ? result + b : result;
}

void draw_pretty_colors(Bitmap bitmap)
{
    int slow_time = time / 2;
    uint8_t darkness = mod(slow_time, 512) < 256 ? (uint8_t)slow_time : 255 - (uint8_t)slow_time;
    for (int y = 0; y < bitmap.height; y++) {
        for (int x = 0; x < bitmap.width; x++) {
            int red;
            int blue;
            if (mod(y, 512) < 256) {
                red = (y & 255) - darkness;
            } else {
                red = 255 - (y & 255) - darkness;
            }
            if (mod(x, 512) < 256) {
                blue = (x & 255) - darkness;
            } else {
                blue = 255 - (x & 255) - darkness;
            }
            if (red < 0) {
                red = 0;
            }
            if (blue < 0) {
                blue = 0;
            }
            bitmap.memory[y * bitmap.width + x].r = (uint8_t)red;
            bitmap.memory[y * bitmap.width + x].b = (uint8_t)blue;
        }
    }
}

void copy_bitmap_to_window(HDC device_context, Bitmap bitmap)
{
    BITMAPINFO bitmap_info = {
        .bmiHeader =
                {
                    .biSize = sizeof(BITMAPINFOHEADER),
                    .biWidth = bitmap.width,
                    .biHeight = -bitmap.height,
                    .biPlanes = 1,
                    .biBitCount = 32,
                    .biCompression = BI_RGB,
                },
    };
    StretchDIBits(device_context,
            0,
            0,
            bitmap.width,
            bitmap.height,
            0,
            0,
            bitmap.width,
            bitmap.height,
            bitmap.memory,
            &bitmap_info,
            DIB_RGB_COLORS,
            SRCCOPY);
}

LRESULT window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;

    switch (message) {
    case WM_DESTROY:
    case WM_CLOSE: {
        running = 0;
    } break;

    case WM_SIZE: {
        update_dimensions(&window_bitmap, window);
    } break;

    case WM_PAINT: {
        draw_pretty_colors(window_bitmap);

        PAINTSTRUCT paint;
        HDC device_context = BeginPaint(window, &paint);
        copy_bitmap_to_window(device_context, window_bitmap);
        EndPaint(window, &paint);
    } break;

    default: {
        result = DefWindowProcA(window, message, wparam, lparam);
    } break;
    }

    return result;
}

int WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR command_line, int show_code)
{
    window_bitmap.memory = VirtualAlloc(0, 512llu * 1024 * 1024, MEM_COMMIT, PAGE_READWRITE);

    if (window_bitmap.memory) {
        WNDCLASSA window_class = {
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = window_proc,
            .hInstance = instance,
            .lpszClassName = "jk_chess_window_class",
        };

        RegisterClassA(&window_class);
        HWND window = CreateWindowExA(0,
                window_class.lpszClassName,
                "Chess",
                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                0,
                0,
                instance,
                0);
        if (window) {
            update_dimensions(&window_bitmap, window);

            MSG message;
            running = 1;
            time = 0;
            while (running) {
                while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        running = false;
                    }
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }

                draw_pretty_colors(window_bitmap);

                HDC device_context = GetDC(window);
                copy_bitmap_to_window(device_context, window_bitmap);
                ReleaseDC(window, device_context);

                time += 1;
            }
        } else {
            OutputDebugStringA("CreateWindowExA failed\n");
        }
    } else {
        OutputDebugStringA("Failed to allocate memory\n");
    }

    return 0;
}
