#define JK_PLATFORM_DESKTOP_APP 1

// #jk_build link User32 Gdi32 Winmm

#include <windows.h>

// #jk_build run jk_src/pikuma/graphics/graphics_assets_pack.c
// #jk_build build jk_src/pikuma/graphics/graphics.c
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/pikuma/graphics/graphics.h>
// #jk_build dependencies_end

#if JK_BUILD_MODE == JK_RELEASE
#include <jk_gen/pikuma/graphics/assets.c>
#endif

#define FRAME_RATE 60
#define MOUSE_SENSITIVITY 0.6f

typedef struct Global {
    JkPlatformArenaVirtualRoot arena_root;
    JkArena arena;
    Assets *assets;
    RenderFunction *render;
    JkIntVec2 window_dimensions;
    b32 running;
    HCURSOR cursor_arrow;
    State state;

    _Alignas(64) SRWLOCK keyboard_lock;
    _Alignas(64) JkKeyboard keyboard;

    _Alignas(64) SRWLOCK mouse_lock;
    _Alignas(64) JkMouse mouse;
    HCURSOR cursor;
} Global;

static Global g = {.keyboard_lock = SRWLOCK_INIT};
static JkColor clear_color = {.r = CLEAR_COLOR_R, .g = CLEAR_COLOR_G, .b = CLEAR_COLOR_B, .a = 255};

static void update_dimensions(HWND window)
{
    RECT rect;
    GetClientRect(window, &rect);
    g.window_dimensions.x = rect.right - rect.left;
    g.window_dimensions.y = rect.bottom - rect.top;
}

static void copy_draw_buffer_to_window(HWND window, HDC device_context)
{
    HBRUSH brush = CreateSolidBrush(RGB(CLEAR_COLOR_R, CLEAR_COLOR_G, CLEAR_COLOR_B));
    if (g.state.dimensions.x < g.window_dimensions.x) {
        RECT rect = {
            .left = g.state.dimensions.x,
            .right = g.window_dimensions.x,
            .top = 0,
            .bottom = g.window_dimensions.y,
        };
        FillRect(device_context, &rect, brush);
    }
    if (g.state.dimensions.y < g.window_dimensions.y) {
        RECT rect = {
            .left = 0,
            .right = g.window_dimensions.x,
            .top = g.state.dimensions.y,
            .bottom = g.window_dimensions.y,
        };
        FillRect(device_context, &rect, brush);
    }
    DeleteObject(brush);

    BITMAPINFO bitmap_info = {
        .bmiHeader =
                {
                    .biSize = JK_SIZEOF(BITMAPINFOHEADER),
                    .biWidth = DRAW_BUFFER_SIDE_LENGTH,
                    .biHeight = -DRAW_BUFFER_SIDE_LENGTH,
                    .biPlanes = 1,
                    .biBitCount = 24,
                    .biCompression = BI_RGB,
                },
    };
    StretchDIBits(device_context,
            0,
            0,
            g.state.dimensions.x,
            g.state.dimensions.y,
            0,
            0,
            g.state.dimensions.x,
            g.state.dimensions.y,
            g.state.draw_buffer,
            &bitmap_info,
            DIB_RGB_COLORS,
            SRCCOPY);
}

// clang-format off
JkKey make_code_map[] = {
    0x00, 0x29, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x2a, 0x2b,
    0x14, 0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c,
    0x12, 0x13, 0x2f, 0x30, 0x28, 0xe0, 0x04, 0x16,
    0x07, 0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33,
    0x34, 0x35, 0xe1, 0x31, 0x1d, 0x1b, 0x06, 0x19,
    0x05, 0x11, 0x10, 0x36, 0x37, 0x38, 0xe5, 0x55,
    0xe2, 0x2c, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e,
    0x3f, 0x40, 0x41, 0x42, 0x43, 0x53, 0x47, 0x5f,
    0x60, 0x61, 0x56, 0x5c, 0x5d, 0x5e, 0x57, 0x59,
    0x5a, 0x5b, 0x62, 0x63, 0x46, 0x00, 0x64, 0x44,
    0x45, 0x67, 0x00, 0x00, 0x8c, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x68, 0x69, 0x6a, 0x6b,
    0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x00,
    0x88, 0x91, 0x90, 0x87, 0x00, 0x00, 0x73, 0x93,
    0x92, 0x8a, 0x00, 0x8b, 0x00, 0x89, 0x85,
};

