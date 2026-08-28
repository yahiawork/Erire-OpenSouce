#ifndef ERIRE_STUDIO_RUNNER_H
#define ERIRE_STUDIO_RUNNER_H

#include <stdbool.h>
#include <windows.h>

#include "error.h"

#define STUDIO_WM_RUNNER_OUTPUT (WM_APP + 40)
#define STUDIO_WM_RUNNER_EXIT (WM_APP + 41)

typedef enum StudioProcessKind {
    STUDIO_PROCESS_NONE = 0,
    STUDIO_PROCESS_RUN,
    STUDIO_PROCESS_CHECK,
    STUDIO_PROCESS_BUILD,
    STUDIO_PROCESS_PYTHON,
    STUDIO_PROCESS_TERMINAL
} StudioProcessKind;

typedef struct StudioRunnerChunk {
    StudioProcessKind kind;
    char stream[16];
    char *text;
} StudioRunnerChunk;

typedef struct StudioRunnerExit {
    StudioProcessKind kind;
    DWORD exit_code;
} StudioRunnerExit;

typedef struct StudioRunner {
    HWND notify_hwnd;
    HANDLE process_handle;
    HANDLE thread_handle;
    HANDLE stop_event;
    DWORD process_id;
    StudioProcessKind active_kind;
    bool running;
} StudioRunner;

void studio_runner_init(StudioRunner *runner, HWND notify_hwnd);
bool studio_runner_start(
    StudioRunner *runner,
    StudioProcessKind kind,
    const char *exe_path,
    const char *arguments,
    const char *working_directory,
    ErError *error
);
void studio_runner_stop(StudioRunner *runner);
void studio_runner_dispose(StudioRunner *runner);

#endif
