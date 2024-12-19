// #jk_build linker_arguments User32.lib Gdi32.lib

#include <dsound.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include <xinput.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

typedef DWORD (*XInputGetStatePointer)(DWORD dwUserIndex, XINPUT_STATE *pState);
typedef DWORD (*XInputSetStatePointer)(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);

XInputGetStatePointer xinput_get_state;
XInputSetStatePointer xinput_set_state;

typedef HRESULT (*DirectSoundCreatePointer)(
        LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);

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

typedef struct AudioBufferRegion {
    DWORD size;
    void *data;
} AudioBufferRegion;

typedef enum AudioChannel {
    AUDIO_CHANNEL_LEFT,
    AUDIO_CHANNEL_RIGHT,
    AUDIO_CHANNEL_COUNT,
} AudioChannel;

typedef struct AudioSample {
    int16_t channels[AUDIO_CHANNEL_COUNT];
} AudioSample;

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

typedef enum Button {
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_CONFIRM,
    BUTTON_CANCEL,
} Button;

#define BUTTON_FLAG_UP (1 << BUTTON_UP)
#define BUTTON_FLAG_DOWN (1 << BUTTON_DOWN)
#define BUTTON_FLAG_LEFT (1 << BUTTON_LEFT)
#define BUTTON_FLAG_RIGHT (1 << BUTTON_RIGHT)
#define BUTTON_FLAG_CONFIRM (1 << BUTTON_CONFIRM)
#define BUTTON_FLAG_CANCEL (1 << BUTTON_CANCEL)

typedef struct Input {
    int64_t button_flags;
} Input;

static b32 global_running;
static int64_t global_time;
static int64_t global_x;
static int64_t global_y;
static int64_t global_keys_down;
static Bitmap global_bitmap;
static LPDIRECTSOUNDBUFFER global_audio_buffer;
static char global_string_buffer[1024];

