#include <pthread.h>
#include <stdio.h>
#include <sys/mman.h>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/QuartzCore.h>

// #jk_build run jk_src/pikuma/graphics/graphics_assets_pack.c
// #jk_build single_translation_unit

// clang-format off
// #jk_build compiler_arguments -Wno-deprecated-declarations -framework Cocoa -framework Metal -framework MetalKit -framework QuartzCore
// clang-format on

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/pikuma/graphics/graphics.h>
// #jk_build dependencies_end

typedef enum MouseFlags {
    MOUSE_LEFT_DOWN,
    MOUSE_LEFT_PRESSED,
    MOUSE_LEFT_RELEASED,
} MouseFlags;

typedef struct Vertex {
    simd_float4 position;
    simd_float2 texture_coordinate;
} Vertex;

typedef struct IntArray4 {
    int32_t a[4];
} IntArray4;

typedef struct MyRect {
    union {
        struct {
            JkIntVec2 pos;
            JkIntVec2 dimensions;
        };
        int32_t a[4];
    };
} MyRect;

typedef struct Global {
    b32 running;
    b32 capture_mouse;
    Assets *assets;
    State state;

    _Alignas(64) pthread_mutex_t keyboard_lock;
    _Alignas(64) JkKeyboard keyboard;

    _Alignas(64) pthread_mutex_t mouse_lock;
    _Alignas(64) JkVec2 mouse_delta;
    uint32_t mouse_flags;
} Global;

static Global g = {
    .keyboard_lock = PTHREAD_MUTEX_INITIALIZER,
    .mouse_lock = PTHREAD_MUTEX_INITIALIZER,
};

// clang-format off
JkKey key_map[] = {
    0x04, 0x16, 0x07, 0x09, 0x0b, 0x0a, 0x1d, 0x1b,
    0x06, 0x19, 0x64, 0x05, 0x14, 0x1a, 0x08, 0x15,
    0x1c, 0x17, 0x1e, 0x1f, 0x20, 0x21, 0x23, 0x22,
    0x2e, 0x26, 0x24, 0x2d, 0x25, 0x27, 0x30, 0x12,
    0x18, 0x2f, 0x0c, 0x13, 0x28, 0x0f, 0x0d, 0x34,
    0x0e, 0x33, 0x31, 0x36, 0x38, 0x11, 0x10, 0x37,
    0x2b, 0x2c, 0x35, 0x2a, 0x00, 0x29, 0xe7, 0xe3,
    0xe1, 0x39, 0xe2, 0xe0, 0xe5, 0xe6, 0xe4, 0x00,
    0x6c, 0x63, 0x00, 0x55, 0x00, 0x57, 0x00, 0x53,
    0x80, 0x81, 0x7f, 0x54, 0x58, 0x00, 0x56, 0x6d,
    0x6e, 0x67, 0x62, 0x59, 0x5a, 0x5b, 0x5c, 0x5d,
    0x5e, 0x5f, 0x6f, 0x60, 0x61, 0x89, 0x87, 0x8c,
    0x3e, 0x3f, 0x40, 0x3c, 0x41, 0x42, 0x91, 0x44,
    0x90, 0x68, 0x6b, 0x69, 0x00, 0x43, 0x65, 0x45,
    0x00, 0x6a, 0x75, 0x4a, 0x4b, 0x4c, 0x3d, 0x4d,
    0x3b, 0x4e, 0x3a, 0x50, 0x4f, 0x51, 0x52,
};

static char const *const shaders_code =
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "\n"
        "constexpr sampler texture_sampler (coord::pixel, min_filter::nearest, mag_filter::nearest);\n"
        "\n"
        "struct Vertex {\n"
        "    float4 position [[position]];\n"
        "    float2 texture_coordinate;\n"
        "};\n"
        "\n"
        "vertex Vertex vertex_main(const device Vertex *verticies [[buffer(0)]], uint vid [[vertex_id]]) {\n"
        "    return verticies[vid];\n"
        "}\n"
        "\n"
        "fragment half4 fragment_main(texture2d<half> tex [[texture(0)]], Vertex in [[stage_in]]) {\n"
        "    return tex.sample(texture_sampler, in.texture_coordinate);\n"
        "}\n";
