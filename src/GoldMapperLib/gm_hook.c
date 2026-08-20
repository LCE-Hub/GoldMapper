#include "gm.h"
#include <string.h>
#include <xinput.h>
#include <MinHook.h>
#include <stdio.h>
#include <stdarg.h>
#include <objbase.h>
typedef int SDL_bool;
typedef short Sint16;
typedef unsigned int Uint32;
#define SDLCALL __cdecl
typedef struct _SDL_GameController SDL_GameController;
#define SDL_INIT_GAMECONTROLLER 0x00002000
enum {
    SDL_CONTROLLER_BUTTON_A = 0,
    SDL_CONTROLLER_BUTTON_B,
    SDL_CONTROLLER_BUTTON_X,
    SDL_CONTROLLER_BUTTON_Y,
    SDL_CONTROLLER_BUTTON_BACK,
    SDL_CONTROLLER_BUTTON_GUIDE,
    SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_LEFTSTICK,
    SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    SDL_CONTROLLER_BUTTON_DPAD_UP,
    SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    SDL_CONTROLLER_BUTTON_MAX
};

enum {
    SDL_CONTROLLER_AXIS_LEFTX = 0,
    SDL_CONTROLLER_AXIS_LEFTY,
    SDL_CONTROLLER_AXIS_RIGHTX,
    SDL_CONTROLLER_AXIS_RIGHTY,
    SDL_CONTROLLER_AXIS_TRIGGERLEFT,
    SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
    SDL_CONTROLLER_AXIS_MAX
};

static int (SDLCALL *pfnSDL_Init)(Uint32);
static const char * (SDLCALL *pfnSDL_GetError)(void);
static int (SDLCALL *pfnSDL_NumJoysticks)(void);
static SDL_bool (SDLCALL *pfnSDL_IsGameController)(int);
static SDL_GameController * (SDLCALL *pfnSDL_GameControllerOpen)(int);
static const char * (SDLCALL *pfnSDL_GameControllerName)(SDL_GameController *);
static void (SDLCALL *pfnSDL_GameControllerUpdate)(void);
static unsigned char (SDLCALL *pfnSDL_GameControllerGetButton)(SDL_GameController *, int);
static Sint16 (SDLCALL *pfnSDL_GameControllerGetAxis)(SDL_GameController *, int);
static HMODULE s_sdl_module = NULL;
static FILE *g_log = NULL;
void dbg_log(const char *fmt, ...)
{
    va_list args;
    if (!g_log) {
        g_log = fopen("goldmapper.log", "a");
        if (!g_log) return;
    }
    va_start(args, fmt);
    vfprintf(g_log, fmt, args);
    va_end(args);
    fflush(g_log);
}

typedef DWORD (WINAPI *XInputGetState_t)(DWORD, XINPUT_STATE *);
static XInputGetState_t RealXInputGetState = NULL;
WNDPROC g_origWndProc = NULL;
static const int s_mouse_vk[3] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON };
static WORD s_prev_pad_buttons[4];
static int s_prev_mouse_state[3];
static unsigned char s_dinput_buttons[128];
static unsigned char s_prev_dinput_buttons[128];
static short s_dinput_lx;
static short s_dinput_ly;
static short s_dinput_rx;
static short s_dinput_ry;
static unsigned char s_dinput_lt;
static unsigned char s_dinput_rt;
static SDL_GameController *s_sdl_controller = NULL;
static void ensure_dinput_device(void);
static void poll_dinput(void);
static void synth_key_event(int vk, int up)
{
    INPUT inp;
    memset(&inp, 0, sizeof(inp));
    inp.type = INPUT_KEYBOARD;
    inp.ki.wVk = (WORD)vk;
    inp.ki.wScan = GM_SYNTH_SCANCODE;
    if (up) inp.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &inp, sizeof(inp));
}

static void synth_mouse_event(int btn, int up)
{
    INPUT inp;
    DWORD flags;
    memset(&inp, 0, sizeof(inp));
    inp.type = INPUT_MOUSE;
    flags = 0;
    if (btn == 0) flags = up ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN;
    else if (btn == 1) flags = up ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN;
    else if (btn == 2) flags = up ? MOUSEEVENTF_MIDDLEUP : MOUSEEVENTF_MIDDLEDOWN;
    inp.mi.dwFlags = flags;
    SendInput(1, &inp, sizeof(inp));
}

