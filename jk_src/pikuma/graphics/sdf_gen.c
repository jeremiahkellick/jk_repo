// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/jk_shapes/jk_shapes.h>
#include <jk_src/pikuma/graphics/graphics.h>
// #jk_build dependencies_end

#define INPUT_SIDE_LENGTH 64
#define OUTPUT_SIDE_LENGTH 256
#define SDF_PIXEL_COUNT (OUTPUT_SIDE_LENGTH * OUTPUT_SIDE_LENGTH)
#define SUBPIXEL_PRECISION (1 / 64.0f)

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

JkBuffer read_string(JkBuffer buffer, int64_t *cursor, int32_t delim)
{
    int64_t start = *cursor;
    int c;
    do {
        c = jk_buffer_character_next(buffer, cursor);
    } while (!(c == delim || c == JK_OOB));
    return c == delim ? (JkBuffer){.data = buffer.data + start, .size = *cursor - start - 1}
                      : (JkBuffer){0};
}

b32 is_xml_tag_name_character(int32_t c)
{
    c = jk_char_to_lower(c);
    return ('a' <= c && c <= 'z') || jk_char_is_digit(c) || c == '-' || c == '_' || c == ':'
            || c == '.';
}

b32 is_css_attribute_name_character(int32_t c)
{
    c = jk_char_to_lower(c);
    return ('a' <= c && c <= 'z') || jk_char_is_digit(c) || c == '-' || c == '_';
}

uint8_t read_hex_byte(JkBuffer buffer, int64_t *cursor)
{
    uint8_t result = 0;
    for (int64_t i = 0; i < 2; i++) {
        result <<= 4;
        int32_t c = jk_char_to_lower(jk_buffer_character_next(buffer, cursor));
        if (!jk_char_is_hex_digit(c)) {
            return 0;
        }
        result |= jk_char_hex_value(c);
    }
    return result;
}

