// #jk_build link User32 Gdi32 Winmm

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

#include "bezier.h"
#include "jk_src/chess/chess.h"

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

static char debug_print_buffer[4096];

static int win32_debug_printf(char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsnprintf(debug_print_buffer, JK_ARRAY_COUNT(debug_print_buffer), format, args);
    va_end(args);
    OutputDebugStringA(debug_print_buffer);
    return result;
}

static JkIntVector2 global_window_dimensions;
static Bezier global_bezier = {0};
static ChessAssets *global_assets;

static b32 global_running;

static BezierRenderFunction *global_bezier_render = 0;

static JkColor clear_color = {.r = CLEAR_COLOR_R, .g = CLEAR_COLOR_G, .b = CLEAR_COLOR_B, .a = 255};

typedef struct IntArray4 {
    int32_t a[4];
} IntArray4;

static int32_t draw_square_side_length_get(IntArray4 dimensions)
{
    int32_t min_dimension = INT32_MAX;
    for (int32_t i = 0; i < JK_ARRAY_COUNT(dimensions.a); i++) {
        if (dimensions.a[i] < min_dimension) {
            min_dimension = dimensions.a[i];
        }
    }
    return min_dimension;
}

static void update_dimensions(HWND window)
{
    RECT rect;
    GetClientRect(window, &rect);
    global_window_dimensions.x = rect.right - rect.left;
    global_window_dimensions.y = rect.bottom - rect.top;
}

typedef struct WinRect {
    union {
        struct {
            LONG left;
            LONG top;
            LONG right;
            LONG bottom;
        };
        LONG a[4];
    };
} WinRect;

typedef struct Rect {
    union {
        struct {
            JkIntVector2 pos;
            JkIntVector2 dimensions;
        };
        int32_t a[4];
    };
} Rect;

static Rect draw_rect_get(void)
{
    Rect result = {0};

    result.dimensions = (JkIntVector2){
        JK_MIN(global_window_dimensions.x, global_bezier.draw_square_side_length),
        JK_MIN(global_window_dimensions.y, global_bezier.draw_square_side_length),
    };

    int32_t max_dimension_index = global_window_dimensions.x < global_window_dimensions.y ? 1 : 0;
    result.pos.coords[max_dimension_index] =
            (global_window_dimensions.coords[max_dimension_index]
                    - result.dimensions.coords[max_dimension_index])
            / 2;

    return result;
}

static void copy_draw_buffer_to_window(HWND window, HDC device_context, Rect draw_rect)
{
    HBRUSH brush = CreateSolidBrush(RGB(CLEAR_COLOR_R, CLEAR_COLOR_G, CLEAR_COLOR_B));

    WinRect inverse_rect = {draw_rect.pos.x + draw_rect.dimensions.x,
        draw_rect.pos.y + draw_rect.dimensions.y,
        draw_rect.pos.x,
        draw_rect.pos.y};
    for (int i = 0; i < 4; i++) {
        WinRect clear_rect;
        clear_rect.left = 0;
        clear_rect.top = 0;
        clear_rect.right = global_window_dimensions.x;
        clear_rect.bottom = global_window_dimensions.y;

        clear_rect.a[i] = inverse_rect.a[i];

        FillRect(device_context, (RECT *)&clear_rect, brush);
    }

    DeleteObject(brush);

    BITMAPINFO bitmap_info = {
        .bmiHeader =
                {
                    .biSize = JK_SIZEOF(BITMAPINFOHEADER),
                    .biWidth = DRAW_BUFFER_SIDE_LENGTH,
                    .biHeight = -DRAW_BUFFER_SIDE_LENGTH,
                    .biPlanes = 1,
                    .biBitCount = 32,
                    .biCompression = BI_RGB,
                },
    };
    StretchDIBits(device_context,
            draw_rect.pos.x,
            draw_rect.pos.y,
            draw_rect.dimensions.x,
            draw_rect.dimensions.y,
            0,
            0,
            draw_rect.dimensions.x,
            draw_rect.dimensions.y,
            global_bezier.draw_buffer,
            &bitmap_info,
            DIB_RGB_COLORS,
            SRCCOPY);
}