static int is_mouse_down(int btn)
{
    if (btn < 0 || btn > 2) return 0;
    return (GetAsyncKeyState(s_mouse_vk[btn]) & 0x8000) != 0;
}

static int is_key_down(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static int is_mouse_mapped_to_other(int btn)
{
    int i;
    for (i = 0; i < g_config.count; i++) {
        GmMapping *m = &g_config.mappings[i];
        if (m->from_device == GM_DEVICE_MOUSE && m->from_code == btn) {
            if (m->to_device != GM_DEVICE_MOUSE) return 1;
        }
    }
    return 0;
}

static int find_mouse_to_mouse_remap(int btn, int *out_btn)
{
    int i;
    for (i = 0; i < g_config.count; i++) {
        GmMapping *m = &g_config.mappings[i];
        if (m->from_device == GM_DEVICE_MOUSE && m->from_code == btn && m->to_device == GM_DEVICE_MOUSE && m->to_code != btn) {
            *out_btn = m->to_code;
            return 1;
        }
    }
    return 0;
}

static int mouse_msg_for_btn(int btn, int up)
{
    if (btn == 0) return up ? WM_LBUTTONUP : WM_LBUTTONDOWN;
    if (btn == 1) return up ? WM_RBUTTONUP : WM_RBUTTONDOWN;
    if (btn == 2) return up ? WM_MBUTTONUP : WM_MBUTTONDOWN;
    return 0;
}

static int is_key_mapped_to_other(int vk)
{
    int i;
    for (i = 0; i < g_config.count; i++) {
        GmMapping *m = &g_config.mappings[i];
        if (m->from_device == GM_DEVICE_KEY && m->from_code == vk) {
            if (m->to_device != GM_DEVICE_KEY) return 1;
        }
    }
    return 0;
}

static int find_key_to_key_remap(int vk, int *out_vk)
{
    int i;
    for (i = 0; i < g_config.count; i++) {
        GmMapping *m = &g_config.mappings[i];
        if (m->from_device == GM_DEVICE_KEY && m->from_code == vk && m->to_device == GM_DEVICE_KEY && m->to_code != vk) {
            *out_vk = m->to_code;
            return 1;
        }
    }
    return 0;
}

static LRESULT CALLBACK gm_wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int vk, scan, mapped_vk, target_btn, i;
    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        vk = (int)wParam;
        scan = (int)((lParam >> 16) & 0xFF);
        if (scan == GM_SYNTH_SCANCODE) break;
        if (find_key_to_key_remap(vk, &mapped_vk)) {
            for (i = 0; i < g_config.count; i++) {
                GmMapping *m = &g_config.mappings[i];
                if (m->from_device == GM_DEVICE_KEY && m->from_code == vk && m->to_device == GM_DEVICE_KEY) {
                    synth_key_event(m->to_code, 0);
                    return 0;
                }
            }
        }

        if (is_key_mapped_to_other(vk)) {
            return 0;
        }
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        vk = (int)wParam;
        scan = (int)((lParam >> 16) & 0xFF);
        if (scan == GM_SYNTH_SCANCODE) break;
        if (find_key_to_key_remap(vk, &mapped_vk)) {
            for (i = 0; i < g_config.count; i++) {
                GmMapping *m = &g_config.mappings[i];
                if (m->from_device == GM_DEVICE_KEY && m->from_code == vk && m->to_device == GM_DEVICE_KEY) {
                    synth_key_event(m->to_code, 1);
                    return 0;
                }
            }
        }

        if (is_key_mapped_to_other(vk)) {
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        if (msg == WM_LBUTTONDOWN) {
            if (find_mouse_to_mouse_remap(0, &target_btn)) {
                int nmsg = mouse_msg_for_btn(target_btn, 0);
                if (nmsg) PostMessageW(hWnd, nmsg, wParam, lParam);
                return 0;
            }
            if (is_mouse_mapped_to_other(0)) return 0;
        } else {
            if (find_mouse_to_mouse_remap(0, &target_btn)) {
                int nmsg = mouse_msg_for_btn(target_btn, 1);
                if (nmsg) PostMessageW(hWnd, nmsg, wParam, lParam);
                return 0;
            }
            if (is_mouse_mapped_to_other(0)) return 0;
        }
        break;

    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        if (msg == WM_RBUTTONDOWN) {
            if (find_mouse_to_mouse_remap(1, &target_btn)) {
                int nmsg = mouse_msg_for_btn(target_btn, 0);
                if (nmsg) PostMessageW(hWnd, nmsg, wParam, lParam);
                return 0;
            }
            if (is_mouse_mapped_to_other(1)) return 0;
        } else {
            if (find_mouse_to_mouse_remap(1, &target_btn)) {
                int nmsg = mouse_msg_for_btn(target_btn, 1);
                if (nmsg) PostMessageW(hWnd, nmsg, wParam, lParam);
                return 0;
            }
            if (is_mouse_mapped_to_other(1)) return 0;
        }
        break;

    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
        if (msg == WM_MBUTTONDOWN) {
            if (find_mouse_to_mouse_remap(2, &target_btn)) {
                int nmsg = mouse_msg_for_btn(target_btn, 0);
                if (nmsg) PostMessageW(hWnd, nmsg, wParam, lParam);
                return 0;
            }
            if (is_mouse_mapped_to_other(2)) return 0;
        } else {
            if (find_mouse_to_mouse_remap(2, &target_btn)) {
                int nmsg = mouse_msg_for_btn(target_btn, 1);
                if (nmsg) PostMessageW(hWnd, nmsg, wParam, lParam);
                return 0;
            }
            if (is_mouse_mapped_to_other(2)) return 0;
        }
        break;
    }

    return CallWindowProcW(g_origWndProc, hWnd, msg, wParam, lParam);
}