JkKey make_code_map_e0[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xb6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xb5, 0x00, 0x00, 0x58, 0xe4, 0x00, 0x00,
    0xe2, 0x00, 0xcd, 0x00, 0xb7, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0x00, 0x46,
    0xe6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x4a,
    0x52, 0x4b, 0x00, 0x50, 0x00, 0x4f, 0x00, 0x4d,
    0x51, 0x4e, 0x49, 0x4c, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xe3, 0xe7, 0x65, 0x81, 0x82,
    0x00, 0x00, 0x00, 0x83,
};
// clang-format on

static LRESULT window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;

    switch (message) {
    case WM_DESTROY:
    case WM_CLOSE: {
        g.running = 0;
    } break;

    case WM_SIZE: {
        update_dimensions(window);
    } break;

    case WM_INPUT: {
        static _Alignas(4) uint8_t raw_input_bytes[1024];
        UINT size = sizeof(raw_input_bytes);
        UINT bytes_written = GetRawInputData(
                (HRAWINPUT)lparam, RID_INPUT, raw_input_bytes, &size, sizeof(RAWINPUTHEADER));
        RAWINPUT *input = (RAWINPUT *)raw_input_bytes;
        if (bytes_written == (UINT)-1) {
            jk_log(JK_LOG_ERROR, JKS("GetRawInputData returned an error\n"));
        } else if (sizeof(raw_input_bytes) < size || sizeof(raw_input_bytes) < bytes_written) {
            jk_log(JK_LOG_ERROR, JKS("Unexpectedly large data returned from GetRawInputData\n"));
        } else if (input->header.dwType == RIM_TYPEKEYBOARD) {
            USHORT make_code = input->data.keyboard.MakeCode;
            JkKey key = JK_KEY_NONE;
            if (input->data.keyboard.Flags & RI_KEY_E1) {
                if (make_code == 0x45) {
                    key = JK_KEY_PAUSE;
                }
            } else if (input->data.keyboard.Flags & RI_KEY_E0) {
                if (make_code < JK_ARRAY_COUNT(make_code_map_e0)) {
                    key = make_code_map_e0[make_code];
                }
            } else {
                if (make_code < JK_ARRAY_COUNT(make_code_map)) {
                    key = make_code_map[make_code];
                }
            }

            if (key) {
                int64_t byte = key / 8;
                uint8_t flag = key % 8;
                b32 released = input->data.keyboard.Flags & RI_KEY_BREAK;

                AcquireSRWLockExclusive(&g.keyboard_lock);
                b32 was_down = JK_FLAG_GET(g.keyboard.down[byte], flag);
                JK_FLAG_SET(g.keyboard.down[byte], flag, !released);
                if (!was_down && !released) {
                    JK_FLAG_SET(g.keyboard.pressed[byte], flag, 1);
                }
                if (was_down && released) {
                    JK_FLAG_SET(g.keyboard.released[byte], flag, 1);
                }
                ReleaseSRWLockExclusive(&g.keyboard_lock);
            }
        } else if (input->header.dwType == RIM_TYPEMOUSE) {
            RAWMOUSE mouse = input->data.mouse;
            if ((mouse.usFlags & 0x3) == MOUSE_MOVE_RELATIVE) {
                AcquireSRWLockExclusive(&g.mouse_lock);
                if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) {
                    JK_FLAG_SET(g.mouse.flags, JK_MOUSE_LEFT_DOWN, 1);
                    JK_FLAG_SET(g.mouse.flags, JK_MOUSE_LEFT_PRESSED, 1);
                }
                if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) {
                    JK_FLAG_SET(g.mouse.flags, JK_MOUSE_LEFT_DOWN, 0);
                    JK_FLAG_SET(g.mouse.flags, JK_MOUSE_LEFT_RELEASED, 1);
                }
                if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
                    JK_FLAG_SET(g.mouse.flags, JK_MOUSE_RIGHT_DOWN, 1);
                    JK_FLAG_SET(g.mouse.flags, JK_MOUSE_RIGHT_PRESSED, 1);
                }
                if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) {
                    JK_FLAG_SET(g.mouse.flags, JK_MOUSE_RIGHT_DOWN, 0);
                    JK_FLAG_SET(g.mouse.flags, JK_MOUSE_RIGHT_RELEASED, 1);
                }
                g.mouse.delta.x += MOUSE_SENSITIVITY * (float)mouse.lLastX;
                g.mouse.delta.y += MOUSE_SENSITIVITY * (float)mouse.lLastY;
                ReleaseSRWLockExclusive(&g.mouse_lock);
            }
        }
    } break;

    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC device_context = BeginPaint(window, &paint);
        copy_draw_buffer_to_window(window, device_context);
        EndPaint(window, &paint);
    } break;

    case WM_SETCURSOR: {
        if (LOWORD(lparam) == HTCLIENT) {
            AcquireSRWLockShared(&g.mouse_lock);
            SetCursor(g.cursor);
            ReleaseSRWLockShared(&g.mouse_lock);
        } else {
            result = DefWindowProcA(window, message, wparam, lparam);
        }
    } break;

    default: {
        result = DefWindowProcA(window, message, wparam, lparam);
    } break;
    }

    return result;
}

