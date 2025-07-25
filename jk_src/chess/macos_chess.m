#include <AudioToolbox/AudioToolbox.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/mman.h>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/QuartzCore.h>

// #jk_build single_translation_unit

// clang-format off
// #jk_build compiler_arguments -framework Cocoa -framework AudioToolbox -framework Metal -framework MetalKit -framework QuartzCore
// clang-format on

// #jk_build dependencies_begin
#include <jk_src/chess/chess.h>
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

typedef enum MacosInput {
    MACOS_INPUT_MOUSE,
    MACOS_INPUT_R,
} MacosInput;

#define BUTTON_FLAG_MOUSE (1 << MACOS_INPUT_MOUSE)
#define BUTTON_FLAG_R (1 << MACOS_INPUT_R)

typedef struct AudioThread {
    uint64_t time;
} AudioThread;

typedef struct MainThread {
    uint64_t buttons_down;
    Chess chess;
    AudioState audio_state;
    pthread_mutex_t audio_state_lock;
} MainThread;

typedef struct Global {
    _Alignas(64) ChessAssets *assets;

    _Alignas(64) MainThread main;

    _Alignas(64) AudioThread audio;
} Global;

static Global g = {.main.audio_state_lock = PTHREAD_MUTEX_INITIALIZER};

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

static int32_t square_side_length_get(IntArray4 dimensions)
{
    int32_t min_dimension = INT32_MAX;
    for (uint64_t i = 0; i < JK_ARRAY_COUNT(dimensions.a); i++) {
        if (dimensions.a[i] < min_dimension) {
            min_dimension = dimensions.a[i];
        }
    }
    return min_dimension / 10;
}

static MyRect draw_rect_get(JkIntVector2 window_dimensions)
{
    MyRect result = {0};

    result.dimensions = (JkIntVector2){
        JK_MIN(window_dimensions.x, g.main.chess.square_side_length * 10),
        JK_MIN(window_dimensions.y, g.main.chess.square_side_length * 10),
    };

    int32_t max_dimension_index = window_dimensions.x < window_dimensions.y ? 1 : 0;
    result.pos.coords[max_dimension_index] =
            (window_dimensions.coords[max_dimension_index]
                    - result.dimensions.coords[max_dimension_index])
            / 2;

    return result;
}

static void audio_callback(void *context, AudioQueueRef queue, AudioQueueBufferRef buffer)
{
    buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;
    uint32_t sample_count = buffer->mAudioDataByteSize / sizeof(AudioSample);

    pthread_mutex_lock(&g.main.audio_state_lock);
    AudioState state = g.main.audio_state;
    pthread_mutex_unlock(&g.main.audio_state_lock);

    audio(g.assets, state, g.audio.time, sample_count, buffer->mAudioData);
    g.audio.time += sample_count;

    OSStatus error = AudioQueueEnqueueBuffer(queue, buffer, 0, 0);
    if (error) {
        fprintf(stderr, "Failed to re-enqueue an audio buffer (%d)\n", error);
    }
}

static void debug_print(char *string)
{
    printf("%s", string);
}

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
@property(nonatomic) CGFloat scale_factor;

- (instancetype)initWithScaleFactor:(CGFloat)scale_factor;
@end