static void process_cross_device_in_xinput(DWORD dwUserIndex, XINPUT_STATE *state)
{
    int i;
    WORD cur_buttons;
    WORD prev_buttons;
    WORD rising, falling;
    if (dwUserIndex >= 4) return;
    cur_buttons = state->Gamepad.wButtons;
    prev_buttons = s_prev_pad_buttons[dwUserIndex];
    rising = cur_buttons & ~prev_buttons;
    falling = ~cur_buttons & prev_buttons;
    for (i = 0; i < g_config.count; i++) {
        GmMapping *m = &g_config.mappings[i];
        int from_match;
        if (m->from_device != GM_DEVICE_PAD) continue;
        from_match = 0;
        if (m->from_pad_idx == GM_PAD_IDX_ANY) from_match = 1;
        else if (m->from_pad_idx == (int)dwUserIndex) from_match = 1;
        if (!from_match) continue;
        if (m->to_device == GM_DEVICE_KEY) {
            if (rising & (WORD)m->from_code) synth_key_event(m->to_code, 0);
            if (falling & (WORD)m->from_code) synth_key_event(m->to_code, 1);
        } else if (m->to_device == GM_DEVICE_MOUSE) {
            if (rising & (WORD)m->from_code) synth_mouse_event(m->to_code, 0);
            if (falling & (WORD)m->from_code) synth_mouse_event(m->to_code, 1);
        }
    }

    for (i = 0; i < 3; i++) {
        int cur_ms = is_mouse_down(i);
        int prev_ms = s_prev_mouse_state[i];
        if (cur_ms && !prev_ms) {
            int j;
            for (j = 0; j < g_config.count; j++) {
                GmMapping *m2 = &g_config.mappings[j];
                if (m2->from_device == GM_DEVICE_MOUSE && m2->from_code == i && m2->to_device == GM_DEVICE_KEY) {
                    synth_key_event(m2->to_code, 0);
                }
            }
        } else if (!cur_ms && prev_ms) {
            int j;
            for (j = 0; j < g_config.count; j++) {
                GmMapping *m2 = &g_config.mappings[j];
                if (m2->from_device == GM_DEVICE_MOUSE && m2->from_code == i && m2->to_device == GM_DEVICE_KEY) {
                    synth_key_event(m2->to_code, 1);
                }
            }
        }
        s_prev_mouse_state[i] = cur_ms;
    }

    for (i = 0; i < 3; i++) {
        int cur_ms2 = is_mouse_down(i);
        if (cur_ms2) {
            int j;
            for (j = 0; j < g_config.count; j++) {
                GmMapping *m3 = &g_config.mappings[j];
                if (m3->from_device == GM_DEVICE_MOUSE && m3->from_code == i && m3->to_device == GM_DEVICE_PAD) {
                    int to_match = 0;
                    if (m3->to_pad_idx == GM_PAD_IDX_ANY) to_match = 1;
                    else if (m3->to_pad_idx == (int)dwUserIndex) to_match = 1;
                    if (to_match) state->Gamepad.wButtons |= (WORD)m3->to_code;
                }
            }
        }
    }

    for (i = 0; i < 128; i++) {
        int cur_di = (s_dinput_buttons[i] & 0x80) != 0;
        int prev_di = (s_prev_dinput_buttons[i] & 0x80) != 0;
        if (cur_di && !prev_di) {
            int j;
            for (j = 0; j < g_config.count; j++) {
                GmMapping *m2 = &g_config.mappings[j];
                if (m2->from_device == GM_DEVICE_DINPUT && m2->from_code == i) {
                    if (m2->to_device == GM_DEVICE_KEY) synth_key_event(m2->to_code, 0);
                    else if (m2->to_device == GM_DEVICE_MOUSE) synth_mouse_event(m2->to_code, 0);
                }
            }
        } else if (!cur_di && prev_di) {
            int j;
            for (j = 0; j < g_config.count; j++) {
                GmMapping *m2 = &g_config.mappings[j];
                if (m2->from_device == GM_DEVICE_DINPUT && m2->from_code == i) {
                    if (m2->to_device == GM_DEVICE_KEY) synth_key_event(m2->to_code, 1);
                    else if (m2->to_device == GM_DEVICE_MOUSE) synth_mouse_event(m2->to_code, 1);
                }
            }
        }
    }
    memcpy(s_prev_dinput_buttons, s_dinput_buttons, 128);
    s_prev_pad_buttons[dwUserIndex] = cur_buttons;
}