b32 iterate_attributes(JkBuffer svg, int64_t *cursor, JkBuffer *name, JkBuffer *value)
{
    // Name
    jk_buffer_skip_whitespace(svg, cursor);
    int64_t start = *cursor;
    int32_t c;
    while ((c = jk_buffer_character_get(svg, *cursor)) != '=' && !jk_is_space(c) && c != '"'
            && c != '\'' && c != JK_OOB) {
        *cursor += 1;
    }
    *name = (JkBuffer){.data = svg.data + start, .size = *cursor - start};

    // =
    jk_buffer_skip_whitespace(svg, cursor);
    if (jk_buffer_character_get(svg, *cursor) != '=') {
        return 0;
    }
    *cursor += 1;

    // value
    jk_buffer_skip_whitespace(svg, cursor);
    int32_t delim = jk_buffer_character_next(svg, cursor);
    if (delim != '"' && delim != '\'') {
        return 0;
    }
    *value = read_string(svg, cursor, delim);

    return 1;
}

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    jk_platform_set_working_directory_to_executable_directory();

    JkArena *arena = jk_arena_scratch_begin().arena;

    JkColor background_color = {.a = 0xff};
    JkBuffer shape_strings[4] = {0};
    JkColor3 shape_colors[4] = {0};
    JkShape shapes[4] = {0};

    JK_ARENA_SCRATCH_NOT(scratch, arena)
    {
        // Find path data in SVG file
        JkBuffer svg =
                jk_platform_file_read(scratch.arena, JKS("../jk_assets/pikuma/graphics/abcd.svg"));
        if (svg.size == 0) {
            exit(1);
        }
        int64_t cursor = 0;
        while (cursor < svg.size) {
            int first = jk_buffer_character_next(svg, &cursor);
            switch (first) {
            case '"':
            case '\'': {
                read_string(svg, &cursor, first);
            } break;

            case '<': {
                jk_buffer_skip_whitespace(svg, &cursor);
                int64_t tag_start = cursor;
                while (is_xml_tag_name_character(jk_buffer_character_get(svg, cursor))) {
                    cursor++;
                }
                JkBuffer tag = {.data = svg.data + tag_start, .size = cursor - tag_start};
                if (jk_string_equal(tag, JKS("path"))) {
                    int64_t id = -1;
                    JkBuffer shape_string = {0};
                    JkColor3 color = {0};

                    cursor += 4;
                    JkBuffer name;
                    JkBuffer value;
                    while (iterate_attributes(svg, &cursor, &name, &value)) {
                        if (jk_string_equal(name, JKS("d"))) {
                            shape_string = value;
                        } else if (jk_string_equal(name, JKS("id"))) {
                            if (value.size == 1) {
                                int64_t num = value.data[0] - '0';
                                if (0 <= num && num < 4) {
                                    id = num;
                                }
                            }
                        } else if (jk_string_equal(name, JKS("style"))) {
                            int64_t style_cursor = 0;
                            while (style_cursor < value.size) {
                                while (!is_css_attribute_name_character(
                                               jk_buffer_character_get(value, style_cursor))
                                        && style_cursor < value.size) {
                                    style_cursor++;
                                }
                                int64_t start = style_cursor;
                                while (is_css_attribute_name_character(
                                        jk_buffer_character_get(value, style_cursor))) {
                                    style_cursor++;
                                }
                                JkBuffer attribute_name = {
                                    .data = value.data + start, .size = style_cursor - start};

                                if (jk_string_equal(attribute_name, JKS("fill"))) {
                                    jk_buffer_skip_whitespace(value, &style_cursor);
                                    if (jk_buffer_character_get(value, style_cursor) != ':') {
                                        continue;
                                    }
                                    style_cursor++;
                                    jk_buffer_skip_whitespace(value, &style_cursor);
                                    if (jk_buffer_character_get(value, style_cursor) != '#') {
                                        continue;
                                    }
                                    style_cursor++;
                                    color.r = read_hex_byte(value, &style_cursor);
                                    color.g = read_hex_byte(value, &style_cursor);
                                    color.b = read_hex_byte(value, &style_cursor);
                                    break;
                                }
                            }
                        }
                    }

                    if (id != -1) {
                        shape_strings[id] = shape_string;
                        shape_colors[id] = color;
                    }
                } else if (jk_string_equal(tag, JKS("sodipodi:namedview"))) {
                    JkBuffer name;
                    JkBuffer value;
                    while (iterate_attributes(svg, &cursor, &name, &value)) {
                        if (jk_string_equal(name, JKS("pagecolor"))) {
                            int64_t value_cursor = 0;
                            jk_buffer_skip_whitespace(value, &value_cursor);
                            if (jk_buffer_character_get(value, value_cursor) == '#') {
                                value_cursor++;
                                background_color.r = read_hex_byte(value, &value_cursor);
                                background_color.g = read_hex_byte(value, &value_cursor);
                                background_color.b = read_hex_byte(value, &value_cursor);
                            }
                        }
                    }
                }
            } break;
            }
        }

        // Parse the path data
        for (int32_t shape_index = 0; shape_index < 4; shape_index++) {
            JkBuffer piece_string = shape_strings[shape_index];
            JkShape *shape = shapes + shape_index;

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

    JkColor *sdf = jk_arena_push(arena, SDF_PIXEL_COUNT * sizeof(*sdf));

    float pixels_per_unit = (float)OUTPUT_SIDE_LENGTH / INPUT_SIDE_LENGTH;
    for (int64_t shape_index = 0; shape_index < 4; shape_index++) {
        JkArenaScope shape_scope = jk_arena_scope_begin(arena);

        JkShape *shape = shapes + shape_index;

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
                    value = jk_remap_clamped_f32(signed_distance, SDF_SPREAD, -SDF_SPREAD, 0, 255);
                }
                sdf[OUTPUT_SIDE_LENGTH * pos.y + pos.x].v[shape_index] = (uint8_t)value;
            }
        }

        jk_arena_scope_end(shape_scope);
    }

    // Write sdf to a bitmap file
    JkBuffer bitmap_buffer =
            jk_arena_push_buffer(arena, sizeof(JkBitmapHeader) + sizeof(JkColor) * SDF_PIXEL_COUNT);
    JkBitmapHeader *bitmap = (JkBitmapHeader *)bitmap_buffer.data;
    jk_memset(bitmap, 0, sizeof(*bitmap));
    bitmap->identifier = 0x4d42;
    bitmap->size = bitmap_buffer.size;
    bitmap->data_offset = sizeof(JkBitmapHeader);
    bitmap->dib_header_size = 108;
    bitmap->width = OUTPUT_SIDE_LENGTH;
    bitmap->height = OUTPUT_SIDE_LENGTH;
    bitmap->color_plane_count = 1;
    bitmap->bits_per_pixel = 32;
    bitmap->compression_method = 3;
    bitmap->data_size = sizeof(JkColor) * SDF_PIXEL_COUNT;
    bitmap->masks[0] = 0x00ff0000;
    bitmap->masks[1] = 0x0000ff00;
    bitmap->masks[2] = 0x000000ff;
    bitmap->masks[3] = 0xff000000;
    bitmap->color_space_type = 0x73524742; // 'sRGB' (LCS_sRGB)
    JkColor *bitmap_data = (JkColor *)(bitmap_buffer.data + bitmap->data_offset);
    for (JkIntVec2 pos = {0}; pos.y < OUTPUT_SIDE_LENGTH; pos.y++) {
        for (pos.x = 0; pos.x < OUTPUT_SIDE_LENGTH; pos.x++) {
            int32_t index = OUTPUT_SIDE_LENGTH * pos.y + pos.x;
            int32_t bmp_index = OUTPUT_SIDE_LENGTH * (OUTPUT_SIDE_LENGTH - 1 - pos.y) + pos.x;
            bitmap_data[bmp_index] = sdf[index];
        }
    }
    jk_platform_file_write(JKS("sdf.bmp"), bitmap_buffer);

    return 0;
}
