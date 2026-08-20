#include "gm.h"
#include <string.h>
#include <xinput.h>
#include <MinHook.h>
#pragma comment(lib, "xinput9_1_0.lib")
typedef DWORD (WINAPI *XInputGetState_t)(DWORD, XINPUT_STATE *);
static XInputGetState_t RealXInputGetState = NULL;
WNDPROC g_origWndProc = NULL;
static const int s_mouse_vk[3] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON };
static WORD s_prev_pad_buttons[4];
static int s_prev_mouse_state[3];
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
        if (m->from_device == GM_DEVICE_MOUSE && m->from_code == btn && m->to_device == GM_DEVICE_MOUSE) {
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
        if (m->from_device == GM_DEVICE_KEY && m->from_code == vk && m->to_device == GM_DEVICE_KEY) {
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

static DWORD WINAPI hooked_xinput_get_state(DWORD dwUserIndex, XINPUT_STATE *pState)
{
    DWORD result;
    if (!RealXInputGetState) return ERROR_DEVICE_NOT_CONNECTED;
    result = RealXInputGetState(dwUserIndex, pState);
    if (result != ERROR_SUCCESS && pState != NULL && has_cross_to_pad_mapping()) {
        memset(pState, 0, sizeof(XINPUT_STATE));
        result = ERROR_SUCCESS;
    }
    if (result == ERROR_SUCCESS && pState != NULL) {
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

static void find_and_subclass_window(void)
{
    HWND hWnd = NULL;
    DWORD pid = GetCurrentProcessId();
    while (!hWnd) {
        hWnd = FindWindowExW(NULL, NULL, L"MinecraftClass", NULL);
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
