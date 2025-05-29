// #jk_build linker_arguments User32.lib Gdi32.lib Winmm.lib

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

#include "bezier.h"
#include "jk_src/jk_lib/jk_lib.h"
#include "jk_src/jk_shapes/jk_shapes.h"

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

static JkColor clear_color = {CLEAR_COLOR_B, CLEAR_COLOR_G, CLEAR_COLOR_R};

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

static char *piece_string_data[PIECE_COUNT] = {
    "M 32,9.2226562 C 28.491822,9.2250981 25.446533,10.884042 24.488281,14.761719 "
    "23.804009,17.946959 24.60016,20.870505 26.490234,22.148438 19.810658,25.985418 "
    "27.057768,25.543324 28.042969,25.599609 28.042969,25.599609 28.044571,25.812843 "
    "28.050781,25.962891 28.308198,32.248929 23.849484,32.246524 23.126953,43.076172 "
    "18.921981,44.38133 11.312719,50.206349 14.154297,59.197266 26.311624,59.170276 "
    "37.946513,59.194139 49.845703,59.197266 52.687281,50.206349 45.078019,44.38133 "
    "40.873047,43.076172 40.150516,32.246524 35.691802,32.248929 35.949219,25.962891 "
    "35.955429,25.812843 35.957031,25.599609 35.957031,25.599609 36.942232,25.543324 "
    "44.189342,25.985418 37.509766,22.148438 39.39984,20.870505 40.195991,17.946959 "
    "39.511719,14.761719 38.553467,10.884042 35.508178,9.2250981 32,9.2226562 Z",
    "M 8,32 H 26 A 8,16 45 0 0 38,32 H 56 V 56 H 8 Z",
    "M 8,32 H 26 A 8,16 45 0 1 38,32 H 56 V 56 H 8 Z",
    "M 8,32 H 26 A 8,16 45 1 0 38,32 H 56 V 56 H 8 Z",
    "M 8,32 H 26 A 8,16 45 1 1 38,32 H 56 V 56 H 8 Z",
    "M 10,32 A 5,5 0 0 1 54,32 V 54 H 10 Z",
    // "M 55,32 A 22.999999,22.999999 0 0 1 32,55 22.999999,22.999999 0 0 1 9,32 "
    // "22.999999,22.999999 0 0 1 32,9 22.999999,22.999999 0 0 1 55,32",
};

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
    jk_platform_set_working_directory_to_executable_directory();

    global_bezier.draw_buffer = VirtualAlloc(0,
            sizeof(JkColor) * DRAW_BUFFER_SIDE_LENGTH * DRAW_BUFFER_SIDE_LENGTH,
            MEM_COMMIT,
            PAGE_READWRITE);
    global_bezier.arena.memory.size = 1024 * 1024 * 1024;
    global_bezier.arena.memory.data =
            VirtualAlloc(0, global_bezier.arena.memory.size, MEM_COMMIT, PAGE_READWRITE);
    global_bezier.cpu_timer_frequency = jk_platform_cpu_timer_frequency_estimate(100);
    global_bezier.cpu_timer_get = jk_platform_cpu_timer_get;
    global_bezier.debug_print = debug_print;

    JkPlatformArena storage;
    jk_platform_arena_init(&storage, 5llu * 1024 * 1024 * 1024);

    { // Parse shape string
        JkPlatformArena scratch_arena;
        jk_platform_arena_init(&scratch_arena, 5llu * 1024 * 1024 * 1024);

        JkBuffer piece_strings[PIECE_COUNT];
        for (int32_t i = 0; i < PIECE_COUNT; i++) {
            piece_strings[i] = jk_buffer_from_null_terminated(piece_string_data[i]);
        }

        for (int32_t piece_index = 0; piece_index < PIECE_COUNT; piece_index++) {
            JkBuffer piece_string = piece_strings[piece_index];

            global_bezier.shapes[piece_index].dimensions.x = 64.0f;
            global_bezier.shapes[piece_index].dimensions.y = 64.0f;
            global_bezier.shapes[piece_index].commands.items =
                    jk_platform_arena_pointer_get(&storage);

            JkVector2 prev_pos = {0};

            JkVector2 first_pos = {0};
            uint64_t pos = 0;
            int c;
            while ((c = jk_buffer_character_next(piece_string, &pos)) != EOF) {
                switch (c) {
                case 'M':
                case 'L': {
                    FloatArray numbers = parse_numbers(&scratch_arena, piece_string, &pos);
                    JK_ASSERT(numbers.count && numbers.count % 2 == 0);
                    for (int32_t i = 0; i < numbers.count; i += 2) {
                        JkShapesPenCommand *new_command =
                                jk_platform_arena_push_zero(&storage, sizeof(*new_command));
                        new_command->type =
                                c == 'M' ? JK_SHAPES_PEN_COMMAND_MOVE : JK_SHAPES_PEN_COMMAND_LINE;
                        new_command->coords[0] =
                                (JkVector2){numbers.items[i], numbers.items[i + 1]};
                        prev_pos = new_command->coords[0];

                        if (c == 'M') {
                            first_pos = new_command->coords[0];
                        }
                    }
                    scratch_arena.pos = 0;
                } break;

                case 'H':
                case 'V': {
                    FloatArray numbers = parse_numbers(&scratch_arena, piece_string, &pos);
                    JK_ASSERT(numbers.count);
                    for (int32_t i = 0; i < numbers.count; i++) {
                        JkShapesPenCommand *new_command =
                                jk_platform_arena_push_zero(&storage, sizeof(*new_command));
                        new_command->type = JK_SHAPES_PEN_COMMAND_LINE;
                        new_command->coords[0] = c == 'H'
                                ? (JkVector2){numbers.items[i], prev_pos.y}
                                : (JkVector2){prev_pos.x, numbers.items[i]};
                        prev_pos = new_command->coords[0];
                    }
                    scratch_arena.pos = 0;
                } break;

                case 'Q': {
                    FloatArray numbers = parse_numbers(&scratch_arena, piece_string, &pos);
                    JK_ASSERT(numbers.count && numbers.count % 4 == 0);
                    for (int32_t i = 0; i < numbers.count; i += 4) {
                        JkShapesPenCommand *new_command =
                                jk_platform_arena_push_zero(&storage, sizeof(*new_command));
                        new_command->type = JK_SHAPES_PEN_COMMAND_CURVE_QUADRATIC;
                        for (int32_t j = 0; j < 2; j++) {
                            new_command->coords[j] = (JkVector2){
                                numbers.items[i + (j * 2)], numbers.items[i + (j * 2) + 1]};
                        }
                        prev_pos = new_command->coords[1];
                    }
                    scratch_arena.pos = 0;
                } break;

                case 'C': {
                    FloatArray numbers = parse_numbers(&scratch_arena, piece_string, &pos);
                    JK_ASSERT(numbers.count && numbers.count % 6 == 0);
                    for (int32_t i = 0; i < numbers.count; i += 6) {
                        JkShapesPenCommand *new_command =
                                jk_platform_arena_push(&storage, sizeof(*new_command));
                        new_command->type = JK_SHAPES_PEN_COMMAND_CURVE_CUBIC;
                        for (int32_t j = 0; j < 3; j++) {
                            new_command->coords[j] = (JkVector2){
                                numbers.items[i + (j * 2)], numbers.items[i + (j * 2) + 1]};
                        }
                        prev_pos = new_command->coords[2];
                    }
                    scratch_arena.pos = 0;
                } break;

                case 'A': {
                    FloatArray numbers = parse_numbers(&scratch_arena, piece_string, &pos);
                    JK_ASSERT(numbers.count && numbers.count % 7 == 0);
                    for (int32_t i = 0; i < numbers.count; i += 7) {
                        JkShapesPenCommand *new_command =
                                jk_platform_arena_push_zero(&storage, sizeof(*new_command));
                        new_command->type = JK_SHAPES_PEN_COMMAND_ARC;
                        new_command->arc.dimensions.x = numbers.items[i];
                        new_command->arc.dimensions.y = numbers.items[i + 1];
                        new_command->arc.rotation = numbers.items[i + 2] * (float)JK_PI / 180.0f;
                        if (numbers.items[i + 3]) {
                            new_command->arc.flags |= JK_SHAPES_ARC_FLAG_LARGE;
                        }
                        if (numbers.items[i + 4]) {
                            new_command->arc.flags |= JK_SHAPES_ARC_FLAG_SWEEP;
                        }
                        new_command->arc.point_end.x = numbers.items[i + 5];
                        new_command->arc.point_end.y = numbers.items[i + 6];
                        prev_pos = new_command->arc.point_end;
                    }
                    scratch_arena.pos = 0;
                } break;

                case 'Z': {
                    JkShapesPenCommand *new_command =
                            jk_platform_arena_push_zero(&storage, sizeof(*new_command));
                    new_command->type = JK_SHAPES_PEN_COMMAND_LINE;
                    new_command->coords[0] = first_pos;
                } break;

                default: {
                    JK_ASSERT(0 && "Unknown SVG path command character");
                } break;
                }
            }

            global_bezier.shapes[piece_index].commands.count =
                    (JkShapesPenCommand *)jk_platform_arena_pointer_get(&storage)
                    - global_bezier.shapes[piece_index].commands.items;
        }

        jk_platform_arena_terminate(&scratch_arena);
    }

    char *ttf_file_name = "AmiriQuran-Regular.ttf";
    global_bezier.ttf_file = jk_platform_file_read_full(ttf_file_name, &storage);
    if (!global_bezier.ttf_file.size) {
        win32_debug_printf("Failed to read file '%s'\n", ttf_file_name);
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
