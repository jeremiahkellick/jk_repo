#include "jk_src/jk_lib/jk_lib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/stb/stb_truetype.h>
// #jk_build dependencies_end

#include <jk_src/chess/chess.h>
#include <string.h>

char *sound_file_paths[SOUND_COUNT] = {
    0, // SOUND_NONE
    "../jk_assets/chess/move.wav",
    "../jk_assets/chess/capture.wav",
    "../jk_assets/chess/check.wav",
    "../jk_assets/chess/draw.wav",
    "../jk_assets/chess/win.wav",
    "../jk_assets/chess/lose.wav",
};

static JkFloatArray parse_numbers(JkArena *arena, JkBuffer shape_string, uint64_t *pos)
{
    JkFloatArray result = {.items = jk_arena_pointer_current(arena)};
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
            float *new_number = jk_arena_push(arena, sizeof(*new_number));
            *new_number = (float)jk_parse_double(number_string);
        }
    }
    result.count = (float *)jk_arena_pointer_current(arena) - result.items;
    if (c != EOF) {
        (*pos)--;
    }
    return result;
}

int main(int argc, char **argv)
{
    jk_platform_set_working_directory_to_executable_directory();

    JkPlatformArenaVirtualRoot scratch_arena_root;
    JkArena scratch_arena = jk_platform_arena_virtual_init(&scratch_arena_root, JK_GIGABYTE);

    JkPlatformArenaVirtualRoot storage_arena_root;
    JkArena storage = jk_platform_arena_virtual_init(&storage_arena_root, JK_GIGABYTE);

    ChessAssets *assets = jk_arena_push_zero(&storage, sizeof(*assets));

    { // Fill out shapes array with piece data
        JkBufferArray piece_strings =
                jk_platform_file_read_lines(&scratch_arena, "../jk_assets/chess/paths.txt");
        JK_ASSERT(piece_strings.count == PIECE_TYPE_COUNT - 1);

        void *scratch_arena_saved_pointer = jk_arena_pointer_current(&scratch_arena);

        for (int32_t piece_index = 1; piece_index < PIECE_TYPE_COUNT; piece_index++) {
            JkBuffer piece_string = piece_strings.items[piece_index - 1];

            assets->shapes[piece_index].dimensions.x = 64.0f;
            assets->shapes[piece_index].dimensions.y = 64.0f;
            assets->shapes[piece_index].commands.offset = storage.pos;

            JkVector2 prev_pos = {0};

            JkVector2 first_pos = {0};
            uint64_t pos = 0;
            int c;
            while ((c = jk_buffer_character_next(piece_string, &pos)) != EOF) {
                JkArena tmp_arena = jk_arena_child_get(&scratch_arena);

                switch (c) {
                case 'M':
                case 'L': {
                    JkFloatArray numbers = parse_numbers(&tmp_arena, piece_string, &pos);
                    JK_ASSERT(numbers.count && numbers.count % 2 == 0);
                    for (int32_t i = 0; i < numbers.count; i += 2) {
                        JkShapesPenCommand *new_command =
                                jk_arena_push_zero(&storage, sizeof(*new_command));
                        new_command->type =
                                c == 'M' ? JK_SHAPES_PEN_COMMAND_MOVE : JK_SHAPES_PEN_COMMAND_LINE;
                        new_command->coords[0] =
                                (JkVector2){numbers.items[i], numbers.items[i + 1]};
                        prev_pos = new_command->coords[0];

                        if (c == 'M') {
                            first_pos = new_command->coords[0];
                        }
                    }
                } break;

                case 'H':
                case 'V': {
                    JkFloatArray numbers = parse_numbers(&tmp_arena, piece_string, &pos);
                    JK_ASSERT(numbers.count);
                    for (int32_t i = 0; i < numbers.count; i++) {
                        JkShapesPenCommand *new_command =
                                jk_arena_push_zero(&storage, sizeof(*new_command));
                        new_command->type = JK_SHAPES_PEN_COMMAND_LINE;
                        new_command->coords[0] = c == 'H'
                                ? (JkVector2){numbers.items[i], prev_pos.y}
                                : (JkVector2){prev_pos.x, numbers.items[i]};
                        prev_pos = new_command->coords[0];
                    }
                } break;

                case 'Q': {
                    JkFloatArray numbers = parse_numbers(&tmp_arena, piece_string, &pos);
                    JK_ASSERT(numbers.count && numbers.count % 4 == 0);
                    for (int32_t i = 0; i < numbers.count; i += 4) {
                        JkShapesPenCommand *new_command =
                                jk_arena_push_zero(&storage, sizeof(*new_command));
                        new_command->type = JK_SHAPES_PEN_COMMAND_CURVE_QUADRATIC;
                        for (int32_t j = 0; j < 2; j++) {
                            new_command->coords[j] = (JkVector2){
                                numbers.items[i + (j * 2)], numbers.items[i + (j * 2) + 1]};
                        }
                        prev_pos = new_command->coords[1];
                    }
                } break;

                case 'C': {
                    JkFloatArray numbers = parse_numbers(&tmp_arena, piece_string, &pos);
                    JK_ASSERT(numbers.count && numbers.count % 6 == 0);
                    for (int32_t i = 0; i < numbers.count; i += 6) {
                        JkShapesPenCommand *new_command =
                                jk_arena_push(&storage, sizeof(*new_command));
                        new_command->type = JK_SHAPES_PEN_COMMAND_CURVE_CUBIC;
                        for (int32_t j = 0; j < 3; j++) {
                            new_command->coords[j] = (JkVector2){
                                numbers.items[i + (j * 2)], numbers.items[i + (j * 2) + 1]};
                        }
                        prev_pos = new_command->coords[2];
                    }
                } break;

                case 'A': {
                    JkFloatArray numbers = parse_numbers(&tmp_arena, piece_string, &pos);
                    JK_ASSERT(numbers.count && numbers.count % 7 == 0);
                    for (int32_t i = 0; i < numbers.count; i += 7) {
                        JkShapesPenCommand *new_command =
                                jk_arena_push_zero(&storage, sizeof(*new_command));
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
                } break;

                case 'Z': {
                    JkShapesPenCommand *new_command =
                            jk_arena_push_zero(&storage, sizeof(*new_command));
                    new_command->type = JK_SHAPES_PEN_COMMAND_LINE;
                    new_command->coords[0] = first_pos;
                } break;

                default: {
                    JK_ASSERT(0 && "Unknown SVG path command character");
                } break;
                }
            }

            assets->shapes[piece_index].commands.size =
                    storage.pos - assets->shapes[piece_index].commands.offset;
        }

        scratch_arena.pos = 0;
    }

    { // Fill out the rest of the shapes array with font data
        char *ttf_file_name = "../jk_assets/chess/AmiriQuran-Regular.ttf";
        JkBuffer ttf_file = jk_platform_file_read_full(&scratch_arena, ttf_file_name);
        if (!ttf_file.size) {
            fprintf(stderr, "Failed to read file '%s'\n", ttf_file_name);
            exit(1);
        }

        stbtt_fontinfo font;

        stbtt_InitFont(&font, ttf_file.data, stbtt_GetFontOffsetForIndex(ttf_file.data, 0));

        {
            int32_t advance_width;
            stbtt_GetCodepointHMetrics(&font, ' ', &advance_width, 0);
            assets->shapes[PIECE_TYPE_COUNT].advance_width = (float)advance_width;
        }

        assets->font_ascent = INT32_MAX;
        assets->font_descent = INT32_MIN;
        for (int32_t shape_index = PIECE_TYPE_COUNT + 1;
                shape_index < JK_ARRAY_COUNT(assets->shapes);
                shape_index++) {
            JkShape *shape = assets->shapes + shape_index;
            int32_t codepoint = shape_index - CHARACTER_SHAPE_OFFSET;

            int32_t x0, x1, y0, y1;
            stbtt_GetCodepointBox(&font, codepoint, &x0, &y0, &x1, &y1);

            // Since the font uses y values that grow from the bottom. Negate and swap y0 and y1.
            int32_t tmp = y0;
            y0 = -y1;
            y1 = -tmp;

            shape->offset.x = (float)x0;
            shape->offset.y = (float)y0;
            shape->dimensions.x = (float)(x1 - x0);
            shape->dimensions.y = (float)(y1 - y0);

            int32_t advance_width;
            stbtt_GetCodepointHMetrics(&font, codepoint, &advance_width, 0);
            shape->advance_width = (float)advance_width;

            // The font I otherwise like unfortunately has some special characters with oddly low
            // decenders, which throws off the spacing if I use those to calculate the font's line
            // height. So to address this I only consider the ascenders and decenders of
            // "regular" characters.
            if (('0' <= codepoint && codepoint <= '9') || ('A' <= codepoint && codepoint <= 'Z')
                    || ('a' <= codepoint && codepoint <= 'z')) {
                if (y0 < assets->font_ascent) {
                    assets->font_ascent = (float)y0;
                }
                if (assets->font_descent < y1) {
                    assets->font_descent = (float)y1;
                }
            }

            stbtt_vertex *verticies;
            uint64_t command_count = stbtt_GetCodepointShape(&font, codepoint, &verticies);
            shape->commands.size = sizeof(JkShapesPenCommand) * command_count;
            shape->commands.offset = storage.pos;
            JkShapesPenCommand *commands = jk_arena_push_zero(&storage, shape->commands.size);
            for (int32_t i = 0; i < command_count; i++) {
                switch (verticies[i].type) {
                case STBTT_vmove:
                case STBTT_vline: {
                    commands[i].type = verticies[i].type == STBTT_vmove
                            ? JK_SHAPES_PEN_COMMAND_MOVE
                            : JK_SHAPES_PEN_COMMAND_LINE;
                    commands[i].coords[0].x = (float)verticies[i].x;
                    commands[i].coords[0].y = (float)-verticies[i].y;
                } break;

                case STBTT_vcurve: {
                    commands[i].type = JK_SHAPES_PEN_COMMAND_CURVE_QUADRATIC;
                    commands[i].coords[0].x = (float)verticies[i].cx;
                    commands[i].coords[0].y = (float)-verticies[i].cy;
                    commands[i].coords[1].x = (float)verticies[i].x;
                    commands[i].coords[1].y = (float)-verticies[i].y;
                } break;

                case STBTT_vcubic: {
                    commands[i].type = JK_SHAPES_PEN_COMMAND_CURVE_CUBIC;
                    commands[i].coords[0].x = (float)verticies[i].cx;
                    commands[i].coords[0].y = (float)-verticies[i].cy;
                    commands[i].coords[1].x = (float)verticies[i].cx1;
                    commands[i].coords[1].y = (float)-verticies[i].cy1;
                    commands[i].coords[2].x = (float)verticies[i].x;
                    commands[i].coords[2].y = (float)-verticies[i].y;
                } break;

                default: {
                    JK_ASSERT(0 && "Unsupported vertex type");
                } break;
                }
            }
        }
    }

    // Load sounds
    for (SoundIndex i = 0; i < SOUND_COUNT; i++) {
        if (sound_file_paths[i]) {
            JkBuffer audio_file = jk_platform_file_read_full(&scratch_arena, sound_file_paths[i]);
            if (audio_file.size) {
                b32 error = 0;
                JkRiffChunkMain *chunk_main = (JkRiffChunkMain *)audio_file.data;
                if (chunk_main->id == JK_RIFF_ID_RIFF && chunk_main->form_type == JK_RIFF_ID_WAV) {
                    for (JkRiffChunk *chunk = (JkRiffChunk *)chunk_main->chunk_first;
                            jk_riff_chunk_valid(chunk_main, chunk);
                            chunk = jk_riff_chunk_next(chunk)) {
                        switch (chunk->id) {
                        case JK_RIFF_ID_FMT: {
                            JkWavFormat *format = (JkWavFormat *)chunk->data;
                            if (format->format_tag != JK_WAV_FORMAT_PCM
                                    || format->channel_count != 1
                                    || format->samples_per_second != 48000
                                    || format->bits_per_sample != 16) {
                                error = 1;
                            }
                        } break;

                        case JK_RIFF_ID_DATA: {
                            assets->sounds[i].size = chunk->size;
                            assets->sounds[i].offset = storage.pos;
                            memcpy(jk_arena_push(&storage, chunk->size), chunk->data, chunk->size);
                        } break;

                        default: {
                        } break;
                        }
                    }
                } else {
                    error = 1;
                }
                if (error) {
                    fprintf(stderr,
                            "Something's wrong with the contents of %s\n",
                            sound_file_paths[i]);
                    exit(1);
                }
            } else {
                fprintf(stderr, "Failed to load %s\n", sound_file_paths[i]);
                exit(1);
            }
        }
    }

    // Write as binary file to build/chess_assets
    FILE *binary_file = fopen("chess_assets", "wb");
    fwrite(storage.root->memory.data, storage.pos, 1, binary_file);
    fclose(binary_file);

    // Write as C byte array to jk_gen/chess/assets.c
    jk_platform_ensure_directory_exists("../jk_gen/chess/");
    char *assets_file_name = "../jk_gen/chess/assets.c";
    FILE *assets_file = fopen(assets_file_name, "wb");
    if (!assets_file) {
        fprintf(stderr,
                "%s: Failed to open file '%s': %s\n",
                argv[0],
                assets_file_name,
                strerror(errno));
        exit(1);
    }

    fprintf(assets_file,
            "JK_PUBLIC char chess_assets_byte_array[%llu] = {\n",
            (long long unsigned)storage.pos);
    uint64_t byte_index = 0;
    while (byte_index < storage.pos) {
        fprintf(assets_file, "   ");
        for (uint32_t i = 0; i < 16 && byte_index < storage.pos; i++) {
            fprintf(assets_file, " 0x%02x,", (uint32_t)storage.root->memory.data[byte_index++]);
        }
        fprintf(assets_file, "\n");
    }
    fprintf(assets_file, "};\n");

    fclose(assets_file);

    return 0;
}