static LRESULT window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;

    switch (message) {
    case WM_DESTROY:
    case WM_CLOSE: {
        global_running = FALSE;
    } break;

    case WM_SIZE: {
        update_dimensions(window);
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
        switch (wparam) {
        case VK_F4: {
            if ((lparam >> 29) & 1) { // Alt key is down
                global_running = FALSE;
            }
        } break;

        default: {
        } break;
        }
    } break;

    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC device_context = BeginPaint(window, &paint);
        copy_draw_buffer_to_window(window, device_context, draw_rect_get());
        EndPaint(window, &paint);
    } break;

    default: {
        result = DefWindowProcA(window, message, wparam, lparam);
    } break;
    }

    return result;
}

static void debug_print(char *string)
{
    OutputDebugStringA(string);
}

DWORD game_thread(LPVOID param)
{
    HWND window = (HWND)param;

    int64_t frequency = jk_platform_os_timer_frequency();
    int64_t ticks_per_frame = frequency / FRAME_RATE;

    // Set the Windows scheduler granularity to 1ms
    b32 can_sleep = timeBeginPeriod(1) == TIMERR_NOERROR;

    HDC device_context = GetDC(window);
    update_dimensions(window);

    HINSTANCE bezier_library = 0;
    FILETIME bezier_dll_last_modified_time = {0};

    global_bezier.time = 0;
    int64_t work_time_total = 0;
    int64_t work_time_min = LLONG_MAX;
    int64_t work_time_max = LLONG_MIN;
    int64_t frame_time_total = 0;
    int64_t frame_time_min = LLONG_MAX;
    int64_t frame_time_max = LLONG_MIN;
    uint64_t counter_previous = jk_platform_os_timer_get();
    uint64_t target_flip_time = counter_previous + ticks_per_frame;
    while (global_running) {
        // Hot reloading
        WIN32_FILE_ATTRIBUTE_DATA bezier_dll_info;
        if (GetFileAttributesExA("bezier.dll", GetFileExInfoStandard, &bezier_dll_info)) {
            if (CompareFileTime(&bezier_dll_info.ftLastWriteTime, &bezier_dll_last_modified_time)
                    != 0) {
                bezier_dll_last_modified_time = bezier_dll_info.ftLastWriteTime;
                if (bezier_library) {
                    FreeLibrary(bezier_library);
                    global_bezier_render = 0;
                }
                if (!CopyFileA("bezier.dll", "bezier_tmp.dll", FALSE)) {
                    OutputDebugStringA("Failed to copy bezier.dll to bezier_tmp.dll\n");
                }
                bezier_library = LoadLibraryA("bezier_tmp.dll");
                if (bezier_library) {
                    global_bezier_render =
                            (BezierRenderFunction *)GetProcAddress(bezier_library, "bezier_render");
                } else {
                    OutputDebugStringA("Failed to load bezier_tmp.dll\n");
                }
            }
        } else {
            OutputDebugStringA("Failed to get last modified time of bezier.dll\n");
        }

        global_bezier.draw_square_side_length =
                draw_square_side_length_get((IntArray4){global_window_dimensions.x,
                    global_window_dimensions.y,
                    DRAW_BUFFER_SIDE_LENGTH,
                    DRAW_BUFFER_SIDE_LENGTH});
        Rect draw_rect = draw_rect_get();

        global_bezier_render(global_assets, &global_bezier);
        global_bezier.time++;

        uint64_t counter_work = jk_platform_os_timer_get();
        uint64_t counter_current = counter_work;
        int64_t ticks_remaining = target_flip_time - counter_current;
        if (ticks_remaining > 0) {
            do {
                if (can_sleep) {
                    DWORD sleep_ms = (DWORD)((1000 * ticks_remaining) / frequency);
                    if (sleep_ms > 0) {
                        Sleep(sleep_ms);
                    }
                }
                counter_current = jk_platform_os_timer_get();
                ticks_remaining = target_flip_time - counter_current;
            } while (ticks_remaining > 0);
        } else {
            // OutputDebugStringA("Missed a frame\n");

            // If we're off by more than half a frame, give up on catching up
            if (ticks_remaining < -(ticks_per_frame / 2)) {
                target_flip_time = counter_current + ticks_per_frame;
            }
        }

        copy_draw_buffer_to_window(window, device_context, draw_rect);

        int64_t work_time = counter_work - counter_previous;
        work_time_total += work_time;
        if (work_time < work_time_min) {
            work_time_min = work_time;
        }
        if (work_time > work_time_max) {
            work_time_max = work_time;
        }

        int64_t frame_time = counter_current - counter_previous;
        frame_time_total += frame_time;
        if (frame_time < frame_time_min) {
            frame_time_min = frame_time;
        }
        if (frame_time > frame_time_max) {
            frame_time_max = frame_time;
        }

        counter_previous = counter_current;
        target_flip_time += ticks_per_frame;
    }

    win32_debug_printf("\nWork Time\nMin: %.2fms\nMax: %.2fms\nAvg: %.2fms\n",
            (double)work_time_min / (double)frequency * 1000.0,
            (double)work_time_max / (double)frequency * 1000.0,
            ((double)work_time_total / (double)global_bezier.time) / (double)frequency * 1000.0);
    win32_debug_printf("\nFrame Time\nMin: %.2fms\nMax: %.2fms\nAvg: %.2fms\n",
            (double)frame_time_min / (double)frequency * 1000.0,
            (double)frame_time_max / (double)frequency * 1000.0,
            ((double)frame_time_total / (double)global_bezier.time) / (double)frequency * 1000.0);
    win32_debug_printf("\nFPS\nMin: %.2f\nMax: %.2f\nAvg: %.2f\n\n",
            (double)frequency / (double)frame_time_min,
            (double)frequency / (double)frame_time_max,
            ((double)global_bezier.time / (double)frame_time_total) * (double)frequency);

    return 0;
}

int WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR command_line, int show_code)
{
    jk_platform_set_working_directory_to_executable_directory();

    global_bezier.draw_buffer = VirtualAlloc(0,
            JK_SIZEOF(JkColor) * DRAW_BUFFER_SIDE_LENGTH * DRAW_BUFFER_SIDE_LENGTH,
            MEM_COMMIT,
            PAGE_READWRITE);
    global_bezier.cpu_timer_frequency = jk_platform_cpu_timer_frequency_estimate(100);
    global_bezier.cpu_timer_get = jk_platform_cpu_timer_get;
    global_bezier.debug_print = debug_print;

    JkPlatformArenaVirtualRoot arena_root;
    JkArena storage = jk_platform_arena_virtual_init(&arena_root, 5ll * 1024 * 1024 * 1024);

    global_assets = (ChessAssets *)jk_platform_file_read_full(&storage, "chess_assets").data;

    if (global_bezier.draw_buffer) {
        WNDCLASSA window_class = {
            .style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = window_proc,
            .hInstance = instance,
            .lpszClassName = "jk_bezier_window_class",
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
            HANDLE game_thread_handle = CreateThread(0, 0, game_thread, window, 0, 0);
            if (game_thread_handle) {
                global_running = TRUE;
                while (global_running) {
                    MSG message;
                    while (global_running && PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
                        if (message.message == WM_QUIT) {
                            global_running = FALSE;
                        }
                        TranslateMessage(&message);
                        DispatchMessageA(&message);
                    }
                }
                WaitForSingleObject(game_thread_handle, INFINITE);
            } else {
                OutputDebugStringA("Failed to launch game thread\n");
            }
        } else {
            OutputDebugStringA("CreateWindowExA failed\n");
        }
    } else {
        OutputDebugStringA("Failed to allocate memory\n");
    }

    return 0;
}