DWORD app_thread(LPVOID param)
{
    jk_platform_thread_init();

    HWND window = (HWND)param;

    int64_t frequency = g.state.os_timer_frequency;
    int64_t ticks_per_frame = frequency / FRAME_RATE;

    // Set the Windows scheduler granularity to 1ms
    b32 can_sleep = timeBeginPeriod(1) == TIMERR_NOERROR;

    HDC device_context = GetDC(window);
    update_dimensions(window);

#if JK_BUILD_MODE != JK_RELEASE
    HINSTANCE graphics_library = 0;
    FILETIME graphics_dll_last_modified_time = {0};
#endif

    AcquireSRWLockExclusive(&g.keyboard_lock);
    jk_keyboard_clear(&g.keyboard);
    ReleaseSRWLockExclusive(&g.keyboard_lock);

    AcquireSRWLockExclusive(&g.mouse_lock);
    jk_mouse_clear(&g.mouse);
    ReleaseSRWLockExclusive(&g.mouse_lock);

    uint64_t time = 0;
    int64_t work_time_total = 0;
    int64_t work_time_min = INT64_MAX;
    int64_t work_time_max = INT64_MIN;
    int64_t frame_time_total = 0;
    int64_t frame_time_min = INT64_MAX;
    int64_t frame_time_max = INT64_MIN;
    uint64_t counter_previous = jk_platform_os_timer_get();
    uint64_t target_flip_time = counter_previous + ticks_per_frame;
    b32 capture_mouse = 0;
    while (g.running) {
#if JK_BUILD_MODE != JK_RELEASE
        // Hot reloading
        WIN32_FILE_ATTRIBUTE_DATA graphics_dll_info;
        if (GetFileAttributesExA("graphics.dll", GetFileExInfoStandard, &graphics_dll_info)) {
            if (CompareFileTime(
                        &graphics_dll_info.ftLastWriteTime, &graphics_dll_last_modified_time)
                    != 0) {
                graphics_dll_last_modified_time = graphics_dll_info.ftLastWriteTime;
                if (graphics_library) {
                    FreeLibrary(graphics_library);
                    g.render = 0;
                }
                if (!CopyFileA("graphics.dll", "graphics_tmp.dll", FALSE)) {
                    OutputDebugStringA("Failed to copy graphics.dll to graphics_tmp.dll\n");
                }
                graphics_library = LoadLibraryA("graphics_tmp.dll");
                if (graphics_library) {
                    g.render = (RenderFunction *)GetProcAddress(graphics_library, "render");
                } else {
                    OutputDebugStringA("Failed to load graphics_tmp.dll\n");
                }
            }
        } else {
            OutputDebugStringA("Failed to get last modified time of graphics.dll\n");
        }
#endif

        g.state.dimensions.x = JK_MAX(256, JK_MIN(g.window_dimensions.x, DRAW_BUFFER_SIDE_LENGTH));
        g.state.dimensions.y = JK_MAX(256, JK_MIN(g.window_dimensions.y, DRAW_BUFFER_SIDE_LENGTH));

        g.state.os_time = jk_platform_os_timer_get();

        if (window != GetForegroundWindow()) {
            capture_mouse = 0;
            AcquireSRWLockExclusive(&g.keyboard_lock);
            g.keyboard = (JkKeyboard){0};
            ReleaseSRWLockExclusive(&g.keyboard_lock);
            AcquireSRWLockExclusive(&g.mouse_lock);
            g.mouse = (JkMouse){0};
            ReleaseSRWLockExclusive(&g.mouse_lock);
        }

        AcquireSRWLockExclusive(&g.keyboard_lock);
        g.state.keyboard = g.keyboard;
        jk_keyboard_clear(&g.keyboard);
        ReleaseSRWLockExclusive(&g.keyboard_lock);

        AcquireSRWLockExclusive(&g.mouse_lock);
        g.state.mouse = g.mouse;
        jk_mouse_clear(&g.mouse);
        ReleaseSRWLockExclusive(&g.mouse_lock);

        { // Mouse position
            POINT mouse_pos;
            if (GetCursorPos(&mouse_pos)) {
                if (ScreenToClient(window, &mouse_pos)) {
                    g.state.mouse.position.x = mouse_pos.x;
                    g.state.mouse.position.y = mouse_pos.y;
                } else {
                    jk_log(JK_LOG_ERROR, JKS("Failed to get mouse position\n"));
                }
            } else {
                jk_log(JK_LOG_ERROR, JKS("Failed to get mouse position\n"));
            }
        }

        if ((jk_key_down(&g.state.keyboard, JK_KEY_LEFTALT)
                    || jk_key_down(&g.state.keyboard, JK_KEY_RIGHTALT))
                && jk_key_pressed(&g.state.keyboard, JK_KEY_F4)) {
            g.running = 0;
        }

        int32_t deadzone = 10;
        if (JK_FLAG_GET(g.state.mouse.flags, JK_MOUSE_LEFT_PRESSED)
                && deadzone <= g.state.mouse.position.x
                && g.state.mouse.position.x < (g.state.dimensions.x - deadzone)
                && deadzone <= g.state.mouse.position.y
                && g.state.mouse.position.y < (g.state.dimensions.y - deadzone)) {
            capture_mouse = 1;
        }
        if (jk_key_pressed(&g.state.keyboard, JK_KEY_ESC)) {
            capture_mouse = 0;
        }

        AcquireSRWLockExclusive(&g.mouse_lock);
        g.cursor = capture_mouse ? 0 : g.cursor_arrow;
        ReleaseSRWLockExclusive(&g.mouse_lock);

        if (capture_mouse) {
            RECT rect;
            GetWindowRect(window, &rect);
            LONG center_x = (rect.left + rect.right) / 2;
            LONG center_y = (rect.top + rect.bottom) / 2;
            rect.left = center_x;
            rect.right = center_x;
            rect.top = center_y;
            rect.bottom = center_y;
            ClipCursor(&rect);
        } else {
            g.state.mouse.delta = (JkVec2){0};
            ClipCursor(0);
        }

        g.render(jk_context, g.assets, &g.state);
        time++;

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
            if (ticks_remaining < -((int64_t)ticks_per_frame / 2)) {
                target_flip_time = counter_current + ticks_per_frame;
            }
        }

        copy_draw_buffer_to_window(window, device_context);

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

    JK_LOGF(JK_LOG_INFO,
            jkfn("Work Time\nMin: "),
            jkff((double)work_time_min / (double)frequency * 1000.0, 2),
            jkfn("\nMax: "),
            jkff((double)work_time_max / (double)frequency * 1000.0, 2),
            jkfn("\nAvg: "),
            jkff(((double)work_time_total / (double)time) / (double)frequency * 1000.0, 2),
            jkfn("\n\nFrame Time\nMin: "),
            jkff((double)frame_time_min / (double)frequency * 1000.0, 2),
            jkfn("\nMax: "),
            jkff((double)frame_time_max / (double)frequency * 1000.0, 2),
            jkfn("\nAvg: "),
            jkff(((double)frame_time_total / (double)time) / (double)frequency * 1000.0, 2),
            jkfn("\n\nFPS\nMin: "),
            jkff((double)frequency / (double)frame_time_min, 2),
            jkfn("\nMax: "),
            jkff((double)frequency / (double)frame_time_max, 2),
            jkfn("\nAvg: "),
            jkff(((double)time / (double)frame_time_total) * (double)frequency, 2),
            jkf_nl);

    return 0;
}

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    g.cursor_arrow = LoadCursorA(0, IDC_ARROW);
    g.cursor = g.cursor_arrow;

    g.arena = jk_platform_arena_virtual_init(&g.arena_root, 5ll * 1024 * 1024 * 1024);
    if (!jk_arena_valid(&g.arena)) {
        jk_log(JK_LOG_FATAL, JKS("Failed to initialize virtual memory arena\n"));
        exit(1);
    }

