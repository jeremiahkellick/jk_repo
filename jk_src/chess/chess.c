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

static uint32_t running;
static Color *bitmap;
static int global_width;
static int global_height;
static int x_offset;
static int y_offset;

void update_dimensions(HWND window)
{
    RECT rect;
    GetClientRect(window, &rect);
    global_width = rect.right - rect.left;
    global_height = rect.bottom - rect.top;
}

int mod(int a, int b) {
    int result = a % b;
    return result < 0 ? result + b : result;
}

void draw_pretty_colors(void)
{
    int width = global_width;
    int height = global_height;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            bitmap[y * width + x].r = (uint8_t)(y - y_offset);
            if (mod(y - y_offset, 512) < 256) {
                bitmap[y * width + x].r = 255 - bitmap[y * width + x].r;
            }
            bitmap[y * width + x].b = (uint8_t)(x - x_offset);
            if (mod(x - x_offset, 512) < 256) {
                bitmap[y * width + x].b = 255 - bitmap[y * width + x].b;
            }
        }
    }
}

void update_window(HDC device_context)
{
    int width = global_width;
    int height = global_height;
    BITMAPINFO bitmap_info = {
        .bmiHeader =
                {
                    .biSize = sizeof(BITMAPINFOHEADER),
                    .biWidth = width,
                    .biHeight = -height,
                    .biPlanes = 1,
                    .biBitCount = 32,
                    .biCompression = BI_RGB,
                },
    };
    StretchDIBits(device_context,
            0,
            0,
            width,
            height,
            0,
            0,
            width,
            height,
            bitmap,
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
        update_dimensions(window);
    } break;

    case WM_PAINT: {
        draw_pretty_colors();

        PAINTSTRUCT paint;
        HDC device_context = BeginPaint(window, &paint);
        update_window(device_context);
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
    bitmap = VirtualAlloc(0, 512llu * 1024 * 1024, MEM_COMMIT, PAGE_READWRITE);

    if (bitmap) {
        WNDCLASSA window_class = {
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
            update_dimensions(window);

            MSG message;
            running = 1;
            x_offset = 0;
            y_offset = 0;
            while (running) {
                while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        running = false;
                    }
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }

                draw_pretty_colors();

                HDC device_context = GetDC(window);
                update_window(device_context);
                ReleaseDC(window, device_context);

                x_offset += 1;
                y_offset += 1;
            }
        } else {
            OutputDebugStringA("CreateWindowExA failed\n");
        }
    } else {
        OutputDebugStringA("Failed to allocate memory\n");
    }

    return 0;
}
