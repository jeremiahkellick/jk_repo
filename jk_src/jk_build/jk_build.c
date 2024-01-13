#include <windows.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUF_SIZE 4096
#define MAX_SEARCH_LEVELS 20

typedef enum PathInfo {
    PATH_INFO_ACCESS_FAILED,
    PATH_INFO_DIRECTORY,
    PATH_INFO_OTHER,
} PathInfo;

PathInfo get_path_info(char *path)
{
    struct stat info;
    if (stat(path, &info) != 0) {
        return PATH_INFO_ACCESS_FAILED;
    } else if (info.st_mode & S_IFDIR) {
        return PATH_INFO_DIRECTORY;
    } else {
        return PATH_INFO_OTHER;
    }
}

int main(int argc, char **argv)
{
    STARTUPINFO si = {.cb = sizeof(si)};
    PROCESS_INFORMATION pi = {0};

    if (argc != 2) {
        fprintf(stderr, "Usage: %s [program main source file]\n", argv[0]);
        exit(1);
    }

    // Ensure file given as the first argument exists
    if (get_path_info(argv[1]) != PATH_INFO_OTHER) {
        fprintf(stderr, "%s: Invalid build path \"%s\"\n", argv[0], argv[1]);
        exit(1);
    }

    // Find repository root directory from a source file path by going up the file tree looking for
    // a parent directory that contains a 'jk_src' directory. If we find one we assume that's the
    // jk_repo root.
    char root_dir[BUF_SIZE];
    char base_name[BUF_SIZE];
    {
        strncpy(root_dir, argv[1], BUF_SIZE);
        size_t length = strlen(root_dir);
        int last_slash = -1;
        for (int i = 0; i < length; i++) {
            if (root_dir[i] == '/' || root_dir[i] == '\\') {
                last_slash = i;
            }
        }

        // Write source file base name
        int last_dot = length;
        for (int i = last_slash + 1; i < length; i++) {
            if (root_dir[i] == '.') {
                last_dot = i;
            }
        }
        root_dir[last_dot] = '\0';
        strcpy(base_name, &root_dir[last_slash + 1]);

        if (last_slash == -1) {
            strcpy(root_dir, ".\\");
        } else {
            length = last_slash + 1;
            if (length + 1 > BUF_SIZE) {
                fprintf(stderr, "%s: Insufficient BUF_SIZE\n", argv[0]);
                exit(1);
            }
            root_dir[length] = '\0';
        }

        bool found = false;
        int i = 0;
        while (!found) {
            if (i > MAX_SEARCH_LEVELS) {
                fprintf(stderr,
                        "%s: Couldn't find jk_repo root after searching %d levels up from the "
                        "source file",
                        argv[0],
                        MAX_SEARCH_LEVELS);
                exit(1);
            }
            if (length + 10 > BUF_SIZE) {
                fprintf(stderr, "%s: Insufficient BUF_SIZE\n", argv[0]);
                exit(1);
            }
            strcat(root_dir, "..\\jk_src");
            found = get_path_info(root_dir) == PATH_INFO_DIRECTORY;
            length += 3; // keep the appended "..\" but discard the "jk_src"
            root_dir[length] = '\0';
            i++;
        }
    }

    char command[BUF_SIZE];
    int length = snprintf(command,
            BUF_SIZE,
            "cl %s /W4 /D _CRT_SECURE_NO_WARNINGS /Zi /std:c++20 /EHsc /link "
            "/out:\"%sbuild\\%s.exe\"",
            argv[1],
            root_dir,
            base_name);
    if (length + 1 > BUF_SIZE) {
        fprintf(stderr, "%s: Insufficient BUF_SIZE\n", argv[0]);
        exit(1);
    }

    printf("%s\n", command);

    // Start the child process.
    if (!CreateProcess(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "%s: Error attempting to run this command:\n%s\n", argv[0], command);
        DWORD error_code = GetLastError();
        if (error_code == 0) {
            fprintf(stderr, "Unknown\n");
        } else {
            char message_buf[BUF_SIZE] = {'\0'};
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,
                    error_code,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    message_buf,
                    BUF_SIZE - 1,
                    NULL);
            fprintf(stderr, "%s", message_buf);
        }
        exit(1);
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 0;
}