int main(void)
{
    jk_platform_set_working_directory_to_executable_directory();

    uint64_t audio_buffer_size = SAMPLES_PER_SECOND * 2 * sizeof(AudioSample);
    g.main.chess.render_memory.size = 1 * JK_GIGABYTE;
    uint8_t *memory = mmap(NULL,
            audio_buffer_size + DRAW_BUFFER_SIZE + g.main.chess.render_memory.size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANON,
            -1,
            0);
    if (!memory) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }
    g.main.chess.draw_buffer = (JkColor *)(memory + audio_buffer_size);
    g.main.chess.render_memory.data = memory + audio_buffer_size + DRAW_BUFFER_SIZE;

    g.main.chess.os_timer_frequency = jk_platform_os_timer_frequency();
    g.main.chess.debug_print = debug_print;

    // Load image data
    JkPlatformArenaVirtualRoot arena_root;
    JkArena storage = jk_platform_arena_virtual_init(&arena_root, (size_t)1 << 35);
    if (jk_arena_valid(&storage)) {
        g.assets = (ChessAssets *)jk_platform_file_read_full(&storage, "chess_assets").data;
    } else {
        fprintf(stderr, "Failed to initialize arena\n");
    }

    { // Set up audio callback
        AudioStreamBasicDescription format = {0};
        format.mSampleRate = SAMPLES_PER_SECOND;
        format.mFormatID = kAudioFormatLinearPCM;
        format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
        format.mBitsPerChannel = 16;
        format.mChannelsPerFrame = AUDIO_CHANNEL_COUNT;
        format.mBytesPerFrame = sizeof(AudioSample);
        format.mFramesPerPacket = 1;
        format.mBytesPerPacket = format.mFramesPerPacket * format.mBytesPerFrame;

        AudioQueueRef auQueue = 0;
        AudioQueueBufferRef auBuffers[2] = {0};

        OSStatus error;

        // most of the 0 and nullptr params here are for compressed sound formats etc.
        error = AudioQueueNewOutput(&format, audio_callback, 0, 0, 0, 0, &auQueue);
        if (error) {
            fprintf(stderr, "Failed to create audio output queue (%d)\n", error);
        }

        if (!error) {
            AudioStreamBasicDescription actualFormat;
            UInt32 dataSize = sizeof(actualFormat);

            error = AudioQueueGetProperty(
                    auQueue, kAudioQueueProperty_StreamDescription, &actualFormat, &dataSize);

            printf("%.2f\n", actualFormat.mSampleRate);
        }

        // generate buffers holding at most 1 second of data
        uint32_t bufferSize = format.mBytesPerFrame * format.mSampleRate / 16;

        if (!error) {
            error = AudioQueueAllocateBuffer(auQueue, bufferSize, auBuffers + 0);
            if (error) {
                fprintf(stderr, "Failed to allocate audio buffer 0 (%d)\n", error);
            }
        }

        if (!error) {
            error = AudioQueueAllocateBuffer(auQueue, bufferSize, auBuffers + 1);
            if (error) {
                fprintf(stderr, "Failed to allocate audio buffer 1 (%d)\n", error);
            }
        }

        if (!error) {
            audio_callback(0, auQueue, auBuffers[0]);
            audio_callback(0, auQueue, auBuffers[1]);
            error = AudioQueueStart(auQueue, 0);
            if (error) {
                fprintf(stderr, "Failed to start audio queue (%d)\n", error);
            }
        }
    }

    @autoreleasepool {
        CGFloat scale_factor = [NSScreen mainScreen].backingScaleFactor;

        NSApplication *application = [NSApplication sharedApplication];

        AppDelegate *applicationDelegate = [[AppDelegate alloc] initWithScaleFactor:scale_factor];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp activateIgnoringOtherApps:YES];
        [application setDelegate:applicationDelegate];

        [application run];
    }
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
- (void)mouseDown:(NSEvent *)anEvent;
- (void)mouseUp:(NSEvent *)anEvent;
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
        g.main.buttons_down |= BUTTON_FLAG_R;
    }
}

- (void)keyUp:(NSEvent *)anEvent
{
    unsigned short keyCode = [anEvent keyCode];
    if (keyCode == 15) {
        g.main.buttons_down &= ~BUTTON_FLAG_R;
    }
}

- (void)mouseDown:(NSEvent *)anEvent
{
    g.main.buttons_down |= BUTTON_FLAG_MOUSE;
}

