#include <stdio.h>
#include <sys/mman.h>
#include <simd/simd.h>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/QuartzCore.h>

// #jk_build single_translation_unit
// #jk_build compiler_arguments -framework Cocoa -framework Metal -framework MetalKit -framework QuartzCore

// #jk_build dependencies_begin
#include <jk_src/chess/chess.h>
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

typedef struct __attribute__((packed)) BitmapHeader {
    uint16_t identifier;
    uint32_t size;
    uint32_t reserved;
    uint32_t offset;
} BitmapHeader;

typedef enum Button {
    BUTTON_MOUSE,
    BUTTON_R,
} Button;

#define BUTTON_FLAG_MOUSE (1 << BUTTON_MOUSE)
#define BUTTON_FLAG_R (1 << BUTTON_R)

typedef struct ScreenInfo {
    NSSize resolution;
    CGFloat scale_factor;
} ScreenInfo;

static Chess global_chess;
static uint64_t global_buttons_down;

typedef struct Vertex {
    simd_float4 position;
    simd_float2 texture_coordinate;
} Vertex;

static char const *const shaders_code = "#include <metal_stdlib>\n"
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
    "fragment half4 fragment_main(texture2d<half> tex [[texture(0)]],\n"
    "        Vertex in [[stage_in]]) {\n"
    "    return tex.sample(texture_sampler, in.texture_coordinate);\n"
    "}\n";

@interface MetalView : MTKView
@property (nonatomic, strong) id<MTLRenderPipelineState> pipeline;
@property (nonatomic, strong) id<MTLCommandQueue> command_queue;
@property (nonatomic, strong) id<MTLBuffer> vertex_buffer;
@property (nonatomic, strong) id<MTLTexture> texture;
@property (nonatomic) CVDisplayLinkRef display_link;
@property (nonatomic) ScreenInfo screen_info;

- (instancetype)initWithFrame: (CGRect)frameRect screen_info:(ScreenInfo)screen_info;
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong, nonatomic) NSWindow* window;
@property (nonatomic) ScreenInfo screen_info;

- (instancetype)initWithScreenInfo: (ScreenInfo)screen_info;
@end

ScreenInfo screen_info_get(void)
{
    NSScreen *mainScreen = [NSScreen mainScreen];
    NSRect screenFrame = [mainScreen frame];
    CGFloat scaleFactor = mainScreen.backingScaleFactor;

    NSSize nativeResolution;
    nativeResolution.width = screenFrame.size.width * scaleFactor;
    nativeResolution.height = screenFrame.size.height * scaleFactor;

    return (ScreenInfo){.resolution = nativeResolution, .scale_factor = scaleFactor};
}

int main(void)
{
    @autoreleasepool {
        ScreenInfo screen_info = screen_info_get();

        uint64_t audio_buffer_size = SAMPLES_PER_SECOND * 2 * sizeof(AudioSample);
        uint64_t video_buffer_size =
                screen_info.resolution.width * screen_info.resolution.height * sizeof(Color);
        void *memory = mmap(NULL,
                audio_buffer_size + video_buffer_size,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANON,
                -1,
                0);
        global_chess.audio.sample_buffer = memory;
        global_chess.bitmap.memory = (Color *)((uint8_t *)memory + audio_buffer_size);

        // Load image data
        JkPlatformArena storage;
        if (jk_platform_arena_init(&storage, (size_t)1 << 35) == JK_PLATFORM_ARENA_INIT_SUCCESS) {
            JkBuffer image_file = jk_platform_file_read_full("chess_atlas.bmp", &storage);
            if (image_file.size) {
                BitmapHeader *header = (BitmapHeader *)image_file.data;
                Color *pixels = (Color *)(image_file.data + header->offset);
                for (uint64_t y = 0; y < ATLAS_HEIGHT; y++) {
                    uint64_t atlas_y = ATLAS_HEIGHT - y - 1;
                    for (uint64_t x = 0; x < ATLAS_WIDTH; x++) {
                        global_chess.atlas[atlas_y * ATLAS_WIDTH + x] = pixels[y * ATLAS_WIDTH + x].a;
                    }
                }
            } else {
                fprintf(stderr, "Failed to load chess_atlas.bmp\n");
            }
            jk_platform_arena_terminate(&storage);
        }

        NSApplication *application = [NSApplication sharedApplication];

        AppDelegate *applicationDelegate = [[AppDelegate alloc]
                initWithScreenInfo:screen_info];
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
    if (keyCode == 53)
    {
        [self close];
    }
    if (keyCode == 15) {
        global_buttons_down |= BUTTON_FLAG_R;
    }
}

- (void)keyUp:(NSEvent *)anEvent
{
    unsigned short keyCode = [anEvent keyCode];
    if (keyCode == 15) {
        global_buttons_down &= ~BUTTON_FLAG_R;
    }
}

- (void)mouseDown:(NSEvent *)anEvent
{
    global_buttons_down |= BUTTON_FLAG_MOUSE;
}

- (void)mouseUp:(NSEvent *)anEvent
{
    global_buttons_down &= ~BUTTON_FLAG_MOUSE;
}
@end

@implementation AppDelegate
- (instancetype)initWithScreenInfo: (ScreenInfo)screen_info
{
    if (self = [super init]) {
        self.screen_info = screen_info;
    }
    return self;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    NSRect contentSize = NSMakeRect(0.0f, 0.0f, 1280.0f, 720.0f);

    const int style = NSWindowStyleMaskTitled
            | NSWindowStyleMaskClosable
            | NSWindowStyleMaskResizable;
    self.window = [[MyNSWindow alloc] initWithContentRect:contentSize
            styleMask:style
            backing:NSBackingStoreBuffered
            defer:NO];
    [self.window setTitle:@"Chess"];
    [self.window setOpaque:YES];
    [self.window setContentView:[[MetalView alloc] initWithFrame:contentSize
            screen_info:self.screen_info]];
    [self.window makeMainWindow];
    [self.window makeKeyAndOrderFront:nil];
}

- (BOOL) applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)theApplication
{
    return true;
}
@end

