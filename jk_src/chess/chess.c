// #jk_build linker_arguments User32.lib Gdi32.lib

#include <stdint.h>
#include <windows.h>
#include <xinput.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

typedef DWORD (*XInputGetStatePointer)(DWORD dwUserIndex, XINPUT_STATE *pState);
typedef DWORD (*XInputSetStatePointer)(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);

XInputGetStatePointer xinput_get_state;
XInputSetStatePointer xinput_set_state;

typedef struct Color {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t padding[1];
} Color;
_STATIC_ASSERT(sizeof(Color) == 4);

typedef struct Bitmap {
    Color *memory;
    int32_t width;
    int32_t height;
} Bitmap;

typedef enum Button {
    BUTTON_DPAD_UP,
    BUTTON_DPAD_DOWN,
    BUTTON_DPAD_LEFT,
    BUTTON_DPAD_RIGHT,
    BUTTON_A,
    BUTTON_B,
    BUTTON_COUNT,
} Button;

#define BUTTON_FLAG_DPAD_UP (1 << BUTTON_DPAD_UP)
#define BUTTON_FLAG_DPAD_DOWN (1 << BUTTON_DPAD_DOWN)
#define BUTTON_FLAG_DPAD_LEFT (1 << BUTTON_DPAD_LEFT)
#define BUTTON_FLAG_DPAD_RIGHT (1 << BUTTON_DPAD_RIGHT)
#define BUTTON_FLAG_A (1 << BUTTON_A)
#define BUTTON_FLAG_B (1 << BUTTON_B)

typedef struct Input {
    int64_t button_flags;
} Input;

static b32 global_running;
static int64_t global_time;
static Bitmap global_bitmap;

void update_dimensions(Bitmap *bitmap, HWND window)
{
    RECT rect;
    GetClientRect(window, &rect);
    bitmap->width = rect.right - rect.left;
    bitmap->height = rect.bottom - rect.top;
}

int64_t mod(int64_t a, int64_t b)
{
    int64_t result = a % b;
    return result < 0 ? result + b : result;
}

void draw_pretty_colors(Bitmap bitmap, int64_t time)
{
    int64_t red_time = time / 2;
    int64_t blue_time = time * 2 / 3;
    uint8_t red_darkness = mod(red_time, 512) < 256 ? (uint8_t)red_time : 255 - (uint8_t)red_time;
    uint8_t blue_darkness =
            mod(blue_time, 512) < 256 ? (uint8_t)blue_time : 255 - (uint8_t)blue_time;
    for (int64_t y = 0; y < bitmap.height; y++) {
        for (int64_t x = 0; x < bitmap.width; x++) {
            int64_t red;
            int64_t blue;
            if (mod(y, 512) < 256) {
                red = (y & 255) - red_darkness;
            } else {
                red = 255 - (y & 255) - red_darkness;
            }
            if (mod(x, 512) < 256) {
                blue = (x & 255) - blue_darkness;
            } else {
                blue = 255 - (x & 255) - blue_darkness;
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
        global_running = 0;
    } break;

    case WM_SIZE: {
        update_dimensions(&global_bitmap, window);
    } break;

    case WM_PAINT: {
        draw_pretty_colors(global_bitmap, global_time);

        PAINTSTRUCT paint;
        HDC device_context = BeginPaint(window, &paint);
        copy_bitmap_to_window(device_context, global_bitmap);
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
    HINSTANCE xinput_library = LoadLibraryA("xinput1_3.dll");
    if (xinput_library) {
        xinput_get_state = (XInputGetStatePointer)GetProcAddress(xinput_library, "XInputGetState");
        xinput_set_state = (XInputSetStatePointer)GetProcAddress(xinput_library, "XInputGetState");
    } else {
        OutputDebugStringA("Failed to load Xinput1_4.dll\n");
    }

    global_bitmap.memory = VirtualAlloc(0, 512llu * 1024 * 1024, MEM_COMMIT, PAGE_READWRITE);

    if (global_bitmap.memory) {
        WNDCLASSA window_class = {
            .style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
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
            HDC device_context = GetDC(window);
            update_dimensions(&global_bitmap, window);

            MSG message;
            global_running = 1;
            global_time = 0;
            while (global_running) {
                while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        global_running = false;
                    }
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }

                Input input = {0};
                if (xinput_get_state) {
                    for (int32_t i = 0; i < XUSER_MAX_COUNT; i++) {
                        XINPUT_STATE state;
                        if (xinput_get_state(i, &state) == ERROR_SUCCESS) {
                            XINPUT_GAMEPAD *pad = &state.Gamepad;
                            input.button_flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                                    << BUTTON_DPAD_UP;
                            input.button_flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                                    << BUTTON_DPAD_DOWN;
                            input.button_flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                                    << BUTTON_DPAD_LEFT;
                            input.button_flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                                    << BUTTON_DPAD_RIGHT;
                            input.button_flags |= !!(pad->wButtons & XINPUT_GAMEPAD_A) << BUTTON_A;
                            input.button_flags |= !!(pad->wButtons & XINPUT_GAMEPAD_B) << BUTTON_B;
                        }
                    }
                }

                if (input.button_flags & BUTTON_FLAG_DPAD_UP) {
                    OutputDebugStringA("DPAD_UP down\n");
                }
                if (input.button_flags & BUTTON_FLAG_DPAD_DOWN) {
                    OutputDebugStringA("DPAD_DOWN down\n");
                }
                if (input.button_flags & BUTTON_FLAG_DPAD_LEFT) {
                    OutputDebugStringA("DPAD_LEFT down\n");
                }
                if (input.button_flags & BUTTON_FLAG_DPAD_RIGHT) {
                    OutputDebugStringA("DPAD_RIGHT down\n");
                }
                if (input.button_flags & BUTTON_FLAG_A) {
                    OutputDebugStringA("A down\n");
                }
                if (input.button_flags & BUTTON_FLAG_B) {
                    OutputDebugStringA("B down\n");
                }

                draw_pretty_colors(global_bitmap, global_time);

                copy_bitmap_to_window(device_context, global_bitmap);

                global_time += 1;
            }
        } else {
            OutputDebugStringA("CreateWindowExA failed\n");
        }
    } else {
        OutputDebugStringA("Failed to allocate memory\n");
    }

    return 0;
}
