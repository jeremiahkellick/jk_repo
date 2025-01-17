#include "chess.h"

#include <math.h>
#include <string.h>

// #jk_build compiler_arguments /LD
// #jk_build linker_arguments /OUT:chess.dll /EXPORT:update /EXPORT:render
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

_Static_assert(sizeof(Color) == 4, "Color must be 4 bytes");

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
                (audio->audio_time / audio_samples_per_eighth_note(SAMPLES_PER_SECOND))
                % JK_ARRAY_COUNT(lost_woods);

        double x = (double)audio->audio_time / audio_samples_per_eighth_note(SAMPLES_PER_SECOND);
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
        audio->sin_t += 2.0 * PI * ((double)hz / (double)SAMPLES_PER_SECOND);
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

UPDATE_FUNCTION(update)
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

RENDER_FUNCTION(render)
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
            Color *pixel = &chess->bitmap.memory[screen_y * chess->bitmap.width + screen_x];
            memset(pixel, 0, sizeof(*pixel));
            pixel->r = (uint8_t)red;
            pixel->b = (uint8_t)blue;
        }
    }
}
