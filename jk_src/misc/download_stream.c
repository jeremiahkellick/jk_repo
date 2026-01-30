#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "%s: Expected 1 argument, got %d\n", argv[0], argc - 1);
        printf("Usage: %s URL\n", argv[0]);
        exit(1);
    }

    JkBuffer url = jk_buffer_from_null_terminated(argv[1]);

    for (;;) {
        // Get file name from current time
        time_t now;
        char buffer[4096];
        time(&now);
        struct tm *info = localtime(&now);
        strftime(buffer, JK_ARRAY_COUNT(buffer), "%Y-%m-%d_%H-%M", info);
        strcat(buffer, ".mp4");
        JkBuffer file_name = jk_buffer_from_null_terminated(buffer);

        JkBuffer arguments[] = {
            JKSI("yt-dlp"),
            JKSI("-o"),
            file_name,
            url,
        };
        JkBufferArray command = {.count = JK_ARRAY_COUNT(arguments), .e = arguments};

        jk_platform_exec(command);

        jk_platform_sleep(300000);
    }
}
