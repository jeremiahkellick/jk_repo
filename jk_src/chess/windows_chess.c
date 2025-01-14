// #jk_build linker_arguments User32.lib Gdi32.lib Winmm.lib

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

#define PI 3.14159265358979323846

static uint32_t lost_woods[] = {
    349, // F
    440, // A
    494, // B
    494, // B
    349, // F
    440, // A
    494, // B
    494, // B

    349, // F
    440, // A
    494, // B
    659, // E
    587, // D
    587, // D
    494, // B
    523, // C

    494, // B
    392, // G
    330, // Low E
    330, // Low E
    330, // Low E
    330, // Low E
    330, // Low E
    294, // Low D

    330, // Low E
    392, // G
    330, // Low E
    330, // Low E
    330, // Low E
    330, // Low E
    330, // Low E
    330, // Low E
};

#define BPM 140.0

typedef enum InputId {
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_CONFIRM,
    INPUT_CANCEL,
} InputId;

#define INPUT_FLAG_UP (1 << INPUT_UP)
#define INPUT_FLAG_DOWN (1 << INPUT_DOWN)
#define INPUT_FLAG_LEFT (1 << INPUT_LEFT)
#define INPUT_FLAG_RIGHT (1 << INPUT_RIGHT)
#define INPUT_FLAG_CONFIRM (1 << INPUT_CONFIRM)
#define INPUT_FLAG_CANCEL (1 << INPUT_CANCEL)

typedef struct Input {
    int64_t flags;
} Input;

typedef struct Color {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t padding[1];
} Color;
_STATIC_ASSERT(sizeof(Color) == 4);

typedef enum AudioChannel {
    AUDIO_CHANNEL_LEFT,
    AUDIO_CHANNEL_RIGHT,
    AUDIO_CHANNEL_COUNT,
} AudioChannel;

typedef struct AudioSample {
    int16_t channels[AUDIO_CHANNEL_COUNT];
} AudioSample;

typedef struct Audio {
    uint32_t samples_per_second;
    uint32_t sample_count;
    AudioSample *sample_buffer;
    uint32_t audio_time;
    double sin_t;
} Audio;

typedef struct Bitmap {
    Color *memory;
    int32_t width;
    int32_t height;
} Bitmap;

typedef struct Chess {
    Input input;
    Audio audio;
    Bitmap bitmap;
    int64_t time;
    int64_t x;
    int64_t y;
} Chess;

static int64_t mod(int64_t a, int64_t b)
{
    int64_t result = a % b;
    return result < 0 ? result + b : result;
}

static int32_t audio_samples_per_eighth_note(uint32_t samples_per_second)
{
    double eighth_notes_per_second = (BPM / 60.0) * 2.0;
    return (uint32_t)((double)samples_per_second / eighth_notes_per_second);
}

static void audio_write(Audio *audio, int32_t pitch_multiplier)
{
    for (uint32_t sample_index = 0; sample_index < audio->sample_count; sample_index++) {
        uint32_t eighth_note_index =
                (audio->audio_time / audio_samples_per_eighth_note(audio->samples_per_second))
                % JK_ARRAY_COUNT(lost_woods);

        double x = (double)audio->audio_time
                / audio_samples_per_eighth_note(audio->samples_per_second);
        // Number from 0.0 to 2.0 based on how far into the current
        // eighth note we are
        double note_progress = (x - floor(x)) * 2.0;
        double fade_factor = 1.0;
        if (note_progress < 1.0) {
            if (lost_woods[eighth_note_index]
                    != lost_woods[eighth_note_index == 0 ? JK_ARRAY_COUNT(lost_woods) - 1
                                                         : eighth_note_index - 1]) {
                fade_factor = note_progress;
            }
        } else {
            if (lost_woods[eighth_note_index]
                    != lost_woods[(eighth_note_index + 1) % JK_ARRAY_COUNT(lost_woods)]) {
                fade_factor = 2.0 - note_progress;
            }
        }

        uint32_t hz = lost_woods[eighth_note_index] * pitch_multiplier;
        audio->sin_t += 2.0 * PI * ((double)hz / (double)audio->samples_per_second);
        if (audio->sin_t > 2.0 * PI) {
            audio->sin_t -= 2.0 * PI;
        }
        int16_t value = (int16_t)(sin(audio->sin_t) * fade_factor * 2000.0);
        for (int channel_index = 0; channel_index < AUDIO_CHANNEL_COUNT; channel_index++) {
            audio->sample_buffer[sample_index].channels[channel_index] = value;
        }
        audio->audio_time++;
    }
}

