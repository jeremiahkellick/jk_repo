// #jk_build linker_arguments User32.lib Gdi32.lib Winmm.lib

#include <dsound.h>
#include <jk_src/chess/chess.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <xinput.h>

// #jk_build run jk_src/chess/chess_assets_pack.c
// #jk_build build jk_src/chess/chess.c
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

#if JK_BUILD_MODE == JK_RELEASE
#include <jk_gen/chess/assets.c>
#include <jk_src/chess/chess.c>
#include <jk_src/jk_shapes/jk_shapes.c>
#endif

#define FRAME_RATE 60

typedef DWORD (*XInputGetStatePointer)(DWORD dwUserIndex, XINPUT_STATE *pState);
typedef DWORD (*XInputSetStatePointer)(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);

XInputGetStatePointer xinput_get_state;
XInputSetStatePointer xinput_set_state;

typedef HRESULT (*DirectSoundCreatePointer)(
        LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);

typedef struct AudioBufferRegion {
    DWORD size;
    void *data;
} AudioBufferRegion;

typedef enum Key {
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_W,
    KEY_S,
    KEY_A,
    KEY_D,
    KEY_R,
    KEY_C,
    KEY_ENTER,
    KEY_SPACE,
    KEY_ESCAPE,
} Key;

#define AUDIO_DELAY_MS 30

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

typedef struct Shared {
    // Game read-write, AI read-only
    _Alignas(64) AiRequest ai_request;

    // Game read-only, AI read-write
    _Alignas(64) AiResponse ai_response;

    _Alignas(64) SRWLOCK ai_request_lock;

    _Alignas(64) SRWLOCK ai_response_lock;

    _Alignas(64) CONDITION_VARIABLE wants_ai_move;
} Shared;

static Shared g_shared = {
    .ai_request_lock = SRWLOCK_INIT,
    .ai_response_lock = SRWLOCK_INIT,
};

static JkIntVector2 g_window_dimensions;
static Chess g_chess = {0};
static ChessAssets *g_assets;
static JkBuffer g_ai_memory;

static b32 g_running;
static int64_t g_keys_down;
static LPDIRECTSOUNDBUFFER g_audio_buffer;
static AudioSample *g_audio_buffer_tmp;
static char g_string_buffer[1024];

static SRWLOCK g_dll_lock = SRWLOCK_INIT;

static AiInitFunction *g_ai_init = 0;
static AiRunningFunction *g_ai_running = 0;
static UpdateFunction *g_update = 0;
static AudioFunction *g_audio = 0;
static RenderFunction *g_render = 0;

static JkColor window_chess_clear_color = {
    .r = CLEAR_COLOR_R, .g = CLEAR_COLOR_G, .b = CLEAR_COLOR_B, .a = 255};

typedef struct IntArray4 {
    int32_t a[4];
} IntArray4;

static int32_t square_side_length_get(IntArray4 dimensions)
{
    int32_t min_dimension = INT32_MAX;
    for (int32_t i = 0; i < JK_ARRAY_COUNT(dimensions.a); i++) {
        if (dimensions.a[i] < min_dimension) {
            min_dimension = dimensions.a[i];
        }
    }
    return min_dimension / 10;
}

