#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void get_dir(const char *path, char *dir, int sz)
{
    const char *bs = strrchr(path, '\\');
    const char *fs = strrchr(path, '/');
    const char *last = fs && (!bs || fs > bs) ? fs : bs;
    int len;
    if (!last) { dir[0] = 0; return; }
    len = (int)(last - path);
    if (len >= sz) len = sz - 1;
    memcpy(dir, path, (unsigned)len);
    dir[len] = 0;
}

int main(int argc, char *argv[])
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char dll_path[MAX_PATH], launcher_dir[MAX_PATH], target_dir[MAX_PATH], config_path[MAX_PATH];
    char cmd[8192];
    int inject = 1;
    HMODULE hKernel32;
    FARPROC pLoadLibraryW;
    void *remote_mem;
    wchar_t wide_dll[MAX_PATH];
    unsigned char dll_path_bytes[MAX_PATH * 2];
    HANDLE hRemoteThread;
    DWORD wait_result, exit_code;
    DWORD i;
    int exe_arg;
    if (argc < 3) {
        fprintf(stderr, "Usage: %s [--no-inject] <config.json> <exe> [args...]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--no-inject") == 0) {
        inject = 0;
        exe_arg = 2;
    } else {
        exe_arg = 2;
    }

    GetModuleFileNameA(NULL, launcher_dir, sizeof(launcher_dir));
    get_dir(launcher_dir, launcher_dir, sizeof(launcher_dir));
    sprintf(dll_path, "%s\\GoldMapperLib.dll", launcher_dir);
    if (inject && GetFileAttributesA(dll_path) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "Error: %s not found\n", dll_path);
        return 1;
    }

    get_dir(argv[exe_arg], target_dir, sizeof(target_dir));
    cmd[0] = 0;
    for (i = (DWORD)exe_arg; i < (DWORD)argc; i++) {
        if (i > (DWORD)exe_arg) strcat(cmd, " ");
        strcat(cmd, argv[i]);
    }

    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, target_dir[0] ? target_dir : NULL, &si, &pi)) {
        fprintf(stderr, "Error: CreateProcess failed (%lu)\n", GetLastError());
        return 1;
    }
    fprintf(stderr, "Process created (PID %lu)\n", pi.dwProcessId);
    if (!inject) goto resume;
    sprintf(config_path, "%s\\.goldmapper_tmp.json", launcher_dir);
    if (!CopyFileA(argv[1], config_path, FALSE)) {
        fprintf(stderr, "Error: CopyFile failed (%lu)\n", GetLastError());
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 1;
    }

    hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        fprintf(stderr, "Error: kernel32.dll not found\n");
        goto fail;
    }
    pLoadLibraryW = GetProcAddress(hKernel32, "LoadLibraryW");
    if (!pLoadLibraryW) {
        fprintf(stderr, "Error: LoadLibraryW not found\n");
        goto fail;
    }
    {
        DWORD wl = MultiByteToWideChar(CP_ACP, 0, dll_path, -1, wide_dll, MAX_PATH);
        int len_bytes = (int)(wl * sizeof(wchar_t));
        memcpy(dll_path_bytes, wide_dll, len_bytes);
        remote_mem = VirtualAllocEx(pi.hProcess, NULL, (SIZE_T)len_bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remote_mem) {
            fprintf(stderr, "Error: VirtualAllocEx failed (%lu)\n", GetLastError());
            goto fail;
        }

        if (!WriteProcessMemory(pi.hProcess, remote_mem, dll_path_bytes, (SIZE_T)len_bytes, NULL)) {
            fprintf(stderr, "Error: WriteProcessMemory failed (%lu)\n", GetLastError());
            VirtualFreeEx(pi.hProcess, remote_mem, 0, MEM_RELEASE);
            goto fail;
        }

        hRemoteThread = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryW, remote_mem, 0, NULL);
        if (!hRemoteThread) {
            fprintf(stderr, "Error: CreateRemoteThread failed (%lu)\n", GetLastError());
            VirtualFreeEx(pi.hProcess, remote_mem, 0, MEM_RELEASE);
            goto fail;
        }

        fprintf(stderr, "Injecting GoldMapperLib.dll...\n");
        wait_result = WaitForSingleObject(hRemoteThread, 10000);
        if (wait_result != WAIT_OBJECT_0) {
            fprintf(stderr, "Error: DLL injection timeout\n");
            CloseHandle(hRemoteThread);
            VirtualFreeEx(pi.hProcess, remote_mem, 0, MEM_RELEASE);
            goto fail;
        }

        GetExitCodeThread(hRemoteThread, &exit_code);
        CloseHandle(hRemoteThread);
        VirtualFreeEx(pi.hProcess, remote_mem, 0, MEM_RELEASE);
        if (exit_code == 0) {
            fprintf(stderr, "Error: LoadLibraryW returned NULL\n");
            goto fail;
        }

        fprintf(stderr, "DLL loaded (module handle is 0x%lX btw)\n", exit_code);
    }

resume:
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, 3000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    fprintf(stderr, "Done.\n");
    return 0;

fail:
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 1;
}
