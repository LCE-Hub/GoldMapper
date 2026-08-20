#include "gm.h"
#include <stdio.h>
static HINSTANCE g_hModule = NULL;
static const char *s_config_name = ".goldmapper_tmp.json";
static void get_dll_dir(char *out, int sz)
{
    char dll_path[MAX_PATH];
    char *bs;
    GetModuleFileNameA(g_hModule, dll_path, MAX_PATH);
    bs = strrchr(dll_path, '\\');
    if (bs) {
        int len = (int)(bs - dll_path);
        if (len >= sz) len = sz - 1;
        memcpy(out, dll_path, (unsigned)len);
        out[len] = '\0';
    } else {
        out[0] = '\0';
    }
}

static DWORD WINAPI auto_init_thread(LPVOID param)
{
    char config_path[MAX_PATH];
    char *buf;
    FILE *f;
    long size;
    (void)param;
    gm_hook_init();
    Sleep(2000);
    get_dll_dir(config_path, MAX_PATH);
    strcat(config_path, "\\");
    strcat(config_path, s_config_name);
    f = fopen(config_path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return 0;
    }

    buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return 0;
    }

    fread(buf, 1, (size_t)size, f);
    buf[size] = '\0';
    fclose(f);
    gm_parse_config(buf, (int)size, &g_config);
    free(buf);
    DeleteFileA(config_path);
    dbg_log("[INIT] config loaded: %d mappings\n", g_config.count);
    {
        int i;
        for (i = 0; i < g_config.count; i++) {
            GmMapping *m = &g_config.mappings[i];
            dbg_log("[INIT]   mapping[%d]: from_dev=%d from_code=0x%X -> to_dev=%d to_code=0x%X\n",
                i, m->from_device, m->from_code, m->to_device, m->to_code);
        }
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    HANDLE hThread;
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hModule = hinstDLL;
        hThread = CreateThread(NULL, 0, auto_init_thread, NULL, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}

__declspec(dllexport) void __cdecl GoldMapper_Init(const char *config_path)
{
    FILE *f;
    long size;
    char *buf;
    if (!config_path) return;
    f = fopen(config_path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return;
    }

    buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return;
    }

    fread(buf, 1, (size_t)size, f);
    buf[size] = '\0';
    fclose(f);
    gm_parse_config(buf, (int)size, &g_config);
    free(buf);
    DeleteFileA(config_path); //neo: dont be confused, `config_path` here is the tmp config not the config itself
}

__declspec(dllexport) void __cdecl GoldMapper_Shutdown(void)
{
    gm_hook_shutdown();
}
