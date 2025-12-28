#include <windows.h>

#include <dbghelp.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

#pragma comment(lib, "dbghelp.lib")

static SRWLOCK jk_stack_trace_lock = SRWLOCK_INIT;
static uint8_t jk_stack_trace_module_buffer[1024];

static void jk_stack_trace_null_terminated(int64_t max_size, JkBuffer *result, char *string)
{
    for (; result->size < max_size && *string; string++) {
        result->data[result->size++] = *string;
    }
}

static void jk_stack_trace_append_buffer(int64_t max_size, JkBuffer *result, JkBuffer buffer)
{
    int64_t length = JK_MIN(buffer.size, max_size - result->size);
    for (int64_t i = 0; i < length; i++) {
        result->data[result->size++] = buffer.data[i];
    }
}

JK_NOINLINE JkBuffer stack_trace(JkBuffer result_buffer, int64_t skip)
{
    AcquireSRWLockExclusive(&jk_stack_trace_lock);

    JkBuffer result = {.size = 0, .data = result_buffer.data};

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    if (SymInitialize(process, NULL, TRUE)) {
        SymSetOptions(SYMOPT_LOAD_LINES);

        CONTEXT context = {0};
        context.ContextFlags = CONTEXT_FULL;
        RtlCaptureContext(&context);

        STACKFRAME frame = {0};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;

        for (int64_t frame_num = 0; result.size < result_buffer.size
                && StackWalk(IMAGE_FILE_MACHINE_AMD64,
                        process,
                        thread,
                        &frame,
                        &context,
                        NULL,
                        SymFunctionTableAccess,
                        SymGetModuleBase,
                        NULL);
                frame_num++) {
            if (frame_num < skip) {
                continue;
            }
            uint64_t offset = frame.AddrPC.Offset;

            JkBuffer module_name = {0};
            uint64_t module_base = SymGetModuleBase(process, offset);
            if (module_base) {
                uint64_t length = GetModuleFileNameA((HINSTANCE)module_base,
                        (char *)jk_stack_trace_module_buffer,
                        JK_ARRAY_COUNT(jk_stack_trace_module_buffer));
                if (length) {
                    module_name = jk_path_basename((JkBuffer){
                        .size = (int64_t)length, .data = jk_stack_trace_module_buffer});
                }
            }

            JkBuffer function_name = JKSI("Unknown function");
            char symbolBuffer[sizeof(IMAGEHLP_SYMBOL) + 255];
            PIMAGEHLP_SYMBOL symbol = (PIMAGEHLP_SYMBOL)symbolBuffer;
            symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL) + 255;
            symbol->MaxNameLength = 254;
            DWORD64 not_used_0;
            if (SymGetSymFromAddr(process, offset, &not_used_0, symbol)) {
                function_name.size = 0;
                while (symbol->Name[function_name.size]
                        && function_name.size < symbol->MaxNameLength) {
                    function_name.size++;
                }
                function_name.data = (uint8_t *)symbol->Name;
            }

            IMAGEHLP_LINE line_data;
            line_data.SizeOfStruct = sizeof(IMAGEHLP_LINE);

            JkBuffer file_name = {0};
            uint64_t line = 0;
            DWORD not_used_1;
            if (SymGetLineFromAddr(process, offset, &not_used_1, &line_data)) {
                while (line_data.FileName[file_name.size] && file_name.size < 1024) {
                    file_name.size++;
                }
                file_name.data = (uint8_t *)line_data.FileName;
                line = line_data.LineNumber;
            }

            { // Write program counter in hexadecimal
                jk_stack_trace_null_terminated(result_buffer.size, &result, "    0x");
                int64_t start_pos = result.size;
                int64_t width_remaining = 16;
                while (result.size < result_buffer.size && (0 < width_remaining || offset)) {
                    result.data[result.size++] = jk_hex_char[offset & 0xf];
                    offset >>= 4;
                    width_remaining--;
                }
                for (int64_t i = 0; i < (result.size - start_pos) / 2; i++) {
                    JK_SWAP(result.data[start_pos + i], result.data[result.size - 1 - i], uint8_t);
                }
            }

            jk_stack_trace_null_terminated(result_buffer.size, &result, " in ");
            jk_stack_trace_append_buffer(result_buffer.size, &result, function_name);

            if (file_name.size) {
                jk_stack_trace_null_terminated(result_buffer.size, &result, " at ");
                jk_stack_trace_append_buffer(result_buffer.size, &result, file_name);
                jk_stack_trace_null_terminated(result_buffer.size, &result, ":");

                { // Write line number in decimal
                    int64_t start_pos = result.size;
                    int64_t width_remaining = 1;
                    while (result.size < result_buffer.size && (0 < width_remaining || line)) {
                        result.data[result.size++] = '0' + (line % 10);
                        line /= 10;
                        width_remaining--;
                    }
                    for (int64_t i = 0; i < (result.size - start_pos) / 2; i++) {
                        JK_SWAP(result.data[start_pos + i],
                                result.data[result.size - 1 - i],
                                uint8_t);
                    }
                }
            } else if (module_name.size) {
                jk_stack_trace_null_terminated(result_buffer.size, &result, " from ");
                jk_stack_trace_append_buffer(result_buffer.size, &result, module_name);
            }
            jk_stack_trace_null_terminated(result_buffer.size, &result, "\n");

            if (jk_buffer_compare(function_name, JKS("main")) == 0
                    || jk_buffer_compare(function_name, JKS("WinMain")) == 0) {
                break;
            }
        }
        SymCleanup(process);
    }
    ReleaseSRWLockExclusive(&jk_stack_trace_lock);

    return result;
}

uint8_t stack_trace_byte_array[4096];
JkBuffer stack_trace_buffer = JK_BUFFER_INIT_FROM_BYTE_ARRAY(stack_trace_byte_array);

void foo(void)
{
    jk_platform_print_stdout(stack_trace(stack_trace_buffer, 1));
}

void bar(void)
{
    foo();
}

void baz(void)
{
    bar();
}

int main(void)
{
    baz();

    return 0;
}