#if JK_BUILD_MODE == JK_RELEASE
    g.assets = (Assets *)assets_byte_array;
    g.render = render;
#else
    g.assets = (Assets *)jk_platform_file_read_full(&g.arena, "graphics_assets").data;
#endif

    g.state.memory.size = 2 * JK_MEGABYTE;
    uint8_t *memory = VirtualAlloc(
            0, DRAW_BUFFER_SIZE + Z_BUFFER_SIZE + g.state.memory.size, MEM_COMMIT, PAGE_READWRITE);
    if (!memory) {
        jk_log(JK_LOG_FATAL, JKS("Failed to allocate memory\n"));
        exit(1);
    }
    g.state.draw_buffer = (JkColor3 *)memory;
    g.state.z_buffer = (float *)(memory + DRAW_BUFFER_SIZE);
    g.state.memory.data = memory + DRAW_BUFFER_SIZE + Z_BUFFER_SIZE;

    g.state.os_timer_frequency = jk_platform_os_timer_frequency();
    g.state.estimate_cpu_frequency = jk_platform_cpu_timer_frequency_estimate;

    WNDCLASSA window_class = {
        .style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = window_proc,
        .hInstance = jk_platform_hinstance,
        .lpszClassName = "jk_graphics_window_class",
    };
    RegisterClassA(&window_class);
    HWND window = CreateWindowExA(0,
            window_class.lpszClassName,
            "Graphics",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            jk_platform_hinstance,
            0);
    if (window) {
        RAWINPUTDEVICE raw_input_devices[] = {
            {
                .usUsagePage = 0x01,
                .usUsage = 0x06,
                .dwFlags = RIDEV_NOLEGACY,
                .hwndTarget = window,
            },
            {
                .usUsagePage = 0x01,
                .usUsage = 0x02,
                .hwndTarget = window,
            },
        };
        if (!RegisterRawInputDevices(raw_input_devices,
                    JK_ARRAY_COUNT(raw_input_devices),
                    sizeof(*raw_input_devices))) {
            jk_log(JK_LOG_WARNING, JKS("Failed to register raw input devices\n"));
        }

        HANDLE app_thread_handle = CreateThread(0, 0, app_thread, window, 0, 0);
        if (app_thread_handle) {
            g.running = 1;
            while (g.running) {
                MSG message;
                while (g.running && PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        g.running = 0;
                    }
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }
            }
            WaitForSingleObject(app_thread_handle, INFINITE);
        } else {
            jk_log(JK_LOG_FATAL, JKS("Failed to launch app thread\n"));
        }
    } else {
        jk_log(JK_LOG_FATAL, JKS("CreateWindowExA failed\n"));
    }

    return 0;
}
