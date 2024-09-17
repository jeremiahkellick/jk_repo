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

LRESULT window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;

    switch (message) {
    case WM_DESTROY:
    case WM_CLOSE: {
        running = 0;
    } break;

    case WM_PAINT: {
        RECT rect;
        GetClientRect(window, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                bitmap[y * width + x].r = (uint8_t)y;
                bitmap[y * width + x].g = (uint8_t)x;
            }
        }

        PAINTSTRUCT paint;
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
        HDC device_context = BeginPaint(window, &paint);
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
    bitmap = VirtualAlloc(NULL, 512llu * 1024 * 1024, MEM_COMMIT, PAGE_READWRITE);

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
            MSG message;
            running = 1;
            while (running) {
                if (GetMessageA(&message, 0, 0, 0) > 0) {
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                } else {
                    running = 0;
                }
            }
        } else {
            OutputDebugStringA("CreateWindowExA failed\n");
        }
    } else {
        OutputDebugStringA("Failed to allocate memory\n");
    }

    return 0;
}