static void update(Chess *chess)
{
    if (chess->input.flags & INPUT_FLAG_UP) {
        chess->y -= 4;
    }
    if (chess->input.flags & INPUT_FLAG_DOWN) {
        chess->y += 4;
    }
    if (chess->input.flags & INPUT_FLAG_LEFT) {
        chess->x -= 4;
    }
    if (chess->input.flags & INPUT_FLAG_RIGHT) {
        chess->x += 4;
    }

    audio_write(&chess->audio, (chess->input.flags & INPUT_FLAG_UP) ? 2 : 1);

    chess->time++;
}

static void render(Chess *chess)
{
    int64_t red_time = chess->time;
    int64_t blue_time = chess->time * 4 / 3;
    uint8_t red_darkness = mod(red_time, 512) < 256 ? (uint8_t)red_time : 255 - (uint8_t)red_time;
    uint8_t blue_darkness =
            mod(blue_time, 512) < 256 ? (uint8_t)blue_time : 255 - (uint8_t)blue_time;
    for (int64_t screen_y = 0; screen_y < chess->bitmap.height; screen_y++) {
        for (int64_t screen_x = 0; screen_x < chess->bitmap.width; screen_x++) {
            int64_t world_y = screen_y + chess->y;
            int64_t world_x = screen_x + chess->x;
            int64_t red;
            int64_t blue;
            if (mod(world_y, 512) < 256) {
                red = (world_y & 255) - red_darkness;
            } else {
                red = 255 - (world_y & 255) - red_darkness;
            }
            if (mod(world_x, 512) < 256) {
                blue = (world_x & 255) - blue_darkness;
            } else {
                blue = 255 - (world_x & 255) - blue_darkness;
            }
            if (red < 0) {
                red = 0;
            }
            if (blue < 0) {
                blue = 0;
            }
            chess->bitmap.memory[screen_y * chess->bitmap.width + screen_x].r = (uint8_t)red;
            chess->bitmap.memory[screen_y * chess->bitmap.width + screen_x].b = (uint8_t)blue;
        }
    }
}

#ifdef _WIN32
// ---- Windows begin ----------------------------------------------------------

#include <dsound.h>
#include <windows.h>
#include <xinput.h>

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
#define KEY_FLAG_ENTER (1 << KEY_ENTER)
#define KEY_FLAG_SPACE (1 << KEY_SPACE)
#define KEY_FLAG_ESCAPE (1 << KEY_ESCAPE)

#define SAMPLES_PER_SECOND 48000
#define FRAME_RATE 60
#define SAMPLES_PER_FRAME (SAMPLES_PER_SECOND / FRAME_RATE)
#define AUDIO_DELAY_MS 30

typedef struct Memory {
    AudioSample audio[SAMPLES_PER_SECOND * 2 * sizeof(AudioSample)];
    uint8_t video[512llu * 1024 * 1024];
} Memory;

static Chess global_chess = {
    .audio.samples_per_second = SAMPLES_PER_SECOND,
};

static b32 global_running;
static int64_t global_keys_down;
static LPDIRECTSOUNDBUFFER global_audio_buffer;
static char global_string_buffer[1024];

static void update_dimensions(Bitmap *bitmap, HWND window)
{
    RECT rect;
    GetClientRect(window, &rect);
    bitmap->width = rect.right - rect.left;
    bitmap->height = rect.bottom - rect.top;
}

static void copy_bitmap_to_window(HDC device_context, Bitmap bitmap)
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