static void apply_pad_to_pad_remap(DWORD dwUserIndex, XINPUT_STATE *state)
{
    int i;
    WORD original = state->Gamepad.wButtons;
    WORD result = original;
    for (i = 0; i < g_config.count; i++) {
        GmMapping *m = &g_config.mappings[i];
        int from_match;
        if (m->from_device != GM_DEVICE_PAD || m->to_device != GM_DEVICE_PAD) continue;
        from_match = 0;
        if (m->from_pad_idx == GM_PAD_IDX_ANY) from_match = 1;
        else if (m->from_pad_idx == (int)dwUserIndex) from_match = 1;
        if (from_match && (original & (WORD)m->from_code)) {
            result |= (WORD)m->to_code;
            result &= ~(WORD)m->from_code;
        }
    }

    state->Gamepad.wButtons = result;
}

static void apply_key_mouse_to_pad(DWORD dwUserIndex, XINPUT_STATE *state)
{
    int i;
    for (i = 0; i < g_config.count; i++) {
        GmMapping *m = &g_config.mappings[i];
        int active = 0;
        int to_match;
        if (m->to_device != GM_DEVICE_PAD) continue;
        to_match = 0;
        if (m->to_pad_idx == GM_PAD_IDX_ANY) to_match = 1;
        else if (m->to_pad_idx == (int)dwUserIndex) to_match = 1;
        if (!to_match) continue;
        if (m->from_device == GM_DEVICE_KEY) {
            active = is_key_down(m->from_code);
        } else if (m->from_device == GM_DEVICE_MOUSE) {
            active = is_mouse_down(m->from_code);
        } else if (m->from_device == GM_DEVICE_DINPUT) {
            if (m->from_code >= 0 && m->from_code < 128)
                active = (s_dinput_buttons[m->from_code] & 0x80) != 0;
        }

        if (active) state->Gamepad.wButtons |= (WORD)m->to_code;
    }
}

