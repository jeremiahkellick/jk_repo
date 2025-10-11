#include <stdio.h>
#include <sys/mman.h>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/QuartzCore.h>

// #jk_build single_translation_unit

// clang-format off
// #jk_build compiler_arguments -Wno-deprecated-declarations -framework Cocoa -framework Metal -framework MetalKit -framework QuartzCore
// clang-format on

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/pikuma/graphics/graphics.h>
// #jk_build dependencies_end

typedef enum MacosInput {
    MACOS_INPUT_MOUSE,
    MACOS_INPUT_R,
} MacosInput;

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
            JkIntVector2 pos;
            JkIntVector2 dimensions;
        };
        int32_t a[4];
    };
} MyRect;

typedef struct Global {
    b32 running;
    uint32_t buttons_down;
    State state;
} Global;

static Global g;

// clang-format off
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
@property(nonatomic) CVDisplayLinkRef display_link;
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

    g.state.memory.size = 2 * JK_MEGABYTE;
    uint8_t *memory = mmap(NULL,
            DRAW_BUFFER_SIZE + g.state.memory.size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANON,
            -1,
            0);
    if (!memory) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }
    g.state.draw_buffer = (JkColor *)memory;
    g.state.memory.data = memory + DRAW_BUFFER_SIZE;

    g.state.os_timer_frequency = jk_platform_os_timer_frequency();

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

- (void)keyDown:(NSEvent *)anEvent
{
    unsigned short keyCode = [anEvent keyCode];
    if (keyCode == 53) {
        [self close];
    }
    if (keyCode == 15) {
        JK_FLAG_SET(g.buttons_down, MACOS_INPUT_R, 1);
    }
}

- (void)keyUp:(NSEvent *)anEvent
{
    unsigned short keyCode = [anEvent keyCode];
    if (keyCode == 15) {
        JK_FLAG_SET(g.buttons_down, MACOS_INPUT_R, 0);
    }
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
        JK_FLAG_SET(g.buttons_down, MACOS_INPUT_MOUSE, event.type == NSEventTypeLeftMouseDown);
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
        self.preferredFramesPerSecond = 60;

        CVDisplayLinkRef display_link;
        CVDisplayLinkCreateWithActiveCGDisplays(&display_link);
        CVDisplayLinkSetOutputCallback(display_link, &display_link_callback, (__bridge void *)self);
        CVDisplayLinkSetCurrentCGDisplay(display_link, 0);
        CVDisplayLinkStart(display_link);
    }
    return self;
}

- (void)drawRect:(NSRect)dirtyRect
{
    JkIntVector2 window_dimensions = {self.drawableSize.width, self.drawableSize.height};
    g.state.dimensions.x = JK_MAX(256, JK_MIN(window_dimensions.x, DRAW_BUFFER_SIDE_LENGTH));
    g.state.dimensions.y = JK_MAX(256, JK_MIN(window_dimensions.y, DRAW_BUFFER_SIDE_LENGTH));

    g.state.os_time = jk_platform_os_timer_get();

    if (self.window) {
        NSPoint mouse_location = [NSEvent mouseLocation];
        NSRect window_rect = [self.window contentRectForFrameRect:self.window.frame];
        CGFloat x = mouse_location.x - window_rect.origin.x;
        CGFloat y = window_rect.size.height - (mouse_location.y - window_rect.origin.y);
        g.state.input.mouse_pos.x = x * self.scale_factor;
        g.state.input.mouse_pos.y = y * self.scale_factor;
    }

    g.state.input.flags = 0;
    g.state.input.flags |= ((g.buttons_down >> MACOS_INPUT_MOUSE) & 1) << INPUT_CONFIRM;
    g.state.input.flags |= ((g.buttons_down >> MACOS_INPUT_R) & 1) << INPUT_RESET;

    render(&g.state);

    // Copy bitmap buffer into texture
    [self.texture replaceRegion:MTLRegionMake2D(0, 0, g.state.dimensions.x, g.state.dimensions.y)
                    mipmapLevel:0
                      withBytes:g.state.draw_buffer
                    bytesPerRow:DRAW_BUFFER_SIDE_LENGTH * sizeof(JkColor)];

    JkVector2 pos = {-1.0f, 1.0f};

    JkVector2 dimensions;
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

    memcpy([self.vertex_buffer contents], verticies, sizeof(verticies));

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
