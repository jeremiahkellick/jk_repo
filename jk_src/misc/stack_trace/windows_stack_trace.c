#include <windows.h>

#include <dbghelp.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

#pragma comment(lib, "dbghelp.lib")

static SRWLOCK jk_stack_trace_lock = SRWLOCK_INIT;
static uint8_t jk_stack_trace_module_buffer[1024];

JK_NOINLINE JkBuffer stack_trace(JkBuffer buffer, int64_t skip, int64_t indent)
{
    AcquireSRWLockExclusive(&jk_stack_trace_lock);

    JkBuffer result = {.size = 0, .data = buffer.data};

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

        for (int64_t frame_num = 0; result.size < buffer.size
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

            // Indent
            for (int64_t i = 0; i < indent && result.size < buffer.size; i++, result.size++) {
                result.data[result.size] = ' ';
            }

            result.size += snprintf((char *)(result.data + result.size),
                    buffer.size - result.size,
                    "0x%012llx in %.*s",
                    offset,
                    (int)function_name.size,
                    function_name.data);
            if (file_name.size) {
                result.size += snprintf((char *)(result.data + result.size),
                        buffer.size - result.size,
                        " at %.*s:%lld\n",
                        (int)file_name.size,
                        file_name.data,
                        line);
            } else {
                result.size += snprintf((char *)(result.data + result.size),
                        buffer.size - result.size,
                        " from %.*s\n",
                        (int)module_name.size,
                        module_name.data);
            }

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
    jk_platform_print(stack_trace(stack_trace_buffer, 1, 2));
}

void bar(void)
{
    foo();
}

void baz(void)
{
    bar();
}

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    baz();

    return 0;
}
