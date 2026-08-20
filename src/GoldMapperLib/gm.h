#ifndef GM_H
#define GM_H
#include <windows.h>
#define GM_MAX_MAPPINGS 128
#define GM_DEVICE_KEY 0
#define GM_DEVICE_PAD 1
#define GM_DEVICE_MOUSE 2
#define GM_PAD_IDX_ANY (-1)
#define GM_SYNTH_SCANCODE 0x42
typedef struct {
    int from_device;
    int from_code;
    int from_pad_idx;
    int to_device;
    int to_code;
    int to_pad_idx;
} GmMapping;
typedef struct {
    GmMapping mappings[GM_MAX_MAPPINGS];
    int count;
} GmConfig;

extern GmConfig g_config;
extern WNDPROC g_origWndProc;
int gm_parse_config(const char *json, int len, GmConfig *cfg);
int gm_resolve_input(const char *name, int *device, int *code, int *pad_idx);
int gm_resolve_output(const char *name, int *device, int *code, int *pad_idx);
void gm_hook_init(void);
void gm_hook_shutdown(void);
#endif
