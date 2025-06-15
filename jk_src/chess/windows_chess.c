// #jk_build linker_arguments User32.lib Gdi32.lib Winmm.lib

#include "jk_src/jk_lib/jk_lib.h"
#include <dsound.h>
#include <jk_src/chess/chess.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include <xinput.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

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

// ---- File formats begin -----------------------------------------------------

// ---- File formats end -------------------------------------------------------

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

#define KEY_FLAG_UP (1 << KEY_UP)
#define KEY_FLAG_DOWN (1 << KEY_DOWN)
#define KEY_FLAG_LEFT (1 << KEY_LEFT)
#define KEY_FLAG_RIGHT (1 << KEY_RIGHT)
#define KEY_FLAG_W (1 << KEY_W)
#define KEY_FLAG_S (1 << KEY_S)
#define KEY_FLAG_A (1 << KEY_A)
#define KEY_FLAG_D (1 << KEY_D)
#define KEY_FLAG_R (1 << KEY_R)
#define KEY_FLAG_C (1 << KEY_C)
#define KEY_FLAG_ENTER (1 << KEY_ENTER)
#define KEY_FLAG_SPACE (1 << KEY_SPACE)
#define KEY_FLAG_ESCAPE (1 << KEY_ESCAPE)

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

static JkIntVector2 global_window_dimensions;
static Chess global_chess = {0};
static ChessAssets *global_assets;

static b32 global_running;
static int64_t global_keys_down;
static LPDIRECTSOUNDBUFFER global_audio_buffer;
static char global_string_buffer[1024];

static SRWLOCK global_dll_lock = SRWLOCK_INIT;
static HANDLE global_board_lock;
static SRWLOCK global_move_lock = SRWLOCK_INIT;
static Move global_move = {.src = UINT8_MAX};

static AiMoveGetFunction *global_ai_move_get = 0;
static UpdateFunction *global_update = 0;
static RenderFunction *global_render = 0;
static ProfilePrintFunction *global_profile_print = 0;