// clang-format on

static void print_stdout(JkBuffer string)
{
    fwrite(string.data, 1, string.size, stdout);
}

@interface MetalView : MTKView
@property(nonatomic, strong) id<MTLRenderPipelineState> pipeline;
@property(nonatomic, strong) id<MTLCommandQueue> command_queue;
@property(nonatomic, strong) id<MTLBuffer> vertex_buffer;
@property(nonatomic, strong) id<MTLTexture> texture;
@property(nonatomic, assign) CVDisplayLinkRef display_link;
@property(nonatomic) CGFloat scale_factor;

- (instancetype)initWithFrame:(CGRect)frameRect scale_factor:(CGFloat)scale_factor;
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property(strong, nonatomic) NSWindow *window;
@property(strong) id click_monitor;
@property(nonatomic) CGFloat scale_factor;

- (instancetype)initWithScaleFactor:(CGFloat)scale_factor;
@end

int main(void)
{
    jk_platform_set_working_directory_to_executable_directory();
    jk_print = print_stdout;

    JkPlatformArenaVirtualRoot arena_root;
    JkArena arena = jk_platform_arena_virtual_init(&arena_root, 8 * JK_GIGABYTE);
    if (!jk_arena_valid(&arena)) {
        jk_print(JKS("Failed to initialize virtual memory arena\n"));
        exit(1);
    }

    g.assets = (Assets *)jk_platform_file_read_full(&arena, "graphics_assets").data;

    g.state.memory.size = 2 * JK_MEGABYTE;
    uint8_t *memory = mmap(NULL,
            DRAW_BUFFER_SIZE + Z_BUFFER_SIZE + NEXT_BUFFER_SIZE + g.state.memory.size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANON,
            -1,
            0);
    if (!memory) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }
    g.state.draw_buffer = (JkColor *)memory;
    g.state.z_buffer = (float *)(memory + DRAW_BUFFER_SIZE);
    g.state.next_buffer = (PixelIndex *)(memory + DRAW_BUFFER_SIZE + Z_BUFFER_SIZE);
    g.state.memory.data = memory + DRAW_BUFFER_SIZE + Z_BUFFER_SIZE + NEXT_BUFFER_SIZE;

    g.state.print = jk_print;
    g.state.os_timer_frequency = jk_platform_os_timer_frequency();
    g.state.estimate_cpu_frequency = jk_platform_cpu_timer_frequency_estimate;

    g.running = 1;

    @autoreleasepool {
        CGFloat scale_factor = [NSScreen mainScreen].backingScaleFactor;

        NSApplication *application = [NSApplication sharedApplication];

        AppDelegate *applicationDelegate = [[AppDelegate alloc] initWithScaleFactor:scale_factor];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp activateIgnoringOtherApps:YES];
        [application setDelegate:applicationDelegate];

        [application run];
    }

    g.running = 0;
}

static CVReturn display_link_callback(CVDisplayLinkRef displayLink,
        const CVTimeStamp *now,
        const CVTimeStamp *outputTime,
        CVOptionFlags flagsIn,
        CVOptionFlags *flagsOut,
        void *displayLinkContext)
{
    (void)displayLink;
    (void)now;
    (void)outputTime;
    (void)flagsIn;
    (void)flagsOut;

    MetalView *view = (__bridge MetalView *)displayLinkContext;

    dispatch_async(dispatch_get_main_queue(), ^{
      [view setNeedsDisplay:YES];
    });

    return kCVReturnSuccess;
}