static void suppress_mapped_pad_buttons(DWORD dwUserIndex, XINPUT_STATE *state)
{
    int i;
    for (i = 0; i < g_config.count; i++) {
        GmMapping *m = &g_config.mappings[i];
        int from_match;
        if (m->from_device != GM_DEVICE_PAD) continue;
        if (m->to_device == GM_DEVICE_PAD) continue;
        from_match = 0;
        if (m->from_pad_idx == GM_PAD_IDX_ANY) from_match = 1;
        else if (m->from_pad_idx == (int)dwUserIndex) from_match = 1;
        if (from_match) state->Gamepad.wButtons &= ~(WORD)m->from_code;
    }
}

static int has_cross_to_pad_mapping(void)
{
    int i;
    for (i = 0; i < g_config.count; i++) {
        GmMapping *m = &g_config.mappings[i];
        if (m->to_device == GM_DEVICE_PAD && m->from_device != GM_DEVICE_PAD) return 1;
    }
    return 0;
}

static int has_dinput_mapping(void)
{
    int i;
    for (i = 0; i < g_config.count; i++) {
        GmMapping *m = &g_config.mappings[i];
        if (m->from_device == GM_DEVICE_DINPUT || m->to_device == GM_DEVICE_DINPUT) return 1;
    }
    return 0;
}

static DWORD WINAPI hooked_xinput_get_state(DWORD dwUserIndex, XINPUT_STATE *pState)
{
    DWORD result;
    if (!RealXInputGetState) return ERROR_DEVICE_NOT_CONNECTED;
    result = RealXInputGetState(dwUserIndex, pState);
    if (result != ERROR_SUCCESS && pState != NULL && dwUserIndex == 0 && has_cross_to_pad_mapping()) {
        memset(pState, 0, sizeof(XINPUT_STATE));
        result = ERROR_SUCCESS;
    }
    if (dwUserIndex == 0 && has_dinput_mapping()) {
        ensure_dinput_device();
        poll_dinput();
    }
    if (result == ERROR_SUCCESS && pState != NULL) {
        if (dwUserIndex == 0 && has_dinput_mapping()) {
            pState->Gamepad.sThumbLX = s_dinput_lx;
            pState->Gamepad.sThumbLY = (short)(s_dinput_ly == -32768 ? 32767 : -s_dinput_ly);
            pState->Gamepad.sThumbRX = s_dinput_rx;
            pState->Gamepad.sThumbRY = (short)(s_dinput_ry == -32768 ? 32767 : -s_dinput_ry);
            pState->Gamepad.bLeftTrigger = s_dinput_lt;
            pState->Gamepad.bRightTrigger = s_dinput_rt;
        }
        apply_pad_to_pad_remap(dwUserIndex, pState);
        process_cross_device_in_xinput(dwUserIndex, pState);
        apply_key_mouse_to_pad(dwUserIndex, pState);
        suppress_mapped_pad_buttons(dwUserIndex, pState);
    }

    return result;
}

static void hook_xinput(void)
{
    HMODULE hMod;
    FARPROC pfn;
    MH_STATUS st;
    hMod = LoadLibraryA("xinput9_1_0.dll");
    if (!hMod) hMod = LoadLibraryA("xinput1_4.dll");
    if (!hMod) return;
    pfn = GetProcAddress(hMod, "XInputGetState");
    if (!pfn) return;
    st = MH_CreateHook((LPVOID)pfn, (LPVOID)hooked_xinput_get_state, (LPVOID *)&RealXInputGetState);
    if (st != MH_OK) return;
    MH_EnableHook(MH_ALL_HOOKS);
}

