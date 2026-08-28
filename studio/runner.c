#include "runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct StudioRunnerThreadData {
    HANDLE read_pipe;
    HANDLE process_handle;
    HWND notify_hwnd;
    StudioProcessKind kind;
} StudioRunnerThreadData;

static DWORD WINAPI studio_runner_reader_thread(LPVOID parameter) {
    StudioRunnerThreadData *data = (StudioRunnerThreadData *) parameter;
    char buffer[512];
    DWORD bytes_read = 0;
    BOOL ok;

    for (;;) {
        ok = ReadFile(data->read_pipe, buffer, sizeof(buffer) - 1, &bytes_read, NULL);
        if (!ok || bytes_read == 0) {
            break;
        }
        buffer[bytes_read] = '\0';

        StudioRunnerChunk *chunk = (StudioRunnerChunk *) calloc(1, sizeof(*chunk));
        if (chunk) {
            chunk->kind = data->kind;
            strcpy(chunk->stream, "stdout");
            chunk->text = _strdup(buffer);
            PostMessageA(data->notify_hwnd, STUDIO_WM_RUNNER_OUTPUT, 0, (LPARAM) chunk);
        }
    }

    if (data->process_handle) {
        StudioRunnerExit *exit_message = (StudioRunnerExit *) calloc(1, sizeof(*exit_message));
        DWORD exit_code = 0;
        WaitForSingleObject(data->process_handle, INFINITE);
        GetExitCodeProcess(data->process_handle, &exit_code);
        if (exit_message) {
            exit_message->kind = data->kind;
            exit_message->exit_code = exit_code;
            PostMessageA(data->notify_hwnd, STUDIO_WM_RUNNER_EXIT, 0, (LPARAM) exit_message);
        }
    }

    CloseHandle(data->read_pipe);
    free(data);
    return 0;
}

void studio_runner_init(StudioRunner *runner, HWND notify_hwnd) {
    memset(runner, 0, sizeof(*runner));
    runner->notify_hwnd = notify_hwnd;
}

bool studio_runner_start(
    StudioRunner *runner,
    StudioProcessKind kind,
    const char *exe_path,
    const char *arguments,
    const char *working_directory,
    ErError *error
) {
    SECURITY_ATTRIBUTES attributes;
    STARTUPINFOA startup;
    PROCESS_INFORMATION process_info;
    HANDLE read_pipe = NULL;
    HANDLE write_pipe = NULL;
    StudioRunnerThreadData *thread_data = NULL;
    char command_line[4096];

    if (runner->running) {
        er_error_set(error, 0, 0, "A process is already running");
        return false;
    }

    memset(&attributes, 0, sizeof(attributes));
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;

    if (!CreatePipe(&read_pipe, &write_pipe, &attributes, 0)) {
        er_error_set(error, 0, 0, "Could not create process pipe");
        return false;
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    memset(&startup, 0, sizeof(startup));
    memset(&process_info, 0, sizeof(process_info));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = write_pipe;
    startup.hStdError = write_pipe;

    snprintf(command_line, sizeof(command_line), "\"%s\" %s", exe_path, arguments ? arguments : "");
    if (!CreateProcessA(
            NULL,
            command_line,
            NULL,
            NULL,
            TRUE,
            CREATE_NO_WINDOW,
            NULL,
            working_directory && working_directory[0] != '\0' ? working_directory : NULL,
            &startup,
            &process_info)) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        er_error_set(error, 0, 0, "Could not start process: %s", exe_path);
        return false;
    }

    CloseHandle(write_pipe);
    CloseHandle(process_info.hThread);

    thread_data = (StudioRunnerThreadData *) calloc(1, sizeof(*thread_data));
    if (!thread_data) {
        TerminateProcess(process_info.hProcess, 1);
        CloseHandle(process_info.hProcess);
        CloseHandle(read_pipe);
        er_error_set(error, 0, 0, "Out of memory while starting process reader");
        return false;
    }

    thread_data->read_pipe = read_pipe;
    thread_data->process_handle = process_info.hProcess;
    thread_data->notify_hwnd = runner->notify_hwnd;
    thread_data->kind = kind;

    runner->thread_handle = CreateThread(NULL, 0, studio_runner_reader_thread, thread_data, 0, NULL);
    if (!runner->thread_handle) {
        TerminateProcess(process_info.hProcess, 1);
        CloseHandle(process_info.hProcess);
        CloseHandle(read_pipe);
        free(thread_data);
        er_error_set(error, 0, 0, "Could not create process reader thread");
        return false;
    }

    runner->process_handle = process_info.hProcess;
    runner->process_id = process_info.dwProcessId;
    runner->active_kind = kind;
    runner->running = true;
    return true;
}

void studio_runner_stop(StudioRunner *runner) {
    if (!runner || !runner->running || !runner->process_handle) {
        return;
    }
    TerminateProcess(runner->process_handle, 1);
}

void studio_runner_dispose(StudioRunner *runner) {
    if (!runner) {
        return;
    }
    if (runner->running && runner->process_handle) {
        TerminateProcess(runner->process_handle, 1);
    }
    if (runner->thread_handle) {
        WaitForSingleObject(runner->thread_handle, 2000);
        CloseHandle(runner->thread_handle);
        runner->thread_handle = NULL;
    }
    if (runner->process_handle) {
        CloseHandle(runner->process_handle);
        runner->process_handle = NULL;
    }
    runner->running = false;
    runner->active_kind = STUDIO_PROCESS_NONE;
}