uint32_t lost_woods[] = {
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
    for (int64_t screen_y = 0; screen_y < bitmap.height; screen_y++) {
        for (int64_t screen_x = 0; screen_x < bitmap.width; screen_x++) {
            int64_t world_y = screen_y + global_y;
            int64_t world_x = screen_x + global_x;
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
            bitmap.memory[screen_y * bitmap.width + screen_x].r = (uint8_t)red;
            bitmap.memory[screen_y * bitmap.width + screen_x].b = (uint8_t)blue;
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

typedef struct Audio {
    uint32_t samples_per_second;
    uint32_t sample_index;
    uint32_t pitch_multiplier;
    uint32_t latency_sample_count;
    double bpm;
    double sin_t;
} Audio;

int32_t audio_samples_per_eighth_note(Audio *audio)
{
    double bpm = 140.0;
    double eighth_notes_per_second = (bpm / 60.0) * 2.0;
    return (uint32_t)((double)audio->samples_per_second / eighth_notes_per_second);
}

void audio_buffer_write(Audio *audio, uint32_t position, uint32_t size)
{
    AudioBufferRegion regions[2] = {0};
    if (global_audio_buffer->lpVtbl->Lock(global_audio_buffer,
                position,
                size,
                &regions[0].data,
                &regions[0].size,
                &regions[1].data,
                &regions[1].size,
                0)
            == DS_OK) {
        for (int region_index = 0; region_index < 2; region_index++) {
            AudioBufferRegion *region = &regions[region_index];

            AudioSample *region_samples = region->data;
            JK_ASSERT(region->size % sizeof(region_samples[0]) == 0);
            for (DWORD region_offset_index = 0;
                    region_offset_index < region->size / sizeof(region_samples[0]);
                    region_offset_index++) {
                uint32_t eighth_note_index =
                        (audio->sample_index / audio_samples_per_eighth_note(audio))
                        % JK_ARRAY_COUNT(lost_woods);

                double x = (double)audio->sample_index / audio_samples_per_eighth_note(audio);
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

                uint32_t hz = lost_woods[eighth_note_index] * audio->pitch_multiplier;
                audio->sin_t += 2.0 * 3.14159 * ((double)hz / (double)audio->samples_per_second);
                double value = sin(audio->sin_t);
                for (int channel_index = 0; channel_index < AUDIO_CHANNEL_COUNT; channel_index++) {
                    region_samples[region_offset_index].channels[channel_index] =
                            (int16_t)(value * fade_factor * 2000.0);
                }
                audio->sample_index++;
            }
        }

        global_audio_buffer->lpVtbl->Unlock(global_audio_buffer,
                regions[0].data,
                regions[0].size,
                regions[1].data,
                regions[1].size);
    }
}

LRESULT window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;

    switch (message) {
    case WM_DESTROY:
    case WM_CLOSE: {
        global_running = FALSE;
    } break;

    case WM_SIZE: {
        update_dimensions(&global_bitmap, window);
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
            Audio audio = {
                .samples_per_second = 48000,
                .pitch_multiplier = 1,
                .latency_sample_count = audio.samples_per_second / 15,
                .bpm = 140.0,
            };
            uint32_t audio_buffer_seconds = 2;
            uint32_t audio_buffer_sample_count = audio.samples_per_second * audio_buffer_seconds;
            uint32_t audio_buffer_size = audio_buffer_sample_count * sizeof(AudioSample);

            HDC device_context = GetDC(window);
            update_dimensions(&global_bitmap, window);

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
                        .nSamplesPerSec = audio.samples_per_second,
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
                                != DS_OK) {
                            OutputDebugStringA("Failed to create secondary buffer\n");
                        }
                    }
                } else {
                    OutputDebugStringA("Failed to create DirectSound\n");
                }
            } else {
                OutputDebugStringA("Failed to load DirectSound\n");
            }

            audio_buffer_write(&audio, 0, audio.latency_sample_count * sizeof(AudioSample));
            global_audio_buffer->lpVtbl->Play(global_audio_buffer, 0, 0, DSBPLAY_LOOPING);

            global_time = 0;
            global_running = TRUE;
            uint64_t frame_time_total = 0;
            uint64_t frame_time_min = ULLONG_MAX;
            uint64_t frame_time_max = 0;
            uint64_t counter_previous = jk_platform_cpu_timer_get();
            while (global_running) {
                MSG message;
                while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        global_running = FALSE;
                    }
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }

                Input input = {0};

                // Keyboard input
                {
                    input.button_flags |=
                            (((global_keys_down >> KEY_UP) | (global_keys_down >> KEY_W)) & 1)
                            << BUTTON_UP;
                    input.button_flags |=
                            (((global_keys_down >> KEY_DOWN) | (global_keys_down >> KEY_S)) & 1)
                            << BUTTON_DOWN;
                    input.button_flags |=
                            (((global_keys_down >> KEY_LEFT) | (global_keys_down >> KEY_A)) & 1)
                            << BUTTON_LEFT;
                    input.button_flags |=
                            (((global_keys_down >> KEY_RIGHT) | (global_keys_down >> KEY_D)) & 1)
                            << BUTTON_RIGHT;
                    input.button_flags |=
                            (((global_keys_down >> KEY_ENTER) | (global_keys_down >> KEY_SPACE))
                                    & 1)
                            << BUTTON_CONFIRM;
                    input.button_flags |= ((global_keys_down >> KEY_ESCAPE) & 1) << BUTTON_CANCEL;
                }

                // Controller input
                if (xinput_get_state) {
                    for (int32_t i = 0; i < XUSER_MAX_COUNT; i++) {
                        XINPUT_STATE state;
                        if (xinput_get_state(i, &state) == ERROR_SUCCESS) {
                            XINPUT_GAMEPAD *pad = &state.Gamepad;
                            input.button_flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                                    << BUTTON_UP;
                            input.button_flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                                    << BUTTON_DOWN;
                            input.button_flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                                    << BUTTON_LEFT;
                            input.button_flags |= !!(pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                                    << BUTTON_RIGHT;
                            input.button_flags |= !!(pad->wButtons & XINPUT_GAMEPAD_A)
                                    << BUTTON_CONFIRM;
                            input.button_flags |= !!(pad->wButtons & XINPUT_GAMEPAD_B)
                                    << BUTTON_CANCEL;
                        }
                    }
                }

                if (input.button_flags & BUTTON_FLAG_UP) {
                    global_y -= 2;
                }
                if (input.button_flags & BUTTON_FLAG_DOWN) {
                    global_y += 2;
                }
                if (input.button_flags & BUTTON_FLAG_LEFT) {
                    global_x -= 2;
                }
                if (input.button_flags & BUTTON_FLAG_RIGHT) {
                    global_x += 2;
                }

                audio.pitch_multiplier = (input.button_flags & BUTTON_FLAG_UP) ? 2 : 1;

                draw_pretty_colors(global_bitmap, global_time);

                // Write audio to buffer
                {
                    DWORD play_cursor;
                    DWORD write_cursor;
                    if (global_audio_buffer->lpVtbl->GetCurrentPosition(
                                global_audio_buffer, &play_cursor, &write_cursor)
                            == DS_OK) {

                        uint32_t position =
                                (audio.sample_index * sizeof(AudioSample)) % audio_buffer_size;
                        uint32_t target_cursor =
                                (play_cursor + audio.latency_sample_count * sizeof(AudioSample))
                                % audio_buffer_size;
                        uint32_t size;
                        if (position > target_cursor) {
                            size = audio_buffer_size - position + target_cursor;
                        } else {
                            size = target_cursor - position;
                        }

                        audio_buffer_write(&audio, position, size);
                    } else {
                        OutputDebugStringA("Failed to get DirectSound current position\n");
                    }
                }

                copy_bitmap_to_window(device_context, global_bitmap);

                global_time++;

                uint64_t counter_current = jk_platform_cpu_timer_get();
                uint64_t frame_time_current = counter_current - counter_previous;
                counter_previous = counter_current;

                frame_time_total += frame_time_current;
                if (frame_time_current < frame_time_min) {
                    frame_time_min = frame_time_current;
                }
                if (frame_time_current > frame_time_max) {
                    frame_time_max = frame_time_current;
                }
            }

            uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

            snprintf(global_string_buffer,
                    JK_ARRAY_COUNT(global_string_buffer),
                    "\nFrame Time\nMin: %.3fms\nMax: %.3fms\nAvg: %.3fms\n",
                    (double)frame_time_min / (double)frequency * 1000.0,
                    (double)frame_time_max / (double)frequency * 1000.0,
                    ((double)frame_time_total / (double)global_time) / (double)frequency * 1000.0);
            OutputDebugStringA(global_string_buffer);
            snprintf(global_string_buffer,
                    JK_ARRAY_COUNT(global_string_buffer),
                    "\nFPS\nMin: %.0f\nMax: %.0f\nAvg: %.0f\n\n",
                    (double)frequency / (double)frame_time_min,
                    (double)frequency / (double)frame_time_max,
                    ((double)global_time / (double)frame_time_total) * (double)frequency);
            OutputDebugStringA(global_string_buffer);
        } else {
            OutputDebugStringA("CreateWindowExA failed\n");
        }
    } else {
        OutputDebugStringA("Failed to allocate memory\n");
    }

    return 0;
}