static int s_joy_initialized = 0;
static void ensure_dinput_device(void)
{
    int i, n;
    wchar_t dll_path[MAX_PATH];
    wchar_t *slash;
    HMODULE hSelf;
    if (s_joy_initialized) return;
    s_joy_initialized = 1;
    hSelf = GetModuleHandleW(L"GoldMapperLib.dll");
    if (hSelf && GetModuleFileNameW(hSelf, dll_path, MAX_PATH)) {
        slash = wcsrchr(dll_path, L'\\');
        if (slash) {
            wcscpy(slash + 1, L"SDL2.dll");
            s_sdl_module = LoadLibraryW(dll_path);
            dbg_log("[SDL] load from DLL dir: %ls -> %p\n", dll_path, s_sdl_module);
        }
    }
    if (!s_sdl_module) {
        s_sdl_module = LoadLibraryA("SDL2.dll");
        dbg_log("[SDL] load fallback: %p\n", s_sdl_module);
    }
    if (!s_sdl_module) {
        dbg_log("[SDL] LoadLibrary FAILED\n");
        return;
    }

    pfnSDL_Init = (void *)GetProcAddress(s_sdl_module, "SDL_Init");
    pfnSDL_GetError = (void *)GetProcAddress(s_sdl_module, "SDL_GetError");
    pfnSDL_NumJoysticks = (void *)GetProcAddress(s_sdl_module, "SDL_NumJoysticks");
    pfnSDL_IsGameController = (void *)GetProcAddress(s_sdl_module, "SDL_IsGameController");
    pfnSDL_GameControllerOpen = (void *)GetProcAddress(s_sdl_module, "SDL_GameControllerOpen");
    pfnSDL_GameControllerName = (void *)GetProcAddress(s_sdl_module, "SDL_GameControllerName");
    pfnSDL_GameControllerUpdate = (void *)GetProcAddress(s_sdl_module, "SDL_GameControllerUpdate");
    pfnSDL_GameControllerGetButton = (void *)GetProcAddress(s_sdl_module, "SDL_GameControllerGetButton");
    pfnSDL_GameControllerGetAxis = (void *)GetProcAddress(s_sdl_module, "SDL_GameControllerGetAxis");
    if (!pfnSDL_Init || !pfnSDL_NumJoysticks || !pfnSDL_GameControllerOpen || !pfnSDL_GameControllerGetButton) {
        dbg_log("[SDL] GetProcAddress FAILED\n");
        return;
    }

    if (pfnSDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        dbg_log("[SDL] SDL_Init FAILED: %s\n", pfnSDL_GetError ? pfnSDL_GetError() : "?");
        return;
    }
    dbg_log("[SDL] SDL_Init OK\n");
    n = pfnSDL_NumJoysticks();
    dbg_log("[SDL] %d joysticks found\n", n);
    for (i = 0; i < n; i++) {
        if (pfnSDL_IsGameController && pfnSDL_IsGameController(i)) {
            s_sdl_controller = pfnSDL_GameControllerOpen(i);
            if (s_sdl_controller) {
                dbg_log("[SDL] opened controller: %s\n", pfnSDL_GameControllerName ? pfnSDL_GameControllerName(s_sdl_controller) : "?");
                return;
            }
            dbg_log("[SDL] SDL_GameControllerOpen(%d) FAILED: %s\n", i, pfnSDL_GetError ? pfnSDL_GetError() : "?");
        }
    }
    dbg_log("[SDL] no game controller found\n");
}