static JkColor window_chess_clear_color = {CLEAR_COLOR_B, CLEAR_COLOR_G, CLEAR_COLOR_R};

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
        JK_MIN(global_window_dimensions.x, global_chess.square_side_length * 10),
        JK_MIN(global_window_dimensions.y, global_chess.square_side_length * 10),
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
            global_chess.draw_buffer,
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
        int64_t flag = 0;
        switch (wparam) {
        case VK_UP: {
            flag = KEY_FLAG_UP;
        } break;

        case VK_DOWN: {
            flag = KEY_FLAG_DOWN;
        } break;

        case VK_LEFT: {
            flag = KEY_FLAG_LEFT;
        } break;

        case VK_RIGHT: {
            flag = KEY_FLAG_RIGHT;
        } break;

        case 'W': {
            flag = KEY_FLAG_W;
        } break;

        case 'S': {
            flag = KEY_FLAG_S;
        } break;

        case 'A': {
            flag = KEY_FLAG_A;
        } break;

        case 'D': {
            flag = KEY_FLAG_D;
        } break;

        case 'R': {
            flag = KEY_FLAG_R;
        } break;

        case 'C': {
            flag = KEY_FLAG_C;
        } break;

        case VK_RETURN: {
            flag = KEY_FLAG_ENTER;
        } break;

        case VK_SPACE: {
            flag = KEY_FLAG_SPACE;
        } break;

        case VK_ESCAPE: {
            flag = KEY_FLAG_ESCAPE;
        } break;

        case VK_F4: {
            if ((lparam >> 29) & 1) { // Alt key is down
                global_running = FALSE;
            }
        } break;

        default: {
        } break;
        }

        if ((lparam >> 31) & 1) { // key is up
            global_keys_down &= ~flag;
        } else { // key is down
            global_keys_down |= flag;
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

static RecordState global_record_state;
static uint64_t global_recorded_inputs_count;
static Input global_recorded_inputs[1024];
static Chess global_recorded_game_state;

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
                            direct_sound, &buffer_description, &global_audio_buffer, 0)
                        == DS_OK) {
                    global_audio_buffer->lpVtbl->Play(global_audio_buffer, 0, 0, DSBPLAY_LOOPING);
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

    HINSTANCE chess_library = 0;
    FILETIME chess_dll_last_modified_time = {0};

    global_chess.time = 0;
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
    while (global_running) {
        // Hot reloading
        WIN32_FILE_ATTRIBUTE_DATA chess_dll_info;
        if (GetFileAttributesExA("chess.dll", GetFileExInfoStandard, &chess_dll_info)) {
            if (CompareFileTime(&chess_dll_info.ftLastWriteTime, &chess_dll_last_modified_time)
                    != 0) {
                AcquireSRWLockExclusive(&global_dll_lock);
                chess_dll_last_modified_time = chess_dll_info.ftLastWriteTime;
                if (chess_library) {
                    FreeLibrary(chess_library);
                    global_ai_move_get = 0;
                    global_update = 0;
                    global_render = 0;
                    global_profile_print = 0;
                }
                if (!CopyFileA("chess.dll", "chess_tmp.dll", FALSE)) {
                    OutputDebugStringA("Failed to copy chess.dll to chess_tmp.dll\n");
                }
                chess_library = LoadLibraryA("chess_tmp.dll");
                if (chess_library) {
                    global_ai_move_get =
                            (AiMoveGetFunction *)GetProcAddress(chess_library, "ai_move_get");
                    global_update = (UpdateFunction *)GetProcAddress(chess_library, "update");
                    global_render = (RenderFunction *)GetProcAddress(chess_library, "render");
                    global_profile_print =
                            (ProfilePrintFunction *)GetProcAddress(chess_library, "profile_print");
                } else {
                    OutputDebugStringA("Failed to load chess_tmp.dll\n");
                }
                ReleaseSRWLockExclusive(&global_dll_lock);
            }
        } else {
            OutputDebugStringA("Failed to get last modified time of chess.dll\n");
        }

        global_chess.input.flags = 0;

        // Keyboard input
        {
            global_chess.input.flags |=
                    (((global_keys_down >> KEY_UP) | (global_keys_down >> KEY_W)) & 1) << INPUT_UP;
            global_chess.input.flags |=
                    (((global_keys_down >> KEY_DOWN) | (global_keys_down >> KEY_S)) & 1)
                    << INPUT_DOWN;
            global_chess.input.flags |=
                    (((global_keys_down >> KEY_LEFT) | (global_keys_down >> KEY_A)) & 1)
                    << INPUT_LEFT;
            global_chess.input.flags |=
                    (((global_keys_down >> KEY_RIGHT) | (global_keys_down >> KEY_D)) & 1)
                    << INPUT_RIGHT;
            global_chess.input.flags |=
                    (((global_keys_down >> KEY_ENTER) | (global_keys_down >> KEY_SPACE)) & 1)
                    << INPUT_CONFIRM;
            global_chess.input.flags |= ((global_keys_down >> KEY_ESCAPE) & 1) << INPUT_CANCEL;
            global_chess.input.flags |= ((global_keys_down >> KEY_R) & 1) << INPUT_RESET;
        }

        // Controller input
        if (xinput_get_state) {
            for (int32_t i = 0; i < XUSER_MAX_COUNT; i++) {
                XINPUT_STATE state;
                if (xinput_get_state(i, &state) == ERROR_SUCCESS) {
                    XINPUT_GAMEPAD *pad = &state.Gamepad;
                    global_chess.input.flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                            << INPUT_UP;
                    global_chess.input.flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                            << INPUT_DOWN;
                    global_chess.input.flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                            << INPUT_LEFT;
                    global_chess.input.flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                            << INPUT_RIGHT;
                    global_chess.input.flags |= !!(pad->wButtons & XINPUT_GAMEPAD_A)
                            << INPUT_CONFIRM;
                    global_chess.input.flags |= !!(pad->wButtons & XINPUT_GAMEPAD_B)
                            << INPUT_CANCEL;
                }
            }
        }

        global_chess.square_side_length =
                square_side_length_get((IntArray4){global_window_dimensions.x,
                    global_window_dimensions.y,
                    DRAW_BUFFER_SIDE_LENGTH,
                    DRAW_BUFFER_SIDE_LENGTH});
        Rect draw_rect = draw_rect_get();

        // Mouse input
        {
            POINT mouse_pos;
            if (GetCursorPos(&mouse_pos)) {
                if (ScreenToClient(window, &mouse_pos)) {
                    global_chess.input.mouse_pos.x = mouse_pos.x - draw_rect.pos.x;
                    global_chess.input.mouse_pos.y = mouse_pos.y - draw_rect.pos.y;
                } else {
                    OutputDebugStringA("Failed to get mouse position\n");
                }
            } else {
                OutputDebugStringA("Failed to get mouse position\n");
            }

            global_chess.input.flags |= !!(GetKeyState(VK_LBUTTON) & 0x80) << INPUT_CONFIRM;
            global_chess.input.flags |= !!(GetKeyState(VK_RBUTTON) & 0x80) << INPUT_CANCEL;
        }

        // Find necessary audio sample count
        {
            DWORD play_cursor;
            DWORD write_cursor;
            if (global_audio_buffer->lpVtbl->GetCurrentPosition(
                        global_audio_buffer, &play_cursor, &write_cursor)
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
                global_chess.audio.sample_count = size / sizeof(AudioSample);
            } else {
                OutputDebugStringA("Failed to get DirectSound current position\n");
                global_chess.audio.sample_count = SAMPLES_PER_FRAME * sizeof(AudioSample);
            }
        }

        if ((global_keys_down & KEY_FLAG_C) && !(prev_keys & KEY_FLAG_C)) {
            global_record_state = (global_record_state + 1) % RECORD_STATE_COUNT;
            switch (global_record_state) {
            case RECORD_STATE_NONE: {
            } break;

            case RECORD_STATE_RECORDING: {
                global_recorded_game_state = global_chess;
            } break;

            case RECORD_STATE_PLAYING: {
                global_recorded_inputs_count = global_chess.time - global_recorded_game_state.time;
                global_chess = global_recorded_game_state;
            } break;

            case RECORD_STATE_COUNT:
            default: {
                OutputDebugStringA("Invalid RecordState\n");
            } break;
            }
        }

        switch (global_record_state) {
        case RECORD_STATE_NONE: {
        } break;

        case RECORD_STATE_RECORDING: {
            uint64_t i = (global_chess.time - global_recorded_game_state.time)
                    % JK_ARRAY_COUNT(global_recorded_inputs);
            global_recorded_inputs[i] = global_chess.input;
        } break;

        case RECORD_STATE_PLAYING: {
            if ((global_chess.time - global_recorded_game_state.time)
                    > global_recorded_inputs_count) {
                global_chess = global_recorded_game_state;
            }

            uint64_t i = (global_chess.time - global_recorded_game_state.time)
                    % JK_ARRAY_COUNT(global_recorded_inputs);
            global_chess.input = global_recorded_inputs[i];
        } break;

        case RECORD_STATE_COUNT:
        default: {
            OutputDebugStringA("Invalid RecordState\n");
        } break;
        }

        AcquireSRWLockExclusive(&global_move_lock);
        if (global_move.src < 64) {
            global_chess.ai_move = global_move;
            global_move.src = UINT8_MAX;
        }
        ReleaseSRWLockExclusive(&global_move_lock);

        global_update(global_assets, &global_chess);

        if (global_chess.flags & FLAG_REQUEST_AI_MOVE) {
            global_chess.flags &= ~FLAG_REQUEST_AI_MOVE;
            ReleaseSemaphore(global_board_lock, 1, 0);
        }

        // Write audio to buffer
        {
            AudioBufferRegion regions[2] = {0};
            if (global_audio_buffer->lpVtbl->Lock(global_audio_buffer,
                        audio_position,
                        (DWORD)global_chess.audio.sample_count * sizeof(AudioSample),
                        &regions[0].data,
                        &regions[0].size,
                        &regions[1].data,
                        &regions[1].size,
                        0)
                    == DS_OK) {
                uint32_t buffer_index = 0;
                for (int region_index = 0; region_index < 2; region_index++) {
                    AudioBufferRegion *region = &regions[region_index];

                    AudioSample *region_samples = region->data;
                    JK_ASSERT(region->size % sizeof(region_samples[0]) == 0);
                    for (DWORD region_offset_index = 0;
                            region_offset_index < region->size / sizeof(region_samples[0]);
                            region_offset_index++) {
                        region_samples[region_offset_index] =
                                global_chess.audio.sample_buffer[buffer_index++];
                    }
                }

                audio_position =
                        (audio_position + global_chess.audio.sample_count * sizeof(AudioSample))
                        % audio_buffer_size;

                global_audio_buffer->lpVtbl->Unlock(global_audio_buffer,
                        regions[0].data,
                        regions[0].size,
                        regions[1].data,
                        regions[1].size);
            }
        }

        global_render(global_assets, &global_chess);

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
        prev_keys = global_keys_down;
    }
    global_profile_print(global_chess.debug_print);

    snprintf(global_string_buffer,
            JK_ARRAY_COUNT(global_string_buffer),
            "\nWork Time\nMin: %.2fms\nMax: %.2fms\nAvg: %.2fms\n",
            (double)work_time_min / (double)frequency * 1000.0,
            (double)work_time_max / (double)frequency * 1000.0,
            ((double)work_time_total / (double)global_chess.time) / (double)frequency * 1000.0);
    OutputDebugStringA(global_string_buffer);
    snprintf(global_string_buffer,
            JK_ARRAY_COUNT(global_string_buffer),
            "\nFrame Time\nMin: %.2fms\nMax: %.2fms\nAvg: %.2fms\n",
            (double)frame_time_min / (double)frequency * 1000.0,
            (double)frame_time_max / (double)frequency * 1000.0,
            ((double)frame_time_total / (double)global_chess.time) / (double)frequency * 1000.0);
    OutputDebugStringA(global_string_buffer);
    snprintf(global_string_buffer,
            JK_ARRAY_COUNT(global_string_buffer),
            "\nFPS\nMin: %.2f\nMax: %.2f\nAvg: %.2f\n\n",
            (double)frequency / (double)frame_time_min,
            (double)frequency / (double)frame_time_max,
            ((double)global_chess.time / (double)frame_time_total) * (double)frequency);
    OutputDebugStringA(global_string_buffer);

    return 0;
}