@interface MyNSWindow : NSWindow
- (BOOL)canBecomeMainWindow;
- (BOOL)canBecomeKeyWindow;
- (BOOL)acceptsFirstResponder;
- (void)keyDown:(NSEvent *)anEvent;
- (void)keyUp:(NSEvent *)anEvent;
- (void)mouseMoved:(NSEvent *)anEvent;
@end

@implementation MyNSWindow
- (BOOL)canBecomeMainWindow
{
    return YES;
}

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)acceptsMouseMovedEvents
{
    return YES;
}

- (void)keyDown:(NSEvent *)anEvent
{
    unsigned short keyCode = [anEvent keyCode];
    if (keyCode < JK_ARRAY_COUNT(key_map)) {
        JkKey key = key_map[keyCode];
        if (key) {
            uint8_t flag = 1 << (key % 8);
            pthread_mutex_lock(&g.keyboard_lock);
            g.keyboard.down[key / 8] |= flag;
            g.keyboard.pressed[key / 8] |= flag;
            pthread_mutex_unlock(&g.keyboard_lock);
        }
    }
}

- (void)keyUp:(NSEvent *)anEvent
{
    unsigned short keyCode = [anEvent keyCode];
    if (keyCode < JK_ARRAY_COUNT(key_map)) {
        JkKey key = key_map[keyCode];
        if (key) {
            uint8_t flag = 1 << (key % 8);
            pthread_mutex_lock(&g.keyboard_lock);
            g.keyboard.down[key / 8] &= ~flag;
            g.keyboard.released[key / 8] |= flag;
            pthread_mutex_unlock(&g.keyboard_lock);
        }
    }
}

- (void)mouseMoved:(NSEvent *)anEvent
{
    pthread_mutex_lock(&g.mouse_lock);
    g.mouse_delta.x += anEvent.deltaX;
    g.mouse_delta.y += anEvent.deltaY;
    pthread_mutex_unlock(&g.mouse_lock);
}
@end