@implementation MetalView
- (instancetype)initWithFrame: (CGRect)frameRect screen_info:(ScreenInfo)screen_info
{
    if ((self = [super initWithFrame:frameRect]))
    {
        self.screen_info = screen_info;

        self.device = MTLCreateSystemDefaultDevice();
        if (!self.device)
        {
            NSLog(@"Failed to create Metal device\n");
            return self;
        }

        NSString* source = [[NSString alloc] initWithUTF8String:shaders_code];
        MTLCompileOptions* compileOpts = [[MTLCompileOptions alloc] init];
        compileOpts.languageVersion = MTLLanguageVersion2_0;

        NSError* err = nil;
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
            width:self.screen_info.resolution.width
                height:self.screen_info.resolution.height
                mipmapped:NO];
        self.texture = [self.device newTextureWithDescriptor:texture_descriptor];

        self.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        self.enableSetNeedsDisplay = YES;
        self.preferredFramesPerSecond = 60;

        CVDisplayLinkRef display_link;
        CVDisplayLinkCreateWithActiveCGDisplays(&display_link);
        CVDisplayLinkSetOutputCallback(display_link,
                &display_link_callback,
                (__bridge void *)self);
        CVDisplayLinkSetCurrentCGDisplay(display_link, 0);
        CVDisplayLinkStart(display_link);
    }
    return self;
}

- (void)drawRect:(NSRect)dirtyRect
{
    global_chess.bitmap.width = (uint32_t)self.drawableSize.width;
    global_chess.bitmap.height = (uint32_t)self.drawableSize.height;

    if (self.window) {
        NSPoint mouse_location = [NSEvent mouseLocation];
        NSPoint window_location = [self.window frame].origin;
        CGFloat x = mouse_location.x - window_location.x;
        CGFloat y = mouse_location.y - window_location.y;
        global_chess.input.mouse_pos.x = x * self.screen_info.scale_factor;
        global_chess.input.mouse_pos.y =
                self.drawableSize.height - y * self.screen_info.scale_factor;
    }

    global_chess.input.flags = 0;
    global_chess.input.flags |= ((global_buttons_down >> BUTTON_MOUSE) & 1) << INPUT_CONFIRM;
    global_chess.input.flags |= ((global_buttons_down >> BUTTON_R) & 1) << INPUT_RESET;

    update(&global_chess);
    render(&global_chess);

    // Copy bitmap buffer into texture
    [self.texture replaceRegion:MTLRegionMake2D(0, 0, self.drawableSize.width, self.drawableSize.height)
        mipmapLevel:0
            withBytes:global_chess.bitmap.memory
            bytesPerRow:global_chess.bitmap.width * sizeof(Color)];

    Vertex verticies[4];

    verticies[0].position = simd_make_float4(-1.0f, -1.0f, 0.0f, 1.0f);
    verticies[0].texture_coordinate = simd_make_float2(0.0f, self.drawableSize.height);

    verticies[1].position = simd_make_float4(1.0f, -1.0f, 0.0f, 1.0f);
    verticies[1].texture_coordinate =
            simd_make_float2(self.drawableSize.width, self.drawableSize.height);

    verticies[2].position = simd_make_float4(-1.0f, 1.0f, 0.0f, 1.0f);
    verticies[2].texture_coordinate = simd_make_float2(0.0f, 0.0f);

    verticies[3].position = simd_make_float4(1.0f, 1.0f, 0.0f, 1.0f);
    verticies[3].texture_coordinate = simd_make_float2(self.drawableSize.width, 0.0f);

    memcpy([self.vertex_buffer contents], verticies, sizeof(verticies));

    id<CAMetalDrawable> drawable = self.currentDrawable;
    id<MTLTexture> texture = drawable.texture;

    MTLRenderPassDescriptor* passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    passDescriptor.colorAttachments[0].texture = texture;
    passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    //passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.85, 0.85, 0.85, 1.0);

    id<MTLCommandBuffer> commandBuffer = [self.command_queue commandBuffer];

    id<MTLRenderCommandEncoder> commandEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
    [commandEncoder setRenderPipelineState:self.pipeline];
    [commandEncoder setVertexBuffer:self.vertex_buffer offset:0 atIndex:0];
    [commandEncoder setFragmentTexture:self.texture atIndex:0];
    [commandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount: 4];
    [commandEncoder endEncoding];

    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
}
@end