DWORD ai_thread(LPVOID param)
{
    while (global_running) {
        WaitForSingleObject(global_board_lock, INFINITE);

        AcquireSRWLockShared(&global_dll_lock);
        Move move = global_ai_move_get(&global_chess);
        ReleaseSRWLockShared(&global_dll_lock);

        AcquireSRWLockExclusive(&global_move_lock);
        global_move = move;
        ReleaseSRWLockExclusive(&global_move_lock);
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
            jk_round_up_to_power_of_2(SAMPLES_PER_SECOND * 2 * sizeof(AudioSample));
    global_chess.render_memory.size = 64 * JK_MEGABYTE;
    global_chess.ai_memory.size = 8 * JK_GIGABYTE;
    uint8_t *memory = VirtualAlloc(0,
            audio_buffer_size + DRAW_BUFFER_SIZE + global_chess.render_memory.size
                    + global_chess.ai_memory.size,
            MEM_COMMIT,
            PAGE_READWRITE);
    if (!memory) {
        OutputDebugStringA("Failed to allocate memory\n");
    }
    global_chess.audio.sample_buffer = (AudioSample *)memory;
    memory += audio_buffer_size;
    global_chess.draw_buffer = (JkColor *)memory;
    memory += DRAW_BUFFER_SIZE;
    global_chess.render_memory.data = memory;
    memory += global_chess.render_memory.size;
    global_chess.ai_memory.data = memory;

    global_chess.cpu_timer_frequency = jk_platform_cpu_timer_frequency_estimate(100);
    global_chess.cpu_timer_get = jk_platform_cpu_timer_get;
    global_chess.debug_print = debug_print;

    JkPlatformArena storage;
    if (jk_platform_arena_init(&storage, JK_GIGABYTE) == JK_PLATFORM_ARENA_INIT_SUCCESS) {
        global_assets = (ChessAssets *)jk_platform_file_read_full(&storage, "chess_assets").data;
    } else {
        OutputDebugStringA("Failed to initialize storage arena\n");
    }

    global_board_lock = CreateSemaphoreA(0, 0, 1, 0);

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