static void poll_dinput(void)
{
    int b;
    static const struct {
        int sdl_btn;
        int dinput_idx;
    } btn_map[] = {
        { SDL_CONTROLLER_BUTTON_A,            0 },
        { SDL_CONTROLLER_BUTTON_B,            1 },
        { SDL_CONTROLLER_BUTTON_X,            2 },
        { SDL_CONTROLLER_BUTTON_Y,            3 },
        { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 4 },
        { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,5 },
        { SDL_CONTROLLER_BUTTON_BACK,         6 },
        { SDL_CONTROLLER_BUTTON_START,        7 },
        { SDL_CONTROLLER_BUTTON_LEFTSTICK,    8 },
        { SDL_CONTROLLER_BUTTON_RIGHTSTICK,   9 },
        { SDL_CONTROLLER_BUTTON_DPAD_UP,     10 },
        { SDL_CONTROLLER_BUTTON_DPAD_DOWN,   11 },
        { SDL_CONTROLLER_BUTTON_DPAD_LEFT,   12 },
        { SDL_CONTROLLER_BUTTON_DPAD_RIGHT,  13 },
    };

    if (!s_sdl_controller || !pfnSDL_GameControllerUpdate) return;
    pfnSDL_GameControllerUpdate();
    memset(s_dinput_buttons, 0, 128);
    for (b = 0; b < 14; b++) {
        if (pfnSDL_GameControllerGetButton(s_sdl_controller, btn_map[b].sdl_btn))
            s_dinput_buttons[btn_map[b].dinput_idx] = 0x80;
    }

    s_dinput_lx = pfnSDL_GameControllerGetAxis(s_sdl_controller, SDL_CONTROLLER_AXIS_LEFTX);
    s_dinput_ly = pfnSDL_GameControllerGetAxis(s_sdl_controller, SDL_CONTROLLER_AXIS_LEFTY);
    s_dinput_rx = pfnSDL_GameControllerGetAxis(s_sdl_controller, SDL_CONTROLLER_AXIS_RIGHTX);
    s_dinput_ry = pfnSDL_GameControllerGetAxis(s_sdl_controller, SDL_CONTROLLER_AXIS_RIGHTY);
    {
        Sint16 lt_raw = pfnSDL_GameControllerGetAxis(s_sdl_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        Sint16 rt_raw = pfnSDL_GameControllerGetAxis(s_sdl_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        if (lt_raw < 0) lt_raw = 0;
        if (rt_raw < 0) rt_raw = 0;
        s_dinput_lt = (unsigned char)(lt_raw >> 7);
        s_dinput_rt = (unsigned char)(rt_raw >> 7);
    }

    {
        int any = 0;
        for (b = 0; b < 14; b++) {
            if (s_dinput_buttons[b]) { any = 1; break; }
        }
        if (any)
            dbg_log("[SDL] poll: lx=%d ly=%d rx=%d ry=%d lt=%d rt=%d btns=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X\n",
                s_dinput_lx, s_dinput_ly, s_dinput_rx, s_dinput_ry,
                s_dinput_lt, s_dinput_rt,
                s_dinput_buttons[0], s_dinput_buttons[1], s_dinput_buttons[2], s_dinput_buttons[3],
                s_dinput_buttons[4], s_dinput_buttons[5], s_dinput_buttons[6], s_dinput_buttons[7],
                s_dinput_buttons[8], s_dinput_buttons[9], s_dinput_buttons[10], s_dinput_buttons[11],
                s_dinput_buttons[12], s_dinput_buttons[13]);
    }
}

static void find_and_subclass_window(void)
{
    HWND hWnd = NULL;
    DWORD pid = GetCurrentProcessId();
    while (!hWnd) {
        hWnd = FindWindowExW(NULL, NULL, L"MinecraftClass", NULL); //neo: aka the LCE window
        if (hWnd) {
            DWORD owner_pid = 0;
            GetWindowThreadProcessId(hWnd, &owner_pid);
            if (owner_pid == pid) break;
            hWnd = NULL;
        }
        Sleep(100);
    }

    g_origWndProc = (WNDPROC)SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)gm_wndproc);
}

static DWORD WINAPI init_thread(LPVOID param)
{
    (void)param;
    hook_xinput();
    find_and_subclass_window();
    return 0;
}

void gm_hook_init(void)
{
    HANDLE hThread;
    memset(s_prev_pad_buttons, 0, sizeof(s_prev_pad_buttons));
    memset(s_prev_mouse_state, 0, sizeof(s_prev_mouse_state));
    memset(s_dinput_buttons, 0, sizeof(s_dinput_buttons));
    memset(s_prev_dinput_buttons, 0, sizeof(s_prev_dinput_buttons));
    MH_Initialize();
    hThread = CreateThread(NULL, 0, init_thread, NULL, 0, NULL);
    if (hThread) CloseHandle(hThread);
}

void gm_hook_shutdown(void)
{
    if (g_origWndProc) {
        HWND hWnd = FindWindowExW(NULL, NULL, L"MinecraftClass", NULL);
        if (hWnd) SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)g_origWndProc);
        g_origWndProc = NULL;
    }

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}
