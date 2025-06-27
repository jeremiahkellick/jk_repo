#ifndef JK_SHAPES_H
#define JK_SHAPES_H

#include <jk_src/jk_lib/jk_lib.h>

typedef struct JkShapesBitmap {
    JkIntVector2 dimensions;
    uint8_t *data;
} JkShapesBitmap;

// ---- Hash table begin -------------------------------------------------------

// Hash table load factor expressed in tenths (e.g. 7 is 7/10 or 70%)
#define JK_SHAPES_HASH_TABLE_LOAD_FACTOR 7

typedef struct JkShapesHashTableSlot {
    b32 filled;
    uint64_t key;
    JkShapesBitmap value;
} JkShapesHashTableSlot;

typedef struct JkShapesHashTable {
    JkShapesHashTableSlot *slots;
    uint64_t capacity;
    uint64_t count;
} JkShapesHashTable;

JK_PUBLIC JkShapesHashTable *jk_shapes_hash_table_init(JkShapesHashTable *t, JkBuffer memory);

JK_PUBLIC JkShapesHashTableSlot *jk_shapes_hash_table_probe(JkShapesHashTable *t, uint64_t key);

JK_PUBLIC void jk_shapes_hash_table_set(
        JkShapesHashTable *t, JkShapesHashTableSlot *slot, uint64_t key, JkShapesBitmap value);

// ---- Hash table end ---------------------------------------------------------

typedef enum JkShapesArcFlag {
    JK_SHAPES_ARC_FLAG_LARGE,
    JK_SHAPES_ARC_FLAG_SWEEP,
} JkShapesArcFlag;

typedef struct JkShapesArcByEndpoint {
    uint32_t flags;
    JkVector2 dimensions;
    float rotation;
    JkVector2 point_end;
} JkShapesArcByEndpoint;

typedef enum JkShapesPenCommandType {
    JK_SHAPES_PEN_COMMAND_MOVE,
    JK_SHAPES_PEN_COMMAND_LINE,
    JK_SHAPES_PEN_COMMAND_CURVE_QUADRATIC,
    JK_SHAPES_PEN_COMMAND_CURVE_CUBIC,
    JK_SHAPES_PEN_COMMAND_ARC,
} JkShapesPenCommandType;

typedef struct JkShapesPenCommand {
    JkShapesPenCommandType type;
    union {
        JkVector2 coords[3];
        JkShapesArcByEndpoint arc;
    };
} JkShapesPenCommand;

typedef struct JkShapesPenCommandArray {
    uint64_t count;
    JkShapesPenCommand *items;
} JkShapesPenCommandArray;

typedef struct JkShape {
    JkVector2 offset;
    JkVector2 dimensions;
    float advance_width;
    JkSpan commands;
} JkShape;

typedef struct JkShapeArray {
    uint64_t count;
    JkShape *items;
} JkShapeArray;

typedef struct JkShapesDrawCommand {
    JkIntVector2 position;
    JkColor color;
    JkShapesBitmap *bitmap;
} JkShapesDrawCommand;

typedef struct JkShapesDrawCommandListNode {
    struct JkShapesDrawCommandListNode *next;
    JkShapesDrawCommand command;
} JkShapesDrawCommandListNode;

typedef struct JkShapesDrawCommandArray {
    uint64_t count;
    JkShapesDrawCommand *items;
} JkShapesDrawCommandArray;

typedef struct JkShapesRenderer {
    uint8_t *base_pointer;
    JkShapeArray shapes;
    JkArena *arena;
    JkShapesHashTable hash_table;
    JkShapesDrawCommandListNode *draw_commands_head;
} JkShapesRenderer;

typedef struct JkShapesPointListNode {
    struct JkShapesPointListNode *next;
    JkVector2 point;
    float t;
    b32 is_cursor_movement;
} JkShapesPointListNode;

typedef union JkShapesSegment {
    JkVector2 endpoints[2];
    struct {
        JkVector2 p1;
        JkVector2 p2;
    };
} JkShapesSegment;

typedef struct JkShapesEdge {
    JkShapesSegment segment;
    float direction;
} JkShapesEdge;

typedef struct JkShapesEdgeArray {
    uint64_t count;
    JkShapesEdge *items;
} JkShapesEdgeArray;

JK_PUBLIC void jk_shapes_renderer_init(
        JkShapesRenderer *renderer, void *base_pointer, JkShapeArray shapes, JkArena *arena);

JK_PUBLIC JkShapesBitmap *jk_shapes_bitmap_get(
        JkShapesRenderer *renderer, uint32_t shape_index, float scale);

JK_PUBLIC float jk_shapes_draw(JkShapesRenderer *renderer,
        uint32_t shape_index,
        JkVector2 position,
        float scale,
        JkColor color);

JK_PUBLIC JkShapesDrawCommandArray jk_shapes_draw_commands_get(JkShapesRenderer *renderer);

#endif
