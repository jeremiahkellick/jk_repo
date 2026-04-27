// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/jk_shapes/jk_shapes.h>
// #jk_build dependencies_end

#define INPUT_SIDE_LENGTH 64
#define OUTPUT_SIDE_LENGTH 256
#define PIXEL_COUNT (OUTPUT_SIDE_LENGTH * OUTPUT_SIDE_LENGTH)
#define SUBPIXEL_PRECISION (1 / 64.0f)
#define SPREAD 8.0f

static JkFloatArray parse_numbers(JkArena *arena, JkBuffer shape_string, int64_t *pos)
{
    JkFloatArray result = {.e = jk_arena_pointer_current(arena)};
    int c;
    while ((c = jk_buffer_character_next(shape_string, pos)) != EOF
            && (isdigit(c) || isspace(c) || c == ',')) {
        if (isdigit(c)) {
            int64_t start = *pos - 1;
            do {
                c = jk_buffer_character_next(shape_string, pos);
            } while (isdigit(c) || c == '.');
            JkBuffer number_string = {
                .size = (*pos - 1) - start,
                .data = shape_string.data + start,
            };
            float *new_number = jk_arena_push(arena, JK_SIZEOF(*new_number));
            *new_number = (float)jk_parse_double(number_string);
        }
    }
    result.count = (float *)jk_arena_pointer_current(arena) - result.e;
    if (c != EOF) {
        (*pos)--;
    }
    return result;
}

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    jk_platform_set_working_directory_to_executable_directory();

    JkArena *arena = jk_arena_scratch_begin().arena;
    JkShapeArray shapes = {0};

    // Fill out shapes array with shapes.txt
    JK_ARENA_SCRATCH_NOT(scratch, arena)
    {
        JkBufferArray shape_strings = jk_platform_file_read_lines(
                scratch.arena, "../jk_assets/pikuma/graphics/shapes.txt");
        JK_ASSERT(0 <= shape_strings.count && shape_strings.count < 4096);

        JK_ARENA_PUSH_ARRAY(arena, shapes, shape_strings.count);

        for (int32_t shape_index = 0; shape_index < shapes.count; shape_index++) {
            JkBuffer piece_string = shape_strings.e[shape_index];
            JkShape *shape = shapes.e + shape_index;

            shape->dimensions.x = 64.0f;
            shape->dimensions.y = 64.0f;
            shape->commands.offset = arena->pos;

            JkVec2 prev_pos = {0};

            JkVec2 first_pos = {0};
            int64_t pos = 0;
            int c;
            while ((c = jk_buffer_token_character_next(piece_string, &pos)) != EOF) {
                JkArenaScope command_scope = jk_arena_scope_begin(scratch.arena);

                switch (c) {
                case 'M':
                case 'L': {
                    JkFloatArray numbers = parse_numbers(scratch.arena, piece_string, &pos);
                    JK_ASSERT(numbers.count && numbers.count % 2 == 0);
                    for (int64_t i = 0; i < numbers.count; i += 2) {
                        JkShapesPenCommand *new_command =
                                jk_arena_push_zero(arena, JK_SIZEOF(*new_command));
                        new_command->type =
                                c == 'M' ? JK_SHAPES_PEN_COMMAND_MOVE : JK_SHAPES_PEN_COMMAND_LINE;
                        new_command->v[0] = (JkVec2){numbers.e[i], numbers.e[i + 1]};
                        prev_pos = new_command->v[0];

                        if (c == 'M') {
                            first_pos = new_command->v[0];
                        }
                    }
                } break;

                case 'H':
                case 'V': {
                    JkFloatArray numbers = parse_numbers(scratch.arena, piece_string, &pos);
                    JK_ASSERT(numbers.count);
                    for (int64_t i = 0; i < numbers.count; i++) {
                        JkShapesPenCommand *new_command =
                                jk_arena_push_zero(arena, JK_SIZEOF(*new_command));
                        new_command->type = JK_SHAPES_PEN_COMMAND_LINE;
                        new_command->v[0] = c == 'H' ? (JkVec2){numbers.e[i], prev_pos.y}
                                                     : (JkVec2){prev_pos.x, numbers.e[i]};
                        prev_pos = new_command->v[0];
                    }
                } break;

                case 'Q': {
                    JkFloatArray numbers = parse_numbers(scratch.arena, piece_string, &pos);
                    JK_ASSERT(numbers.count && numbers.count % 4 == 0);
                    for (int64_t i = 0; i < numbers.count; i += 4) {
                        JkShapesPenCommand *new_command =
                                jk_arena_push_zero(arena, JK_SIZEOF(*new_command));
                        new_command->type = JK_SHAPES_PEN_COMMAND_CURVE_QUADRATIC;
                        for (int32_t j = 0; j < 2; j++) {
                            new_command->v[j] =
                                    (JkVec2){numbers.e[i + (j * 2)], numbers.e[i + (j * 2) + 1]};
                        }
                        prev_pos = new_command->v[1];
                    }
                } break;

                case 'C': {
                    JkFloatArray numbers = parse_numbers(scratch.arena, piece_string, &pos);
                    JK_ASSERT(numbers.count && numbers.count % 6 == 0);
                    for (int64_t i = 0; i < numbers.count; i += 6) {
                        JkShapesPenCommand *new_command =
                                jk_arena_push(arena, JK_SIZEOF(*new_command));
                        new_command->type = JK_SHAPES_PEN_COMMAND_CURVE_CUBIC;
                        for (int32_t j = 0; j < 3; j++) {
                            new_command->v[j] =
                                    (JkVec2){numbers.e[i + (j * 2)], numbers.e[i + (j * 2) + 1]};
                        }
                        prev_pos = new_command->v[2];
                    }
                } break;

                case 'A': {
                    JkFloatArray numbers = parse_numbers(scratch.arena, piece_string, &pos);
                    JK_ASSERT(numbers.count && numbers.count % 7 == 0);
                    for (int64_t i = 0; i < numbers.count; i += 7) {
                        JkShapesPenCommand *new_command =
                                jk_arena_push_zero(arena, JK_SIZEOF(*new_command));
                        new_command->type = JK_SHAPES_PEN_COMMAND_ARC;
                        new_command->arc.dimensions.x = numbers.e[i];
                        new_command->arc.dimensions.y = numbers.e[i + 1];
                        new_command->arc.rotation = numbers.e[i + 2] * (float)JK_PI / 180.0f;
                        if (numbers.e[i + 3]) {
                            new_command->arc.flags |= JK_SHAPES_ARC_FLAG_LARGE;
                        }
                        if (numbers.e[i + 4]) {
                            new_command->arc.flags |= JK_SHAPES_ARC_FLAG_SWEEP;
                        }
                        new_command->arc.point_end.x = numbers.e[i + 5];
                        new_command->arc.point_end.y = numbers.e[i + 6];
                        prev_pos = new_command->arc.point_end;
                    }
                } break;

                case 'Z': {
                    JkShapesPenCommand *new_command =
                            jk_arena_push_zero(arena, JK_SIZEOF(*new_command));
                    new_command->type = JK_SHAPES_PEN_COMMAND_LINE;
                    new_command->v[0] = first_pos;
                } break;

                default: {
                    JK_ASSERT(0 && "Unknown SVG shape command character");
                } break;
                }

                jk_arena_scope_end(command_scope);
            }

            shape->commands.size = arena->pos - shape->commands.offset;
        }
    }

    float pixels_per_unit = (float)OUTPUT_SIDE_LENGTH / INPUT_SIDE_LENGTH;
    for (int64_t shape_index = 0; shape_index < shapes.count; shape_index++) {
        JkArenaScope shape_scope = jk_arena_scope_begin(arena);

        JkShape *shape = shapes.e + shape_index;
        uint8_t *sdf = jk_arena_push(arena, PIXEL_COUNT * sizeof(*sdf));

        JkShapesPenCommandArray commands;
        commands.count = shape->commands.size / JK_SIZEOF(commands.e[0]);
        commands.e = (JkShapesPenCommand *)(arena->memory.data + shape->commands.offset);
        JkEdgeArray edges = jk_shapes_edges_get(
                arena, commands, (JkVec2){0}, pixels_per_unit, SUBPIXEL_PRECISION, 0);

        float *fill_right = jk_arena_push(arena, OUTPUT_SIDE_LENGTH * sizeof(*fill_right));
        for (JkIntVec2 pos = {0}; pos.y < OUTPUT_SIDE_LENGTH; pos.y++) {
            jk_memset(fill_right, 0, OUTPUT_SIDE_LENGTH * sizeof(*fill_right));
            float sample_y = pos.y + 0.5f;

            for (int64_t edge_index = 0; edge_index < edges.count; edge_index++) {
                JkEdge *edge = edges.e + edge_index;
                if (edge->segment.p0.y == edge->segment.p1.y) {
                    continue;
                }

                if (!(edge->segment.p0.y <= sample_y && sample_y < edge->segment.p1.y)) {
                    continue;
                }

                float scanline_intersect_x = jk_segment_y_intersection(edge->segment, sample_y);
                float leftmost_sample_x = jk_ceil_f32(scanline_intersect_x - 0.5f);
                if (0 <= leftmost_sample_x && leftmost_sample_x < OUTPUT_SIDE_LENGTH) {
                    fill_right[(int32_t)leftmost_sample_x] += edge->direction;
                }
            }

            float winding = 0;
            for (pos.x = 0; pos.x < OUTPUT_SIDE_LENGTH; pos.x++) {
                winding += fill_right[pos.x];
                float sign = winding == 0 ? 1 : -1;

                JkVec2 posf = jk_vec2_add(jk_vec2_from_i32(pos), (JkVec2){0.5f, 0.5f});
                float distance_sqr = jk_infinity_f32.f32;
                for (int64_t i = 0; i < edges.count; i++) {
                    float candidate = jk_distance_to_segment_2d(posf, edges.e[i].segment);
                    if (candidate < distance_sqr) {
                        distance_sqr = candidate;
                    }
                }

                float value = 127.5f;
                if ((SUBPIXEL_PRECISION * SUBPIXEL_PRECISION) < distance_sqr) {
                    float signed_distance = sign * jk_sqrt_f32(distance_sqr);
                    value = jk_remap_clamped_f32(signed_distance, -SPREAD, SPREAD, 0, 255);
                }
                sdf[OUTPUT_SIDE_LENGTH * pos.y + pos.x] = (uint8_t)value;
            }
        }

        // Write sdf to a bitmap file
        JkBuffer bitmap_buffer = jk_arena_push_buffer(
                arena, sizeof(JkBitmapHeader) + sizeof(JkColor3) * PIXEL_COUNT);
        JkBitmapHeader *bitmap = (JkBitmapHeader *)bitmap_buffer.data;
        jk_memset(bitmap, 0, sizeof(*bitmap));
        bitmap->identifier = 0x4d42;
        bitmap->size = bitmap_buffer.size;
        bitmap->data_offset = sizeof(JkBitmapHeader);
        bitmap->dib_header_size = 40;
        bitmap->width = OUTPUT_SIDE_LENGTH;
        bitmap->height = OUTPUT_SIDE_LENGTH;
        bitmap->color_plane_count = 1;
        bitmap->bits_per_pixel = 24;
        JkColor3 *bitmap_data = (JkColor3 *)(bitmap_buffer.data + bitmap->data_offset);
        for (JkIntVec2 pos = {0}; pos.y < OUTPUT_SIDE_LENGTH; pos.y++) {
            for (pos.x = 0; pos.x < OUTPUT_SIDE_LENGTH; pos.x++) {
                int32_t index = OUTPUT_SIDE_LENGTH * pos.y + pos.x;
                int32_t bmp_index = OUTPUT_SIDE_LENGTH * (OUTPUT_SIDE_LENGTH - 1 - pos.y) + pos.x;
                bitmap_data[bmp_index] = (JkColor3){sdf[index], sdf[index], sdf[index]};
            }
        }
        jk_platform_file_write(
                JK_FORMAT(arena, jkfn("sdf"), jkfi(shape_index), jkfn(".bmp")), bitmap_buffer);

        jk_arena_scope_end(shape_scope);
    }

    return 0;
}