static void update_dimensions(HWND window)
{
    RECT rect;
    GetClientRect(window, &rect);
    g_window_dimensions.x = rect.right - rect.left;
    g_window_dimensions.y = rect.bottom - rect.top;
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
        JK_MIN(g_window_dimensions.x, g_chess.square_side_length * 10),
        JK_MIN(g_window_dimensions.y, g_chess.square_side_length * 10),
    };

    int32_t max_dimension_index = g_window_dimensions.x < g_window_dimensions.y ? 1 : 0;
    result.pos.coords[max_dimension_index] =
            (g_window_dimensions.coords[max_dimension_index]
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
        clear_rect.right = g_window_dimensions.x;
        clear_rect.bottom = g_window_dimensions.y;

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
            g_chess.draw_buffer,
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
        g_running = FALSE;
    } break;

    case WM_SIZE: {
        update_dimensions(window);
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
        int64_t flag = 0;
        switch (wparam) {
        case VK_UP: {
            flag = JK_MASK(KEY_UP);
        } break;

        case VK_DOWN: {
            flag = JK_MASK(KEY_DOWN);
        } break;

        case VK_LEFT: {
            flag = JK_MASK(KEY_LEFT);
        } break;

        case VK_RIGHT: {
            flag = JK_MASK(KEY_RIGHT);
        } break;

        case 'W': {
            flag = JK_MASK(KEY_W);
        } break;

        case 'S': {
            flag = JK_MASK(KEY_S);
        } break;

        case 'A': {
            flag = JK_MASK(KEY_A);
        } break;

        case 'D': {
            flag = JK_MASK(KEY_D);
        } break;

        case 'R': {
            flag = JK_MASK(KEY_R);
        } break;

        case 'C': {
            flag = JK_MASK(KEY_C);
        } break;

        case VK_RETURN: {
            flag = JK_MASK(KEY_ENTER);
        } break;

        case VK_SPACE: {
            flag = JK_MASK(KEY_SPACE);
        } break;

        case VK_ESCAPE: {
            flag = JK_MASK(KEY_ESCAPE);
        } break;

        case VK_F4: {
            if ((lparam >> 29) & 1) { // Alt key is down
                g_running = FALSE;
            }
        } break;

        default: {
        } break;
        }

        if ((lparam >> 31) & 1) { // key is up
            g_keys_down &= ~flag;
        } else { // key is down
            g_keys_down |= flag;
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

typedef enum RecordState {
    RECORD_STATE_NONE,
    RECORD_STATE_RECORDING,
    RECORD_STATE_PLAYING,
    RECORD_STATE_COUNT,
} RecordState;

static RecordState g_record_state;
static uint64_t g_recorded_inputs_count;
static Input g_recorded_inputs[1024];
static Chess g_recorded_game_state;

DWORD game_thread(LPVOID param)
{
    HWND window = (HWND)param;

    uint64_t frequency = jk_platform_os_timer_frequency();
    uint64_t ticks_per_frame = frequency / FRAME_RATE;

    // Set the Windows scheduler granularity to 1ms
    b32 can_sleep = timeBeginPeriod(1) == TIMERR_NOERROR;

    uint32_t audio_buffer_seconds = 2;
    uint32_t audio_buffer_sample_count = SAMPLES_PER_SECOND * audio_buffer_seconds;
    uint32_t audio_buffer_size = audio_buffer_sample_count * sizeof(AudioSample);
    uint32_t audio_position = 0;

    HDC device_context = GetDC(window);
    update_dimensions(window);

    // Initialize DirectSound
    HINSTANCE direct_sound_library = LoadLibraryA("dsound.dll");
    if (direct_sound_library) {
        DirectSoundCreatePointer direct_sound_create =
                (DirectSoundCreatePointer)GetProcAddress(direct_sound_library, "DirectSoundCreate");
        LPDIRECTSOUND direct_sound;
        if (direct_sound_create && (direct_sound_create(0, &direct_sound, 0) == DS_OK)) {
            WAVEFORMATEX wave_format = {
                .wFormatTag = WAVE_FORMAT_PCM,
                .nChannels = AUDIO_CHANNEL_COUNT,
                .nSamplesPerSec = SAMPLES_PER_SECOND,
                .wBitsPerSample = 16,
            };
            wave_format.nBlockAlign = (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
            wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
            if (direct_sound->lpVtbl->SetCooperativeLevel(direct_sound, window, DSSCL_PRIORITY)
                    == DS_OK) {
                DSBUFFERDESC buffer_desciption = {
                    .dwSize = sizeof(buffer_desciption),
                    .dwFlags = DSBCAPS_PRIMARYBUFFER,
                };
                LPDIRECTSOUNDBUFFER primary_buffer;
                if (direct_sound->lpVtbl->CreateSoundBuffer(
                            direct_sound, &buffer_desciption, &primary_buffer, 0)
                        == DS_OK) {
                    if (primary_buffer->lpVtbl->SetFormat(primary_buffer, &wave_format) != DS_OK) {
                        OutputDebugStringA("DirectSound SetFormat on primary buffer failed\n");
                    }
                } else {
                    OutputDebugStringA("Failed to create primary buffer\n");
                }
            } else {
                OutputDebugStringA("DirectSound SetCooperativeLevel failed\n");
            }

            {
                DSBUFFERDESC buffer_description = {
                    .dwSize = sizeof(buffer_description),
                    .dwBufferBytes = audio_buffer_size,
                    .lpwfxFormat = &wave_format,
                };
                if (direct_sound->lpVtbl->CreateSoundBuffer(
                            direct_sound, &buffer_description, &g_audio_buffer, 0)
                        == DS_OK) {
                    g_audio_buffer->lpVtbl->Play(g_audio_buffer, 0, 0, DSBPLAY_LOOPING);
                } else {
                    OutputDebugStringA("Failed to create secondary buffer\n");
                }
            }
        } else {
            OutputDebugStringA("Failed to create DirectSound\n");
        }
    } else {
        OutputDebugStringA("Failed to load DirectSound\n");
    }

#if JK_BUILD_MODE == JK_RELEASE
    g_ai_init = ai_init;
    g_ai_running = ai_running;
    g_update = update;
    g_audio = audio;
    g_render = render;
#else
    HINSTANCE chess_library = 0;
    FILETIME chess_dll_last_modified_time = {0};
#endif

    g_chess.time = 0;
    uint64_t work_time_total = 0;
    uint64_t work_time_min = ULLONG_MAX;
    uint64_t work_time_max = 0;
    uint64_t frame_time_total = 0;
    uint64_t frame_time_min = ULLONG_MAX;
    uint64_t frame_time_max = 0;
    uint64_t counter_previous = jk_platform_os_timer_get();
    uint64_t target_flip_time = counter_previous + ticks_per_frame;
    b32 reset_audio_position = TRUE;
    uint64_t prev_keys = 0;
    while (g_running) {
#if JK_BUILD_MODE != JK_RELEASE
        // Hot reloading
        WIN32_FILE_ATTRIBUTE_DATA chess_dll_info;
        if (GetFileAttributesExA("chess.dll", GetFileExInfoStandard, &chess_dll_info)) {
            if (CompareFileTime(&chess_dll_info.ftLastWriteTime, &chess_dll_last_modified_time) != 0
                    && TryAcquireSRWLockExclusive(&g_dll_lock)) {
                chess_dll_last_modified_time = chess_dll_info.ftLastWriteTime;
                if (chess_library) {
                    g_ai_init = 0;
                    g_ai_running = 0;
                    g_update = 0;
                    g_audio = 0;
                    g_render = 0;
                    FreeLibrary(chess_library);
                }
                if (!CopyFileA("chess.dll", "chess_tmp.dll", FALSE)) {
                    OutputDebugStringA("Failed to copy chess.dll to chess_tmp.dll\n");
                }
                chess_library = LoadLibraryA("chess_tmp.dll");
                if (chess_library) {
                    g_ai_init = (AiInitFunction *)GetProcAddress(chess_library, "ai_init");
                    g_ai_running = (AiRunningFunction *)GetProcAddress(chess_library, "ai_running");
                    g_update = (UpdateFunction *)GetProcAddress(chess_library, "update");
                    g_audio = (AudioFunction *)GetProcAddress(chess_library, "audio");
                    g_render = (RenderFunction *)GetProcAddress(chess_library, "render");
                } else {
                    OutputDebugStringA("Failed to load chess_tmp.dll\n");
                }
                ReleaseSRWLockExclusive(&g_dll_lock);
            }
        } else {
            OutputDebugStringA("Failed to get last modified time of chess.dll\n");
        }
#endif

        g_chess.input.flags = 0;

        // Keyboard input
        int64_t keys_down = g_keys_down;
        {
            g_chess.input.flags |= (((keys_down >> KEY_UP) | (keys_down >> KEY_W)) & 1) << INPUT_UP;
            g_chess.input.flags |= (((keys_down >> KEY_DOWN) | (keys_down >> KEY_S)) & 1)
                    << INPUT_DOWN;
            g_chess.input.flags |= (((keys_down >> KEY_LEFT) | (keys_down >> KEY_A)) & 1)
                    << INPUT_LEFT;
            g_chess.input.flags |= (((keys_down >> KEY_RIGHT) | (keys_down >> KEY_D)) & 1)
                    << INPUT_RIGHT;
            g_chess.input.flags |= (((keys_down >> KEY_ENTER) | (keys_down >> KEY_SPACE)) & 1)
                    << INPUT_CONFIRM;
            g_chess.input.flags |= ((keys_down >> KEY_ESCAPE) & 1) << INPUT_CANCEL;
            g_chess.input.flags |= ((keys_down >> KEY_R) & 1) << INPUT_RESET;
        }

        // Controller input
        if (xinput_get_state) {
            for (int32_t i = 0; i < XUSER_MAX_COUNT; i++) {
                XINPUT_STATE state;
                if (xinput_get_state(i, &state) == ERROR_SUCCESS) {
                    XINPUT_GAMEPAD *pad = &state.Gamepad;
                    g_chess.input.flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) << INPUT_UP;
                    g_chess.input.flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                            << INPUT_DOWN;
                    g_chess.input.flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                            << INPUT_LEFT;
                    g_chess.input.flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                            << INPUT_RIGHT;
                    g_chess.input.flags |= !!(pad->wButtons & XINPUT_GAMEPAD_A) << INPUT_CONFIRM;
                    g_chess.input.flags |= !!(pad->wButtons & XINPUT_GAMEPAD_B) << INPUT_CANCEL;
                }
            }
        }

        g_chess.square_side_length = square_side_length_get((IntArray4){g_window_dimensions.x,
            g_window_dimensions.y,
            DRAW_BUFFER_SIDE_LENGTH,
            DRAW_BUFFER_SIDE_LENGTH});
        Rect draw_rect = draw_rect_get();

        // Mouse input
        {
            POINT mouse_pos;
            if (GetCursorPos(&mouse_pos)) {
                if (ScreenToClient(window, &mouse_pos)) {
                    g_chess.input.mouse_pos.x = mouse_pos.x - draw_rect.pos.x;
                    g_chess.input.mouse_pos.y = mouse_pos.y - draw_rect.pos.y;
                } else {
                    OutputDebugStringA("Failed to get mouse position\n");
                }
            } else {
                OutputDebugStringA("Failed to get mouse position\n");
            }

            g_chess.input.flags |= !!(GetKeyState(VK_LBUTTON) & 0x80) << INPUT_CONFIRM;
            g_chess.input.flags |= !!(GetKeyState(VK_RBUTTON) & 0x80) << INPUT_CANCEL;
        }

        // Find necessary audio sample count
        uint64_t sample_count;
        {
            DWORD play_cursor;
            DWORD write_cursor;
            if (g_audio_buffer->lpVtbl->GetCurrentPosition(
                        g_audio_buffer, &play_cursor, &write_cursor)
                    == DS_OK) {
                uint32_t safe_start_point = (write_cursor
                                                    + ((SAMPLES_PER_SECOND * AUDIO_DELAY_MS) / 1000)
                                                            * sizeof(AudioSample))
                        % audio_buffer_size;
                if (reset_audio_position) {
                    reset_audio_position = FALSE;
                    audio_position = safe_start_point;
                }
                uint32_t end_point = (safe_start_point + SAMPLES_PER_FRAME * sizeof(AudioSample))
                        % audio_buffer_size;
                uint32_t size;
                if (end_point < audio_position) {
                    size = audio_buffer_size - audio_position + end_point;
                } else {
                    size = end_point - audio_position;
                }
                sample_count = size / sizeof(AudioSample);
            } else {
                OutputDebugStringA("Failed to get DirectSound current position\n");
                sample_count = SAMPLES_PER_FRAME * sizeof(AudioSample);
            }
        }

        if (JK_FLAG_GET(keys_down, KEY_C) && !JK_FLAG_GET(prev_keys, KEY_C)) {
            g_record_state = (g_record_state + 1) % RECORD_STATE_COUNT;
            switch (g_record_state) {
            case RECORD_STATE_NONE: {
            } break;

            case RECORD_STATE_RECORDING: {
                g_recorded_game_state = g_chess;
            } break;

            case RECORD_STATE_PLAYING: {
                g_recorded_inputs_count = g_chess.time - g_recorded_game_state.time;
                g_chess = g_recorded_game_state;
            } break;

            case RECORD_STATE_COUNT:
            default: {
                OutputDebugStringA("Invalid RecordState\n");
            } break;
            }
        }

        switch (g_record_state) {
        case RECORD_STATE_NONE: {
        } break;

        case RECORD_STATE_RECORDING: {
            uint64_t i =
                    (g_chess.time - g_recorded_game_state.time) % JK_ARRAY_COUNT(g_recorded_inputs);
            g_recorded_inputs[i] = g_chess.input;
        } break;

        case RECORD_STATE_PLAYING: {
            if ((g_chess.time - g_recorded_game_state.time) > g_recorded_inputs_count) {
                g_chess = g_recorded_game_state;
            }

            uint64_t i =
                    (g_chess.time - g_recorded_game_state.time) % JK_ARRAY_COUNT(g_recorded_inputs);
            g_chess.input = g_recorded_inputs[i];
        } break;

        case RECORD_STATE_COUNT:
        default: {
            OutputDebugStringA("Invalid RecordState\n");
        } break;
        }

        AcquireSRWLockShared(&g_shared.ai_response_lock);
        g_chess.ai_response = g_shared.ai_response;
        ReleaseSRWLockShared(&g_shared.ai_response_lock);

        g_chess.os_time = jk_platform_os_timer_get();

        g_update(g_assets, &g_chess);

        if (!g_shared.ai_request.wants_ai_move
                        != !JK_FLAG_GET(g_chess.flags, CHESS_FLAG_WANTS_AI_MOVE)
                || memcmp(&g_shared.ai_request.board, &g_chess.board, sizeof(Board)) != 0) {
            AcquireSRWLockExclusive(&g_shared.ai_request_lock);
            g_shared.ai_request.wants_ai_move =
                    JK_FLAG_GET(g_chess.flags, CHESS_FLAG_WANTS_AI_MOVE);
            g_shared.ai_request.board = g_chess.board;
            if (g_shared.ai_request.wants_ai_move) {
                WakeAllConditionVariable(&g_shared.wants_ai_move);
            }
            ReleaseSRWLockExclusive(&g_shared.ai_request_lock);
        }

        g_audio(g_assets,
                g_chess.audio_state,
                g_chess.audio_time,
                sample_count,
                g_audio_buffer_tmp);
        g_chess.audio_time += sample_count;

        { // Write audio to the DirectSound buffer
            AudioBufferRegion regions[2] = {0};
            if (g_audio_buffer->lpVtbl->Lock(g_audio_buffer,
                        audio_position,
                        sample_count * sizeof(AudioSample),
                        &regions[0].data,
                        &regions[0].size,
                        &regions[1].data,
                        &regions[1].size,
                        0)
                    == DS_OK) {
                uint64_t buffer_index = 0;
                for (int region_index = 0; region_index < 2; region_index++) {
                    AudioBufferRegion *region = &regions[region_index];

                    AudioSample *region_samples = region->data;
                    JK_ASSERT(region->size % sizeof(region_samples[0]) == 0);
                    for (DWORD region_offset_index = 0;
                            region_offset_index < region->size / sizeof(region_samples[0]);
                            region_offset_index++) {
                        region_samples[region_offset_index] = g_audio_buffer_tmp[buffer_index++];
                    }
                }

                audio_position =
                        (audio_position + sample_count * sizeof(AudioSample)) % audio_buffer_size;

                g_audio_buffer->lpVtbl->Unlock(g_audio_buffer,
                        regions[0].data,
                        regions[0].size,
                        regions[1].data,
                        regions[1].size);
            }
        }

        g_render(g_assets, &g_chess);

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
                reset_audio_position = TRUE;
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
        prev_keys = keys_down;
    }

    snprintf(g_string_buffer,
            JK_ARRAY_COUNT(g_string_buffer),
            "\nWork Time\nMin: %.2fms\nMax: %.2fms\nAvg: %.2fms\n",
            (double)work_time_min / (double)frequency * 1000.0,
            (double)work_time_max / (double)frequency * 1000.0,
            ((double)work_time_total / (double)g_chess.time) / (double)frequency * 1000.0);
    OutputDebugStringA(g_string_buffer);
    snprintf(g_string_buffer,
            JK_ARRAY_COUNT(g_string_buffer),
            "\nFrame Time\nMin: %.2fms\nMax: %.2fms\nAvg: %.2fms\n",
            (double)frame_time_min / (double)frequency * 1000.0,
            (double)frame_time_max / (double)frequency * 1000.0,
            ((double)frame_time_total / (double)g_chess.time) / (double)frequency * 1000.0);
    OutputDebugStringA(g_string_buffer);
    snprintf(g_string_buffer,
            JK_ARRAY_COUNT(g_string_buffer),
            "\nFPS\nMin: %.2f\nMax: %.2f\nAvg: %.2f\n\n",
            (double)frequency / (double)frame_time_min,
            (double)frequency / (double)frame_time_max,
            ((double)g_chess.time / (double)frame_time_total) * (double)frequency);
    OutputDebugStringA(g_string_buffer);

    return 0;
}

DWORD ai_thread(LPVOID param)
{
    while (g_running) {
        AcquireSRWLockShared(&g_shared.ai_request_lock);
        while (!(g_shared.ai_request.wants_ai_move
                && memcmp(&g_shared.ai_request.board, &g_shared.ai_response.board, sizeof(Board))
                        != 0)) {
            SleepConditionVariableSRW(&g_shared.wants_ai_move,
                    &g_shared.ai_request_lock,
                    INFINITE,
                    CONDITION_VARIABLE_LOCKMODE_SHARED);
        }
        Board board = g_shared.ai_request.board;
        ReleaseSRWLockShared(&g_shared.ai_request_lock);

        AcquireSRWLockShared(&g_dll_lock);

        JkArenaRoot arena_root;
        JkArena arena = jk_arena_fixed_init(&arena_root, g_ai_memory);

        Ai ai;
        g_ai_init(&arena,
                &ai,
                board,
                jk_platform_os_timer_get(),
                jk_platform_os_timer_frequency(),
                debug_print);

        while (g_ai_running(&ai)) {
            AcquireSRWLockShared(&g_shared.ai_request_lock);
            b32 cancel = !g_shared.ai_request.wants_ai_move
                    || memcmp(&g_shared.ai_request.board, &ai.response.board, sizeof(Board)) != 0;
            ReleaseSRWLockShared(&g_shared.ai_request_lock);
            if (cancel) {
                break;
            }

            ai.time = jk_platform_os_timer_get();
        }

        ReleaseSRWLockShared(&g_dll_lock);

        if (ai.response.move.src || ai.response.move.dest) {
            AcquireSRWLockExclusive(&g_shared.ai_response_lock);
            g_shared.ai_response = ai.response;
            ReleaseSRWLockExclusive(&g_shared.ai_response_lock);
        }
    }

    return 0;
}

int WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR command_line, int show_code)
{
    jk_platform_set_working_directory_to_executable_directory();

    HINSTANCE xinput_library = LoadLibraryA("xinput1_4.dll");
    if (!xinput_library) {
        xinput_library = LoadLibraryA("xinput9_1_0.dll");
    }
    if (!xinput_library) {
        xinput_library = LoadLibraryA("xinput1_3.dll");
    }
    if (xinput_library) {
        xinput_get_state = (XInputGetStatePointer)GetProcAddress(xinput_library, "XInputGetState");
        xinput_set_state = (XInputSetStatePointer)GetProcAddress(xinput_library, "XInputSetState");
    } else {
        OutputDebugStringA("Failed to load XInput\n");
    }

    uint64_t audio_buffer_size =
            jk_round_up_to_power_of_2(10 * SAMPLES_PER_FRAME * sizeof(AudioSample));
    g_chess.render_memory.size = 64 * JK_MEGABYTE;
    g_ai_memory.size = 8 * JK_GIGABYTE;
    uint8_t *memory = VirtualAlloc(0,
            audio_buffer_size + DRAW_BUFFER_SIZE + g_chess.render_memory.size + g_ai_memory.size,
            MEM_COMMIT,
            PAGE_READWRITE);
    if (!memory) {
        OutputDebugStringA("Failed to allocate memory\n");
    }
    g_audio_buffer_tmp = (AudioSample *)memory;
    memory += audio_buffer_size;
    g_chess.draw_buffer = (JkColor *)memory;
    memory += DRAW_BUFFER_SIZE;
    g_chess.render_memory.data = memory;
    memory += g_chess.render_memory.size;
    g_ai_memory.data = memory;

    g_chess.os_timer_frequency = jk_platform_os_timer_frequency();
    g_chess.debug_print = debug_print;

#if JK_BUILD_MODE == JK_RELEASE
    g_assets = (ChessAssets *)chess_assets_byte_array;
#else
    JkPlatformArenaVirtualRoot arena_root;
    JkArena storage = jk_platform_arena_virtual_init(&arena_root, JK_GIGABYTE);
    if (jk_arena_valid(&storage)) {
        g_assets = (ChessAssets *)jk_platform_file_read_full(&storage, "chess_assets").data;
    } else {
        OutputDebugStringA("Failed to initialize storage arena\n");
    }
#endif

    InitializeConditionVariable(&g_shared.wants_ai_move);

    if (memory) {
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
            HANDLE ai_thread_handle = CreateThread(0, 0, ai_thread, 0, 0, 0);
            if (!ai_thread_handle) {
                OutputDebugStringA("Failed to launch AI thread\n");
            }
            HANDLE game_thread_handle = CreateThread(0, 0, game_thread, window, 0, 0);
            if (game_thread_handle) {
                g_running = TRUE;
                while (g_running) {
                    MSG message;
                    while (g_running && PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
                        if (message.message == WM_QUIT) {
                            g_running = FALSE;
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