@implementation AppDelegate
- (instancetype)initWithScaleFactor:(CGFloat)scale_factor
{
    if (self = [super init]) {
        self.scale_factor = scale_factor;
    }
    return self;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    NSRect contentSize = NSMakeRect(0.0f, 0.0f, 1280.0f, 720.0f);

    const int style =
            NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;
    self.window = [[MyNSWindow alloc] initWithContentRect:contentSize
                                                styleMask:style
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    [self.window setTitle:@"Graphics"];
    [self.window setOpaque:YES];
    [self.window setContentView:[[MetalView alloc] initWithFrame:contentSize
                                                    scale_factor:self.scale_factor]];
    [self.window center];
    [self.window makeMainWindow];
    [self.window makeKeyAndOrderFront:nil];

    // clang-format off
    self.click_monitor = [NSEvent
            addLocalMonitorForEventsMatchingMask:(NSEventMaskLeftMouseDown|NSEventMaskLeftMouseUp)
            handler:^NSEvent *(NSEvent *event) {
        pthread_mutex_lock(&g.mouse_lock);
        JK_FLAG_SET(g.mouse_flags, MOUSE_LEFT_DOWN, event.type == NSEventTypeLeftMouseDown);
        if (event.type == NSEventTypeLeftMouseDown) {
            JK_FLAG_SET(g.mouse_flags, MOUSE_LEFT_PRESSED, 1);
        } else {
            JK_FLAG_SET(g.mouse_flags, MOUSE_LEFT_RELEASED, 1);
        }
        pthread_mutex_unlock(&g.mouse_lock);
        return event;
    }];
    // clang-format on
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
    return true;
}
@end

@implementation MetalView
- (instancetype)initWithFrame:(CGRect)frameRect scale_factor:(CGFloat)scale_factor
{
    if ((self = [super initWithFrame:frameRect])) {
        self.scale_factor = scale_factor;

        self.device = MTLCreateSystemDefaultDevice();
        if (!self.device) {
            NSLog(@"Failed to create Metal device\n");
            return self;
        }

        NSString *source = [[NSString alloc] initWithUTF8String:shaders_code];
        MTLCompileOptions *compileOpts = [[MTLCompileOptions alloc] init];
        compileOpts.languageVersion = MTLLanguageVersion2_0;

        NSError *err = nil;
        id<MTLLibrary> library = [self.device newLibraryWithSource:source
                                                           options:compileOpts
                                                             error:&err];
        if (err) {
            NSLog(@"Failed to compile shaders:\n%@", err.localizedDescription);
        }
        id<MTLFunction> vertex_main = [library newFunctionWithName:@"vertex_main"];
        id<MTLFunction> fragment_main = [library newFunctionWithName:@"fragment_main"];

        MTLRenderPipelineDescriptor *pipeline_descriptor = [MTLRenderPipelineDescriptor new];
        pipeline_descriptor.vertexFunction = vertex_main;
        pipeline_descriptor.fragmentFunction = fragment_main;
        pipeline_descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        self.pipeline = [self.device newRenderPipelineStateWithDescriptor:pipeline_descriptor
                                                                    error:&err];
        if (err) {
            NSLog(@"Failed to create render pipeline\n%@", err.localizedDescription);
        }

        self.command_queue = [self.device newCommandQueue];

        self.vertex_buffer = [self.device newBufferWithLength:4
                                                      options:MTLResourceCPUCacheModeDefaultCache];

        MTLTextureDescriptor *texture_descriptor =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                   width:DRAW_BUFFER_SIDE_LENGTH
                                                                  height:DRAW_BUFFER_SIDE_LENGTH
                                                               mipmapped:NO];
        self.texture = [self.device newTextureWithDescriptor:texture_descriptor];

        self.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        self.enableSetNeedsDisplay = YES;
        self.preferredFramesPerSecond = FPS;

        pthread_mutex_lock(&g.keyboard_lock);
        jk_keyboard_clear(&g.keyboard);
        pthread_mutex_unlock(&g.keyboard_lock);

        pthread_mutex_lock(&g.mouse_lock);
        g.mouse_delta = (JkVec2){0};
        pthread_mutex_unlock(&g.mouse_lock);

        CVDisplayLinkCreateWithActiveCGDisplays(&_display_link);
        CVDisplayLinkSetOutputCallback(
                self.display_link, &display_link_callback, (__bridge void *)self);
        CVDisplayLinkSetCurrentCGDisplay(self.display_link, 0);
        CVDisplayLinkStart(self.display_link);
    }
    return self;
}

