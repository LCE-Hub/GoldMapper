#include "gm.h"
#include <string.h>
#include <stdio.h>
#include "cJSON.h"
GmConfig g_config;
typedef struct {
    const char *name;
    int value;
} NameEntry;

static const NameEntry s_key_names[] = {
    { "KEY_A", 0x41 }, { "KEY_B", 0x42 }, { "KEY_C", 0x43 },
    { "KEY_D", 0x44 }, { "KEY_E", 0x45 }, { "KEY_F", 0x46 },
    { "KEY_G", 0x47 }, { "KEY_H", 0x48 }, { "KEY_I", 0x49 },
    { "KEY_J", 0x4A }, { "KEY_K", 0x4B }, { "KEY_L", 0x4C },
    { "KEY_M", 0x4D }, { "KEY_N", 0x4E }, { "KEY_O", 0x4F },
    { "KEY_P", 0x50 }, { "KEY_Q", 0x51 }, { "KEY_R", 0x52 },
    { "KEY_S", 0x53 }, { "KEY_T", 0x54 }, { "KEY_U", 0x55 },
    { "KEY_V", 0x56 }, { "KEY_W", 0x57 }, { "KEY_X", 0x58 },
    { "KEY_Y", 0x59 }, { "KEY_Z", 0x5A },
    { "KEY_0", 0x30 }, { "KEY_1", 0x31 }, { "KEY_2", 0x32 },
    { "KEY_3", 0x33 }, { "KEY_4", 0x34 }, { "KEY_5", 0x35 },
    { "KEY_6", 0x36 }, { "KEY_7", 0x37 }, { "KEY_8", 0x38 },
    { "KEY_9", 0x39 },
    { "KEY_F1",  0x70 }, { "KEY_F2",  0x71 }, { "KEY_F3",  0x72 },
    { "KEY_F4",  0x73 }, { "KEY_F5",  0x74 }, { "KEY_F6",  0x75 },
    { "KEY_F7",  0x76 }, { "KEY_F8",  0x77 }, { "KEY_F9",  0x78 },
    { "KEY_F10", 0x79 }, { "KEY_F11", 0x7A }, { "KEY_F12", 0x7B },
    { "KEY_SPACE",     0x20 }, { "KEY_RETURN",    0x0D },
    { "KEY_ESCAPE",    0x1B }, { "KEY_TAB",       0x09 },
    { "KEY_BACKSPACE", 0x08 }, { "KEY_DELETE",    0x2E },
    { "KEY_INSERT",    0x2D }, { "KEY_HOME",      0x24 },
    { "KEY_END",       0x23 }, { "KEY_PAGEUP",    0x21 },
    { "KEY_PAGEDOWN",  0x22 },
    { "KEY_UP",    0x26 }, { "KEY_DOWN",  0x28 },
    { "KEY_LEFT",  0x25 }, { "KEY_RIGHT", 0x27 },
    { "KEY_PRINTSCREEN", 0x2C }, { "KEY_PAUSE",    0x13 },
    { "KEY_CAPSLOCK",    0x14 }, { "KEY_NUMLOCK",  0x90 },
    { "KEY_SCROLLLOCK",  0x91 },
    { "KEY_LSHIFT",     0xA0 }, { "KEY_RSHIFT",     0xA1 },
    { "KEY_LCTRL",      0xA2 }, { "KEY_RCTRL",      0xA3 },
    { "KEY_LALT",       0xA4 }, { "KEY_RALT",       0xA5 },
    { "KEY_LCONTROL",   0xA2 }, { "KEY_RCONTROL",   0xA3 },
    { "KEY_LEFT_SHIFT", 0xA0 }, { "KEY_RIGHT_SHIFT", 0xA1 },
    { "KEY_LEFT_CTRL",  0xA2 }, { "KEY_RIGHT_CTRL",  0xA3 },
    { "KEY_LEFT_ALT",   0xA4 }, { "KEY_RIGHT_ALT",   0xA5 },
    { "KEY_LMENU",      0xA4 }, { "KEY_RMENU",      0xA5 },
    { "KEY_LWIN", 0x5B }, { "KEY_RWIN", 0x5C }, { "KEY_APPS", 0x5D },
    { "KEY_NUMPAD0", 0x60 }, { "KEY_NUMPAD1", 0x61 },
    { "KEY_NUMPAD2", 0x62 }, { "KEY_NUMPAD3", 0x63 },
    { "KEY_NUMPAD4", 0x64 }, { "KEY_NUMPAD5", 0x65 },
    { "KEY_NUMPAD6", 0x66 }, { "KEY_NUMPAD7", 0x67 },
    { "KEY_NUMPAD8", 0x68 }, { "KEY_NUMPAD9", 0x69 },
    { "KEY_MULTIPLY",   0x6A }, { "KEY_ADD",      0x6B },
    { "KEY_SUBTRACT",   0x6D }, { "KEY_DECIMAL",  0x6E },
    { "KEY_DIVIDE",     0x6F },
    { "KEY_SEMICOLON",   0xBA }, { "KEY_EQUALS",   0xBB },
    { "KEY_COMMA",       0xBC }, { "KEY_MINUS",    0xBD },
    { "KEY_PERIOD",      0xBE }, { "KEY_SLASH",    0xBF },
    { "KEY_GRAVE",       0xC0 }, { "KEY_LBRACKET", 0xDB },
    { "KEY_BACKSLASH",   0xDC }, { "KEY_RBRACKET", 0xDD },
    { "KEY_APOSTROPHE",  0xDE },
    { NULL, 0 }
};