static LRESULT window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;

    switch (message) {
    case WM_DESTROY:
    case WM_CLOSE: {
        global_running = FALSE;
    } break;

    case WM_SIZE: {
        update_dimensions(&global_chess.bitmap, window);
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
        render(&global_chess);

        PAINTSTRUCT paint;
        HDC device_context = BeginPaint(window, &paint);
        copy_bitmap_to_window(device_context, global_chess.bitmap);
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
    uint64_t frequency = jk_platform_os_timer_frequency();
    uint64_t ticks_per_frame = frequency / FRAME_RATE;

    // Set the Windows scheduler granularity to 1ms
    timeBeginPeriod(1);
    b32 can_sleep = timeBeginPeriod(1) == TIMERR_NOERROR;

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

    Memory *memory = VirtualAlloc(0, sizeof(Memory), MEM_COMMIT, PAGE_READWRITE);
    global_chess.audio.sample_buffer = memory->audio;
    global_chess.bitmap.memory = (Color *)memory->video;

    if (global_chess.bitmap.memory) {
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
            uint32_t audio_buffer_seconds = 2;
            uint32_t audio_buffer_sample_count =
                    global_chess.audio.samples_per_second * audio_buffer_seconds;
            uint32_t audio_buffer_size = audio_buffer_sample_count * sizeof(AudioSample);
            uint32_t audio_position = 0;

            HDC device_context = GetDC(window);
            update_dimensions(&global_chess.bitmap, window);

            // Initialize DirectSound
            HINSTANCE direct_sound_library = LoadLibraryA("dsound.dll");
            if (direct_sound_library) {
                DirectSoundCreatePointer direct_sound_create =
                        (DirectSoundCreatePointer)GetProcAddress(
                                direct_sound_library, "DirectSoundCreate");
                LPDIRECTSOUND direct_sound;
                if (direct_sound_create && (direct_sound_create(0, &direct_sound, 0) == DS_OK)) {
                    WAVEFORMATEX wave_format = {
                        .wFormatTag = WAVE_FORMAT_PCM,
                        .nChannels = AUDIO_CHANNEL_COUNT,
                        .nSamplesPerSec = global_chess.audio.samples_per_second,
                        .wBitsPerSample = 16,
                    };
                    wave_format.nBlockAlign =
                            (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
                    wave_format.nAvgBytesPerSec =
                            wave_format.nSamplesPerSec * wave_format.nBlockAlign;
                    if (direct_sound->lpVtbl->SetCooperativeLevel(
                                direct_sound, window, DSSCL_PRIORITY)
                            == DS_OK) {
                        DSBUFFERDESC buffer_desciption = {
                            .dwSize = sizeof(buffer_desciption),
                            .dwFlags = DSBCAPS_PRIMARYBUFFER,
                        };
                        LPDIRECTSOUNDBUFFER primary_buffer;
                        if (direct_sound->lpVtbl->CreateSoundBuffer(
                                    direct_sound, &buffer_desciption, &primary_buffer, 0)
                                == DS_OK) {
                            if (primary_buffer->lpVtbl->SetFormat(primary_buffer, &wave_format)
                                    != DS_OK) {
                                OutputDebugStringA(
                                        "DirectSound SetFormat on primary buffer failed\n");
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
                            global_audio_buffer->lpVtbl->Play(
                                    global_audio_buffer, 0, 0, DSBPLAY_LOOPING);
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

            global_chess.time = 0;
            global_running = TRUE;
            uint64_t work_time_total = 0;
            uint64_t work_time_min = ULLONG_MAX;
            uint64_t work_time_max = 0;
            uint64_t frame_time_total = 0;
            uint64_t frame_time_min = ULLONG_MAX;
            uint64_t frame_time_max = 0;
            uint64_t counter_previous = jk_platform_os_timer_get();
            uint64_t target_flip_time = counter_previous + ticks_per_frame;
            b32 reset_audio_position = TRUE;
            while (global_running) {
                MSG message;
                while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        global_running = FALSE;
                    }
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }

                memset(&global_chess.input, 0, sizeof(global_chess.input));

                // Keyboard input
                {
                    global_chess.input.flags |=
                            (((global_keys_down >> KEY_UP) | (global_keys_down >> KEY_W)) & 1)
                            << INPUT_UP;
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
                            (((global_keys_down >> KEY_ENTER) | (global_keys_down >> KEY_SPACE))
                                    & 1)
                            << INPUT_CONFIRM;
                    global_chess.input.flags |= ((global_keys_down >> KEY_ESCAPE) & 1)
                            << INPUT_CANCEL;
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
                            global_chess.input.flags |=
                                    !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) << INPUT_RIGHT;
                            global_chess.input.flags |= !!(pad->wButtons & XINPUT_GAMEPAD_A)
                                    << INPUT_CONFIRM;
                            global_chess.input.flags |= !!(pad->wButtons & XINPUT_GAMEPAD_B)
                                    << INPUT_CANCEL;
                        }
                    }
                }

                // Find necessary audio sample count
                {
                    DWORD play_cursor;
                    DWORD write_cursor;
                    if (global_audio_buffer->lpVtbl->GetCurrentPosition(
                                global_audio_buffer, &play_cursor, &write_cursor)
                            == DS_OK) {
                        uint32_t safe_start_point =
                                (write_cursor
                                        + ((SAMPLES_PER_SECOND * AUDIO_DELAY_MS) / 1000)
                                                * sizeof(AudioSample))
                                % audio_buffer_size;
                        if (reset_audio_position) {
                            reset_audio_position = FALSE;
                            audio_position = safe_start_point;
                        }
                        uint32_t end_point =
                                (safe_start_point + SAMPLES_PER_FRAME * sizeof(AudioSample))
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

                update(&global_chess);

                // Write audio to buffer
                {
                    AudioBufferRegion regions[2] = {0};
                    if (global_audio_buffer->lpVtbl->Lock(global_audio_buffer,
                                audio_position,
                                global_chess.audio.sample_count * sizeof(AudioSample),
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
                                (audio_position
                                        + global_chess.audio.sample_count * sizeof(AudioSample))
                                % audio_buffer_size;

                        global_audio_buffer->lpVtbl->Unlock(global_audio_buffer,
                                regions[0].data,
                                regions[0].size,
                                regions[1].data,
                                regions[1].size);
                    }
                }

                render(&global_chess);

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
                    OutputDebugStringA("Missed a frame\n");

                    // If we're off by more than half a frame, give up on catching up
                    if (ticks_remaining < -((int64_t)ticks_per_frame / 2)) {
                        target_flip_time = counter_current + ticks_per_frame;
                        reset_audio_position = TRUE;
                    }
                }

                copy_bitmap_to_window(device_context, global_chess.bitmap);

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

            snprintf(global_string_buffer,
                    JK_ARRAY_COUNT(global_string_buffer),
                    "\nWork Time\nMin: %.2fms\nMax: %.2fms\nAvg: %.2fms\n",
                    (double)work_time_min / (double)frequency * 1000.0,
                    (double)work_time_max / (double)frequency * 1000.0,
                    ((double)work_time_total / (double)global_chess.time) / (double)frequency
                            * 1000.0);
            OutputDebugStringA(global_string_buffer);
            snprintf(global_string_buffer,
                    JK_ARRAY_COUNT(global_string_buffer),
                    "\nFrame Time\nMin: %.2fms\nMax: %.2fms\nAvg: %.2fms\n",
                    (double)frame_time_min / (double)frequency * 1000.0,
                    (double)frame_time_max / (double)frequency * 1000.0,
                    ((double)frame_time_total / (double)global_chess.time) / (double)frequency
                            * 1000.0);
            OutputDebugStringA(global_string_buffer);
            snprintf(global_string_buffer,
                    JK_ARRAY_COUNT(global_string_buffer),
                    "\nFPS\nMin: %.2f\nMax: %.2f\nAvg: %.2f\n\n",
                    (double)frequency / (double)frame_time_min,
                    (double)frequency / (double)frame_time_max,
                    ((double)global_chess.time / (double)frame_time_total) * (double)frequency);
            OutputDebugStringA(global_string_buffer);
        } else {
            OutputDebugStringA("CreateWindowExA failed\n");
        }
    } else {
        OutputDebugStringA("Failed to allocate memory\n");
    }

    return 0;
}

// ---- Windows end ------------------------------------------------------------
#endif
