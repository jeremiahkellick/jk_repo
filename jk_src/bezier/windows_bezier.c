// #jk_build linker_arguments User32.lib Gdi32.lib Winmm.lib

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "bezier.h"
#include "jk_src/jk_lib/jk_lib.h"

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

static b32 global_running;

static RenderFunction *global_render = 0;

static Color clear_color = {CLEAR_COLOR_B, CLEAR_COLOR_G, CLEAR_COLOR_R};

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

static Rect draw_rect_get()
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
                    .biSize = sizeof(BITMAPINFOHEADER),
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

    uint64_t frequency = jk_platform_os_timer_frequency();
    uint64_t ticks_per_frame = frequency / FRAME_RATE;

    // Set the Windows scheduler granularity to 1ms
    b32 can_sleep = timeBeginPeriod(1) == TIMERR_NOERROR;

    HDC device_context = GetDC(window);
    update_dimensions(window);

    HINSTANCE bezier_library = 0;
    FILETIME bezier_dll_last_modified_time = {0};

    global_bezier.time = 0;
    uint64_t work_time_total = 0;
    uint64_t work_time_min = ULLONG_MAX;
    uint64_t work_time_max = 0;
    uint64_t frame_time_total = 0;
    uint64_t frame_time_min = ULLONG_MAX;
    uint64_t frame_time_max = 0;
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
                    global_render = 0;
                }
                if (!CopyFileA("bezier.dll", "bezier_tmp.dll", FALSE)) {
                    OutputDebugStringA("Failed to copy bezier.dll to bezier_tmp.dll\n");
                }
                bezier_library = LoadLibraryA("bezier_tmp.dll");
                if (bezier_library) {
                    global_render = (RenderFunction *)GetProcAddress(bezier_library, "render");
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

        global_render(&global_bezier);
        global_bezier.time++;

        uint64_t counter_work = jk_platform_os_timer_get();
        uint64_t counter_current = counter_work;
        int64_t ticks_remaining = (uint64_t)target_flip_time - (uint64_t)counter_current;
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
            if (ticks_remaining < -((int64_t)ticks_per_frame / 2)) {
                target_flip_time = counter_current + ticks_per_frame;
            }
        }

        copy_draw_buffer_to_window(window, device_context, draw_rect);

        uint64_t work_time = counter_work - counter_previous;
        work_time_total += work_time;
        if (work_time < work_time_min) {
            work_time_min = work_time;
        }
        if (work_time > work_time_max) {
            work_time_max = work_time;
        }

        uint64_t frame_time = counter_current - counter_previous;
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

static char *shape_string_data =
        "M 32,9.2226562 C 28.491821,9.2226562 25.446533,10.884042 24.488281,14.761719 "
        "23.804009,17.946959 24.60016,20.870505 26.490234,22.148438 19.810658,25.985418 "
        "27.057768,25.543324 28.042969,25.599609 L 28.050781,25.962891 C 28.308198,32.248929 "
        "23.849484,32.246524 23.126953,43.076172 18.921981,44.38133 11.312719,50.206349 "
        "14.154297,59.197266 H 49.845703 C 52.687281,50.206349 45.078019,44.38133 "
        "40.873047,43.076172 40.150516,32.246524 35.691802,32.248929 35.949219,25.962891 L "
        "35.957031,25.599609 C 36.942232,25.543324 44.189342,25.985418 37.509766,22.148438 "
        "39.39984,20.870505 40.195991,17.946959 39.511719,14.761719 38.553467,10.884042 "
        "35.508179,9.2226562 32,9.2226562";

static char *b_string_data =
        "M 39,92 C 54.333333,92.666667 74.166667,93.333333 98.5,94 122.83333,94.666667 147,95 "
        "171,95 203,95 233.5,94.666667 262.5,94 291.5,93.333333 312,93 324,93 398.66667,93 "
        "454.5,107.66667 491.5,137 528.5,166.33333 547,204 547,250 547,273.33333 541.5,296.5 "
        "530.5,319.5 519.5,342.5 501.66667,363.16667 477,381.5 452.33333,399.83333 "
        "419.66667,414.33333 379,425 V 427 C 435,433 479,445 511,463 543,481 565.66667,502.5 "
        "579,527.5 592.33333,552.5 599,578.66667 599,606 599,664.66667 576.83333,711.66667 "
        "532.5,747 488.16667,782.33333 426.33333,800 347,800 332.33333,800 310.16667,799.5 "
        "280.5,798.5 250.83333,797.5 215,797 173,797 147.66667,797 122.83333,797.16667 98.5,797.5 "
        "74.166667,797.83333 54.333333,798.66667 39,800 V 780 C 61.666667,778.66667 78.666667,776 "
        "90,772 101.33333,768 108.83333,760 112.5,748 116.16667,736 118,718 118,694 V 198 C "
        "118,173.33333 116.16667,155.16667 112.5,143.5 108.83333,131.83333 101.16667,123.83333 "
        "89.5,119.5 77.833333,115.16667 61,112.66667 39,112 Z M 303,112 C 274.33333,112 "
        "255.5,117.66667 246.5,129 237.5,140.33333 233,163.33333 233,198 V 694 C 233,728.66667 "
        "237.66667,751.16667 247,761.5 256.33333,771.83333 276,777 306,777 366,777 409.5,761.83333 "
        "436.5,731.5 463.5,701.16667 477,658 477,602 477,550.66667 463.5,511 436.5,483 409.5,455 "
        "365,441 303,441 H 206 V 432.5 424 H 292 C 328.66667,424 357,416.5 377,401.5 397,386.5 "
        "410.66667,366.5 418,341.5 425.33333,316.5 429,289.66667 429,261 429,211.66667 "
        "419.33333,174.5 400,149.5 380.66667,124.5 348.33333,112 303,112 Z";