- (void)drawRect:(NSRect)dirtyRect
{
    JkIntVec2 window_dimensions = {self.drawableSize.width, self.drawableSize.height};
    g.state.dimensions.x = JK_MAX(256, JK_MIN(window_dimensions.x, DRAW_BUFFER_SIDE_LENGTH));
    g.state.dimensions.y = JK_MAX(256, JK_MIN(window_dimensions.y, DRAW_BUFFER_SIDE_LENGTH));

    g.state.os_time = jk_platform_os_timer_get();

    if (self.window) {
        NSPoint mouse_location = [NSEvent mouseLocation];
        NSRect window_rect = [self.window contentRectForFrameRect:self.window.frame];
        CGFloat x = mouse_location.x - window_rect.origin.x;
        CGFloat y = window_rect.size.height - (mouse_location.y - window_rect.origin.y);
        g.state.mouse_pos.x = x * self.scale_factor;
        g.state.mouse_pos.y = y * self.scale_factor;
    }

    pthread_mutex_lock(&g.keyboard_lock);
    g.state.keyboard = g.keyboard;
    jk_keyboard_clear(&g.keyboard);
    pthread_mutex_unlock(&g.keyboard_lock);

    pthread_mutex_lock(&g.mouse_lock);
    JkVec2 mouse_delta = g.mouse_delta;
    g.mouse_delta = (JkVec2){0};
    uint32 mouse_flags = g.mouse_flags;
    g.mouse_flags &= JK_MASK(MOUSE_LEFT_DOWN);
    pthread_mutex_unlock(&g.mouse_lock);

    int32_t deadzone = 10;

    if (JK_FLAG_GET(mouse_flags, MOUSE_LEFT_PRESSED) && !g.capture_mouse
            && deadzone <= g.state.mouse_pos.x
            && g.state.mouse_pos.x < (g.state.dimensions.x - deadzone)
            && deadzone <= g.state.mouse_pos.y
            && g.state.mouse_pos.y < (g.state.dimensions.y - deadzone)) {
        g.capture_mouse = 1;
        CGDisplayHideCursor(CVDisplayLinkGetCurrentCGDisplay(self.display_link));
    }
    if (g.capture_mouse
            && (jk_key_pressed(&g.state.keyboard, JK_KEY_ESC)
                    || (self.window && ![self.window isKeyWindow]))) {
        g.capture_mouse = 0;
        CGDisplayShowCursor(CVDisplayLinkGetCurrentCGDisplay(self.display_link));
    }
    CGAssociateMouseAndMouseCursorPosition(!g.capture_mouse);

    g.state.mouse_delta = g.capture_mouse ? mouse_delta : (JkVec2){0};

    render(g.assets, &g.state);

    // Copy bitmap buffer into texture
    [self.texture replaceRegion:MTLRegionMake2D(0, 0, g.state.dimensions.x, g.state.dimensions.y)
                    mipmapLevel:0
                      withBytes:g.state.draw_buffer
                    bytesPerRow:DRAW_BUFFER_SIDE_LENGTH * JK_SIZEOF(JkColor)];

    JkVec2 pos = {-1.0f, 1.0f};

    JkVec2 dimensions;
    dimensions.x = g.state.dimensions.x * 2.0f / window_dimensions.x;
    dimensions.y = -(g.state.dimensions.y * 2.0f / window_dimensions.y);

    Vertex verticies[4];
    verticies[0].position = simd_make_float4(pos.x, pos.y + dimensions.y, 0.0f, 1.0f);
    verticies[0].texture_coordinate = simd_make_float2(0.0f, g.state.dimensions.y);

    verticies[1].position =
            simd_make_float4(pos.x + dimensions.x, pos.y + dimensions.y, 0.0f, 1.0f);
    verticies[1].texture_coordinate = simd_make_float2(g.state.dimensions.x, g.state.dimensions.y);

    verticies[2].position = simd_make_float4(pos.x, pos.y, 0.0f, 1.0f);
    verticies[2].texture_coordinate = simd_make_float2(0.0f, 0.0f);

    verticies[3].position = simd_make_float4(pos.x + dimensions.x, pos.y, 0.0f, 1.0f);
    verticies[3].texture_coordinate = simd_make_float2(g.state.dimensions.x, 0.0f);

    memcpy([self.vertex_buffer contents], verticies, JK_SIZEOF(verticies));

    id<CAMetalDrawable> drawable = self.currentDrawable;
    id<MTLTexture> texture = drawable.texture;

    MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    passDescriptor.colorAttachments[0].texture = texture;
    passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(
            CLEAR_COLOR_R / 255.0f, CLEAR_COLOR_G / 255.0f, CLEAR_COLOR_B / 255.0f, 1);

    id<MTLCommandBuffer> commandBuffer = [self.command_queue commandBuffer];

    id<MTLRenderCommandEncoder> commandEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
    [commandEncoder setRenderPipelineState:self.pipeline];
    [commandEncoder setVertexBuffer:self.vertex_buffer offset:0 atIndex:0];
    [commandEncoder setFragmentTexture:self.texture atIndex:0];
    [commandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
    [commandEncoder endEncoding];

    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
}
@end