- (void)mouseUp:(NSEvent *)anEvent
{
    g.main.buttons_down &= ~BUTTON_FLAG_MOUSE;
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
    [self.window setTitle:@"Chess"];
    [self.window setOpaque:YES];
    [self.window setContentView:[[MetalView alloc] initWithFrame:contentSize
                                                    scale_factor:self.scale_factor]];
    [self.window makeMainWindow];
    [self.window makeKeyAndOrderFront:nil];
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
    IntArray4 bounds = {
        window_dimensions.x,
        window_dimensions.y,
        DRAW_BUFFER_SIDE_LENGTH,
        DRAW_BUFFER_SIDE_LENGTH,
    };
    g.main.chess.square_side_length = square_side_length_get(bounds);
    MyRect draw_rect = draw_rect_get(window_dimensions);

    if (self.window) {
        NSPoint mouse_location = [NSEvent mouseLocation];
        NSRect window_rect = [self.window contentRectForFrameRect:self.window.frame];
        CGFloat x = mouse_location.x - window_rect.origin.x;
        CGFloat y = window_rect.size.height - (mouse_location.y - window_rect.origin.y);
        g.main.chess.input.mouse_pos.x = x * self.scale_factor - draw_rect.pos.x;
        g.main.chess.input.mouse_pos.y = y * self.scale_factor - draw_rect.pos.y;
    }

    g.main.chess.input.flags = 0;
    g.main.chess.input.flags |= ((g.main.buttons_down >> MACOS_INPUT_MOUSE) & 1) << INPUT_CONFIRM;
    g.main.chess.input.flags |= ((g.main.buttons_down >> MACOS_INPUT_R) & 1) << INPUT_RESET;

    g.main.chess.os_time = jk_platform_os_timer_get();

    g.main.chess.audio_time = g.audio.time;

    update(g.assets, &g.main.chess);

    if (memcmp(&g.main.chess.audio_state, &g.main.audio_state, sizeof(g.main.audio_state)) != 0) {
        pthread_mutex_lock(&g.main.audio_state_lock);
        g.main.audio_state = g.main.chess.audio_state;
        pthread_mutex_unlock(&g.main.audio_state_lock);
    }

    render(g.assets, &g.main.chess);

    // Copy bitmap buffer into texture
    [self.texture replaceRegion:MTLRegionMake2D(0, 0, window_dimensions.x, window_dimensions.y)
                    mipmapLevel:0
                      withBytes:g.main.chess.draw_buffer
                    bytesPerRow:DRAW_BUFFER_SIDE_LENGTH * sizeof(JkColor)];

    JkVector2 pos;
    pos.x = (draw_rect.pos.x * 2.0f / window_dimensions.x) - 1.0f;
    pos.y = -((draw_rect.pos.y * 2.0f / window_dimensions.y) - 1.0f);

    JkVector2 dimensions;
    dimensions.x = ((g.main.chess.square_side_length * 10) * 2.0f / window_dimensions.x);
    dimensions.y = -(((g.main.chess.square_side_length * 10) * 2.0f / window_dimensions.y));

    Vertex verticies[4];
    verticies[0].position = simd_make_float4(pos.x, pos.y + dimensions.y, 0.0f, 1.0f);
    verticies[0].texture_coordinate = simd_make_float2(0.0f, g.main.chess.square_side_length * 10);

    verticies[1].position =
            simd_make_float4(pos.x + dimensions.x, pos.y + dimensions.y, 0.0f, 1.0f);
    verticies[1].texture_coordinate = simd_make_float2(
            g.main.chess.square_side_length * 10, g.main.chess.square_side_length * 10);

    verticies[2].position = simd_make_float4(pos.x, pos.y, 0.0f, 1.0f);
    verticies[2].texture_coordinate = simd_make_float2(0.0f, 0.0f);

    verticies[3].position = simd_make_float4(pos.x + dimensions.x, pos.y, 0.0f, 1.0f);
    verticies[3].texture_coordinate = simd_make_float2(g.main.chess.square_side_length * 10, 0.0f);

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