static const NameEntry s_pad_names[] = {
    { "A",          0x1000 },
    { "B",          0x2000 },
    { "X",          0x4000 },
    { "Y",          0x8000 },
    { "LB",         0x0100 },
    { "RB",         0x0200 },
    { "LT",         0x0000 },
    { "RT",         0x0000 },
    { "LTRIGGER",   0x0000 },
    { "RTRIGGER",   0x0000 },
    { "BACK",       0x0020 },
    { "START",      0x0010 },
    { "LTHUMB",     0x0040 },
    { "RTHUMB",     0x0080 },
    { "LSTICK",     0x0040 },
    { "RSTICK",     0x0080 },
    { "DPAD_UP",    0x0001 },
    { "DPAD_DOWN",  0x0002 },
    { "DPAD_LEFT",  0x0004 },
    { "DPAD_RIGHT", 0x0008 },
    { NULL, 0 }
};

static const NameEntry s_mouse_names[] = {
    { "MOUSE_LEFT",   0 },
    { "MOUSE_RIGHT",  1 },
    { "MOUSE_MIDDLE", 2 },
    { NULL, 0 }
};

static int parse_dinput_name(const char *name, int *code)
{
    int n = 0;
    if (name[0] != 'D' || name[1] != 'I' || name[2] != 'N' || name[3] != 'P' || name[4] != 'U' || name[5] != 'T' || name[6] != '_') return -1;
    if (name[7] < '0' || name[7] > '9') return -1;
    n = atoi(name + 7);
    if (n < 0 || n > 127) return -1;
    *code = n;
    return 0;
}

static int lookup_name(const char *name, const NameEntry *table)
{
    int i;
    for (i = 0; table[i].name != NULL; i++) {
        if (strcmp(name, table[i].name) == 0) return table[i].value;
    }
    return -1;
}

static int parse_pad_with_index(const char *name, int *code, int *pad_idx)
{
    const char *underscore;
    char button_name[32];
    int name_len, btn_len;
    int val;
    underscore = strchr(name, '_');
    if (!underscore) return -1;
    name_len = (int)strlen(name);
    if (name[0] == 'P' && name[1] == 'A' && name[2] == 'D' && name[3] >= '1' && name[3] <= '4' && name[4] == '_') {
        *pad_idx = name[3] - '1';
        underscore = name + 4;
    } else if (name[0] == 'P' && name[1] == 'A' && name[2] == 'D' && name[3] == '_') {
        *pad_idx = GM_PAD_IDX_ANY;
        underscore = name + 3;
    } else {
        return -1;
    }

    underscore++;
    btn_len = name_len - (int)(underscore - name);
    if (btn_len <= 0 || btn_len >= 32) return -1;
    memcpy(button_name, underscore, btn_len);
    button_name[btn_len] = '\0';
    val = lookup_name(button_name, s_pad_names);
    if (val < 0) return -1;
    if ((val == 0x0000) && (strcmp(button_name, "LT") == 0 || strcmp(button_name, "LTRIGGER") == 0)) {
        *code = 0x0000;
        return 0;
    }
    if ((val == 0x0000) && (strcmp(button_name, "RT") == 0 || strcmp(button_name, "RTRIGGER") == 0)) {
        *code = 0x0000;
        return 0;
    }

    *code = val;
    return 0;
}

int gm_resolve_input(const char *name, int *device, int *code, int *pad_idx)
{
    int val;
    *pad_idx = GM_PAD_IDX_ANY;
    if (strncmp(name, "KEY_", 4) == 0) {
        val = lookup_name(name, s_key_names);
        if (val < 0) return -1;
        *device = GM_DEVICE_KEY;
        *code = val;
        return 0;
    }

    if (strncmp(name, "MOUSE_", 6) == 0) {
        val = lookup_name(name, s_mouse_names);
        if (val < 0) return -1;
        *device = GM_DEVICE_MOUSE;
        *code = val;
        return 0;
    }

    if (strncmp(name, "DINPUT_", 7) == 0) {
        if (parse_dinput_name(name, &val) < 0) return -1;
        *device = GM_DEVICE_DINPUT;
        *code = val;
        return 0;
    }

    if (strncmp(name, "PAD", 3) == 0) {
        if (parse_pad_with_index(name, code, pad_idx) < 0) return -1;
        *device = GM_DEVICE_PAD;
        return 0;
    }

    return -1;
}

int gm_resolve_output(const char *name, int *device, int *code, int *pad_idx)
{
    return gm_resolve_input(name, device, code, pad_idx);
}

int gm_parse_config(const char *json, int len, GmConfig *cfg)
{
    cJSON *root, *mappings, *item;
    (void)len;
    cfg->count = 0;
    root = cJSON_Parse(json);
    if (!root) return -1;
    mappings = cJSON_GetObjectItem(root, "mappings");
    if (!mappings || !cJSON_IsArray(mappings)) { cJSON_Delete(root); return -1; }
    cJSON_ArrayForEach(item, mappings) {
        cJSON *from_val = cJSON_GetObjectItem(item, "from");
        cJSON *to_val = cJSON_GetObjectItem(item, "to");
        GmMapping *m;
        if (!from_val || !to_val) continue;
        if (!cJSON_IsString(from_val) || !cJSON_IsString(to_val)) continue;
        if (cfg->count >= GM_MAX_MAPPINGS) break;
        m = &cfg->mappings[cfg->count];
        if (gm_resolve_input(from_val->valuestring, &m->from_device, &m->from_code, &m->from_pad_idx) < 0) continue;
        if (gm_resolve_output(to_val->valuestring, &m->to_device, &m->to_code, &m->to_pad_idx) < 0) continue;
        cfg->count++;
    }
    cJSON_Delete(root);
    return 0;
}