typedef struct FloatArray {
    uint64_t count;
    float *items;
} FloatArray;

static FloatArray parse_numbers(JkPlatformArena *arena, JkBuffer shape_string, uint64_t *pos)
{
    FloatArray result = {.items = jk_platform_arena_pointer_get(arena)};
    int c;
    while ((c = jk_buffer_character_next(shape_string, pos)) != EOF
            && (isdigit(c) || isspace(c) || c == ',')) {
        if (isdigit(c)) {
            uint64_t start = *pos - 1;
            do {
                c = jk_buffer_character_next(shape_string, pos);
            } while (isdigit(c) || c == '.');
            JkBuffer number_string = {
                .size = (*pos - 1) - start,
                .data = shape_string.data + start,
            };
            float *new_number = jk_platform_arena_push(arena, sizeof(*new_number));
            *new_number = (float)jk_parse_double(number_string);
        }
    }
    result.count = (float *)jk_platform_arena_pointer_get(arena) - result.items;
    if (c != EOF) {
        (*pos)--;
    }
    return result;
}

int WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR command_line, int show_code)
{
    global_bezier.draw_buffer = VirtualAlloc(0,
            sizeof(Color) * DRAW_BUFFER_SIDE_LENGTH * DRAW_BUFFER_SIDE_LENGTH,
            MEM_COMMIT,
            PAGE_READWRITE);
    global_bezier.cpu_timer_frequency = jk_platform_cpu_timer_frequency_estimate(100);
    global_bezier.cpu_timer_get = jk_platform_cpu_timer_get;
    global_bezier.debug_print = debug_print;

    JkPlatformArena storage;
    jk_platform_arena_init(&storage, 5llu * 1024 * 1024 * 1024);

    { // Parse shape string
        JkPlatformArena scratch_arena;
        jk_platform_arena_init(&scratch_arena, 5llu * 1024 * 1024 * 1024);

        JkBuffer shape_string = {
            .size = strlen(b_string_data),
            .data = (uint8_t *)b_string_data,
        };

        PenCommandArray shape = {.items = jk_platform_arena_pointer_get(&storage)};
        JkVector2 prev_pos = {0};

        JkVector2 first_pos = {0};
        uint64_t pos = 0;
        int c;
        while ((c = jk_buffer_character_next(shape_string, &pos)) != EOF) {
            switch (c) {
            case 'M':
            case 'L': {
                FloatArray numbers = parse_numbers(&scratch_arena, shape_string, &pos);
                JK_ASSERT(numbers.count && numbers.count % 2 == 0);
                for (int32_t i = 0; i < numbers.count; i += 2) {
                    PenCommand *new_command =
                            jk_platform_arena_push_zero(&storage, sizeof(*new_command));
                    new_command->type = c == 'M' ? PEN_COMMAND_MOVE : PEN_COMMAND_LINE;
                    new_command->coords[0] = (JkVector2){numbers.items[i], numbers.items[i + 1]};
                    prev_pos = new_command->coords[0];

                    if (c == 'M') {
                        first_pos = new_command->coords[0];
                    }
                }
                scratch_arena.pos = 0;
            } break;

            case 'H':
            case 'V': {
                FloatArray numbers = parse_numbers(&scratch_arena, shape_string, &pos);
                JK_ASSERT(numbers.count);
                for (int32_t i = 0; i < numbers.count; i++) {
                    PenCommand *new_command =
                            jk_platform_arena_push_zero(&storage, sizeof(*new_command));
                    new_command->type = PEN_COMMAND_LINE;
                    new_command->coords[0] = c == 'H' ? (JkVector2){numbers.items[i], prev_pos.y}
                                                      : (JkVector2){prev_pos.x, numbers.items[i]};
                    prev_pos = new_command->coords[0];
                }
                scratch_arena.pos = 0;
            } break;

            case 'C': {
                FloatArray numbers = parse_numbers(&scratch_arena, shape_string, &pos);
                JK_ASSERT(numbers.count && numbers.count % 6 == 0);
                for (int32_t i = 0; i < numbers.count; i += 6) {
                    PenCommand *new_command =
                            jk_platform_arena_push(&storage, sizeof(*new_command));
                    new_command->type = PEN_COMMAND_CURVE;
                    for (int32_t j = 0; j < 3; j++) {
                        new_command->coords[j] = (JkVector2){
                            numbers.items[i + (j * 2)], numbers.items[i + (j * 2) + 1]};
                    }
                    prev_pos = new_command->coords[2];
                }
                scratch_arena.pos = 0;
            } break;

            case 'Z': {
                PenCommand *new_command =
                        jk_platform_arena_push_zero(&storage, sizeof(*new_command));
                new_command->type = PEN_COMMAND_LINE;
                new_command->coords[0] = first_pos;
            } break;

            default: {
            } break;
            }
        }

        shape.count = (PenCommand *)jk_platform_arena_pointer_get(&storage) - shape.items;

        global_bezier.shape = shape;

        jk_platform_arena_terminate(&scratch_arena);
    }

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
