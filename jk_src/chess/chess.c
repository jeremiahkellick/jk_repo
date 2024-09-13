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
static BITMAPINFO bitmap_info = {
    .bmiHeader =
            {
                .biSize = sizeof(BITMAPINFOHEADER),
                .biPlanes = 1,
                .biBitCount = 32,
                .biCompression = BI_RGB,
            },
};
static Color *bitmap_memory;
static int bitmap_width;
static int bitmap_height;

static void resize_dib_section(int width, int height)
{
    bitmap_width = width;
    bitmap_height = height;

    bitmap_info.bmiHeader.biWidth = bitmap_width;
    bitmap_info.bmiHeader.biHeight = -bitmap_height;
}

static void update_window(
        HDC device_context, RECT *window_rect, int x, int y, int width, int height)
{
    int window_width = window_rect->right - window_rect->left;
    int window_height = window_rect->bottom - window_rect->top;
    StretchDIBits(device_context,
            /*
            x,
            y,
            width,
            height,
            x,
            y,
            width,
            height,
            */
            0,
            0,
            bitmap_width,
            bitmap_height,
            0,
            0,
            window_width,
            window_height,
            bitmap_memory,
            &bitmap_info,
            DIB_RGB_COLORS,
            SRCCOPY);
}

LRESULT window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;

    switch (message) {
    case WM_SIZE: {
        RECT rect;
        GetClientRect(window, &rect);
        resize_dib_section(rect.right - rect.left, rect.bottom - rect.top);
    } break;

    case WM_DESTROY:
    case WM_CLOSE: {
        running = 0;
    } break;

    case WM_PAINT: {
        for (int y = 0; y < bitmap_height; y++) {
            for (int x = 0; x < bitmap_width; x++) {
                bitmap_memory[y * bitmap_width + x].r = (uint8_t)y;
                bitmap_memory[y * bitmap_width + x].g = (uint8_t)x;
            }
        }

        PAINTSTRUCT paint;
        HDC device_context = BeginPaint(window, &paint);

        RECT rect;
        GetClientRect(window, &rect);

        update_window(device_context,
                &rect,
                paint.rcPaint.left,
                paint.rcPaint.top,
                paint.rcPaint.right - paint.rcPaint.left,
                paint.rcPaint.top - paint.rcPaint.bottom);

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
    bitmap_memory = VirtualAlloc(NULL, 512llu * 1024 * 1024, MEM_COMMIT, PAGE_READWRITE);

    if (bitmap_memory) {
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
