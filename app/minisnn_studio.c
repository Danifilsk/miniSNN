#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scenario_config.h"
#include "scenario_runner.h"

#define APP_TITLE "miniSNN Studio"
#define TEXT_BUFFER_SIZE 128
#define STATUS_BUFFER_SIZE 1024
#define SUMMARY_BUFFER_SIZE 2048
#define PYTHON_COMMAND_BUFFER_SIZE 2400
#define PYTHON_MESSAGE_BUFFER_SIZE 6000
#define STUDIO_FIELD_COUNT 21
#define STUDIO_BUTTON_COUNT 23
#define REWARD_EVENTS_TEXT_SIZE 4096

#define STUDIO_MIN_CLIENT_WIDTH 1320
#define STUDIO_MIN_CLIENT_HEIGHT 980
#define STUDIO_INITIAL_CLIENT_WIDTH STUDIO_MIN_CLIENT_WIDTH
#define STUDIO_INITIAL_CLIENT_HEIGHT STUDIO_MIN_CLIENT_HEIGHT

#define STUDIO_LEFT_X 28
#define STUDIO_MIDDLE_X 570
#define STUDIO_TOP_Y 120
#define STUDIO_LABEL_WIDTH 220
#define STUDIO_FIELD_WIDTH 130
#define STUDIO_TOPOLOGY_FIELD_WIDTH 190
#define STUDIO_TOPOLOGY_BUTTON_WIDTH 92
#define STUDIO_ROW_HEIGHT 36
#define STUDIO_LABEL_HEIGHT 24
#define STUDIO_LABEL_OFFSET_Y 4
#define STUDIO_FIELD_HEIGHT 28

#define STUDIO_PANEL_TOP 94
#define STUDIO_PANEL_BOTTOM_MARGIN 24
#define STUDIO_LEFT_PANEL_LEFT 18
#define STUDIO_LEFT_PANEL_RIGHT 558
#define STUDIO_MIDDLE_PANEL_LEFT 562
#define STUDIO_MIDDLE_PANEL_RIGHT 930
#define STUDIO_RIGHT_PANEL_WIDTH 360
#define STUDIO_RIGHT_PANEL_MARGIN 18
#define STUDIO_RIGHT_CONTENT_PADDING 10

#define STUDIO_BUTTON_WIDTH_LEFT 156
#define STUDIO_BUTTON_WIDTH_RIGHT 174
#define STUDIO_BUTTON_GAP 10
#define STUDIO_BUTTON_HEIGHT 38
#define STUDIO_BUTTON_ROW_HEIGHT 46

#define IDC_RUN_NAME 1001
#define IDC_TOPOLOGY 1002
#define IDC_NEURONS 1003
#define IDC_INHIBITORY_PERCENT 1004
#define IDC_CONNECTION_PROBABILITY 1005
#define IDC_SEED 1006
#define IDC_DELAY 1007
#define IDC_MAX_DELAY 1008
#define IDC_EXC_WEIGHT 1009
#define IDC_INH_WEIGHT 1010
#define IDC_SOURCE_COUNT 1011
#define IDC_INPUT_CURRENT 1012
#define IDC_RECORD_NEURON 1013
#define IDC_STEPS 1014
#define IDC_DT 1015
#define IDC_TAU 1016
#define IDC_V_REST 1017
#define IDC_V_RESET 1018
#define IDC_V_THRESHOLD 1019
#define IDC_RESISTANCE 1020
#define IDC_SYNAPTIC_DECAY 1021
#define IDC_DIAGNOSTICS_LEVEL 1022

#define IDC_BTN_NEW 2001
#define IDC_BTN_LOAD 2002
#define IDC_BTN_SAVE 2003
#define IDC_BTN_RUN 2004
#define IDC_BTN_PLOT 2005
#define IDC_BTN_OPEN 2006
#define IDC_BTN_OPTIONS 2007
#define IDC_BTN_OPEN_NEURON_CSV 2008
#define IDC_BTN_PLOT_NEURON 2009
#define IDC_BTN_OPEN_NEURON_PNG 2010
#define IDC_BTN_COMPARE_RUNS 2011
#define IDC_BTN_OPEN_COMPARISON 2012
#define IDC_BTN_OPEN_RESULTS_ROOT 2013
#define IDC_BTN_OPEN_HISTORY 2014
#define IDC_BTN_GENERATE_DIAGNOSTICS 2015
#define IDC_BTN_OPEN_METRICS 2016
#define IDC_BTN_OPEN_DIAGNOSTICS 2017
#define IDC_BTN_PLASTICITY 2018
#define IDC_BTN_PLOT_PLASTICITY 2019
#define IDC_BTN_OPEN_WEIGHTS 2020
#define IDC_BTN_OPEN_STDP 2021
#define IDC_BTN_HOMEOSTASIS 2022
#define IDC_BTN_PLOT_HOMEOSTASIS 2023
#define IDC_BTN_OPEN_HOMEOSTASIS 2024
#define IDC_BTN_REWARD 2025
#define IDC_BTN_PLOT_REWARD 2026
#define IDC_BTN_OPEN_REWARD 2027

#define IDC_STATUS 3001
#define IDC_SUMMARY 3002

#define IDC_OPT_ALLOW_SELF 4001
#define IDC_OPT_ALLOW_INH_TO_INH 4002
#define IDC_OPT_DENSITY 4003
#define IDC_OPT_SEED 4004
#define IDC_OPT_DELAY 4005
#define IDC_OPT_MAX_DELAY 4006
#define IDC_OPT_SMALL_NEIGHBORS 4007
#define IDC_OPT_SMALL_REWIRE 4008
#define IDC_OPT_FEEDFORWARD_LAYERS 4009
#define IDC_OPT_APPLY 4010
#define IDC_OPT_CANCEL 4011

#define IDC_PLASTICITY_ENABLED 5001
#define IDC_PLASTICITY_RULE 5002
#define IDC_PLASTICITY_A_PLUS 5003
#define IDC_PLASTICITY_A_MINUS 5004
#define IDC_PLASTICITY_TAU_PLUS 5005
#define IDC_PLASTICITY_TAU_MINUS 5006
#define IDC_PLASTICITY_TRACE_INCREMENT 5007
#define IDC_PLASTICITY_WEIGHT_MIN 5008
#define IDC_PLASTICITY_WEIGHT_MAX 5009
#define IDC_PLASTICITY_RECORD_WEIGHTS 5010
#define IDC_PLASTICITY_RECORD_HISTORY 5011
#define IDC_PLASTICITY_INTERVAL 5012
#define IDC_PLASTICITY_LIMIT 5013
#define IDC_PLASTICITY_APPLY 5014
#define IDC_PLASTICITY_CANCEL 5015
#define IDC_PLASTICITY_LEARNING_MODE 5016

#define IDC_HOME_ENABLED 6001
#define IDC_HOME_INTRINSIC 6002
#define IDC_HOME_TARGET_RATE 6003
#define IDC_HOME_RATE_TAU 6004
#define IDC_HOME_UPDATE_INTERVAL 6005
#define IDC_HOME_THRESHOLD_ETA 6006
#define IDC_HOME_THRESHOLD_MIN 6007
#define IDC_HOME_THRESHOLD_MAX 6008
#define IDC_HOME_SCALING 6009
#define IDC_HOME_SCALING_ETA 6010
#define IDC_HOME_SCALING_FACTOR_MIN 6011
#define IDC_HOME_SCALING_FACTOR_MAX 6012
#define IDC_HOME_SCALING_WEIGHT_MIN 6013
#define IDC_HOME_SCALING_WEIGHT_MAX 6014
#define IDC_HOME_GAIN 6015
#define IDC_HOME_GAIN_INITIAL 6016
#define IDC_HOME_GAIN_ETA 6017
#define IDC_HOME_GAIN_MIN 6018
#define IDC_HOME_GAIN_MAX 6019
#define IDC_HOME_RECORD_HISTORY 6020
#define IDC_HOME_RECORD_INTERVAL 6021
#define IDC_HOME_RECORD_LIMIT 6022
#define IDC_HOME_APPLY 6023
#define IDC_HOME_CANCEL 6024

#define IDC_REWARD_ENABLED 7001
#define IDC_REWARD_MODE 7002
#define IDC_REWARD_LEARNING_RATE 7003
#define IDC_REWARD_ELIGIBILITY_TAU 7004
#define IDC_REWARD_ELIGIBILITY_MIN 7005
#define IDC_REWARD_ELIGIBILITY_MAX 7006
#define IDC_REWARD_MIN 7007
#define IDC_REWARD_MAX 7008
#define IDC_REWARD_CLIP 7009
#define IDC_REWARD_RECORD_HISTORY 7010
#define IDC_REWARD_RECORD_INTERVAL 7011
#define IDC_REWARD_RECORD_LIMIT 7012
#define IDC_REWARD_EVENTS 7013
#define IDC_REWARD_APPLY 7014
#define IDC_REWARD_CANCEL 7015

#define STUDIO_INIT_TIMER_ID 1

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

typedef struct
{
    int id;
    const char *label;
    HWND label_hwnd;
    HWND control_hwnd;
} StudioField;

typedef struct
{
    HWND window;
    HFONT title_font;
    HFONT normal_font;
    HFONT edit_font;
    HFONT summary_font;
    HBRUSH background_brush;
    HBRUSH edit_brush;
    COLORREF background_color;
    COLORREF edit_color;
    COLORREF text_color;
    COLORREF secondary_text_color;
    COLORREF disabled_text_color;
    COLORREF accent_color;
    HPEN button_pen;
    HPEN button_focus_pen;
    HPEN button_primary_pen;
    HPEN panel_pen;

    StudioField fields[STUDIO_FIELD_COUNT];
    int field_count;

    HWND topology_combo;
    HWND diagnostics_combo;
    HWND topology_options_button;
    HWND plasticity_button;
    HWND homeostasis_button;
    HWND reward_button;
    HWND status_label;
    HWND summary_box;
    HWND execution_section_label;
    HWND diagnostics_label;
    HWND buttons[STUDIO_BUTTON_COUNT];

    ScenarioConfig current_config;
    ScenarioRunResult last_result;
    int has_result;
    int has_comparison;
    char last_comparison_dir[MAX_PATH];

    char project_root[MAX_PATH];
    char pixel_font_face[LF_FACESIZE];
    int resolved_python_uses_py_launcher;
} StudioState;

typedef struct
{
    HWND window;
    ScenarioConfig original_config;
    ScenarioConfig working_config;
    char topology[SCENARIO_TOPOLOGY_MAX];
    int applied;

    HWND allow_self_checkbox;
    HWND allow_self_label;
    HWND allow_inh_to_inh_checkbox;
    HWND allow_inh_to_inh_label;
    HWND density_edit;
    HWND seed_edit;
    HWND delay_edit;
    HWND max_delay_edit;
    HWND small_neighbors_edit;
    HWND small_rewire_edit;
    HWND feedforward_layers_edit;
} TopologyOptionsDialog;

typedef struct
{
    HWND window;
    ScenarioConfig working_config;
    HWND enabled_checkbox;
    HWND rule_combo;
    HWND learning_mode_combo;
    HWND a_plus_edit;
    HWND a_minus_edit;
    HWND tau_plus_edit;
    HWND tau_minus_edit;
    HWND trace_increment_edit;
    HWND weight_min_edit;
    HWND weight_max_edit;
    HWND record_weights_checkbox;
    HWND record_history_checkbox;
    HWND interval_edit;
    HWND limit_edit;
} PlasticityDialog;

typedef struct
{
    HWND window;
    ScenarioConfig working_config;
    HWND enabled_checkbox;
    HWND intrinsic_checkbox;
    HWND target_rate_edit;
    HWND rate_tau_edit;
    HWND update_interval_edit;
    HWND threshold_eta_edit;
    HWND threshold_min_edit;
    HWND threshold_max_edit;
    HWND scaling_checkbox;
    HWND scaling_eta_edit;
    HWND scaling_factor_min_edit;
    HWND scaling_factor_max_edit;
    HWND scaling_weight_min_edit;
    HWND scaling_weight_max_edit;
    HWND gain_checkbox;
    HWND gain_initial_edit;
    HWND gain_eta_edit;
    HWND gain_min_edit;
    HWND gain_max_edit;
    HWND record_history_checkbox;
    HWND record_interval_edit;
    HWND record_limit_edit;
} HomeostasisDialog;

typedef struct
{
    HWND window;
    ScenarioConfig working_config;
    HWND enabled_checkbox;
    HWND mode_combo;
    HWND learning_rate_edit;
    HWND eligibility_tau_edit;
    HWND eligibility_min_edit;
    HWND eligibility_max_edit;
    HWND reward_min_edit;
    HWND reward_max_edit;
    HWND clip_checkbox;
    HWND record_history_checkbox;
    HWND record_interval_edit;
    HWND record_limit_edit;
    HWND events_edit;
} RewardDialog;

static StudioState g_app;
static TopologyOptionsDialog g_options;
static PlasticityDialog g_plasticity;
static HomeostasisDialog g_homeostasis;
static RewardDialog g_reward;

static void draw_button(const DRAWITEMSTRUCT *item);
static LRESULT handle_color(HDC hdc, HWND hwnd);
static LRESULT handle_edit_color(HDC hdc);

static void set_status(const char *message)
{
    SetWindowTextA(g_app.status_label, message);
}

static HWND field_control(int id)
{
    for (int i = 0; i < g_app.field_count; i++)
    {
        if (g_app.fields[i].id == id)
        {
            if (g_app.fields[i].control_hwnd != NULL &&
                IsWindow(g_app.fields[i].control_hwnd))
            {
                return g_app.fields[i].control_hwnd;
            }

            break;
        }
    }

    if (g_app.window != NULL)
        return GetDlgItem(g_app.window, id);

    return NULL;
}

static HWND summary_control(void)
{
    if (g_app.summary_box != NULL && IsWindow(g_app.summary_box))
        return g_app.summary_box;

    if (g_app.window != NULL)
        return GetDlgItem(g_app.window, IDC_SUMMARY);

    return NULL;
}

static void show_error(const char *title, const char *message)
{
    MessageBoxA(g_app.window, message, title, MB_ICONERROR | MB_OK);
}

static void show_info(const char *title, const char *message)
{
    MessageBoxA(g_app.window, message, title, MB_ICONINFORMATION | MB_OK);
}

static int topology_uses_density(const char *topology)
{
    return strcmp(topology, "random") == 0 ||
           strcmp(topology, "random_balanced") == 0 ||
           strcmp(topology, "feedforward") == 0;
}

static int topology_uses_seed(const char *topology)
{
    return strcmp(topology, "random") == 0 ||
           strcmp(topology, "random_balanced") == 0 ||
           strcmp(topology, "small_world") == 0 ||
           strcmp(topology, "feedforward") == 0;
}

static int topology_can_allow_self_connections(const char *topology)
{
    return strcmp(topology, "all_to_all") == 0 ||
           strcmp(topology, "random") == 0 ||
           strcmp(topology, "random_balanced") == 0 ||
           strcmp(topology, "small_world") == 0;
}

static int topology_uses_small_world_options(const char *topology)
{
    return strcmp(topology, "small_world") == 0;
}

static int topology_uses_feedforward_options(const char *topology)
{
    return strcmp(topology, "feedforward") == 0;
}

static void enable_dark_title_bar(HWND window)
{
    typedef HRESULT(WINAPI *DwmSetWindowAttributeFn)(
        HWND,
        DWORD,
        LPCVOID,
        DWORD);

    HMODULE dwmapi;
    FARPROC proc;
    DwmSetWindowAttributeFn set_window_attribute;
    BOOL enabled = TRUE;
    HRESULT result;

    if (window == NULL)
        return;

    dwmapi = LoadLibraryA("dwmapi.dll");
    if (dwmapi == NULL)
        return;

    proc = GetProcAddress(
        dwmapi,
        "DwmSetWindowAttribute");
    set_window_attribute = NULL;

    if (proc != NULL)
        memcpy(&set_window_attribute, &proc, sizeof(set_window_attribute));

    if (set_window_attribute != NULL)
    {
        result = set_window_attribute(
            window,
            DWMWA_USE_IMMERSIVE_DARK_MODE,
            &enabled,
            sizeof(enabled));

        if (result < 0)
        {
            set_window_attribute(
                window,
                19,
                &enabled,
                sizeof(enabled));
        }
    }

    FreeLibrary(dwmapi);
}

static int text_equals_ignore_case(const char *a, const char *b)
{
    if (a == NULL || b == NULL)
        return 0;

    while (*a != '\0' && *b != '\0')
    {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;

        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static int read_selected_font_face(
    HFONT font,
    char *out_face,
    size_t out_face_size)
{
    HDC hdc;
    HGDIOBJ old_font;
    int length;

    if (font == NULL || out_face == NULL || out_face_size == 0)
        return 0;

    hdc = GetDC(NULL);
    if (hdc == NULL)
        return 0;

    old_font = SelectObject(hdc, font);
    if (old_font == NULL || old_font == HGDI_ERROR)
    {
        ReleaseDC(NULL, hdc);
        return 0;
    }

    length = GetTextFaceA(hdc, (int)out_face_size, out_face);

    SelectObject(hdc, old_font);
    ReleaseDC(NULL, hdc);

    return length > 0;
}

static void remember_pixel_font_face(const char *face_name)
{
    if (g_app.pixel_font_face[0] != '\0' ||
        face_name == NULL ||
        face_name[0] == '\0')
    {
        return;
    }

    snprintf(g_app.pixel_font_face, sizeof(g_app.pixel_font_face), "%s", face_name);
}

static HFONT create_pixel_font(
    int height,
    int weight,
    const char *face_name)
{
    HFONT font;
    char effective_face[LF_FACESIZE];

    if (face_name == NULL)
        return NULL;

    font = CreateFontA(
        height,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        ANSI_CHARSET,
        OUT_RASTER_PRECIS,
        CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN,
        face_name);

    if (font == NULL)
        return NULL;

    if (!read_selected_font_face(font, effective_face, sizeof(effective_face)) ||
        !text_equals_ignore_case(effective_face, face_name))
    {
        DeleteObject(font);
        return NULL;
    }

    remember_pixel_font_face(effective_face);
    return font;
}

static HFONT create_oem_fixed_font(int height, int weight)
{
    HFONT stock_font;
    LOGFONTA log_font;
    HFONT font;
    char effective_face[LF_FACESIZE];

    stock_font = (HFONT)GetStockObject(OEM_FIXED_FONT);
    if (stock_font == NULL ||
        GetObjectA(stock_font, sizeof(log_font), &log_font) == 0)
    {
        return stock_font;
    }

    log_font.lfHeight = height;
    log_font.lfWeight = weight;
    log_font.lfOutPrecision = OUT_RASTER_PRECIS;
    log_font.lfQuality = NONANTIALIASED_QUALITY;
    log_font.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;

    font = CreateFontIndirectA(&log_font);
    if (font != NULL &&
        read_selected_font_face(font, effective_face, sizeof(effective_face)))
    {
        remember_pixel_font_face(effective_face);
        return font;
    }

    if (font != NULL)
        DeleteObject(font);

    if (read_selected_font_face(stock_font, effective_face, sizeof(effective_face)))
        remember_pixel_font_face(effective_face);

    return stock_font;
}

static HFONT create_studio_font(int height, int weight)
{
    HFONT font;

    font = create_pixel_font(height, weight, "Terminal");
    if (font != NULL)
        return font;

    font = create_pixel_font(height, weight, "Fixedsys");
    if (font != NULL)
        return font;

    return create_oem_fixed_font(height, weight);
}

static void destroy_studio_font(HFONT font)
{
    if (font != NULL &&
        font != (HFONT)GetStockObject(OEM_FIXED_FONT) &&
        font != (HFONT)GetStockObject(ANSI_FIXED_FONT) &&
        font != (HFONT)GetStockObject(SYSTEM_FIXED_FONT))
    {
        DeleteObject(font);
    }
}

static void client_size_to_window_size(
    int client_width,
    int client_height,
    int *window_width,
    int *window_height)
{
    RECT rect;

    rect.left = 0;
    rect.top = 0;
    rect.right = client_width;
    rect.bottom = client_height;

    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    *window_width = rect.right - rect.left;
    *window_height = rect.bottom - rect.top;
}

static int right_panel_left(int client_width)
{
    return client_width - STUDIO_RIGHT_PANEL_WIDTH - STUDIO_RIGHT_PANEL_MARGIN;
}

static int right_content_left(int client_width)
{
    return right_panel_left(client_width) + STUDIO_RIGHT_CONTENT_PADDING;
}

static int file_exists(const char *path)
{
    DWORD attributes = GetFileAttributesA(path);

    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int directory_exists(const char *path)
{
    DWORD attributes = GetFileAttributesA(path);

    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static int copy_path(char *destination, size_t destination_size, const char *source);

static int ensure_directory_exists(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return 0;

    if (directory_exists(path))
        return 1;

    if (CreateDirectoryA(path, NULL))
        return 1;

    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return directory_exists(path);

    return 0;
}

static int project_path(
    char *out_path,
    size_t out_path_size,
    const char *relative_path)
{
    size_t root_length;
    const char *tail = relative_path;

    if (out_path == NULL ||
        out_path_size == 0 ||
        relative_path == NULL ||
        g_app.project_root[0] == '\0')
    {
        return 0;
    }

    while (*tail == '\\' || *tail == '/')
        tail++;

    root_length = strlen(g_app.project_root);

    if (snprintf(
            out_path,
            out_path_size,
            "%s%s%s",
            g_app.project_root,
            root_length > 0 && g_app.project_root[root_length - 1] == '\\' ? "" : "\\",
            tail) >= (int)out_path_size)
    {
        return 0;
    }

    for (char *c = out_path; *c != '\0'; c++)
    {
        if (*c == '/')
            *c = '\\';
    }

    return 1;
}

static int scenarios_directory_path(char *out_path, size_t out_path_size)
{
    char results_path[MAX_PATH];
    char scenarios_path[MAX_PATH];

    if (!project_path(results_path, sizeof(results_path), "results") ||
        !project_path(scenarios_path, sizeof(scenarios_path), "results\\scenarios"))
    {
        return copy_path(out_path, out_path_size, g_app.project_root);
    }

    if (!ensure_directory_exists(results_path) ||
        !ensure_directory_exists(scenarios_path))
    {
        return copy_path(out_path, out_path_size, g_app.project_root);
    }

    return copy_path(out_path, out_path_size, scenarios_path);
}

static int copy_path(
    char *out_path,
    size_t out_path_size,
    const char *path)
{
    size_t length;

    if (out_path == NULL || out_path_size == 0 || path == NULL)
        return 0;

    length = strlen(path);

    if (length >= out_path_size)
        return 0;

    memcpy(out_path, path, length + 1);
    return 1;
}

static int contains_text_case_insensitive(
    const char *text,
    const char *needle)
{
    size_t needle_length;

    if (text == NULL || needle == NULL)
        return 0;

    needle_length = strlen(needle);

    if (needle_length == 0)
        return 1;

    for (size_t i = 0; text[i] != '\0'; i++)
    {
        size_t j = 0;

        while (needle[j] != '\0' &&
               text[i + j] != '\0' &&
               tolower((unsigned char)text[i + j]) ==
                   tolower((unsigned char)needle[j]))
        {
            j++;
        }

        if (j == needle_length)
            return 1;
    }

    return 0;
}

static int python_path_is_forbidden(const char *path)
{
    return contains_text_case_insensitive(path, "\\.cache\\") ||
           contains_text_case_insensitive(path, "/.cache/") ||
           contains_text_case_insensitive(path, "codex-runtimes") ||
           contains_text_case_insensitive(path, "\\Steam\\") ||
           contains_text_case_insensitive(path, "/Steam/") ||
           contains_text_case_insensitive(path, "\\WARNO\\") ||
           contains_text_case_insensitive(path, "/WARNO/");
}

static int run_hidden_process(
    char *command,
    DWORD *out_exit_code)
{
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    DWORD exit_code = 1;

    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);

    if (!CreateProcessA(
            NULL,
            command,
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            g_app.project_root[0] != '\0' ? g_app.project_root : NULL,
            &startup,
            &process))
    {
        return 0;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);

    if (out_exit_code != NULL)
        *out_exit_code = exit_code;

    return 1;
}

static int python_has_plot_dependencies(
    const char *python_path,
    char *error_message,
    size_t error_message_size)
{
    char command[PYTHON_COMMAND_BUFFER_SIZE];
    DWORD exit_code = 1;

    if (python_path == NULL || !file_exists(python_path))
    {
        snprintf(
            error_message,
            error_message_size,
            "Python invalido ou inexistente.");
        return 0;
    }

    if (python_path_is_forbidden(python_path))
    {
        snprintf(
            error_message,
            error_message_size,
            "Python rejeitado:\n%s\n\n"
            "Runtimes internos, jogos e caches nao devem ser usados.",
            python_path);
        return 0;
    }

    if (snprintf(
            command,
            sizeof(command),
            "\"%s\" -c \"import sys, pandas, matplotlib; print(sys.executable)\"",
            python_path) >= (int)sizeof(command))
    {
        snprintf(error_message, error_message_size, "Caminho do Python muito longo.");
        return 0;
    }

    if (!run_hidden_process(command, &exit_code) || exit_code != 0)
    {
        snprintf(
            error_message,
            error_message_size,
            "Python rejeitado:\n%s\n\n"
            "O Studio precisa de pandas e matplotlib neste interpretador.",
            python_path);
        return 0;
    }

    return 1;
}

static int py_launcher_has_plot_dependencies(
    const char *py_path,
    char *error_message,
    size_t error_message_size)
{
    char command[PYTHON_COMMAND_BUFFER_SIZE];
    DWORD exit_code = 1;

    if (py_path == NULL || !file_exists(py_path))
    {
        snprintf(error_message, error_message_size, "py.exe invalido ou inexistente.");
        return 0;
    }

    if (python_path_is_forbidden(py_path))
        return 0;

    if (snprintf(
            command,
            sizeof(command),
            "\"%s\" -3 -c \"import sys, pandas, matplotlib; print(sys.executable)\"",
            py_path) >= (int)sizeof(command))
    {
        snprintf(error_message, error_message_size, "Caminho do py.exe muito longo.");
        return 0;
    }

    if (!run_hidden_process(command, &exit_code) || exit_code != 0)
    {
        snprintf(
            error_message,
            error_message_size,
            "py.exe rejeitado:\n%s\n\n"
            "O Python acionado por py -3 precisa de pandas e matplotlib.",
            py_path);
        return 0;
    }

    return 1;
}

static int search_path_executable(
    const char *name,
    char *out_path,
    size_t out_path_size)
{
    DWORD result;

    if (out_path_size > MAXDWORD)
        return 0;

    result = SearchPathA(
        NULL,
        name,
        NULL,
        (DWORD)out_path_size,
        out_path,
        NULL);

    return result > 0 && result < out_path_size && file_exists(out_path);
}

static int search_python_subdirectories(
    const char *base_directory,
    char *out_path,
    size_t out_path_size,
    char *error_message,
    size_t error_message_size)
{
    char search_pattern[MAX_PATH];
    WIN32_FIND_DATAA find_data;
    HANDLE find_handle;

    if (base_directory == NULL || !directory_exists(base_directory))
        return 0;

    if (snprintf(
            search_pattern,
            sizeof(search_pattern),
            "%s\\*",
            base_directory) >= (int)sizeof(search_pattern))
    {
        return 0;
    }

    find_handle = FindFirstFileA(search_pattern, &find_data);

    if (find_handle == INVALID_HANDLE_VALUE)
        return 0;

    do
    {
        char candidate[MAX_PATH];

        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            continue;

        if (strcmp(find_data.cFileName, ".") == 0 ||
            strcmp(find_data.cFileName, "..") == 0)
        {
            continue;
        }

        if (snprintf(
                candidate,
                sizeof(candidate),
                "%s\\%s\\python.exe",
                base_directory,
                find_data.cFileName) >= (int)sizeof(candidate))
        {
            continue;
        }

        if (file_exists(candidate) &&
            python_has_plot_dependencies(
                candidate,
                error_message,
                error_message_size))
        {
            FindClose(find_handle);
            return copy_path(out_path, out_path_size, candidate);
        }
    } while (FindNextFileA(find_handle, &find_data));

    FindClose(find_handle);
    return 0;
}

static int find_standard_python(
    char *out_path,
    size_t out_path_size,
    char *error_message,
    size_t error_message_size)
{
    char local_appdata[MAX_PATH];
    char user_profile[MAX_PATH];
    char base_directory[MAX_PATH];
    DWORD env_length;

    env_length = GetEnvironmentVariableA(
        "LOCALAPPDATA",
        local_appdata,
        sizeof(local_appdata));

    if (env_length == 0 || env_length >= sizeof(local_appdata))
    {
        env_length = GetEnvironmentVariableA(
            "USERPROFILE",
            user_profile,
            sizeof(user_profile));

        if (env_length == 0 || env_length >= sizeof(user_profile))
            return 0;

        if (snprintf(
                local_appdata,
                sizeof(local_appdata),
                "%s\\AppData\\Local",
                user_profile) >= (int)sizeof(local_appdata))
        {
            return 0;
        }
    }

    if (snprintf(
            base_directory,
            sizeof(base_directory),
            "%s\\Programs\\Python",
            local_appdata) < (int)sizeof(base_directory) &&
        search_python_subdirectories(
            base_directory,
            out_path,
            out_path_size,
            error_message,
            error_message_size))
    {
        return 1;
    }

    if (snprintf(
            base_directory,
            sizeof(base_directory),
            "%s\\Python",
            local_appdata) < (int)sizeof(base_directory) &&
        search_python_subdirectories(
            base_directory,
            out_path,
            out_path_size,
            error_message,
            error_message_size))
    {
        return 1;
    }

    return 0;
}

static int resolve_python_executable(
    char *out_path,
    size_t out_path_size)
{
    char env_python[MAX_PATH];
    char candidate[MAX_PATH];
    char validation_error[512];
    DWORD env_length;

    g_app.resolved_python_uses_py_launcher = 0;

    env_length = GetEnvironmentVariableA(
        "MINISNN_PYTHON",
        env_python,
        sizeof(env_python));

    if (env_length > 0 &&
        env_length < sizeof(env_python) &&
        file_exists(env_python) &&
        python_has_plot_dependencies(
            env_python,
            validation_error,
            sizeof(validation_error)) &&
        copy_path(out_path, out_path_size, env_python))
    {
        return 1;
    }

    if (find_standard_python(
            out_path,
            out_path_size,
            validation_error,
            sizeof(validation_error)))
    {
        return 1;
    }

    if (file_exists("C:\\msys64\\ucrt64\\bin\\python.exe") &&
        python_has_plot_dependencies(
            "C:\\msys64\\ucrt64\\bin\\python.exe",
            validation_error,
            sizeof(validation_error)) &&
        copy_path(
            out_path,
            out_path_size,
            "C:\\msys64\\ucrt64\\bin\\python.exe"))
    {
        return 1;
    }

    if (file_exists("C:\\msys64\\mingw64\\bin\\python.exe") &&
        python_has_plot_dependencies(
            "C:\\msys64\\mingw64\\bin\\python.exe",
            validation_error,
            sizeof(validation_error)) &&
        copy_path(
            out_path,
            out_path_size,
            "C:\\msys64\\mingw64\\bin\\python.exe"))
    {
        return 1;
    }

    if (search_path_executable("py.exe", candidate, sizeof(candidate)) &&
        py_launcher_has_plot_dependencies(
            candidate,
            validation_error,
            sizeof(validation_error)) &&
        copy_path(out_path, out_path_size, candidate))
    {
        g_app.resolved_python_uses_py_launcher = 1;
        return 1;
    }

    if (search_path_executable("python.exe", candidate, sizeof(candidate)) &&
        python_has_plot_dependencies(
            candidate,
            validation_error,
            sizeof(validation_error)) &&
        copy_path(out_path, out_path_size, candidate))
    {
        return 1;
    }

    return 0;
}

static int parse_int_field(
    int id,
    const char *field_name,
    int *out_value,
    char *error_message,
    size_t error_message_size)
{
    char text[TEXT_BUFFER_SIZE];
    char *end;
    long value;

    GetWindowTextA(field_control(id), text, sizeof(text));

    errno = 0;
    value = strtol(text, &end, 10);

    while (*end != '\0' && isspace((unsigned char)*end))
        end++;

    if (text == end || errno != 0 || *end != '\0' ||
        value < -2147483647L - 1L || value > 2147483647L)
    {
        snprintf(error_message, error_message_size, "Campo invalido: %s.", field_name);
        return 0;
    }

    *out_value = (int)value;
    return 1;
}

static int parse_uint_field(
    int id,
    const char *field_name,
    unsigned int *out_value,
    char *error_message,
    size_t error_message_size)
{
    char text[TEXT_BUFFER_SIZE];
    char *end;
    char *start;
    unsigned long value;

    GetWindowTextA(field_control(id), text, sizeof(text));

    start = text;
    while (*start != '\0' && isspace((unsigned char)*start))
        start++;

    if (*start == '-' || *start == '+')
    {
        snprintf(error_message, error_message_size, "Campo invalido: %s.", field_name);
        return 0;
    }

    errno = 0;
    value = strtoul(text, &end, 10);

    while (*end != '\0' && isspace((unsigned char)*end))
        end++;

    if (text == end || errno != 0 || *end != '\0' || value > 4294967295UL)
    {
        snprintf(error_message, error_message_size, "Campo invalido: %s.", field_name);
        return 0;
    }

    *out_value = (unsigned int)value;
    return 1;
}

static int parse_double_field(
    int id,
    const char *field_name,
    double *out_value,
    char *error_message,
    size_t error_message_size)
{
    char text[TEXT_BUFFER_SIZE];
    char *end;
    double value;

    GetWindowTextA(field_control(id), text, sizeof(text));

    errno = 0;
    value = strtod(text, &end);

    while (*end != '\0' && isspace((unsigned char)*end))
        end++;

    if (text == end || errno != 0 || *end != '\0' || !isfinite(value))
    {
        snprintf(error_message, error_message_size, "Campo invalido: %s.", field_name);
        return 0;
    }

    *out_value = value;
    return 1;
}

static void set_edit_text(int id, const char *text)
{
    SetWindowTextA(field_control(id), text);
}

static void set_edit_int(int id, int value)
{
    char text[TEXT_BUFFER_SIZE];
    snprintf(text, sizeof(text), "%d", value);
    set_edit_text(id, text);
}

static void set_edit_uint(int id, unsigned int value)
{
    char text[TEXT_BUFFER_SIZE];
    snprintf(text, sizeof(text), "%u", value);
    set_edit_text(id, text);
}

static void format_double_for_field(
    char *buffer,
    size_t buffer_size,
    double value)
{
    char *decimal;
    char *end;

    if (buffer == NULL || buffer_size == 0)
        return;

    if (!isfinite(value))
    {
        snprintf(buffer, buffer_size, "%.17g", value);
        return;
    }

    if (fabs(value) < 0.0000000000005)
        value = 0.0;

    if (snprintf(buffer, buffer_size, "%.12f", value) < 0)
    {
        buffer[0] = '\0';
        return;
    }

    buffer[buffer_size - 1] = '\0';

    decimal = strchr(buffer, '.');
    if (decimal == NULL)
        return;

    end = buffer + strlen(buffer) - 1;
    while (end > decimal && *end == '0')
    {
        *end = '\0';
        end--;
    }

    if (end == decimal)
        *end = '\0';
}

static void set_edit_double(int id, double value)
{
    char text[TEXT_BUFFER_SIZE];
    format_double_for_field(text, sizeof(text), value);
    set_edit_text(id, text);
}

static void update_density_enabled(void)
{
    char topology[SCENARIO_TOPOLOGY_MAX];
    GetWindowTextA(g_app.topology_combo, topology, sizeof(topology));
    EnableWindow(
        field_control(IDC_CONNECTION_PROBABILITY),
        topology_uses_density(topology));
}

static void config_to_controls(const ScenarioConfig *config)
{
    set_edit_text(IDC_RUN_NAME, config->run_name);

    SendMessageA(g_app.topology_combo, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)config->topology);

    set_edit_int(IDC_NEURONS, config->neurons);
    set_edit_double(IDC_INHIBITORY_PERCENT, config->inhibitory_fraction * 100.0);
    set_edit_double(IDC_CONNECTION_PROBABILITY, config->connection_probability);
    set_edit_uint(IDC_SEED, config->seed);
    set_edit_int(IDC_DELAY, config->delay);
    set_edit_int(IDC_MAX_DELAY, config->max_synaptic_delay);
    set_edit_double(IDC_EXC_WEIGHT, config->excitatory_weight);
    set_edit_double(IDC_INH_WEIGHT, config->inhibitory_weight);
    set_edit_int(IDC_SOURCE_COUNT, config->source_count);
    set_edit_double(IDC_INPUT_CURRENT, config->input_current);
    set_edit_int(IDC_RECORD_NEURON, config->record_neuron);
    set_edit_int(IDC_STEPS, config->steps);
    set_edit_double(IDC_DT, config->dt);
    set_edit_double(IDC_TAU, config->tau);
    set_edit_double(IDC_V_REST, config->v_rest);
    set_edit_double(IDC_V_RESET, config->v_reset);
    set_edit_double(IDC_V_THRESHOLD, config->v_threshold);
    set_edit_double(IDC_RESISTANCE, config->resistance);
    set_edit_double(IDC_SYNAPTIC_DECAY, config->synaptic_decay);
    SendMessageA(
        g_app.diagnostics_combo,
        CB_SELECTSTRING,
        (WPARAM)-1,
        (LPARAM)config->diagnostics_level);

    update_density_enabled();
}

static int controls_to_config(
    ScenarioConfig *config,
    char *error_message,
    size_t error_message_size)
{
    double inhibitory_percent;

    *config = g_app.current_config;

    GetWindowTextA(
        field_control(IDC_RUN_NAME),
        config->run_name,
        sizeof(config->run_name));

    GetWindowTextA(
        g_app.topology_combo,
        config->topology,
        sizeof(config->topology));

    GetWindowTextA(
        g_app.diagnostics_combo,
        config->diagnostics_level,
        sizeof(config->diagnostics_level));

    if (!parse_int_field(IDC_NEURONS, "Neuronios", &config->neurons, error_message, error_message_size) ||
        !parse_double_field(IDC_INHIBITORY_PERCENT, "Proporcao inibitoria (%)", &inhibitory_percent, error_message, error_message_size) ||
        !parse_double_field(IDC_CONNECTION_PROBABILITY, "Densidade de conexao", &config->connection_probability, error_message, error_message_size) ||
        !parse_uint_field(IDC_SEED, "Seed", &config->seed, error_message, error_message_size) ||
        !parse_int_field(IDC_DELAY, "Delay", &config->delay, error_message, error_message_size) ||
        !parse_int_field(IDC_MAX_DELAY, "Delay maximo", &config->max_synaptic_delay, error_message, error_message_size) ||
        !parse_double_field(IDC_EXC_WEIGHT, "Peso excitatorio", &config->excitatory_weight, error_message, error_message_size) ||
        !parse_double_field(IDC_INH_WEIGHT, "Peso inibitorio", &config->inhibitory_weight, error_message, error_message_size) ||
        !parse_int_field(IDC_SOURCE_COUNT, "Neuronios com entrada", &config->source_count, error_message, error_message_size) ||
        !parse_double_field(IDC_INPUT_CURRENT, "Corrente externa", &config->input_current, error_message, error_message_size) ||
        !parse_int_field(IDC_RECORD_NEURON, "Neuronio detalhado", &config->record_neuron, error_message, error_message_size) ||
        !parse_int_field(IDC_STEPS, "Passos", &config->steps, error_message, error_message_size) ||
        !parse_double_field(IDC_DT, "dt", &config->dt, error_message, error_message_size) ||
        !parse_double_field(IDC_TAU, "tau", &config->tau, error_message, error_message_size) ||
        !parse_double_field(IDC_V_REST, "V_rest", &config->v_rest, error_message, error_message_size) ||
        !parse_double_field(IDC_V_RESET, "V_reset", &config->v_reset, error_message, error_message_size) ||
        !parse_double_field(IDC_V_THRESHOLD, "V_threshold", &config->v_threshold, error_message, error_message_size) ||
        !parse_double_field(IDC_RESISTANCE, "Resistencia", &config->resistance, error_message, error_message_size) ||
        !parse_double_field(IDC_SYNAPTIC_DECAY, "Decaimento sinaptico", &config->synaptic_decay, error_message, error_message_size))
    {
        return 0;
    }

    if (inhibitory_percent < 0.0 || inhibitory_percent > 100.0)
    {
        snprintf(error_message, error_message_size, "Campo invalido: Proporcao inibitoria deve ficar entre 0 e 100.");
        return 0;
    }

    config->inhibitory_fraction = inhibitory_percent / 100.0;

    if (!scenario_config_validate(config, error_message, error_message_size))
        return 0;

    return 1;
}

static void update_summary(
    const ScenarioConfig *config,
    const ScenarioRunResult *result)
{
    char text[SUMMARY_BUFFER_SIZE];

    snprintf(
        text,
        sizeof(text),
        "STATUS: SIMULACAO CONCLUIDA\r\n"
        "\r\n"
        "Nome da execucao: %s\r\n"
        "Pasta real: %s\r\n"
        "Topologia: %s\r\n"
        "STDP: %s\r\n"
        "Reward: %s\r\n"
        "Homeostase: %s\r\n"
        "Numero de neuronios: %d\r\n"
        "Numero de conexoes: %d\r\n"
        "Neuronios inibitorios: %d\r\n"
        "Spikes totais: %d\r\n"
        "Spikes EXC: %d\r\n"
        "Spikes INH: %d\r\n"
        "Primeiro timestep ativo: %d\r\n"
        "Ultimo timestep ativo: %d\r\n"
        "Pasta de saida:\r\n%s\r\n",
        config->run_name,
        result->actual_run_name,
        config->topology,
        config->plasticity_enabled ? "ON" : "OFF",
        config->reward_enabled ? "ON (R-STDP)" : "OFF",
        config->homeostasis_enabled ? "ON" : "OFF",
        config->neurons,
        result->connection_count,
        result->inhibitory_count,
        result->spikes_total,
        result->spikes_exc,
        result->spikes_inh,
        result->first_active_step,
        result->last_active_step,
        result->output_directory);

    SetWindowTextA(summary_control(), text);
}

static void set_result_buttons_enabled(BOOL enabled)
{
    for (int i = 4; i <= 8; i++)
        EnableWindow(g_app.buttons[i], enabled);

    for (int i = 13; i <= 15; i++)
        EnableWindow(g_app.buttons[i], enabled);

    for (int i = 16; i <= 18; i++)
        EnableWindow(g_app.buttons[i], enabled);

    for (int i = 19; i <= 20; i++)
        EnableWindow(g_app.buttons[i], enabled);

    for (int i = 21; i <= 22; i++)
        EnableWindow(g_app.buttons[i], enabled);
}

static void set_comparison_buttons_enabled(void)
{
    EnableWindow(g_app.buttons[9], TRUE);
    EnableWindow(g_app.buttons[10], TRUE);
    EnableWindow(g_app.buttons[11], TRUE);
    EnableWindow(g_app.buttons[12], g_app.has_comparison ? TRUE : FALSE);
}

static void reset_to_default(void)
{
    scenario_config_default(&g_app.current_config);
    config_to_controls(&g_app.current_config);
    SetWindowTextA(summary_control(), "STATUS: PRONTO PARA EXECUTAR\r\n");
    g_app.has_result = 0;
    set_result_buttons_enabled(FALSE);
    set_status("PRONTO PARA EXECUTAR");
}

static void load_scenario_file(void)
{
    OPENFILENAMEA ofn;
    char filename[MAX_PATH] = "";
    char configs_path[MAX_PATH];
    char error[256];

    if (!project_path(configs_path, sizeof(configs_path), "configs"))
    {
        show_error("Erro interno", "Nao foi possivel montar o caminho de configs.");
        return;
    }

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_app.window;
    ofn.lpstrFilter = "Arquivos INI (*.ini)\0*.ini\0Todos (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrInitialDir = configs_path;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameA(&ofn))
    {
        SetCurrentDirectoryA(g_app.project_root);
        return;
    }

    SetCurrentDirectoryA(g_app.project_root);

    if (!scenario_config_load_file(
            filename,
            &g_app.current_config,
            error,
            sizeof(error)))
    {
        show_error("Erro ao carregar cenario", error);
        return;
    }

    config_to_controls(&g_app.current_config);
    SetWindowTextA(summary_control(), "STATUS: PRONTO PARA EXECUTAR\r\n");
    g_app.has_result = 0;
    set_result_buttons_enabled(FALSE);

    {
        char status[STATUS_BUFFER_SIZE];
        snprintf(status, sizeof(status), "CENARIO CARREGADO: %s", filename);
        set_status(status);
    }
}

static void save_scenario_file(void)
{
    OPENFILENAMEA ofn;
    ScenarioConfig config;
    char filename[MAX_PATH];
    char configs_path[MAX_PATH];
    char error[256];

    if (!controls_to_config(&config, error, sizeof(error)))
    {
        show_error("Configuracao invalida", error);
        return;
    }

    snprintf(filename, sizeof(filename), "%s.ini", config.run_name);

    if (!project_path(configs_path, sizeof(configs_path), "configs"))
    {
        show_error("Erro interno", "Nao foi possivel montar o caminho de configs.");
        return;
    }

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_app.window;
    ofn.lpstrFilter = "Arquivos INI (*.ini)\0*.ini\0Todos (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrInitialDir = configs_path;
    ofn.lpstrDefExt = "ini";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameA(&ofn))
    {
        SetCurrentDirectoryA(g_app.project_root);
        return;
    }

    SetCurrentDirectoryA(g_app.project_root);

    if (!scenario_config_save_file(filename, &config, error, sizeof(error)))
    {
        show_error("Erro ao salvar cenario", error);
        return;
    }

    g_app.current_config = config;
    set_status("CENARIO SALVO");
}

static void run_scenario(void)
{
    ScenarioConfig config;
    ScenarioRunResult result;
    char error[256];

    if (!controls_to_config(&config, error, sizeof(error)))
    {
        show_error("Configuracao invalida", error);
        return;
    }

    config.auto_unique_run = 1;

    set_status("SIMULACAO EM EXECUCAO...");
    UpdateWindow(g_app.window);

    if (!SetCurrentDirectoryA(g_app.project_root))
    {
        show_error(
            "Erro interno",
            "Erro interno: nao foi possivel acessar a raiz do projeto.");
        set_status("ERRO AO ACESSAR RAIZ DO PROJETO");
        return;
    }

    if (!scenario_runner_execute(
            &config,
            NULL,
            &result,
            error,
            sizeof(error)))
    {
        show_error("Erro ao rodar simulacao", error);
        set_status("ERRO AO EXECUTAR SIMULACAO");
        return;
    }

    g_app.current_config = config;
    g_app.last_result = result;
    g_app.has_result = 1;

    update_summary(&config, &result);
    set_result_buttons_enabled(TRUE);
    set_status("SIMULACAO CONCLUIDA");
}

static void generate_graphs(void)
{
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    char python_path[MAX_PATH];
    char script_path[MAX_PATH];
    char output_path[MAX_PATH];
    char command[PYTHON_COMMAND_BUFFER_SIZE];
    char status[STATUS_BUFFER_SIZE];
    char old_backend[TEXT_BUFFER_SIZE];
    DWORD old_backend_length;
    int had_old_backend = 0;
    DWORD exit_code = 1;

    if (!g_app.has_result)
    {
        show_error("Sem resultados", "Rode uma simulacao antes de gerar graficos.");
        return;
    }

    if (!resolve_python_executable(python_path, sizeof(python_path)))
    {
        show_error(
            "Python nao encontrado",
            "Nao foi encontrado um Python compativel com pandas e matplotlib.\n\n"
            "O miniSNN Studio procura automaticamente instalacoes padrao do Windows e MSYS2.\n\n"
            "Instale Python com pandas e matplotlib e abra o Studio novamente.");
        set_status("PYTHON COMPATIVEL NAO ENCONTRADO");
        return;
    }

    if (!project_path(script_path, sizeof(script_path), "scripts\\plot_scenario.py") ||
        !project_path(output_path, sizeof(output_path), g_app.last_result.output_directory))
    {
        show_error(
            "Erro ao gerar graficos",
            "Nao foi possivel montar caminhos absolutos para script ou resultados.");
        set_status("ERRO AO MONTAR CAMINHOS DOS GRAFICOS");
        return;
    }

    snprintf(
        command,
        sizeof(command),
        g_app.resolved_python_uses_py_launcher ?
            "\"%s\" -3 \"%s\" \"%s\"" :
            "\"%s\" \"%s\" \"%s\"",
        python_path,
        script_path,
        output_path);

    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);

    snprintf(
        status,
        sizeof(status),
        "PYTHON VALIDO DETECTADO: %s",
        python_path);
    set_status(status);
    UpdateWindow(g_app.window);

    snprintf(
        status,
        sizeof(status),
        "GERANDO GRAFICOS COM %s",
        python_path);
    set_status(status);
    UpdateWindow(g_app.window);

    old_backend_length = GetEnvironmentVariableA(
        "MPLBACKEND",
        old_backend,
        sizeof(old_backend));
    had_old_backend = old_backend_length > 0 &&
                      old_backend_length < sizeof(old_backend);
    SetEnvironmentVariableA("MPLBACKEND", "Agg");

    if (!CreateProcessA(
            NULL,
            command,
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            g_app.project_root,
            &startup,
            &process))
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];

        if (had_old_backend)
            SetEnvironmentVariableA("MPLBACKEND", old_backend);
        else
            SetEnvironmentVariableA("MPLBACKEND", NULL);

        snprintf(
            message,
            sizeof(message),
            "Nao foi possivel executar Python.\n\nPython usado:\n%s\n\n"
            "Script:\n%s\n\n"
            "Pasta de resultados:\n%s\n\n"
            "Pandas e matplotlib tambem sao necessarios.\n\n"
            "Teste manual:\n%s",
            python_path,
            script_path,
            output_path,
            command);
        show_error("Erro ao gerar graficos", message);
        set_status("ERRO AO GERAR GRAFICOS");
        return;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);

    if (had_old_backend)
        SetEnvironmentVariableA("MPLBACKEND", old_backend);
    else
        SetEnvironmentVariableA("MPLBACKEND", NULL);

    if (exit_code != 0)
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];
        snprintf(
            message,
            sizeof(message),
            "O script de graficos terminou com erro.\n\nPython usado:\n%s\n\n"
            "Script:\n%s\n\n"
            "Pasta de resultados:\n%s\n\n"
            "Pandas e matplotlib podem estar ausentes.\n\n"
            "Teste manual:\n%s",
            python_path,
            script_path,
            output_path,
            command);
        show_error("Erro ao gerar graficos", message);
        set_status("ERRO AO GERAR GRAFICOS");
        return;
    }

    show_info(
        "Graficos gerados",
        "Arquivos criados:\n- population_activity.png\n- mean_state.png\n- raster.png");
    snprintf(
        status,
        sizeof(status),
        "GRAFICOS GERADOS. PYTHON: %s",
        python_path);
    set_status(status);
}

static void generate_diagnostics(void)
{
    char python_path[MAX_PATH];
    char script_path[MAX_PATH];
    char output_path[MAX_PATH];
    char command[PYTHON_COMMAND_BUFFER_SIZE];
    char message[PYTHON_MESSAGE_BUFFER_SIZE];
    char status[STATUS_BUFFER_SIZE];
    DWORD exit_code = 1;

    if (!g_app.has_result)
    {
        show_error("Sem resultados", "Rode uma simulacao antes de gerar o diagnostico.");
        return;
    }

    if (strcmp(g_app.current_config.diagnostics_level, "off") == 0)
    {
        show_error(
            "Diagnostico desativado",
            "O nivel de diagnostico esta OFF. Selecione BASIC ou FULL e rode o cenario novamente.");
        set_status("DIAGNOSTICO OFF");
        return;
    }

    if (!resolve_python_executable(python_path, sizeof(python_path)))
    {
        show_error(
            "Python nao encontrado",
            "Python com pandas e matplotlib e necessario para gerar o diagnostico.");
        set_status("PYTHON COMPATIVEL NAO ENCONTRADO");
        return;
    }

    if (!project_path(script_path, sizeof(script_path), "scripts\\analyze_run.py") ||
        !project_path(output_path, sizeof(output_path), g_app.last_result.output_directory))
    {
        show_error("Erro interno", "Nao foi possivel montar os caminhos do diagnostico.");
        return;
    }

    snprintf(
        command,
        sizeof(command),
        g_app.resolved_python_uses_py_launcher ?
            "\"%s\" -3 \"%s\" \"%s\" --level %s" :
            "\"%s\" \"%s\" \"%s\" --level %s",
        python_path,
        script_path,
        output_path,
        g_app.current_config.diagnostics_level);

    set_status("DIAGNOSTICO EM EXECUCAO...");
    UpdateWindow(g_app.window);

    if (!run_hidden_process(command, &exit_code) || exit_code != 0)
    {
        snprintf(
            message,
            sizeof(message),
            "O analisador terminou com erro. Verifique pandas e matplotlib.\n\n"
            "Python: %s\nComando: %s\nCodigo: %lu",
            python_path,
            command,
            (unsigned long)exit_code);
        show_error("Erro ao gerar diagnostico", message);
        set_status("ERRO AO GERAR DIAGNOSTICO");
        return;
    }

    snprintf(
        status,
        sizeof(status),
        "DIAGNOSTICO %s GERADO. PYTHON: %s",
        g_app.current_config.diagnostics_level,
        python_path);
    set_status(status);
    show_info(
        "Diagnostico gerado",
        "Arquivos principais criados:\n- metrics.csv\n- metrics_report.txt\n- diagnostics_overview.png");
}

static void open_last_execution(void)
{
    char output_path[MAX_PATH];
    HINSTANCE result;

    if (!g_app.has_result)
    {
        show_error("Sem resultados", "Nenhuma execucao rodada ainda.");
        return;
    }

    if (!project_path(output_path, sizeof(output_path), g_app.last_result.output_directory))
    {
        show_error(
            "Erro ao abrir resultados",
            "Nao foi possivel montar o caminho absoluto da pasta de resultados.");
        return;
    }

    if (!directory_exists(output_path))
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];
        snprintf(
            message,
            sizeof(message),
            "A pasta de resultados nao existe ou nao e um diretorio.\n\n"
            "Caminho tentado:\n%s\n\n"
            "Abra a pasta manualmente se ela existir em outro local.",
            output_path);
        show_error("Erro ao abrir resultados", message);
        return;
    }

    result = ShellExecuteA(
        g_app.window,
        "open",
        output_path,
        NULL,
        g_app.project_root,
        SW_SHOWNORMAL);

    if ((INT_PTR)result <= 32)
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];
        snprintf(
            message,
            sizeof(message),
            "Nao foi possivel abrir a pasta de resultados.\n\n"
            "Caminho tentado:\n%s\n\n"
            "Codigo retornado pelo ShellExecuteA: %ld\n\n"
            "Abra a pasta manualmente pelo Explorer.",
            output_path,
            (long)(INT_PTR)result);
        show_error("Erro ao abrir resultados", message);
        return;
    }

    set_status("PASTA DA ULTIMA EXECUCAO ABERTA");
}

static void open_results_root(void)
{
    char results_path[MAX_PATH];
    HINSTANCE result;

    if (!project_path(results_path, sizeof(results_path), "results"))
    {
        show_error(
            "Erro ao abrir resultados",
            "Nao foi possivel montar o caminho da pasta results.");
        return;
    }

    if (!ensure_directory_exists(results_path))
    {
        show_error(
            "Erro ao abrir resultados",
            "Nao foi possivel criar ou acessar a pasta results.");
        return;
    }

    result = ShellExecuteA(
        g_app.window,
        "open",
        results_path,
        NULL,
        g_app.project_root,
        SW_SHOWNORMAL);

    if ((INT_PTR)result <= 32)
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];
        snprintf(
            message,
            sizeof(message),
            "Nao foi possivel abrir results.\n\n"
            "Caminho tentado:\n%s\n\n"
            "Codigo retornado pelo ShellExecuteA: %ld",
            results_path,
            (long)(INT_PTR)result);
        show_error("Erro ao abrir resultados", message);
        return;
    }

    set_status("PASTA RESULTS ABERTA");
}

static int ensure_scenario_history_file(char *history_path, size_t history_path_size)
{
    char scenarios_path[MAX_PATH];
    FILE *file;

    if (!scenarios_directory_path(scenarios_path, sizeof(scenarios_path)) ||
        !project_path(history_path, history_path_size, "results\\scenarios\\index.csv"))
    {
        return 0;
    }

    if (file_exists(history_path))
        return 1;

    file = fopen(history_path, "w");
    if (file == NULL)
        return 0;

    if (fprintf(
            file,
            "timestamp,run_name,actual_run_name,run_path,config_path,topology,num_neurons,steps,dt,seed,recorded_neuron,total_connections,total_spikes,first_active_step,last_active_step,status\n") < 0)
    {
        fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static void open_scenario_history(void)
{
    char index_path[MAX_PATH];
    char history_path[MAX_PATH];
    char scenarios_path[MAX_PATH];
    char python_path[MAX_PATH];
    char script_path[MAX_PATH];
    char command[PYTHON_COMMAND_BUFFER_SIZE];
    DWORD exit_code = 1;
    HINSTANCE result;

    if (!ensure_scenario_history_file(index_path, sizeof(index_path)) ||
        !scenarios_directory_path(scenarios_path, sizeof(scenarios_path)) ||
        !project_path(
            history_path,
            sizeof(history_path),
            "results\\scenarios\\history.html") ||
        !project_path(
            script_path,
            sizeof(script_path),
            "scripts\\generate_history_report.py"))
    {
        show_error(
            "Historico indisponivel",
            "Historico ainda nao existe e nao foi possivel cria-lo. Rode um cenario primeiro.");
        return;
    }

    if (resolve_python_executable(python_path, sizeof(python_path)))
    {
        snprintf(
            command,
            sizeof(command),
            g_app.resolved_python_uses_py_launcher ?
                "\"%s\" -3 \"%s\" \"%s\"" :
                "\"%s\" \"%s\" \"%s\"",
            python_path,
            script_path,
            scenarios_path);
        set_status("GERANDO HISTORICO HTML...");
        UpdateWindow(g_app.window);
        if (!run_hidden_process(command, &exit_code) ||
            exit_code != 0 ||
            !file_exists(history_path))
        {
            char message[PYTHON_MESSAGE_BUFFER_SIZE];
            snprintf(
                message,
                sizeof(message),
                "Nao foi possivel gerar o historico HTML.\n\n"
                "Python usado:\n%s\n\n"
                "Execute manualmente:\n%s\n\n"
                "Codigo de saida: %lu",
                python_path,
                command,
                (unsigned long)exit_code);
            show_error("Erro ao gerar historico", message);
            set_status("ERRO AO GERAR HISTORICO HTML");
            return;
        }
    }
    else if (file_exists(history_path))
    {
        show_info(
            "Python nao encontrado",
            "Python nao foi encontrado.\n"
            "O ultimo historico HTML existente sera aberto e pode estar\n"
            "desatualizado em relacao ao index.csv.");
    }
    else
    {
        show_error(
            "Historico HTML indisponivel",
            "Nao foi possivel gerar o historico HTML.\n\n"
            "Verifique o Python ou execute manualmente:\n\n"
            "python scripts/generate_history_report.py results/scenarios");
        set_status("HISTORICO HTML INDISPONIVEL");
        return;
    }

    result = ShellExecuteA(
        g_app.window,
        "open",
        history_path,
        NULL,
        g_app.project_root,
        SW_SHOWNORMAL);

    if ((INT_PTR)result <= 32)
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];
        snprintf(
            message,
            sizeof(message),
            "Nao foi possivel abrir o historico.\n\n"
            "Caminho tentado:\n%s\n\n"
            "Codigo retornado pelo ShellExecuteA: %ld",
            history_path,
            (long)(INT_PTR)result);
        show_error("Erro ao abrir historico", message);
        return;
    }

    set_status("HISTORICO HTML ABERTO");
}

static int get_detailed_neuron_id(int *out_neuron_id)
{
    char error[256];

    if (!parse_int_field(
            IDC_RECORD_NEURON,
            "Neuronio detalhado",
            out_neuron_id,
            error,
            sizeof(error)))
    {
        show_error("Neuronio invalido", error);
        return 0;
    }

    if (*out_neuron_id < 0)
    {
        show_error(
            "Neuronio invalido",
            "O ID do neuronio detalhado deve ser maior ou igual a zero.");
        return 0;
    }

    return 1;
}

static int build_neuron_artifact_path(
    char *out_path,
    size_t out_path_size,
    int neuron_id,
    const char *suffix)
{
    char relative_path[MAX_PATH];
    char filename[80];

    if (snprintf(
            filename,
            sizeof(filename),
            "neuron_%d%s",
            neuron_id,
            suffix) >= (int)sizeof(filename))
    {
        return 0;
    }

    if (snprintf(
            relative_path,
            sizeof(relative_path),
            "%s\\%s",
            g_app.last_result.output_directory,
            filename) >= (int)sizeof(relative_path))
    {
        return 0;
    }

    return project_path(out_path, out_path_size, relative_path);
}

static void open_existing_file(
    const char *path,
    const char *title,
    const char *missing_message,
    const char *success_status)
{
    HINSTANCE result;

    if (!file_exists(path))
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];
        snprintf(
            message,
            sizeof(message),
            "%s\n\nCaminho tentado:\n%s",
            missing_message,
            path);
        show_error(title, message);
        return;
    }

    result = ShellExecuteA(
        g_app.window,
        "open",
        path,
        NULL,
        g_app.project_root,
        SW_SHOWNORMAL);

    if ((INT_PTR)result <= 32)
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];
        snprintf(
            message,
            sizeof(message),
            "Nao foi possivel abrir o arquivo.\n\n"
            "Caminho tentado:\n%s\n\n"
            "Codigo retornado pelo ShellExecuteA: %ld",
            path,
            (long)(INT_PTR)result);
        show_error(title, message);
        return;
    }

    set_status(success_status);
}

static int build_run_artifact_path(
    char *out_path,
    size_t out_path_size,
    const char *filename)
{
    char relative_path[MAX_PATH];

    if (snprintf(
            relative_path,
            sizeof(relative_path),
            "%s\\%s",
            g_app.last_result.output_directory,
            filename) >= (int)sizeof(relative_path))
    {
        return 0;
    }

    return project_path(out_path, out_path_size, relative_path);
}

static int ensure_run_report(
    const char *report_filename,
    const char *source_filename,
    const char *mode,
    const char *missing_source_message,
    const char *failure_message)
{
    char report_path[MAX_PATH];
    char source_path[MAX_PATH];
    char python_path[MAX_PATH];
    char script_path[MAX_PATH];
    char output_path[MAX_PATH];
    char command[PYTHON_COMMAND_BUFFER_SIZE];
    char message[PYTHON_MESSAGE_BUFFER_SIZE];
    char status[STATUS_BUFFER_SIZE];
    DWORD exit_code = 1;

    if (!build_run_artifact_path(
            report_path,
            sizeof(report_path),
            report_filename) ||
        !build_run_artifact_path(
            source_path,
            sizeof(source_path),
            source_filename))
    {
        show_error(
            "Erro interno",
            "Nao foi possivel montar os caminhos do relatorio HTML.");
        return 0;
    }

    if (file_exists(report_path))
        return 1;

    if (!file_exists(source_path))
    {
        snprintf(
            message,
            sizeof(message),
            "%s\n\nCaminho verificado:\n%s",
            missing_source_message,
            source_path);
        show_error("Relatorio indisponivel", message);
        return 0;
    }

    if (!resolve_python_executable(python_path, sizeof(python_path)))
    {
        show_error(
            "Python nao encontrado",
            "Nao foi encontrado um Python compativel para gerar o relatorio HTML.\n\n"
            "Verifique o Python e os arquivos da execucao.");
        set_status("PYTHON COMPATIVEL NAO ENCONTRADO");
        return 0;
    }

    if (!project_path(
            script_path,
            sizeof(script_path),
            "scripts\\generate_run_reports.py") ||
        !project_path(
            output_path,
            sizeof(output_path),
            g_app.last_result.output_directory))
    {
        show_error(
            "Erro interno",
            "Nao foi possivel montar o comando do gerador de relatorios.");
        return 0;
    }

    snprintf(
        command,
        sizeof(command),
        g_app.resolved_python_uses_py_launcher ?
            "\"%s\" -3 \"%s\" \"%s\" %s" :
            "\"%s\" \"%s\" \"%s\" %s",
        python_path,
        script_path,
        output_path,
        mode);

    snprintf(
        status,
        sizeof(status),
        "GERANDO %s...",
        report_filename);
    set_status(status);
    UpdateWindow(g_app.window);

    if (!run_hidden_process(command, &exit_code) ||
        exit_code != 0 ||
        !file_exists(report_path))
    {
        snprintf(
            message,
            sizeof(message),
            "%s\n\n"
            "Python usado:\n%s\n\n"
            "Pasta da execucao:\n%s\n\n"
            "Comando para diagnostico:\n%s\n\n"
            "Codigo de saida: %lu",
            failure_message,
            python_path,
            output_path,
            command,
            (unsigned long)exit_code);
        show_error("Erro ao gerar relatorio", message);
        set_status("ERRO AO GERAR RELATORIO HTML");
        return 0;
    }

    return 1;
}

static void open_metrics(void)
{
    char path[MAX_PATH];

    if (!g_app.has_result)
    {
        show_error("Sem resultados", "Rode uma simulacao antes de abrir as metricas.");
        return;
    }

    if (!ensure_run_report(
            "metrics_report.html",
            "metrics.csv",
            "--metrics",
            "metrics.csv nao existe. Gere o diagnostico ou use nivel BASIC/FULL.",
            "Nao foi possivel gerar o relatorio de metricas.\n"
            "Verifique o Python e os arquivos da execucao."))
    {
        return;
    }

    if (!build_run_artifact_path(path, sizeof(path), "metrics_report.html"))
    {
        show_error("Erro interno", "Nao foi possivel montar o caminho de metrics_report.html.");
        return;
    }

    open_existing_file(
        path,
        "Metricas nao encontradas",
        "metrics_report.html nao existe. Gere o diagnostico novamente.",
        "RELATORIO DE METRICAS ABERTO");
}

static void open_diagnostics(void)
{
    char path[MAX_PATH];

    if (!g_app.has_result)
    {
        show_error("Sem resultados", "Rode uma simulacao antes de abrir o diagnostico.");
        return;
    }

    if (!build_run_artifact_path(path, sizeof(path), "diagnostics_overview.png"))
    {
        show_error("Erro interno", "Nao foi possivel montar o caminho do diagnostico.");
        return;
    }

    open_existing_file(
        path,
        "Diagnostico nao encontrado",
        "diagnostics_overview.png nao existe. Clique em GERAR DIAGNOSTICO.",
        "DIAGNOSTICO ABERTO");
}

static void generate_plasticity_graph(void)
{
    char python_path[MAX_PATH];
    char script_path[MAX_PATH];
    char output_path[MAX_PATH];
    char png_path[MAX_PATH];
    char metrics_path[MAX_PATH];
    char command[PYTHON_COMMAND_BUFFER_SIZE];
    char status[STATUS_BUFFER_SIZE];
    char old_backend[TEXT_BUFFER_SIZE];
    DWORD old_backend_length;
    DWORD exit_code = 1;
    int had_old_backend;

    if (!g_app.has_result)
    {
        show_error("Sem resultados", "Rode uma simulacao antes de gerar o grafico STDP.");
        return;
    }

    if (!build_run_artifact_path(metrics_path, sizeof(metrics_path), "plasticity_metrics.csv") ||
        !build_run_artifact_path(png_path, sizeof(png_path), "plasticity_overview.png"))
    {
        show_error("Erro interno", "Nao foi possivel montar os caminhos de plasticidade.");
        return;
    }

    if (!file_exists(metrics_path))
    {
        show_error(
            "Plasticidade nao encontrada",
            "plasticity_metrics.csv nao existe. Rode um cenario com STDP ativado e registro de pesos.");
        return;
    }

    if (!resolve_python_executable(python_path, sizeof(python_path)))
    {
        show_error(
            "Python nao encontrado",
            "Nao foi encontrado um Python compativel com pandas e matplotlib.");
        set_status("PYTHON COMPATIVEL NAO ENCONTRADO");
        return;
    }

    if (!project_path(script_path, sizeof(script_path), "scripts\\plot_plasticity.py") ||
        !project_path(output_path, sizeof(output_path), g_app.last_result.output_directory))
    {
        show_error("Erro interno", "Nao foi possivel montar o comando do grafico STDP.");
        return;
    }

    snprintf(
        command,
        sizeof(command),
        g_app.resolved_python_uses_py_launcher ?
            "\"%s\" -3 \"%s\" \"%s\"" :
            "\"%s\" \"%s\" \"%s\"",
        python_path,
        script_path,
        output_path);

    old_backend_length = GetEnvironmentVariableA(
        "MPLBACKEND",
        old_backend,
        sizeof(old_backend));
    had_old_backend = old_backend_length > 0 &&
                      old_backend_length < sizeof(old_backend);
    SetEnvironmentVariableA("MPLBACKEND", "Agg");
    set_status("GERANDO GRAFICO STDP...");
    UpdateWindow(g_app.window);

    if (!run_hidden_process(command, &exit_code))
        exit_code = 1;

    if (had_old_backend)
        SetEnvironmentVariableA("MPLBACKEND", old_backend);
    else
        SetEnvironmentVariableA("MPLBACKEND", NULL);

    if (exit_code != 0 || !file_exists(png_path))
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];
        snprintf(
            message,
            sizeof(message),
            "O grafico STDP nao foi gerado.\n\nPython usado:\n%s\n\n"
            "Pandas e matplotlib podem estar ausentes, ou os CSVs de pesos podem nao existir.\n\n"
            "Teste manual:\n%s",
            python_path,
            command);
        show_error("Erro ao gerar grafico STDP", message);
        set_status("ERRO AO GERAR GRAFICO STDP");
        return;
    }

    snprintf(status, sizeof(status), "GRAFICO STDP GERADO: %s", png_path);
    show_info("Grafico STDP gerado", "plasticity_overview.png foi criado na ultima execucao.");
    set_status(status);
}

static void open_weights(void)
{
    char path[MAX_PATH];

    if (!g_app.has_result)
    {
        show_error("Sem resultados", "Rode uma simulacao antes de abrir os pesos.");
        return;
    }

    if (!ensure_run_report(
            "weights_report.html",
            "weights_final.csv",
            "--weights",
            "Esta execucao nao possui relatorio de pesos.\n"
            "O STDP pode estar desligado ou os arquivos de plasticidade podem nao ter sido gerados.",
            "Nao foi possivel gerar o relatorio de pesos.\n"
            "Verifique o Python e os arquivos da execucao."))
    {
        return;
    }

    if (!build_run_artifact_path(path, sizeof(path), "weights_report.html"))
    {
        show_error("Erro interno", "Nao foi possivel montar o caminho dos pesos.");
        return;
    }

    open_existing_file(
        path,
        "Pesos nao encontrados",
        "weights_report.html nao existe. Gere o relatorio de pesos novamente.",
        "RELATORIO DE PESOS ABERTO");
}

static void open_stdp_plot(void)
{
    char path[MAX_PATH];

    if (!g_app.has_result)
    {
        show_error("Sem resultados", "Rode uma simulacao antes de abrir o grafico STDP.");
        return;
    }

    if (!build_run_artifact_path(path, sizeof(path), "plasticity_overview.png"))
    {
        show_error("Erro interno", "Nao foi possivel montar o caminho do grafico STDP.");
        return;
    }

    open_existing_file(
        path,
        "Grafico STDP nao encontrado",
        "plasticity_overview.png nao existe. Clique em GRAFICO STDP.",
        "GRAFICO STDP ABERTO");
}

static void generate_homeostasis_graph(void)
{
    char python_path[MAX_PATH];
    char script_path[MAX_PATH];
    char output_path[MAX_PATH];
    char metrics_path[MAX_PATH];
    char png_path[MAX_PATH];
    char command[PYTHON_COMMAND_BUFFER_SIZE];
    char status[STATUS_BUFFER_SIZE];
    char old_backend[TEXT_BUFFER_SIZE];
    DWORD old_backend_length;
    DWORD exit_code = 1;
    int had_old_backend;

    if (!g_app.has_result)
    {
        show_error("Sem resultados", "Rode uma simulacao antes de gerar o grafico de homeostase.");
        return;
    }
    if (!build_run_artifact_path(metrics_path, sizeof(metrics_path), "homeostasis_metrics.csv") ||
        !build_run_artifact_path(png_path, sizeof(png_path), "homeostasis_overview.png"))
    {
        show_error("Erro interno", "Nao foi possivel montar os caminhos de homeostase.");
        return;
    }
    if (!file_exists(metrics_path))
    {
        show_error(
            "Homeostase nao encontrada",
            "Esta execucao nao possui dados homeostaticos.\nA homeostase pode estar desligada.");
        return;
    }
    if (!resolve_python_executable(python_path, sizeof(python_path)))
    {
        show_error("Python nao encontrado", "Nao foi encontrado um Python compativel com pandas e matplotlib.");
        return;
    }
    if (!project_path(script_path, sizeof(script_path), "scripts\\plot_homeostasis.py") ||
        !project_path(output_path, sizeof(output_path), g_app.last_result.output_directory))
    {
        show_error("Erro interno", "Nao foi possivel montar o comando homeostatico.");
        return;
    }
    snprintf(
        command,
        sizeof(command),
        g_app.resolved_python_uses_py_launcher ?
            "\"%s\" -3 \"%s\" \"%s\"" :
            "\"%s\" \"%s\" \"%s\"",
        python_path,
        script_path,
        output_path);

    old_backend_length = GetEnvironmentVariableA("MPLBACKEND", old_backend, sizeof(old_backend));
    had_old_backend = old_backend_length > 0 && old_backend_length < sizeof(old_backend);
    SetEnvironmentVariableA("MPLBACKEND", "Agg");
    set_status("GERANDO GRAFICO HOMEOSTASE...");
    UpdateWindow(g_app.window);
    if (!run_hidden_process(command, &exit_code))
        exit_code = 1;
    if (had_old_backend)
        SetEnvironmentVariableA("MPLBACKEND", old_backend);
    else
        SetEnvironmentVariableA("MPLBACKEND", NULL);

    if (exit_code != 0 || !file_exists(png_path))
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];
        snprintf(
            message,
            sizeof(message),
            "O grafico de homeostase nao foi gerado.\n\nPython usado:\n%s\n\n"
            "Pandas e matplotlib podem estar ausentes.\n\nTeste manual:\n%s",
            python_path,
            command);
        show_error("Erro ao gerar homeostase", message);
        set_status("ERRO AO GERAR GRAFICO HOMEOSTASE");
        return;
    }

    snprintf(status, sizeof(status), "GRAFICO HOMEOSTASE GERADO: %s", png_path);
    show_info("Grafico de homeostase", "homeostasis_overview.png foi criado na ultima execucao.");
    set_status(status);
}

static void open_homeostasis_report(void)
{
    char path[MAX_PATH];

    if (!g_app.has_result)
    {
        show_error("Sem resultados", "Rode uma simulacao antes de abrir a homeostase.");
        return;
    }
    if (!build_run_artifact_path(path, sizeof(path), "homeostasis_report.html"))
    {
        show_error("Erro interno", "Nao foi possivel montar o caminho homeostatico.");
        return;
    }
    open_existing_file(
        path,
        "Homeostase nao encontrada",
        "Esta execucao nao possui dados homeostaticos.\nA homeostase pode estar desligada.",
        "RELATORIO DE HOMEOSTASE ABERTO");
}

static void generate_reward_graph(void)
{
    char python_path[MAX_PATH];
    char script_path[MAX_PATH];
    char output_path[MAX_PATH];
    char metrics_path[MAX_PATH];
    char png_path[MAX_PATH];
    char command[PYTHON_COMMAND_BUFFER_SIZE];
    char status[STATUS_BUFFER_SIZE];
    char old_backend[TEXT_BUFFER_SIZE];
    DWORD old_backend_length;
    DWORD exit_code = 1;
    int had_old_backend;

    if (!g_app.has_result)
    {
        show_error("Sem resultados", "Rode uma simulacao antes de gerar o grafico de recompensa.");
        return;
    }
    if (!build_run_artifact_path(metrics_path, sizeof(metrics_path), "reward_metrics.csv") ||
        !build_run_artifact_path(png_path, sizeof(png_path), "reward_overview.png"))
    {
        show_error("Erro interno", "Nao foi possivel montar os caminhos de recompensa.");
        return;
    }
    if (!file_exists(metrics_path))
    {
        show_error(
            "Recompensa nao encontrada",
            "Esta execucao nao possui dados de recompensa.\n"
            "O aprendizado por recompensa pode estar desligado.");
        return;
    }
    if (!resolve_python_executable(python_path, sizeof(python_path)))
    {
        show_error("Python nao encontrado", "Nao foi encontrado um Python compativel com pandas e matplotlib.");
        return;
    }
    if (!project_path(script_path, sizeof(script_path), "scripts\\plot_reward.py") ||
        !project_path(output_path, sizeof(output_path), g_app.last_result.output_directory))
    {
        show_error("Erro interno", "Nao foi possivel montar o comando do grafico de recompensa.");
        return;
    }
    snprintf(
        command,
        sizeof(command),
        g_app.resolved_python_uses_py_launcher ?
            "\"%s\" -3 \"%s\" \"%s\"" :
            "\"%s\" \"%s\" \"%s\"",
        python_path,
        script_path,
        output_path);

    old_backend_length = GetEnvironmentVariableA("MPLBACKEND", old_backend, sizeof(old_backend));
    had_old_backend = old_backend_length > 0 && old_backend_length < sizeof(old_backend);
    SetEnvironmentVariableA("MPLBACKEND", "Agg");
    set_status("GERANDO GRAFICO RECOMPENSA...");
    UpdateWindow(g_app.window);
    if (!run_hidden_process(command, &exit_code))
        exit_code = 1;
    if (had_old_backend)
        SetEnvironmentVariableA("MPLBACKEND", old_backend);
    else
        SetEnvironmentVariableA("MPLBACKEND", NULL);

    if (exit_code != 0 || !file_exists(png_path))
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];
        snprintf(
            message,
            sizeof(message),
            "O grafico de recompensa nao foi gerado.\n\nPython usado:\n%s\n\n"
            "Pandas e matplotlib podem estar ausentes.\n\nTeste manual:\n%s",
            python_path,
            command);
        show_error("Erro ao gerar recompensa", message);
        set_status("ERRO AO GERAR GRAFICO RECOMPENSA");
        return;
    }

    if (!ensure_run_report(
            "reward_report.html",
            "reward_metrics.csv",
            "--reward",
            "Esta execucao nao possui dados de recompensa.\n"
            "O aprendizado por recompensa pode estar desligado.",
            "Nao foi possivel atualizar o relatorio de recompensa."))
    {
        return;
    }

    snprintf(status, sizeof(status), "GRAFICO RECOMPENSA GERADO: %s", png_path);
    show_info("Grafico de recompensa", "reward_overview.png foi criado na ultima execucao.");
    set_status(status);
}

static void open_reward_report(void)
{
    char path[MAX_PATH];

    if (!g_app.has_result)
    {
        show_error("Sem resultados", "Rode uma simulacao antes de abrir a recompensa.");
        return;
    }
    if (!ensure_run_report(
            "reward_report.html",
            "reward_metrics.csv",
            "--reward",
            "Esta execucao nao possui dados de recompensa.\n"
            "O aprendizado por recompensa pode estar desligado.",
            "Nao foi possivel gerar o relatorio de recompensa.\n"
            "Verifique o Python e os arquivos da execucao."))
    {
        return;
    }
    if (!build_run_artifact_path(path, sizeof(path), "reward_report.html"))
    {
        show_error("Erro interno", "Nao foi possivel montar o caminho da recompensa.");
        return;
    }
    open_existing_file(
        path,
        "Recompensa nao encontrada",
        "Esta execucao nao possui dados de recompensa.\n"
        "O aprendizado por recompensa pode estar desligado.",
        "RELATORIO DE RECOMPENSA ABERTO");
}

static void open_neuron_csv(void)
{
    char csv_path[MAX_PATH];
    int neuron_id;

    if (!g_app.has_result)
    {
        show_error(
            "Sem resultados",
            "Rode uma simulacao antes de abrir o CSV do neuronio.");
        return;
    }

    if (!get_detailed_neuron_id(&neuron_id))
        return;

    if (!build_neuron_artifact_path(
            csv_path,
            sizeof(csv_path),
            neuron_id,
            ".csv"))
    {
        show_error(
            "Erro ao abrir CSV",
            "Nao foi possivel montar o caminho do CSV do neuronio.");
        return;
    }

    open_existing_file(
        csv_path,
        "Arquivo do neuronio nao encontrado",
        "Arquivo do neuronio nao encontrado.\n"
        "Execute o cenario primeiro e verifique se o ID do neuronio detalhado e valido.",
        "CSV DO NEURONIO ABERTO");
}

static void open_neuron_plot(void)
{
    char png_path[MAX_PATH];
    int neuron_id;

    if (!g_app.has_result)
    {
        show_error(
            "Sem resultados",
            "Rode uma simulacao antes de abrir o grafico do neuronio.");
        return;
    }

    if (!get_detailed_neuron_id(&neuron_id))
        return;

    if (!build_neuron_artifact_path(
            png_path,
            sizeof(png_path),
            neuron_id,
            "_detail.png"))
    {
        show_error(
            "Erro ao abrir grafico",
            "Nao foi possivel montar o caminho do grafico do neuronio.");
        return;
    }

    open_existing_file(
        png_path,
        "Grafico do neuronio nao encontrado",
        "Grafico do neuronio nao encontrado.\n"
        "Gere o grafico individual primeiro.",
        "GRAFICO DO NEURONIO ABERTO");
}

static void generate_neuron_graph(void)
{
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    char python_path[MAX_PATH];
    char script_path[MAX_PATH];
    char output_path[MAX_PATH];
    char png_path[MAX_PATH];
    char command[PYTHON_COMMAND_BUFFER_SIZE];
    char status[STATUS_BUFFER_SIZE];
    char old_backend[TEXT_BUFFER_SIZE];
    char neuron_id_text[TEXT_BUFFER_SIZE];
    DWORD old_backend_length;
    int had_old_backend = 0;
    DWORD exit_code = 1;
    int neuron_id;

    if (!g_app.has_result)
    {
        show_error(
            "Sem resultados",
            "Rode uma simulacao antes de gerar o grafico do neuronio.");
        return;
    }

    if (!get_detailed_neuron_id(&neuron_id))
        return;

    snprintf(neuron_id_text, sizeof(neuron_id_text), "%d", neuron_id);

    if (!resolve_python_executable(python_path, sizeof(python_path)))
    {
        show_error(
            "Python nao encontrado",
            "Nao foi encontrado um Python compativel com pandas e matplotlib.\n\n"
            "Instale Python com pandas e matplotlib e abra o Studio novamente.");
        set_status("PYTHON COMPATIVEL NAO ENCONTRADO");
        return;
    }

    if (!project_path(script_path, sizeof(script_path), "scripts\\plot_neuron.py") ||
        !project_path(output_path, sizeof(output_path), g_app.last_result.output_directory) ||
        !build_neuron_artifact_path(
            png_path,
            sizeof(png_path),
            neuron_id,
            "_detail.png"))
    {
        show_error(
            "Erro ao gerar grafico",
            "Nao foi possivel montar caminhos absolutos para o grafico do neuronio.");
        set_status("ERRO AO MONTAR CAMINHOS DO GRAFICO DO NEURONIO");
        return;
    }

    snprintf(
        command,
        sizeof(command),
        g_app.resolved_python_uses_py_launcher ?
            "\"%s\" -3 \"%s\" \"%s\" \"%s\"" :
            "\"%s\" \"%s\" \"%s\" \"%s\"",
        python_path,
        script_path,
        output_path,
        neuron_id_text);

    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);

    snprintf(
        status,
        sizeof(status),
        "GERANDO GRAFICO DO NEURONIO %d",
        neuron_id);
    set_status(status);
    UpdateWindow(g_app.window);

    old_backend_length = GetEnvironmentVariableA(
        "MPLBACKEND",
        old_backend,
        sizeof(old_backend));
    had_old_backend = old_backend_length > 0 &&
                      old_backend_length < sizeof(old_backend);
    SetEnvironmentVariableA("MPLBACKEND", "Agg");

    if (!CreateProcessA(
            NULL,
            command,
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            g_app.project_root,
            &startup,
            &process))
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];

        if (had_old_backend)
            SetEnvironmentVariableA("MPLBACKEND", old_backend);
        else
            SetEnvironmentVariableA("MPLBACKEND", NULL);

        snprintf(
            message,
            sizeof(message),
            "Nao foi possivel executar Python.\n\nPython usado:\n%s\n\n"
            "Script:\n%s\n\n"
            "Pasta de resultados:\n%s\n\n"
            "Teste manual:\n%s",
            python_path,
            script_path,
            output_path,
            command);
        show_error("Erro ao gerar grafico do neuronio", message);
        set_status("ERRO AO GERAR GRAFICO DO NEURONIO");
        return;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);

    if (had_old_backend)
        SetEnvironmentVariableA("MPLBACKEND", old_backend);
    else
        SetEnvironmentVariableA("MPLBACKEND", NULL);

    if (exit_code != 0 || !file_exists(png_path))
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];
        snprintf(
            message,
            sizeof(message),
            "O script do grafico do neuronio terminou com erro.\n\n"
            "Python usado:\n%s\n\n"
            "Script:\n%s\n\n"
            "Pasta de resultados:\n%s\n\n"
            "Pandas e matplotlib podem estar ausentes, ou o CSV do neuronio pode nao existir.\n\n"
            "Teste manual:\n%s",
            python_path,
            script_path,
            output_path,
            command);
        show_error("Erro ao gerar grafico do neuronio", message);
        set_status("ERRO AO GERAR GRAFICO DO NEURONIO");
        return;
    }

    snprintf(
        status,
        sizeof(status),
        "GRAFICO DO NEURONIO GERADO: neuron_%d_detail.png",
        neuron_id);
    show_info("Grafico do neuronio gerado", status);
    set_status(status);
}

static int CALLBACK browse_folder_callback(
    HWND hwnd,
    UINT message,
    LPARAM lparam,
    LPARAM data)
{
    (void)lparam;

    if (message == BFFM_INITIALIZED && data != 0)
    {
        SendMessageA(hwnd, BFFM_SETSELECTIONA, TRUE, data);
    }

    return 0;
}

static int select_folder(
    const char *title,
    const char *initial_path,
    char *out_path,
    size_t out_path_size)
{
    BROWSEINFOA browse_info;
    LPITEMIDLIST item_id;
    char selected_path[MAX_PATH];

    memset(&browse_info, 0, sizeof(browse_info));
    browse_info.hwndOwner = g_app.window;
    browse_info.lpszTitle = title;
    browse_info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    if (initial_path != NULL && initial_path[0] != '\0' && directory_exists(initial_path))
    {
        browse_info.lpfn = browse_folder_callback;
        browse_info.lParam = (LPARAM)initial_path;
    }

    item_id = SHBrowseForFolderA(&browse_info);
    if (item_id == NULL)
        return 0;

    if (!SHGetPathFromIDListA(item_id, selected_path))
    {
        CoTaskMemFree(item_id);
        return 0;
    }

    CoTaskMemFree(item_id);
    return copy_path(out_path, out_path_size, selected_path);
}

static void make_comparison_name(
    char *out_name,
    size_t out_name_size)
{
    SYSTEMTIME now;

    GetLocalTime(&now);
    snprintf(
        out_name,
        out_name_size,
        "studio_compare_%04d%02d%02d_%02d%02d%02d_%03d",
        now.wYear,
        now.wMonth,
        now.wDay,
        now.wHour,
        now.wMinute,
        now.wSecond,
        now.wMilliseconds);
}

static void open_comparison_results(void)
{
    HINSTANCE result;

    if (!g_app.has_comparison)
    {
        show_error(
            "Comparacao nao encontrada",
            "Gere uma comparacao antes de abrir a pasta.");
        return;
    }

    if (!directory_exists(g_app.last_comparison_dir))
    {
        show_error(
            "Comparacao nao encontrada",
            "A pasta da ultima comparacao nao existe mais.");
        g_app.has_comparison = 0;
        set_comparison_buttons_enabled();
        return;
    }

    result = ShellExecuteA(
        g_app.window,
        "open",
        g_app.last_comparison_dir,
        NULL,
        g_app.project_root,
        SW_SHOWNORMAL);

    if ((INT_PTR)result <= 32)
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];
        snprintf(
            message,
            sizeof(message),
            "Nao foi possivel abrir a pasta da comparacao.\n\n"
            "Caminho tentado:\n%s\n\n"
            "Codigo retornado pelo ShellExecuteA: %ld",
            g_app.last_comparison_dir,
            (long)(INT_PTR)result);
        show_error("Erro ao abrir comparacao", message);
        return;
    }

    set_status("PASTA DA COMPARACAO ABERTA");
}

static void compare_runs_from_studio(void)
{
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    char first_run[MAX_PATH];
    char second_run[MAX_PATH];
    char python_path[MAX_PATH];
    char script_path[MAX_PATH];
    char scenarios_path[MAX_PATH];
    char comparison_name[128];
    char comparison_relative[MAX_PATH];
    char comparison_path[MAX_PATH];
    char command[PYTHON_COMMAND_BUFFER_SIZE];
    char status[STATUS_BUFFER_SIZE];
    char old_backend[TEXT_BUFFER_SIZE];
    DWORD old_backend_length;
    int had_old_backend = 0;
    DWORD exit_code = 1;

    if (!scenarios_directory_path(scenarios_path, sizeof(scenarios_path)))
    {
        show_error(
            "Erro ao comparar execucoes",
            "Nao foi possivel localizar a pasta inicial de cenarios.");
        return;
    }

    if (!select_folder(
            "Selecione a primeira pasta de execucao em results/scenarios",
            scenarios_path,
            first_run,
            sizeof(first_run)))
    {
        set_status("SELECAO DE COMPARACAO CANCELADA");
        return;
    }

    if (!select_folder(
            "Selecione a segunda pasta de execucao em results/scenarios",
            scenarios_path,
            second_run,
            sizeof(second_run)))
    {
        set_status("SELECAO DE COMPARACAO CANCELADA");
        return;
    }

    if (!resolve_python_executable(python_path, sizeof(python_path)))
    {
        show_error(
            "Python nao encontrado",
            "Python valido nao encontrado.\n"
            "Verifique se pandas e matplotlib estao instalados.");
        set_status("PYTHON COMPATIVEL NAO ENCONTRADO");
        return;
    }

    make_comparison_name(comparison_name, sizeof(comparison_name));

    if (!project_path(script_path, sizeof(script_path), "scripts\\compare_runs.py") ||
        snprintf(
            comparison_relative,
            sizeof(comparison_relative),
            "results\\comparisons\\%s",
            comparison_name) >= (int)sizeof(comparison_relative) ||
        !project_path(comparison_path, sizeof(comparison_path), comparison_relative))
    {
        show_error(
            "Erro ao comparar execucoes",
            "Nao foi possivel montar caminhos para a comparacao.");
        return;
    }

    snprintf(
        command,
        sizeof(command),
        g_app.resolved_python_uses_py_launcher ?
            "\"%s\" -3 \"%s\" \"%s\" \"%s\" --out-name \"%s\"" :
            "\"%s\" \"%s\" \"%s\" \"%s\" --out-name \"%s\"",
        python_path,
        script_path,
        first_run,
        second_run,
        comparison_name);

    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);

    snprintf(
        status,
        sizeof(status),
        "COMPARANDO EXECUCOES COM %s",
        python_path);
    set_status(status);
    UpdateWindow(g_app.window);

    old_backend_length = GetEnvironmentVariableA(
        "MPLBACKEND",
        old_backend,
        sizeof(old_backend));
    had_old_backend = old_backend_length > 0 &&
                      old_backend_length < sizeof(old_backend);
    SetEnvironmentVariableA("MPLBACKEND", "Agg");

    if (!CreateProcessA(
            NULL,
            command,
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            g_app.project_root,
            &startup,
            &process))
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];

        if (had_old_backend)
            SetEnvironmentVariableA("MPLBACKEND", old_backend);
        else
            SetEnvironmentVariableA("MPLBACKEND", NULL);

        snprintf(
            message,
            sizeof(message),
            "Nao foi possivel executar Python.\n\nPython usado:\n%s\n\n"
            "Script:\n%s\n\nTeste manual:\n%s",
            python_path,
            script_path,
            command);
        show_error("Erro ao comparar execucoes", message);
        set_status("ERRO AO COMPARAR EXECUCOES");
        return;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);

    if (had_old_backend)
        SetEnvironmentVariableA("MPLBACKEND", old_backend);
    else
        SetEnvironmentVariableA("MPLBACKEND", NULL);

    if (exit_code != 0 || !directory_exists(comparison_path))
    {
        char message[PYTHON_MESSAGE_BUFFER_SIZE];
        snprintf(
            message,
            sizeof(message),
            "A comparacao terminou com erro.\n\n"
            "Python usado:\n%s\n\n"
            "Primeira execucao:\n%s\n\n"
            "Segunda execucao:\n%s\n\n"
            "Teste manual:\n%s",
            python_path,
            first_run,
            second_run,
            command);
        show_error("Erro ao comparar execucoes", message);
        set_status("ERRO AO COMPARAR EXECUCOES");
        return;
    }

    copy_path(g_app.last_comparison_dir, sizeof(g_app.last_comparison_dir), comparison_path);
    g_app.has_comparison = 1;
    set_comparison_buttons_enabled();

    snprintf(
        status,
        sizeof(status),
        "COMPARACAO GERADA: %s",
        comparison_relative);
    show_info("Comparacao gerada", status);
    set_status(status);
}

static HWND create_static(
    HWND parent,
    const char *text,
    int x,
    int y,
    int w,
    int h,
    int id)
{
    HWND hwnd = CreateWindowExA(
        0,
        "STATIC",
        text,
        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
        x,
        y,
        w,
        h,
        parent,
        (HMENU)(INT_PTR)id,
        GetModuleHandleA(NULL),
        NULL);

    SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_app.normal_font, TRUE);
    return hwnd;
}

static HWND create_edit(
    HWND parent,
    int id,
    int x,
    int y,
    int w,
    int h)
{
    HWND hwnd = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x,
        y,
        w,
        h,
        parent,
        (HMENU)(INT_PTR)id,
        GetModuleHandleA(NULL),
        NULL);

    SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_app.edit_font, TRUE);
    return hwnd;
}

static HWND create_button(
    HWND parent,
    const char *text,
    int id,
    int x,
    int y,
    int w,
    int h)
{
    HWND hwnd = CreateWindowExA(
        0,
        "BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x,
        y,
        w,
        h,
        parent,
        (HMENU)(INT_PTR)id,
        GetModuleHandleA(NULL),
        NULL);

    SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_app.normal_font, TRUE);
    return hwnd;
}

static HWND create_checkbox(
    HWND parent,
    const char *text,
    int id,
    int x,
    int y,
    int w,
    int h)
{
    HWND hwnd = CreateWindowExA(
        0,
        "BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x,
        y,
        w,
        h,
        parent,
        (HMENU)(INT_PTR)id,
        GetModuleHandleA(NULL),
        NULL);

    SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_app.normal_font, TRUE);
    return hwnd;
}

static int parse_int_from_window(
    HWND hwnd,
    const char *field_name,
    int *out_value,
    char *error_message,
    size_t error_message_size)
{
    char text[TEXT_BUFFER_SIZE];
    char *end;
    long value;

    GetWindowTextA(hwnd, text, sizeof(text));

    errno = 0;
    value = strtol(text, &end, 10);

    while (*end != '\0' && isspace((unsigned char)*end))
        end++;

    if (text == end || errno != 0 || *end != '\0' ||
        value < -2147483647L - 1L || value > 2147483647L)
    {
        snprintf(error_message, error_message_size, "Campo invalido: %s.", field_name);
        return 0;
    }

    *out_value = (int)value;
    return 1;
}

static int parse_uint_from_window(
    HWND hwnd,
    const char *field_name,
    unsigned int *out_value,
    char *error_message,
    size_t error_message_size)
{
    char text[TEXT_BUFFER_SIZE];
    char *end;
    char *start;
    unsigned long value;

    GetWindowTextA(hwnd, text, sizeof(text));

    start = text;
    while (*start != '\0' && isspace((unsigned char)*start))
        start++;

    if (*start == '-' || *start == '+')
    {
        snprintf(error_message, error_message_size, "Campo invalido: %s.", field_name);
        return 0;
    }

    errno = 0;
    value = strtoul(text, &end, 10);

    while (*end != '\0' && isspace((unsigned char)*end))
        end++;

    if (text == end || errno != 0 || *end != '\0' || value > 4294967295UL)
    {
        snprintf(error_message, error_message_size, "Campo invalido: %s.", field_name);
        return 0;
    }

    *out_value = (unsigned int)value;
    return 1;
}

static int parse_double_from_window(
    HWND hwnd,
    const char *field_name,
    double *out_value,
    char *error_message,
    size_t error_message_size)
{
    char text[TEXT_BUFFER_SIZE];
    char *end;
    double value;

    GetWindowTextA(hwnd, text, sizeof(text));

    errno = 0;
    value = strtod(text, &end);

    while (*end != '\0' && isspace((unsigned char)*end))
        end++;

    if (text == end || errno != 0 || *end != '\0' || !isfinite(value))
    {
        snprintf(error_message, error_message_size, "Campo invalido: %s.", field_name);
        return 0;
    }

    *out_value = value;
    return 1;
}

static void set_window_double(HWND hwnd, double value)
{
    char text[TEXT_BUFFER_SIZE];
    format_double_for_field(text, sizeof(text), value);
    SetWindowTextA(hwnd, text);
}

static void update_options_enabled(void)
{
    int density_enabled = topology_uses_density(g_options.topology);
    int seed_enabled = topology_uses_seed(g_options.topology);
    int small_world_enabled =
        topology_uses_small_world_options(g_options.topology);
    int feedforward_enabled =
        topology_uses_feedforward_options(g_options.topology);
    int self_enabled =
        topology_can_allow_self_connections(g_options.topology);
    int inh_enabled = g_options.working_config.inhibitory_fraction > 0.0;

    EnableWindow(g_options.allow_self_checkbox, self_enabled);
    EnableWindow(g_options.allow_self_label, self_enabled);
    EnableWindow(g_options.allow_inh_to_inh_checkbox, inh_enabled);
    EnableWindow(g_options.allow_inh_to_inh_label, inh_enabled);
    EnableWindow(g_options.density_edit, density_enabled);
    EnableWindow(g_options.seed_edit, seed_enabled);
    EnableWindow(g_options.delay_edit, TRUE);
    EnableWindow(g_options.max_delay_edit, TRUE);
    EnableWindow(g_options.small_neighbors_edit, small_world_enabled);
    EnableWindow(g_options.small_rewire_edit, small_world_enabled);
    EnableWindow(g_options.feedforward_layers_edit, feedforward_enabled);
}

static void options_to_controls(void)
{
    char text[TEXT_BUFFER_SIZE];

    SendMessageA(
        g_options.allow_self_checkbox,
        BM_SETCHECK,
        g_options.working_config.allow_self_connections ? BST_CHECKED : BST_UNCHECKED,
        0);
    SendMessageA(
        g_options.allow_inh_to_inh_checkbox,
        BM_SETCHECK,
        g_options.working_config.allow_inh_to_inh ? BST_CHECKED : BST_UNCHECKED,
        0);

    set_window_double(
        g_options.density_edit,
        g_options.working_config.connection_probability);
    snprintf(text, sizeof(text), "%u", g_options.working_config.seed);
    SetWindowTextA(g_options.seed_edit, text);
    snprintf(text, sizeof(text), "%d", g_options.working_config.delay);
    SetWindowTextA(g_options.delay_edit, text);
    snprintf(text, sizeof(text), "%d", g_options.working_config.max_synaptic_delay);
    SetWindowTextA(g_options.max_delay_edit, text);
    snprintf(text, sizeof(text), "%d", g_options.working_config.small_world_neighbors);
    SetWindowTextA(g_options.small_neighbors_edit, text);
    set_window_double(
        g_options.small_rewire_edit,
        g_options.working_config.small_world_rewire_probability);
    snprintf(text, sizeof(text), "%d", g_options.working_config.feedforward_layers);
    SetWindowTextA(g_options.feedforward_layers_edit, text);

    update_options_enabled();
}

static int apply_topology_options(void)
{
    ScenarioConfig candidate = g_options.working_config;
    char error[256];

    candidate.allow_self_connections =
        SendMessageA(g_options.allow_self_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
    candidate.allow_inh_to_inh =
        SendMessageA(g_options.allow_inh_to_inh_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;

    if (!parse_double_from_window(
            g_options.density_edit,
            "Densidade de conexao",
            &candidate.connection_probability,
            error,
            sizeof(error)) ||
        !parse_uint_from_window(
            g_options.seed_edit,
            "Seed",
            &candidate.seed,
            error,
            sizeof(error)) ||
        !parse_int_from_window(
            g_options.delay_edit,
            "Delay",
            &candidate.delay,
            error,
            sizeof(error)) ||
        !parse_int_from_window(
            g_options.max_delay_edit,
            "Delay maximo",
            &candidate.max_synaptic_delay,
            error,
            sizeof(error)) ||
        !parse_int_from_window(
            g_options.small_neighbors_edit,
            "Small-world: vizinhos locais",
            &candidate.small_world_neighbors,
            error,
            sizeof(error)) ||
        !parse_double_from_window(
            g_options.small_rewire_edit,
            "Small-world: probabilidade de reconexao",
            &candidate.small_world_rewire_probability,
            error,
            sizeof(error)) ||
        !parse_int_from_window(
            g_options.feedforward_layers_edit,
            "Feedforward: numero de camadas",
            &candidate.feedforward_layers,
            error,
            sizeof(error)))
    {
        show_error("Opcoes invalidas", error);
        return 0;
    }

    if (!scenario_config_validate(&candidate, error, sizeof(error)))
    {
        show_error("Opcoes invalidas", error);
        return 0;
    }

    g_app.current_config = candidate;
    config_to_controls(&g_app.current_config);
    set_status("OPCOES DE TOPOLOGIA ATUALIZADAS");
    g_options.applied = 1;
    DestroyWindow(g_options.window);
    return 1;
}

static LRESULT CALLBACK topology_options_proc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam)
{
    (void)lparam;

    switch (message)
    {
    case WM_CREATE:
        g_options.window = hwnd;

        create_static(hwnd, "OPCOES DA TOPOLOGIA", 22, 18, 360, 24, 0);
        g_options.allow_self_checkbox = create_checkbox(
            hwnd,
            "",
            IDC_OPT_ALLOW_SELF,
            24,
            54,
            24,
            24);
        g_options.allow_self_label = create_static(
            hwnd,
            "Permitir auto-conexao",
            56,
            56,
            260,
            24,
            0);
        g_options.allow_inh_to_inh_checkbox = create_checkbox(
            hwnd,
            "",
            IDC_OPT_ALLOW_INH_TO_INH,
            24,
            84,
            24,
            24);
        g_options.allow_inh_to_inh_label = create_static(
            hwnd,
            "Permitir INH -> INH",
            56,
            86,
            260,
            24,
            0);

        create_static(hwnd, "Densidade de conexao", 24, 128, 260, 24, 0);
        g_options.density_edit = create_edit(hwnd, IDC_OPT_DENSITY, 300, 124, 140, 28);
        create_static(hwnd, "Seed", 24, 164, 260, 24, 0);
        g_options.seed_edit = create_edit(hwnd, IDC_OPT_SEED, 300, 160, 140, 28);
        create_static(hwnd, "Delay", 24, 200, 260, 24, 0);
        g_options.delay_edit = create_edit(hwnd, IDC_OPT_DELAY, 300, 196, 140, 28);
        create_static(hwnd, "Delay maximo", 24, 236, 260, 24, 0);
        g_options.max_delay_edit = create_edit(hwnd, IDC_OPT_MAX_DELAY, 300, 232, 140, 28);

        create_static(hwnd, "Small-world: vizinhos locais", 24, 280, 260, 24, 0);
        g_options.small_neighbors_edit = create_edit(hwnd, IDC_OPT_SMALL_NEIGHBORS, 300, 276, 140, 28);
        create_static(hwnd, "Small-world: probabilidade de reconexao", 24, 316, 270, 24, 0);
        g_options.small_rewire_edit = create_edit(hwnd, IDC_OPT_SMALL_REWIRE, 300, 312, 140, 28);
        create_static(hwnd, "Feedforward: numero de camadas", 24, 352, 260, 24, 0);
        g_options.feedforward_layers_edit = create_edit(hwnd, IDC_OPT_FEEDFORWARD_LAYERS, 300, 348, 140, 28);

        create_button(hwnd, "APLICAR", IDC_OPT_APPLY, 110, 400, 130, 36);
        create_button(hwnd, "CANCELAR", IDC_OPT_CANCEL, 260, 400, 130, 36);

        options_to_controls();
        enable_dark_title_bar(hwnd);
        return 0;

    case WM_COMMAND:
        if (HIWORD(wparam) == BN_CLICKED)
        {
            if (LOWORD(wparam) == IDC_OPT_APPLY)
            {
                apply_topology_options();
                return 0;
            }

            if (LOWORD(wparam) == IDC_OPT_CANCEL)
            {
                DestroyWindow(hwnd);
                return 0;
            }
        }
        break;

    case WM_CTLCOLORSTATIC:
        return handle_color((HDC)wparam, (HWND)lparam);

    case WM_CTLCOLOREDIT:
        return handle_edit_color((HDC)wparam);

    case WM_DRAWITEM:
        draw_button((const DRAWITEMSTRUCT *)lparam);
        return TRUE;

    case WM_ERASEBKGND:
    {
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect((HDC)wparam, &rect, g_app.background_brush);
        return 1;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_options.window = NULL;
        return 0;

    default:
        break;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static int ensure_options_class_registered(void)
{
    static int registered = 0;
    WNDCLASSA window_class;

    if (registered)
        return 1;

    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = topology_options_proc;
    window_class.hInstance = GetModuleHandleA(NULL);
    window_class.lpszClassName = "MiniSNNTopologyOptionsWindow";
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = g_app.background_brush;

    if (!RegisterClassA(&window_class))
        return 0;

    registered = 1;
    return 1;
}

static void open_topology_options(void)
{
    ScenarioConfig config;
    char error[256];
    HWND dialog;
    MSG message;

    if (!controls_to_config(&config, error, sizeof(error)))
    {
        show_error("Configuracao invalida", error);
        return;
    }

    if (!ensure_options_class_registered())
    {
        show_error("Erro interno", "Nao foi possivel criar a janela de opcoes.");
        return;
    }

    memset(&g_options, 0, sizeof(g_options));
    g_options.original_config = config;
    g_options.working_config = config;
    snprintf(g_options.topology, sizeof(g_options.topology), "%s", config.topology);

    dialog = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        "MiniSNNTopologyOptionsWindow",
        "OPCOES DA TOPOLOGIA",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        500,
        490,
        g_app.window,
        NULL,
        GetModuleHandleA(NULL),
        NULL);

    if (dialog == NULL)
    {
        show_error("Erro interno", "Nao foi possivel abrir a janela de opcoes.");
        return;
    }

    EnableWindow(g_app.window, FALSE);
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);

    while (g_options.window != NULL && GetMessageA(&message, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessageA(dialog, &message))
        {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }

    EnableWindow(g_app.window, TRUE);
    SetActiveWindow(g_app.window);
}

static void plasticity_to_controls(void)
{
    const ScenarioConfig *config = &g_plasticity.working_config;

    SendMessageA(
        g_plasticity.enabled_checkbox,
        BM_SETCHECK,
        config->plasticity_enabled ? BST_CHECKED : BST_UNCHECKED,
        0);
    SendMessageA(
        g_plasticity.rule_combo,
        CB_SELECTSTRING,
        (WPARAM)-1,
        (LPARAM)config->plasticity_rule);
    SendMessageA(
        g_plasticity.learning_mode_combo,
        CB_SELECTSTRING,
        (WPARAM)-1,
        (LPARAM)config->plasticity_learning_mode);
    set_window_double(g_plasticity.a_plus_edit, config->plasticity_a_plus);
    set_window_double(g_plasticity.a_minus_edit, config->plasticity_a_minus);
    set_window_double(g_plasticity.tau_plus_edit, config->plasticity_tau_plus);
    set_window_double(g_plasticity.tau_minus_edit, config->plasticity_tau_minus);
    set_window_double(
        g_plasticity.trace_increment_edit,
        config->plasticity_trace_increment);
    set_window_double(g_plasticity.weight_min_edit, config->plasticity_weight_min);
    set_window_double(g_plasticity.weight_max_edit, config->plasticity_weight_max);
    SendMessageA(
        g_plasticity.record_weights_checkbox,
        BM_SETCHECK,
        config->plasticity_record_weights ? BST_CHECKED : BST_UNCHECKED,
        0);
    SendMessageA(
        g_plasticity.record_history_checkbox,
        BM_SETCHECK,
        config->plasticity_record_history ? BST_CHECKED : BST_UNCHECKED,
        0);
    {
        char text[TEXT_BUFFER_SIZE];
        snprintf(text, sizeof(text), "%d", config->plasticity_record_interval_steps);
        SetWindowTextA(g_plasticity.interval_edit, text);
        snprintf(text, sizeof(text), "%d", config->plasticity_record_connection_limit);
        SetWindowTextA(g_plasticity.limit_edit, text);
    }
}

static int apply_plasticity_options(void)
{
    ScenarioConfig candidate = g_plasticity.working_config;
    char error[256];

    candidate.plasticity_enabled =
        SendMessageA(g_plasticity.enabled_checkbox, BM_GETCHECK, 0, 0) ==
        BST_CHECKED;
    candidate.plasticity_record_weights =
        SendMessageA(
            g_plasticity.record_weights_checkbox,
            BM_GETCHECK,
            0,
            0) == BST_CHECKED;
    candidate.plasticity_record_history =
        SendMessageA(
            g_plasticity.record_history_checkbox,
            BM_GETCHECK,
            0,
            0) == BST_CHECKED;
    GetWindowTextA(
        g_plasticity.rule_combo,
        candidate.plasticity_rule,
        sizeof(candidate.plasticity_rule));
    GetWindowTextA(
        g_plasticity.learning_mode_combo,
        candidate.plasticity_learning_mode,
        sizeof(candidate.plasticity_learning_mode));

    if (strcmp(candidate.plasticity_learning_mode,
               "reward_modulated_stdp") == 0)
    {
        candidate.plasticity_enabled = 1;
        candidate.reward_enabled = 1;
    }
    else if (candidate.reward_enabled)
    {
        candidate.reward_enabled = 0;
        candidate.reward_event_count = 0;
    }

    if (!parse_double_from_window(
            g_plasticity.a_plus_edit,
            "A+",
            &candidate.plasticity_a_plus,
            error,
            sizeof(error)) ||
        !parse_double_from_window(
            g_plasticity.a_minus_edit,
            "A-",
            &candidate.plasticity_a_minus,
            error,
            sizeof(error)) ||
        !parse_double_from_window(
            g_plasticity.tau_plus_edit,
            "TAU+",
            &candidate.plasticity_tau_plus,
            error,
            sizeof(error)) ||
        !parse_double_from_window(
            g_plasticity.tau_minus_edit,
            "TAU-",
            &candidate.plasticity_tau_minus,
            error,
            sizeof(error)) ||
        !parse_double_from_window(
            g_plasticity.trace_increment_edit,
            "TRACE INC",
            &candidate.plasticity_trace_increment,
            error,
            sizeof(error)) ||
        !parse_double_from_window(
            g_plasticity.weight_min_edit,
            "PESO MIN",
            &candidate.plasticity_weight_min,
            error,
            sizeof(error)) ||
        !parse_double_from_window(
            g_plasticity.weight_max_edit,
            "PESO MAX",
            &candidate.plasticity_weight_max,
            error,
            sizeof(error)) ||
        !parse_int_from_window(
            g_plasticity.interval_edit,
            "INTERVALO",
            &candidate.plasticity_record_interval_steps,
            error,
            sizeof(error)) ||
        !parse_int_from_window(
            g_plasticity.limit_edit,
            "LIMITE DE CONEXOES",
            &candidate.plasticity_record_connection_limit,
            error,
            sizeof(error)))
    {
        show_error("Plasticidade invalida", error);
        return 0;
    }

    if (!scenario_config_validate(&candidate, error, sizeof(error)))
    {
        show_error("Plasticidade invalida", error);
        return 0;
    }

    g_app.current_config = candidate;
    g_plasticity.working_config = candidate;
    set_status(candidate.plasticity_enabled ? "STDP ATIVADO" : "STDP DESATIVADO");
    DestroyWindow(g_plasticity.window);
    return 1;
}

static LRESULT CALLBACK plasticity_options_proc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam)
{
    (void)lparam;

    switch (message)
    {
    case WM_CREATE:
        g_plasticity.window = hwnd;
        create_static(hwnd, "PLASTICIDADE SINAPTICA", 24, 18, 360, 24, 0);
        g_plasticity.enabled_checkbox = create_checkbox(
            hwnd, "", IDC_PLASTICITY_ENABLED, 26, 54, 24, 24);
        create_static(hwnd, "STDP: OFF / ON", 58, 56, 220, 24, 0);
        create_static(hwnd, "Regra", 330, 56, 80, 24, 0);
        g_plasticity.rule_combo = CreateWindowExA(
            0,
            "COMBOBOX",
            "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            410,
            52,
            190,
            100,
            hwnd,
            (HMENU)(INT_PTR)IDC_PLASTICITY_RULE,
            GetModuleHandleA(NULL),
            NULL);
        SendMessageA(
            g_plasticity.rule_combo,
            WM_SETFONT,
            (WPARAM)g_app.edit_font,
            TRUE);
        SendMessageA(
            g_plasticity.rule_combo,
            CB_ADDSTRING,
            0,
            (LPARAM)"stdp_pair_trace");

        create_static(hwnd, "MODO DE APRENDIZADO", 24, 94, 190, 24, 0);
        g_plasticity.learning_mode_combo = CreateWindowExA(
            0,
            "COMBOBOX",
            "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            215,
            90,
            385,
            120,
            hwnd,
            (HMENU)(INT_PTR)IDC_PLASTICITY_LEARNING_MODE,
            GetModuleHandleA(NULL),
            NULL);
        SendMessageA(g_plasticity.learning_mode_combo, WM_SETFONT,
            (WPARAM)g_app.edit_font, TRUE);
        SendMessageA(g_plasticity.learning_mode_combo, CB_ADDSTRING, 0,
            (LPARAM)"direct_stdp");
        SendMessageA(g_plasticity.learning_mode_combo, CB_ADDSTRING, 0,
            (LPARAM)"reward_modulated_stdp");

        create_static(hwnd, "A+", 24, 150, 180, 24, 0);
        g_plasticity.a_plus_edit = create_edit(hwnd, IDC_PLASTICITY_A_PLUS, 190, 146, 120, 28);
        create_static(hwnd, "A-", 330, 150, 180, 24, 0);
        g_plasticity.a_minus_edit = create_edit(hwnd, IDC_PLASTICITY_A_MINUS, 480, 146, 120, 28);
        create_static(hwnd, "TAU+", 24, 190, 180, 24, 0);
        g_plasticity.tau_plus_edit = create_edit(hwnd, IDC_PLASTICITY_TAU_PLUS, 190, 186, 120, 28);
        create_static(hwnd, "TAU-", 330, 190, 180, 24, 0);
        g_plasticity.tau_minus_edit = create_edit(hwnd, IDC_PLASTICITY_TAU_MINUS, 480, 186, 120, 28);
        create_static(hwnd, "TRACE INC", 24, 230, 180, 24, 0);
        g_plasticity.trace_increment_edit = create_edit(hwnd, IDC_PLASTICITY_TRACE_INCREMENT, 190, 226, 120, 28);
        create_static(hwnd, "PESO MIN", 24, 270, 180, 24, 0);
        g_plasticity.weight_min_edit = create_edit(hwnd, IDC_PLASTICITY_WEIGHT_MIN, 190, 266, 120, 28);
        create_static(hwnd, "PESO MAX", 330, 270, 180, 24, 0);
        g_plasticity.weight_max_edit = create_edit(hwnd, IDC_PLASTICITY_WEIGHT_MAX, 480, 266, 120, 28);

        create_static(hwnd, "REGISTRO", 24, 326, 220, 24, 0);
        g_plasticity.record_weights_checkbox = create_checkbox(
            hwnd, "", IDC_PLASTICITY_RECORD_WEIGHTS, 26, 362, 24, 24);
        create_static(hwnd, "REGISTRAR PESOS", 58, 364, 220, 24, 0);
        g_plasticity.record_history_checkbox = create_checkbox(
            hwnd, "", IDC_PLASTICITY_RECORD_HISTORY, 330, 362, 24, 24);
        create_static(hwnd, "REGISTRAR HISTORICO", 362, 364, 238, 24, 0);
        create_static(hwnd, "INTERVALO", 24, 412, 180, 24, 0);
        g_plasticity.interval_edit = create_edit(hwnd, IDC_PLASTICITY_INTERVAL, 190, 408, 120, 28);
        create_static(hwnd, "LIMITE DE CONEXOES", 330, 412, 200, 24, 0);
        g_plasticity.limit_edit = create_edit(hwnd, IDC_PLASTICITY_LIMIT, 480, 408, 120, 28);

        create_static(
            hwnd,
            "STDP aditivo por traces; apenas sinapses de origem EXC sao plasticas.",
            24,
            468,
            576,
            24,
            0);
        create_button(hwnd, "APLICAR", IDC_PLASTICITY_APPLY, 170, 516, 130, 36);
        create_button(hwnd, "CANCELAR", IDC_PLASTICITY_CANCEL, 330, 516, 130, 36);
        plasticity_to_controls();
        enable_dark_title_bar(hwnd);
        return 0;

    case WM_COMMAND:
        if (HIWORD(wparam) == BN_CLICKED)
        {
            if (LOWORD(wparam) == IDC_PLASTICITY_APPLY)
            {
                apply_plasticity_options();
                return 0;
            }
            if (LOWORD(wparam) == IDC_PLASTICITY_CANCEL)
            {
                DestroyWindow(hwnd);
                return 0;
            }
        }
        break;

    case WM_CTLCOLORSTATIC:
        return handle_color((HDC)wparam, (HWND)lparam);
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        return handle_edit_color((HDC)wparam);
    case WM_DRAWITEM:
        draw_button((const DRAWITEMSTRUCT *)lparam);
        return TRUE;
    case WM_ERASEBKGND:
    {
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect((HDC)wparam, &rect, g_app.background_brush);
        return 1;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_plasticity.window = NULL;
        return 0;
    default:
        break;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static int ensure_plasticity_class_registered(void)
{
    static int registered = 0;
    WNDCLASSA window_class;

    if (registered)
        return 1;

    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = plasticity_options_proc;
    window_class.hInstance = GetModuleHandleA(NULL);
    window_class.lpszClassName = "MiniSNNPlasticityOptionsWindow";
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = g_app.background_brush;

    if (!RegisterClassA(&window_class))
        return 0;

    registered = 1;
    return 1;
}

static void open_plasticity_options(void)
{
    ScenarioConfig config;
    char error[256];
    HWND dialog;
    MSG message;

    if (!controls_to_config(&config, error, sizeof(error)))
    {
        show_error("Configuracao invalida", error);
        return;
    }

    if (!ensure_plasticity_class_registered())
    {
        show_error("Erro interno", "Nao foi possivel criar a janela de plasticidade.");
        return;
    }

    memset(&g_plasticity, 0, sizeof(g_plasticity));
    g_plasticity.working_config = config;

    dialog = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        "MiniSNNPlasticityOptionsWindow",
        "PLASTICIDADE",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        650,
        620,
        g_app.window,
        NULL,
        GetModuleHandleA(NULL),
        NULL);

    if (dialog == NULL)
    {
        show_error("Erro interno", "Nao foi possivel abrir a janela de plasticidade.");
        return;
    }

    EnableWindow(g_app.window, FALSE);
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);

    while (g_plasticity.window != NULL && GetMessageA(&message, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessageA(dialog, &message))
        {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }

    EnableWindow(g_app.window, TRUE);
    SetActiveWindow(g_app.window);
}

static void homeostasis_to_controls(void)
{
    const ScenarioConfig *config = &g_homeostasis.working_config;
    char text[TEXT_BUFFER_SIZE];

    SendMessageA(g_homeostasis.enabled_checkbox, BM_SETCHECK,
        config->homeostasis_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(g_homeostasis.intrinsic_checkbox, BM_SETCHECK,
        config->homeostasis_intrinsic_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(g_homeostasis.scaling_checkbox, BM_SETCHECK,
        config->homeostasis_synaptic_scaling_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(g_homeostasis.gain_checkbox, BM_SETCHECK,
        config->homeostasis_inhibitory_gain_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(g_homeostasis.record_history_checkbox, BM_SETCHECK,
        config->homeostasis_record_history ? BST_CHECKED : BST_UNCHECKED, 0);

    set_window_double(g_homeostasis.target_rate_edit, config->homeostasis_target_rate);
    set_window_double(g_homeostasis.rate_tau_edit, config->homeostasis_rate_tau);
    set_window_double(g_homeostasis.threshold_eta_edit, config->homeostasis_threshold_eta);
    set_window_double(g_homeostasis.threshold_min_edit, config->homeostasis_threshold_min);
    set_window_double(g_homeostasis.threshold_max_edit, config->homeostasis_threshold_max);
    set_window_double(g_homeostasis.scaling_eta_edit, config->homeostasis_scaling_eta);
    set_window_double(g_homeostasis.scaling_factor_min_edit, config->homeostasis_scaling_min_factor);
    set_window_double(g_homeostasis.scaling_factor_max_edit, config->homeostasis_scaling_max_factor);
    set_window_double(g_homeostasis.scaling_weight_min_edit, config->homeostasis_scaling_weight_min);
    set_window_double(g_homeostasis.scaling_weight_max_edit, config->homeostasis_scaling_weight_max);
    set_window_double(g_homeostasis.gain_initial_edit, config->homeostasis_inhibitory_gain_initial);
    set_window_double(g_homeostasis.gain_eta_edit, config->homeostasis_inhibitory_gain_eta);
    set_window_double(g_homeostasis.gain_min_edit, config->homeostasis_inhibitory_gain_min);
    set_window_double(g_homeostasis.gain_max_edit, config->homeostasis_inhibitory_gain_max);

    snprintf(text, sizeof(text), "%d", config->homeostasis_update_interval_steps);
    SetWindowTextA(g_homeostasis.update_interval_edit, text);
    snprintf(text, sizeof(text), "%d", config->homeostasis_record_interval_steps);
    SetWindowTextA(g_homeostasis.record_interval_edit, text);
    snprintf(text, sizeof(text), "%d", config->homeostasis_record_neuron_limit);
    SetWindowTextA(g_homeostasis.record_limit_edit, text);
}

static int apply_homeostasis_options(void)
{
    ScenarioConfig candidate = g_homeostasis.working_config;
    char error[256];

    candidate.homeostasis_enabled =
        SendMessageA(g_homeostasis.enabled_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
    candidate.homeostasis_intrinsic_enabled =
        SendMessageA(g_homeostasis.intrinsic_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
    candidate.homeostasis_synaptic_scaling_enabled =
        SendMessageA(g_homeostasis.scaling_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
    candidate.homeostasis_inhibitory_gain_enabled =
        SendMessageA(g_homeostasis.gain_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
    candidate.homeostasis_record_history =
        SendMessageA(g_homeostasis.record_history_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
    snprintf(
        candidate.homeostasis_scaling_target_mode,
        sizeof(candidate.homeostasis_scaling_target_mode),
        "initial_incoming_sum");

    if (!parse_double_from_window(g_homeostasis.target_rate_edit, "TAXA ALVO",
            &candidate.homeostasis_target_rate, error, sizeof(error)) ||
        !parse_double_from_window(g_homeostasis.rate_tau_edit, "TAU TAXA",
            &candidate.homeostasis_rate_tau, error, sizeof(error)) ||
        !parse_int_from_window(g_homeostasis.update_interval_edit, "INTERVALO",
            &candidate.homeostasis_update_interval_steps, error, sizeof(error)) ||
        !parse_double_from_window(g_homeostasis.threshold_eta_edit, "ETA THRESHOLD",
            &candidate.homeostasis_threshold_eta, error, sizeof(error)) ||
        !parse_double_from_window(g_homeostasis.threshold_min_edit, "THRESHOLD MIN",
            &candidate.homeostasis_threshold_min, error, sizeof(error)) ||
        !parse_double_from_window(g_homeostasis.threshold_max_edit, "THRESHOLD MAX",
            &candidate.homeostasis_threshold_max, error, sizeof(error)) ||
        !parse_double_from_window(g_homeostasis.scaling_eta_edit, "ETA SCALING",
            &candidate.homeostasis_scaling_eta, error, sizeof(error)) ||
        !parse_double_from_window(g_homeostasis.scaling_factor_min_edit, "FATOR MIN",
            &candidate.homeostasis_scaling_min_factor, error, sizeof(error)) ||
        !parse_double_from_window(g_homeostasis.scaling_factor_max_edit, "FATOR MAX",
            &candidate.homeostasis_scaling_max_factor, error, sizeof(error)) ||
        !parse_double_from_window(g_homeostasis.scaling_weight_min_edit, "PESO MIN",
            &candidate.homeostasis_scaling_weight_min, error, sizeof(error)) ||
        !parse_double_from_window(g_homeostasis.scaling_weight_max_edit, "PESO MAX",
            &candidate.homeostasis_scaling_weight_max, error, sizeof(error)) ||
        !parse_double_from_window(g_homeostasis.gain_initial_edit, "GANHO INICIAL",
            &candidate.homeostasis_inhibitory_gain_initial, error, sizeof(error)) ||
        !parse_double_from_window(g_homeostasis.gain_eta_edit, "ETA GANHO",
            &candidate.homeostasis_inhibitory_gain_eta, error, sizeof(error)) ||
        !parse_double_from_window(g_homeostasis.gain_min_edit, "GANHO MIN",
            &candidate.homeostasis_inhibitory_gain_min, error, sizeof(error)) ||
        !parse_double_from_window(g_homeostasis.gain_max_edit, "GANHO MAX",
            &candidate.homeostasis_inhibitory_gain_max, error, sizeof(error)) ||
        !parse_int_from_window(g_homeostasis.record_interval_edit, "INTERVALO REGISTRO",
            &candidate.homeostasis_record_interval_steps, error, sizeof(error)) ||
        !parse_int_from_window(g_homeostasis.record_limit_edit, "LIMITE NEURONIOS",
            &candidate.homeostasis_record_neuron_limit, error, sizeof(error)))
    {
        show_error("Homeostase invalida", error);
        return 0;
    }

    if (!scenario_config_validate(&candidate, error, sizeof(error)))
    {
        show_error("Homeostase invalida", error);
        return 0;
    }

    g_app.current_config = candidate;
    g_homeostasis.working_config = candidate;
    set_status(candidate.homeostasis_enabled ? "HOMEOSTASE ATIVADA" : "HOMEOSTASE DESATIVADA");
    DestroyWindow(g_homeostasis.window);
    return 1;
}

static LRESULT CALLBACK homeostasis_options_proc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam)
{
    (void)lparam;
    switch (message)
    {
    case WM_CREATE:
        g_homeostasis.window = hwnd;
        create_static(hwnd, "HOMEOSTASE", 24, 16, 300, 28, 0);
        g_homeostasis.enabled_checkbox = create_checkbox(hwnd, "", IDC_HOME_ENABLED, 26, 50, 24, 24);
        create_static(hwnd, "HOMEOSTASE: OFF / ON", 58, 52, 240, 24, 0);
        g_homeostasis.intrinsic_checkbox = create_checkbox(hwnd, "", IDC_HOME_INTRINSIC, 320, 50, 24, 24);
        create_static(hwnd, "INTRINSECA: OFF / ON", 352, 52, 240, 24, 0);

        create_static(hwnd, "TAXA ALVO", 24, 100, 130, 24, 0);
        g_homeostasis.target_rate_edit = create_edit(hwnd, IDC_HOME_TARGET_RATE, 150, 96, 100, 28);
        create_static(hwnd, "TAU TAXA", 280, 100, 130, 24, 0);
        g_homeostasis.rate_tau_edit = create_edit(hwnd, IDC_HOME_RATE_TAU, 400, 96, 100, 28);
        create_static(hwnd, "INTERVALO", 530, 100, 130, 24, 0);
        g_homeostasis.update_interval_edit = create_edit(hwnd, IDC_HOME_UPDATE_INTERVAL, 650, 96, 100, 28);

        create_static(hwnd, "ETA THRESHOLD", 24, 140, 130, 24, 0);
        g_homeostasis.threshold_eta_edit = create_edit(hwnd, IDC_HOME_THRESHOLD_ETA, 150, 136, 100, 28);
        create_static(hwnd, "THRESHOLD MIN", 280, 140, 130, 24, 0);
        g_homeostasis.threshold_min_edit = create_edit(hwnd, IDC_HOME_THRESHOLD_MIN, 400, 136, 100, 28);
        create_static(hwnd, "THRESHOLD MAX", 530, 140, 130, 24, 0);
        g_homeostasis.threshold_max_edit = create_edit(hwnd, IDC_HOME_THRESHOLD_MAX, 650, 136, 100, 28);

        g_homeostasis.scaling_checkbox = create_checkbox(hwnd, "", IDC_HOME_SCALING, 26, 198, 24, 24);
        create_static(hwnd, "SCALING: OFF / ON", 58, 200, 210, 24, 0);
        create_static(hwnd, "ALVO: INITIAL INCOMING SUM", 320, 200, 340, 24, 0);
        create_static(hwnd, "ETA SCALING", 24, 244, 130, 24, 0);
        g_homeostasis.scaling_eta_edit = create_edit(hwnd, IDC_HOME_SCALING_ETA, 150, 240, 100, 28);
        create_static(hwnd, "FATOR MIN", 280, 244, 130, 24, 0);
        g_homeostasis.scaling_factor_min_edit = create_edit(hwnd, IDC_HOME_SCALING_FACTOR_MIN, 400, 240, 100, 28);
        create_static(hwnd, "FATOR MAX", 530, 244, 130, 24, 0);
        g_homeostasis.scaling_factor_max_edit = create_edit(hwnd, IDC_HOME_SCALING_FACTOR_MAX, 650, 240, 100, 28);
        create_static(hwnd, "PESO MIN", 24, 284, 130, 24, 0);
        g_homeostasis.scaling_weight_min_edit = create_edit(hwnd, IDC_HOME_SCALING_WEIGHT_MIN, 150, 280, 100, 28);
        create_static(hwnd, "PESO MAX", 280, 284, 130, 24, 0);
        g_homeostasis.scaling_weight_max_edit = create_edit(hwnd, IDC_HOME_SCALING_WEIGHT_MAX, 400, 280, 100, 28);

        g_homeostasis.gain_checkbox = create_checkbox(hwnd, "", IDC_HOME_GAIN, 26, 342, 24, 24);
        create_static(hwnd, "GANHO INH: OFF / ON", 58, 344, 240, 24, 0);
        create_static(hwnd, "GANHO INICIAL", 24, 388, 130, 24, 0);
        g_homeostasis.gain_initial_edit = create_edit(hwnd, IDC_HOME_GAIN_INITIAL, 150, 384, 100, 28);
        create_static(hwnd, "ETA GANHO", 280, 388, 130, 24, 0);
        g_homeostasis.gain_eta_edit = create_edit(hwnd, IDC_HOME_GAIN_ETA, 400, 384, 100, 28);
        create_static(hwnd, "GANHO MIN", 530, 388, 130, 24, 0);
        g_homeostasis.gain_min_edit = create_edit(hwnd, IDC_HOME_GAIN_MIN, 650, 384, 100, 28);
        create_static(hwnd, "GANHO MAX", 530, 428, 130, 24, 0);
        g_homeostasis.gain_max_edit = create_edit(hwnd, IDC_HOME_GAIN_MAX, 650, 424, 100, 28);

        g_homeostasis.record_history_checkbox = create_checkbox(hwnd, "", IDC_HOME_RECORD_HISTORY, 26, 490, 24, 24);
        create_static(hwnd, "REGISTRAR HISTORICO", 58, 492, 230, 24, 0);
        create_static(hwnd, "INTERVALO REGISTRO", 280, 492, 180, 24, 0);
        g_homeostasis.record_interval_edit = create_edit(hwnd, IDC_HOME_RECORD_INTERVAL, 450, 488, 100, 28);
        create_static(hwnd, "LIMITE NEURONIOS", 24, 532, 180, 24, 0);
        g_homeostasis.record_limit_edit = create_edit(hwnd, IDC_HOME_RECORD_LIMIT, 190, 528, 100, 28);

        create_static(hwnd,
            "Mecanismos simplificados de controle; nao garantem estabilidade universal.",
            24, 578, 700, 24, 0);
        create_button(hwnd, "APLICAR", IDC_HOME_APPLY, 245, 620, 130, 36);
        create_button(hwnd, "CANCELAR", IDC_HOME_CANCEL, 405, 620, 130, 36);
        homeostasis_to_controls();
        enable_dark_title_bar(hwnd);
        return 0;
    case WM_COMMAND:
        if (HIWORD(wparam) == BN_CLICKED)
        {
            if (LOWORD(wparam) == IDC_HOME_APPLY)
            {
                apply_homeostasis_options();
                return 0;
            }
            if (LOWORD(wparam) == IDC_HOME_CANCEL)
            {
                DestroyWindow(hwnd);
                return 0;
            }
        }
        break;
    case WM_CTLCOLORSTATIC:
        return handle_color((HDC)wparam, (HWND)lparam);
    case WM_CTLCOLOREDIT:
        return handle_edit_color((HDC)wparam);
    case WM_DRAWITEM:
        draw_button((const DRAWITEMSTRUCT *)lparam);
        return TRUE;
    case WM_ERASEBKGND:
    {
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect((HDC)wparam, &rect, g_app.background_brush);
        return 1;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_homeostasis.window = NULL;
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static int ensure_homeostasis_class_registered(void)
{
    static int registered = 0;
    WNDCLASSA window_class;
    if (registered)
        return 1;
    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = homeostasis_options_proc;
    window_class.hInstance = GetModuleHandleA(NULL);
    window_class.lpszClassName = "MiniSNNHomeostasisOptionsWindow";
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = g_app.background_brush;
    if (!RegisterClassA(&window_class))
        return 0;
    registered = 1;
    return 1;
}

static void open_homeostasis_options(void)
{
    ScenarioConfig config;
    char error[256];
    HWND dialog;
    MSG message;

    if (!controls_to_config(&config, error, sizeof(error)))
    {
        show_error("Configuracao invalida", error);
        return;
    }
    if (!ensure_homeostasis_class_registered())
    {
        show_error("Erro interno", "Nao foi possivel criar a janela de homeostase.");
        return;
    }
    memset(&g_homeostasis, 0, sizeof(g_homeostasis));
    g_homeostasis.working_config = config;
    dialog = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        "MiniSNNHomeostasisOptionsWindow",
        "HOMEOSTASE",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        710,
        g_app.window,
        NULL,
        GetModuleHandleA(NULL),
        NULL);
    if (dialog == NULL)
    {
        show_error("Erro interno", "Nao foi possivel abrir a janela de homeostase.");
        return;
    }
    EnableWindow(g_app.window, FALSE);
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);
    while (g_homeostasis.window != NULL && GetMessageA(&message, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessageA(dialog, &message))
        {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }
    EnableWindow(g_app.window, TRUE);
    SetActiveWindow(g_app.window);
}

static char *trim_reward_token(char *text)
{
    char *end;

    while (*text != '\0' && isspace((unsigned char)*text))
        text++;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
        end--;
    *end = '\0';
    return text;
}

static int reward_events_to_text(
    const ScenarioConfig *config,
    char *buffer,
    size_t buffer_size)
{
    size_t used = 0;
    int index;

    if (config == NULL || buffer == NULL || buffer_size == 0)
        return 0;
    buffer[0] = '\0';
    for (index = 0; index < config->reward_event_count; index++)
    {
        int written = snprintf(
            buffer + used,
            buffer_size - used,
            "%s%d:%.17g",
            index == 0 ? "" : "; ",
            config->reward_events[index].step,
            config->reward_events[index].value);
        if (written < 0 || (size_t)written >= buffer_size - used)
            return 0;
        used += (size_t)written;
    }
    return 1;
}

static int parse_reward_events_text(
    HWND edit,
    ScenarioConfig *config,
    char *error,
    size_t error_size)
{
    char text[REWARD_EVENTS_TEXT_SIZE];
    char *token;
    int length;
    int count = 0;

    length = GetWindowTextLengthA(edit);
    if (length < 0 || length >= (int)sizeof(text))
    {
        snprintf(error, error_size, "Lista de eventos excede o limite seguro.");
        return 0;
    }
    GetWindowTextA(edit, text, sizeof(text));
    token = strtok(text, ";");
    while (token != NULL)
    {
        char *entry = trim_reward_token(token);
        char *separator = strchr(entry, ':');
        char *step_end;
        char *value_end;
        long step;
        double value;
        int previous;

        if (*entry == '\0')
        {
            snprintf(error, error_size, "Evento vazio na lista de recompensa.");
            return 0;
        }
        if (separator == NULL || strchr(separator + 1, ':') != NULL)
        {
            snprintf(error, error_size,
                "Evento invalido: use STEP:VALOR separado por ponto e virgula.");
            return 0;
        }
        *separator = '\0';
        entry = trim_reward_token(entry);
        separator = trim_reward_token(separator + 1);
        errno = 0;
        step = strtol(entry, &step_end, 10);
        step_end = trim_reward_token(step_end);
        if (errno != 0 || *entry == '\0' || *step_end != '\0' ||
            step < 0 || step >= config->steps)
        {
            snprintf(error, error_size,
                "Step de reward invalido; use 0 ate %d.", config->steps - 1);
            return 0;
        }
        errno = 0;
        value = strtod(separator, &value_end);
        value_end = trim_reward_token(value_end);
        if (errno != 0 || *separator == '\0' || *value_end != '\0' ||
            !isfinite(value))
        {
            snprintf(error, error_size, "Valor de reward invalido.");
            return 0;
        }
        if (count >= SCENARIO_MAX_REWARD_EVENTS)
        {
            snprintf(error, error_size, "Limite de %d eventos excedido.",
                SCENARIO_MAX_REWARD_EVENTS);
            return 0;
        }
        for (previous = 0; previous < count; previous++)
        {
            if (config->reward_events[previous].step == (int)step &&
                config->reward_events[previous].value == value)
            {
                snprintf(error, error_size,
                    "Evento duplicado: %ld:%.17g.", step, value);
                return 0;
            }
        }
        config->reward_events[count].index = count;
        config->reward_events[count].step = (int)step;
        config->reward_events[count].value = value;
        count++;
        token = strtok(NULL, ";");
    }
    config->reward_event_count = count;
    return 1;
}

static void reward_to_controls(void)
{
    const ScenarioConfig *config = &g_reward.working_config;
    char text[REWARD_EVENTS_TEXT_SIZE];

    SendMessageA(g_reward.enabled_checkbox, BM_SETCHECK,
        config->reward_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(g_reward.clip_checkbox, BM_SETCHECK,
        config->reward_clip ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(g_reward.record_history_checkbox, BM_SETCHECK,
        config->reward_record_history ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(g_reward.mode_combo, CB_SELECTSTRING, (WPARAM)-1,
        (LPARAM)config->reward_mode);
    set_window_double(g_reward.learning_rate_edit, config->reward_learning_rate);
    set_window_double(g_reward.eligibility_tau_edit, config->reward_eligibility_tau);
    set_window_double(g_reward.eligibility_min_edit, config->reward_eligibility_min);
    set_window_double(g_reward.eligibility_max_edit, config->reward_eligibility_max);
    set_window_double(g_reward.reward_min_edit, config->reward_min);
    set_window_double(g_reward.reward_max_edit, config->reward_max);
    snprintf(text, sizeof(text), "%d", config->reward_record_interval_steps);
    SetWindowTextA(g_reward.record_interval_edit, text);
    snprintf(text, sizeof(text), "%d", config->reward_record_connection_limit);
    SetWindowTextA(g_reward.record_limit_edit, text);
    if (reward_events_to_text(config, text, sizeof(text)))
        SetWindowTextA(g_reward.events_edit, text);
}

static int apply_reward_options(void)
{
    ScenarioConfig candidate = g_reward.working_config;
    char error[256];

    candidate.reward_enabled =
        SendMessageA(g_reward.enabled_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
    candidate.reward_clip =
        SendMessageA(g_reward.clip_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
    candidate.reward_record_history =
        SendMessageA(g_reward.record_history_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
    GetWindowTextA(g_reward.mode_combo, candidate.reward_mode,
        sizeof(candidate.reward_mode));

    if (!parse_double_from_window(g_reward.learning_rate_edit, "LEARNING RATE",
            &candidate.reward_learning_rate, error, sizeof(error)) ||
        !parse_double_from_window(g_reward.eligibility_tau_edit, "TAU ELIGIBILIDADE",
            &candidate.reward_eligibility_tau, error, sizeof(error)) ||
        !parse_double_from_window(g_reward.eligibility_min_edit, "ELIGIBILIDADE MIN",
            &candidate.reward_eligibility_min, error, sizeof(error)) ||
        !parse_double_from_window(g_reward.eligibility_max_edit, "ELIGIBILIDADE MAX",
            &candidate.reward_eligibility_max, error, sizeof(error)) ||
        !parse_double_from_window(g_reward.reward_min_edit, "REWARD MIN",
            &candidate.reward_min, error, sizeof(error)) ||
        !parse_double_from_window(g_reward.reward_max_edit, "REWARD MAX",
            &candidate.reward_max, error, sizeof(error)) ||
        !parse_int_from_window(g_reward.record_interval_edit, "INTERVALO",
            &candidate.reward_record_interval_steps, error, sizeof(error)) ||
        !parse_int_from_window(g_reward.record_limit_edit, "LIMITE DE CONEXOES",
            &candidate.reward_record_connection_limit, error, sizeof(error)) ||
        !parse_reward_events_text(g_reward.events_edit, &candidate,
            error, sizeof(error)))
    {
        show_error("Recompensa invalida", error);
        return 0;
    }

    if (candidate.reward_enabled)
    {
        candidate.plasticity_enabled = 1;
        snprintf(candidate.plasticity_learning_mode,
            sizeof(candidate.plasticity_learning_mode),
            "reward_modulated_stdp");
    }
    else if (strcmp(candidate.plasticity_learning_mode,
                    "reward_modulated_stdp") == 0)
    {
        snprintf(candidate.plasticity_learning_mode,
            sizeof(candidate.plasticity_learning_mode), "direct_stdp");
        candidate.reward_event_count = 0;
    }

    if (!scenario_config_validate(&candidate, error, sizeof(error)))
    {
        show_error("Recompensa invalida", error);
        return 0;
    }
    g_app.current_config = candidate;
    g_reward.working_config = candidate;
    set_status(candidate.reward_enabled ? "RECOMPENSA R-STDP ATIVADA" :
        "RECOMPENSA DESATIVADA");
    DestroyWindow(g_reward.window);
    return 1;
}

static LRESULT CALLBACK reward_options_proc(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)lparam;
    switch (message)
    {
    case WM_CREATE:
        g_reward.window = hwnd;
        create_static(hwnd, "RECOMPENSA", 24, 16, 300, 28, 0);
        g_reward.enabled_checkbox = create_checkbox(hwnd, "", IDC_REWARD_ENABLED,
            26, 50, 24, 24);
        create_static(hwnd, "RECOMPENSA: OFF / ON", 58, 52, 250, 24, 0);
        create_static(hwnd, "MODO", 410, 52, 80, 24, 0);
        g_reward.mode_combo = CreateWindowExA(0, "COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            490, 48, 240, 100, hwnd, (HMENU)(INT_PTR)IDC_REWARD_MODE,
            GetModuleHandleA(NULL), NULL);
        SendMessageA(g_reward.mode_combo, WM_SETFONT,
            (WPARAM)g_app.edit_font, TRUE);
        SendMessageA(g_reward.mode_combo, CB_ADDSTRING, 0, (LPARAM)"rstdp");

        create_static(hwnd, "LEARNING RATE", 24, 104, 170, 24, 0);
        g_reward.learning_rate_edit = create_edit(hwnd,
            IDC_REWARD_LEARNING_RATE, 190, 100, 120, 28);
        create_static(hwnd, "TAU ELIGIBILIDADE", 390, 104, 190, 24, 0);
        g_reward.eligibility_tau_edit = create_edit(hwnd,
            IDC_REWARD_ELIGIBILITY_TAU, 580, 100, 150, 28);
        create_static(hwnd, "ELIGIBILIDADE MIN", 24, 150, 170, 24, 0);
        g_reward.eligibility_min_edit = create_edit(hwnd,
            IDC_REWARD_ELIGIBILITY_MIN, 190, 146, 120, 28);
        create_static(hwnd, "ELIGIBILIDADE MAX", 390, 150, 190, 24, 0);
        g_reward.eligibility_max_edit = create_edit(hwnd,
            IDC_REWARD_ELIGIBILITY_MAX, 580, 146, 150, 28);
        create_static(hwnd, "REWARD MIN", 24, 196, 170, 24, 0);
        g_reward.reward_min_edit = create_edit(hwnd, IDC_REWARD_MIN,
            190, 192, 120, 28);
        create_static(hwnd, "REWARD MAX", 390, 196, 190, 24, 0);
        g_reward.reward_max_edit = create_edit(hwnd, IDC_REWARD_MAX,
            580, 192, 150, 28);
        g_reward.clip_checkbox = create_checkbox(hwnd, "", IDC_REWARD_CLIP,
            26, 240, 24, 24);
        create_static(hwnd, "CLIP REWARD", 58, 242, 190, 24, 0);

        g_reward.record_history_checkbox = create_checkbox(hwnd, "",
            IDC_REWARD_RECORD_HISTORY, 26, 290, 24, 24);
        create_static(hwnd, "REGISTRAR HISTORICO", 58, 292, 230, 24, 0);
        create_static(hwnd, "INTERVALO", 310, 292, 130, 24, 0);
        g_reward.record_interval_edit = create_edit(hwnd,
            IDC_REWARD_RECORD_INTERVAL, 430, 288, 100, 28);
        create_static(hwnd, "LIMITE", 550, 292, 90, 24, 0);
        g_reward.record_limit_edit = create_edit(hwnd,
            IDC_REWARD_RECORD_LIMIT, 630, 288, 100, 28);

        create_static(hwnd, "EVENTOS (STEP:VALOR; STEP:VALOR)",
            24, 342, 520, 24, 0);
        g_reward.events_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE |
            ES_AUTOVSCROLL | WS_VSCROLL,
            24, 372, 706, 120, hwnd,
            (HMENU)(INT_PTR)IDC_REWARD_EVENTS, GetModuleHandleA(NULL), NULL);
        SendMessageA(g_reward.events_edit, WM_SETFONT,
            (WPARAM)g_app.edit_font, TRUE);
        SendMessageA(g_reward.events_edit, EM_SETLIMITTEXT,
            REWARD_EVENTS_TEXT_SIZE - 1, 0);
        create_static(hwnd,
            "O reward do step k e aplicado depois da elegibilidade do proprio step.",
            24, 510, 706, 24, 0);
        create_button(hwnd, "APLICAR", IDC_REWARD_APPLY, 230, 554, 130, 36);
        create_button(hwnd, "CANCELAR", IDC_REWARD_CANCEL, 390, 554, 130, 36);
        reward_to_controls();
        enable_dark_title_bar(hwnd);
        return 0;
    case WM_COMMAND:
        if (HIWORD(wparam) == BN_CLICKED)
        {
            if (LOWORD(wparam) == IDC_REWARD_APPLY)
            {
                apply_reward_options();
                return 0;
            }
            if (LOWORD(wparam) == IDC_REWARD_CANCEL)
            {
                DestroyWindow(hwnd);
                return 0;
            }
        }
        break;
    case WM_CTLCOLORSTATIC:
        return handle_color((HDC)wparam, (HWND)lparam);
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        return handle_edit_color((HDC)wparam);
    case WM_DRAWITEM:
        draw_button((const DRAWITEMSTRUCT *)lparam);
        return TRUE;
    case WM_ERASEBKGND:
    {
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect((HDC)wparam, &rect, g_app.background_brush);
        return 1;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_reward.window = NULL;
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static int ensure_reward_class_registered(void)
{
    static int registered = 0;
    WNDCLASSA window_class;
    if (registered)
        return 1;
    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = reward_options_proc;
    window_class.hInstance = GetModuleHandleA(NULL);
    window_class.lpszClassName = "MiniSNNRewardOptionsWindow";
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = g_app.background_brush;
    if (!RegisterClassA(&window_class))
        return 0;
    registered = 1;
    return 1;
}

static void open_reward_options(void)
{
    ScenarioConfig config;
    char error[256];
    HWND dialog;
    MSG message;

    if (!controls_to_config(&config, error, sizeof(error)))
    {
        show_error("Configuracao invalida", error);
        return;
    }
    if (!ensure_reward_class_registered())
    {
        show_error("Erro interno", "Nao foi possivel criar a janela de recompensa.");
        return;
    }
    memset(&g_reward, 0, sizeof(g_reward));
    g_reward.working_config = config;
    dialog = CreateWindowExA(WS_EX_DLGMODALFRAME,
        "MiniSNNRewardOptionsWindow", "RECOMPENSA",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 780, 640, g_app.window,
        NULL, GetModuleHandleA(NULL), NULL);
    if (dialog == NULL)
    {
        show_error("Erro interno", "Nao foi possivel abrir a janela de recompensa.");
        return;
    }
    EnableWindow(g_app.window, FALSE);
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);
    while (g_reward.window != NULL && GetMessageA(&message, NULL, 0, 0) > 0)
    {
        if (!IsDialogMessageA(dialog, &message))
        {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }
    EnableWindow(g_app.window, TRUE);
    SetActiveWindow(g_app.window);
}

static void add_field(
    HWND parent,
    int id,
    const char *label,
    int x,
    int y,
    int label_w,
    int control_w)
{
    StudioField *field;

    if (g_app.field_count >= STUDIO_FIELD_COUNT)
    {
        show_error(
            "Erro interno",
            "Limite de campos da interface excedido.");
        return;
    }

    field = &g_app.fields[g_app.field_count++];

    field->id = id;
    field->label = label;
    field->label_hwnd = create_static(
        parent,
        label,
        x,
        y + STUDIO_LABEL_OFFSET_Y,
        label_w,
        STUDIO_LABEL_HEIGHT,
        0);
    field->control_hwnd = create_edit(
        parent,
        id,
        x + label_w,
        y,
        control_w,
        STUDIO_FIELD_HEIGHT);
}

static void create_controls(HWND hwnd)
{
    int label_w = STUDIO_LABEL_WIDTH;
    int control_w = STUDIO_FIELD_WIDTH;
    int topology_w = STUDIO_TOPOLOGY_FIELD_WIDTH;
    int x1 = STUDIO_LEFT_X;
    int x2 = STUDIO_MIDDLE_X;
    int y = STUDIO_TOP_Y;
    int right_x = right_content_left(STUDIO_INITIAL_CLIENT_WIDTH);
    HWND title;
    HWND subtitle;
    HWND section;

    title = create_static(hwnd, "miniSNN STUDIO", 24, 18, 430, 34, 0);
    SendMessageA(title, WM_SETFONT, (WPARAM)g_app.title_font, TRUE);
    subtitle = create_static(hwnd, "LABORATORIO DE CENARIOS CONFIGURAVEIS", 24, 56, 610, 26, 0);
    SendMessageA(subtitle, WM_SETFONT, (WPARAM)g_app.summary_font, TRUE);
    create_static(hwnd, "Edite, execute e compare redes sem alterar o nucleo da miniSNN.", 24, 84, 760, 24, 0);

    section = create_static(hwnd, "[ CENARIO E TOPOLOGIA ]", x1, 100, 330, 24, 0);
    SendMessageA(section, WM_SETFONT, (WPARAM)g_app.summary_font, TRUE);
    add_field(hwnd, IDC_RUN_NAME, "Nome da execucao", x1, y, label_w, control_w);

    if (g_app.field_count >= STUDIO_FIELD_COUNT)
    {
        show_error(
            "Erro interno",
            "Limite de campos da interface excedido.");
        return;
    }

    g_app.fields[g_app.field_count].id = IDC_TOPOLOGY;
    g_app.fields[g_app.field_count].label = "Topologia";
    g_app.fields[g_app.field_count].label_hwnd = create_static(
        hwnd,
        "Topologia",
        x1,
        y + STUDIO_ROW_HEIGHT + STUDIO_LABEL_OFFSET_Y,
        label_w,
        STUDIO_LABEL_HEIGHT,
        0);
    g_app.topology_combo = CreateWindowExA(
        0,
        "COMBOBOX",
        "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        x1 + label_w,
        y + STUDIO_ROW_HEIGHT,
        topology_w,
        160,
        hwnd,
        (HMENU)(INT_PTR)IDC_TOPOLOGY,
        GetModuleHandleA(NULL),
        NULL);
    SendMessageA(g_app.topology_combo, WM_SETFONT, (WPARAM)g_app.edit_font, TRUE);
    SendMessageA(g_app.topology_combo, CB_ADDSTRING, 0, (LPARAM)"chain");
    SendMessageA(g_app.topology_combo, CB_ADDSTRING, 0, (LPARAM)"ring");
    SendMessageA(g_app.topology_combo, CB_ADDSTRING, 0, (LPARAM)"all_to_all");
    SendMessageA(g_app.topology_combo, CB_ADDSTRING, 0, (LPARAM)"random");
    SendMessageA(g_app.topology_combo, CB_ADDSTRING, 0, (LPARAM)"random_balanced");
    SendMessageA(g_app.topology_combo, CB_ADDSTRING, 0, (LPARAM)"small_world");
    SendMessageA(g_app.topology_combo, CB_ADDSTRING, 0, (LPARAM)"feedforward");
    SendMessageA(g_app.topology_combo, CB_SETDROPPEDWIDTH, (WPARAM)topology_w, 0);
    g_app.topology_options_button = create_button(
        hwnd,
        "OPCOES",
        IDC_BTN_OPTIONS,
        x1 + label_w + topology_w + STUDIO_BUTTON_GAP,
        y + STUDIO_ROW_HEIGHT - 1,
        STUDIO_TOPOLOGY_BUTTON_WIDTH,
        STUDIO_FIELD_HEIGHT + 2);
    g_app.fields[g_app.field_count].control_hwnd = g_app.topology_combo;
    g_app.field_count++;

    add_field(hwnd, IDC_NEURONS, "Neuronios", x1, y + 2 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_INHIBITORY_PERCENT, "Proporcao inibitoria (%)", x1, y + 3 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_CONNECTION_PROBABILITY, "Densidade de conexao", x1, y + 4 * STUDIO_ROW_HEIGHT, label_w, control_w);
    create_static(
        hwnd,
        "Usada em random, random_balanced e feedforward.",
        x1,
        y + 5 * STUDIO_ROW_HEIGHT - 9,
        STUDIO_LEFT_PANEL_RIGHT - STUDIO_LEFT_PANEL_LEFT - 20,
        22,
        0);
    add_field(hwnd, IDC_SEED, "Seed", x1, y + 6 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_DELAY, "Delay", x1, y + 7 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_MAX_DELAY, "Delay maximo", x1, y + 8 * STUDIO_ROW_HEIGHT, label_w, control_w);

    section = create_static(hwnd, "[ PESOS, ENTRADA E SIMULACAO ]", x2, 100, 350, 24, 0);
    SendMessageA(section, WM_SETFONT, (WPARAM)g_app.summary_font, TRUE);
    add_field(hwnd, IDC_EXC_WEIGHT, "Peso excitatorio", x2, y, label_w, control_w);
    add_field(hwnd, IDC_INH_WEIGHT, "Peso inibitorio", x2, y + STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_SOURCE_COUNT, "Neuronios com entrada", x2, y + 2 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_INPUT_CURRENT, "Corrente externa", x2, y + 3 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_RECORD_NEURON, "Neuronio detalhado", x2, y + 4 * STUDIO_ROW_HEIGHT, label_w, control_w);

    section = create_static(hwnd, "[ PARAMETROS LIF ]", x2, y + 6 * STUDIO_ROW_HEIGHT, 280, 24, 0);
    SendMessageA(section, WM_SETFONT, (WPARAM)g_app.summary_font, TRUE);
    add_field(hwnd, IDC_STEPS, "Passos", x2, y + 7 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_DT, "dt", x2, y + 8 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_TAU, "tau", x2, y + 9 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_V_REST, "V_rest", x2, y + 10 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_V_RESET, "V_reset", x2, y + 11 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_V_THRESHOLD, "V_threshold", x2, y + 12 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_RESISTANCE, "Resistencia", x2, y + 13 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_SYNAPTIC_DECAY, "Decaimento sinaptico", x2, y + 14 * STUDIO_ROW_HEIGHT, label_w, control_w);
    g_app.plasticity_button = create_button(
        hwnd,
        "PLASTICIDADE",
        IDC_BTN_PLASTICITY,
        x2,
        y + 15 * STUDIO_ROW_HEIGHT + 2,
        280,
        STUDIO_BUTTON_HEIGHT);
    g_app.homeostasis_button = create_button(
        hwnd,
        "HOMEOSTASE",
        IDC_BTN_HOMEOSTASIS,
        x2,
        y + 16 * STUDIO_ROW_HEIGHT + 2,
        280,
        STUDIO_BUTTON_HEIGHT);
    g_app.reward_button = create_button(
        hwnd,
        "RECOMPENSA",
        IDC_BTN_REWARD,
        x2,
        y + 17 * STUDIO_ROW_HEIGHT + 2,
        280,
        STUDIO_BUTTON_HEIGHT);

    g_app.execution_section_label = create_static(
        hwnd,
        "[ EXECUCAO E RESULTADOS ]",
        right_x,
        92,
        174,
        24,
        0);
    SendMessageA(g_app.execution_section_label, WM_SETFONT, (WPARAM)g_app.summary_font, TRUE);

    g_app.diagnostics_label = create_static(
        hwnd,
        "DIAG:",
        right_x + 178,
        94,
        54,
        24,
        0);
    g_app.diagnostics_combo = CreateWindowExA(
        0,
        "COMBOBOX",
        "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        right_x + 230,
        90,
        110,
        120,
        hwnd,
        (HMENU)(INT_PTR)IDC_DIAGNOSTICS_LEVEL,
        GetModuleHandleA(NULL),
        NULL);
    SendMessageA(g_app.diagnostics_combo, WM_SETFONT, (WPARAM)g_app.edit_font, TRUE);
    SendMessageA(g_app.diagnostics_combo, CB_ADDSTRING, 0, (LPARAM)"off");
    SendMessageA(g_app.diagnostics_combo, CB_ADDSTRING, 0, (LPARAM)"basic");
    SendMessageA(g_app.diagnostics_combo, CB_ADDSTRING, 0, (LPARAM)"full");

    g_app.buttons[0] = create_button(hwnd, "NOVO PADRAO", IDC_BTN_NEW, right_x, 116, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[1] = create_button(hwnd, "CARREGAR CENARIO", IDC_BTN_LOAD, right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[2] = create_button(hwnd, "SALVAR CENARIO", IDC_BTN_SAVE, right_x, 116 + STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[3] = create_button(hwnd, "RODAR SIMULACAO", IDC_BTN_RUN, right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[4] = create_button(hwnd, "GERAR GRAFICOS", IDC_BTN_PLOT, right_x, 116 + 2 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[5] = create_button(hwnd, "ABRIR ULTIMA", IDC_BTN_OPEN, right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 2 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[6] = create_button(hwnd, "CSV NEURONIO", IDC_BTN_OPEN_NEURON_CSV, right_x, 116 + 3 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[7] = create_button(hwnd, "GRAFICO NEURONIO", IDC_BTN_PLOT_NEURON, right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 3 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[8] = create_button(hwnd, "ABRIR GRAFICO", IDC_BTN_OPEN_NEURON_PNG, right_x, 116 + 4 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[9] = create_button(hwnd, "ABRIR RESULTADOS", IDC_BTN_OPEN_RESULTS_ROOT, right_x, 116 + 5 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[10] = create_button(hwnd, "ABRIR HISTORICO", IDC_BTN_OPEN_HISTORY, right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 5 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[11] = create_button(hwnd, "COMPARAR EXECUCOES", IDC_BTN_COMPARE_RUNS, right_x, 116 + 6 * STUDIO_BUTTON_ROW_HEIGHT, 340, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[12] = create_button(hwnd, "ABRIR COMPARACAO", IDC_BTN_OPEN_COMPARISON, right_x, 116 + 7 * STUDIO_BUTTON_ROW_HEIGHT, 340, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[13] = create_button(hwnd, "GERAR DIAGNOSTICO", IDC_BTN_GENERATE_DIAGNOSTICS, right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 4 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[14] = create_button(hwnd, "ABRIR METRICAS", IDC_BTN_OPEN_METRICS, right_x, 116 + 8 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[15] = create_button(hwnd, "ABRIR DIAGNOSTICO", IDC_BTN_OPEN_DIAGNOSTICS, right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 8 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[16] = create_button(hwnd, "GRAFICO STDP", IDC_BTN_PLOT_PLASTICITY, right_x, 116 + 9 * STUDIO_BUTTON_ROW_HEIGHT, 340, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[17] = create_button(hwnd, "ABRIR PESOS", IDC_BTN_OPEN_WEIGHTS, right_x, 116 + 10 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[18] = create_button(hwnd, "ABRIR STDP", IDC_BTN_OPEN_STDP, right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 10 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[19] = create_button(hwnd, "GRAFICO HOMEOSTASE", IDC_BTN_PLOT_HOMEOSTASIS, right_x, 116 + 11 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[20] = create_button(hwnd, "ABRIR HOMEOSTASE", IDC_BTN_OPEN_HOMEOSTASIS, right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 11 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[21] = create_button(hwnd, "GRAFICO RECOMPENSA", IDC_BTN_PLOT_REWARD, right_x, 116 + 12 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[22] = create_button(hwnd, "ABRIR RECOMPENSA", IDC_BTN_OPEN_REWARD, right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 12 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT);
    g_app.status_label = create_static(hwnd, "PRONTO PARA EXECUTAR", right_x, 714, 340, 42, IDC_STATUS);
    g_app.summary_box = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        right_x,
        766,
        340,
        145,
        hwnd,
        (HMENU)(INT_PTR)IDC_SUMMARY,
        GetModuleHandleA(NULL),
        NULL);
    SendMessageA(g_app.summary_box, WM_SETFONT, (WPARAM)g_app.summary_font, TRUE);

    set_result_buttons_enabled(FALSE);
    set_comparison_buttons_enabled();
}

static void layout_controls(HWND hwnd)
{
    RECT rect;
    int width;
    int height;
    int right_x;
    int summary_height;
    int content_width;

    GetClientRect(hwnd, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    right_x = right_content_left(width);
    content_width = STUDIO_RIGHT_PANEL_WIDTH - 2 * STUDIO_RIGHT_CONTENT_PADDING;
    summary_height = height - 786;

    if (summary_height < 140)
        summary_height = 140;

    MoveWindow(g_app.execution_section_label, right_x, 92, 174, 24, TRUE);
    MoveWindow(g_app.diagnostics_label, right_x + 178, 94, 54, 24, TRUE);
    MoveWindow(g_app.diagnostics_combo, right_x + 230, 90, 110, 120, TRUE);
    MoveWindow(g_app.status_label, right_x, 714, content_width, 42, TRUE);
    MoveWindow(g_app.summary_box, right_x, 766, content_width, summary_height, TRUE);

    MoveWindow(g_app.buttons[0], right_x, 116, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[1], right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[2], right_x, 116 + STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[3], right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[4], right_x, 116 + 2 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[5], right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 2 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[6], right_x, 116 + 3 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[7], right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 3 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[8], right_x, 116 + 4 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[9], right_x, 116 + 5 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[10], right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 5 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[11], right_x, 116 + 6 * STUDIO_BUTTON_ROW_HEIGHT, content_width, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[12], right_x, 116 + 7 * STUDIO_BUTTON_ROW_HEIGHT, content_width, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[13], right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 4 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[14], right_x, 116 + 8 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[15], right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 8 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[16], right_x, 116 + 9 * STUDIO_BUTTON_ROW_HEIGHT, content_width, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[17], right_x, 116 + 10 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[18], right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 10 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[19], right_x, 116 + 11 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[20], right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 11 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[21], right_x, 116 + 12 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[22], right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 12 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT, TRUE);

    InvalidateRect(hwnd, NULL, TRUE);
}

static void draw_button(const DRAWITEMSTRUCT *item)
{
    HBRUSH brush;
    HPEN pen;
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    HGDIOBJ old_font;
    RECT rect = item->rcItem;
    RECT text_rect;
    char text[TEXT_BUFFER_SIZE];
    UINT state = item->itemState;
    COLORREF fill = RGB(0, 0, 0);
    COLORREF text_color = g_app.text_color;

    if ((state & ODS_DISABLED) != 0)
    {
        fill = RGB(8, 8, 8);
        text_color = g_app.disabled_text_color;
    }
    else if ((state & ODS_SELECTED) != 0)
    {
        fill = RGB(32, 32, 32);
    }

    brush = CreateSolidBrush(fill);
    pen = g_app.button_pen;

    if (item->CtlID == IDC_BTN_RUN)
        pen = g_app.button_primary_pen;

    if ((state & ODS_FOCUS) != 0)
        pen = g_app.button_focus_pen;

    FillRect(item->hDC, &rect, brush);
    old_pen = SelectObject(item->hDC, pen);
    old_brush = SelectObject(item->hDC, GetStockObject(NULL_BRUSH));
    Rectangle(item->hDC, rect.left, rect.top, rect.right, rect.bottom);

    if (item->CtlID == IDC_BTN_RUN)
    {
        RECT inner = rect;
        InflateRect(&inner, -3, -3);
        Rectangle(item->hDC, inner.left, inner.top, inner.right, inner.bottom);
    }

    GetWindowTextA(item->hwndItem, text, sizeof(text));
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, text_color);
    old_font = SelectObject(item->hDC, g_app.normal_font);
    text_rect = rect;
    InflateRect(&text_rect, -4, 0);
    DrawTextA(
        item->hDC,
        text,
        -1,
        &text_rect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(item->hDC, old_font);
    SelectObject(item->hDC, old_pen);
    SelectObject(item->hDC, old_brush);
    DeleteObject(brush);
}

static LRESULT handle_color(HDC hdc, HWND hwnd)
{
    SetBkColor(hdc, g_app.background_color);
    SetTextColor(
        hdc,
        hwnd != NULL && !IsWindowEnabled(hwnd) ?
        g_app.disabled_text_color :
        g_app.text_color);
    return (LRESULT)g_app.background_brush;
}

static LRESULT handle_edit_color(HDC hdc)
{
    SetBkColor(hdc, g_app.edit_color);
    SetTextColor(hdc, g_app.text_color);
    return (LRESULT)g_app.edit_brush;
}

static int setup_project_root(void)
{
    char path[MAX_PATH];
    char *slash;

    if (GetModuleFileNameA(NULL, path, sizeof(path)) == 0)
        return 0;

    slash = strrchr(path, '\\');
    if (slash == NULL)
        return 0;

    *slash = '\0';

    slash = strrchr(path, '\\');
    if (slash != NULL && strcmp(slash + 1, "build") == 0)
        *slash = '\0';

    if (!copy_path(g_app.project_root, sizeof(g_app.project_root), path))
        return 0;

    return SetCurrentDirectoryA(g_app.project_root) != 0;
}

static LRESULT CALLBACK window_proc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam)
{
    switch (message)
    {
    case WM_CREATE:
        g_app.window = hwnd;
        g_app.background_color = RGB(0, 0, 0);
        g_app.edit_color = RGB(4, 4, 4);
        g_app.text_color = RGB(255, 255, 255);
        g_app.secondary_text_color = RGB(190, 190, 190);
        g_app.disabled_text_color = RGB(96, 96, 96);
        g_app.accent_color = RGB(220, 220, 220);
        g_app.background_brush = CreateSolidBrush(g_app.background_color);
        g_app.edit_brush = CreateSolidBrush(g_app.edit_color);
        g_app.button_pen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
        g_app.button_focus_pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        g_app.button_primary_pen = CreatePen(PS_SOLID, 2, RGB(240, 240, 240));
        g_app.panel_pen = CreatePen(PS_SOLID, 1, RGB(90, 90, 90));
        g_app.title_font = create_studio_font(28, FW_BOLD);
        g_app.normal_font = create_studio_font(16, FW_NORMAL);
        g_app.edit_font = create_studio_font(16, FW_NORMAL);
        g_app.summary_font = create_studio_font(16, FW_NORMAL);
        create_controls(hwnd);
        layout_controls(hwnd);
        SetTimer(hwnd, STUDIO_INIT_TIMER_ID, 100, NULL);
        return 0;

    case WM_SIZE:
        layout_controls(hwnd);
        return 0;

    case WM_TIMER:
        if (wparam == STUDIO_INIT_TIMER_ID)
        {
            KillTimer(hwnd, STUDIO_INIT_TIMER_ID);
            reset_to_default();
            return 0;
        }
        break;

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *minmax = (MINMAXINFO *)lparam;
        int min_width;
        int min_height;

        client_size_to_window_size(
            STUDIO_MIN_CLIENT_WIDTH,
            STUDIO_MIN_CLIENT_HEIGHT,
            &min_width,
            &min_height);

        minmax->ptMinTrackSize.x = min_width;
        minmax->ptMinTrackSize.y = min_height;
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wparam) == IDC_TOPOLOGY &&
            HIWORD(wparam) == CBN_SELCHANGE)
        {
            update_density_enabled();
            return 0;
        }

        if (HIWORD(wparam) == BN_CLICKED)
        {
            switch (LOWORD(wparam))
            {
            case IDC_BTN_NEW:
                reset_to_default();
                return 0;
            case IDC_BTN_LOAD:
                load_scenario_file();
                return 0;
            case IDC_BTN_SAVE:
                save_scenario_file();
                return 0;
            case IDC_BTN_RUN:
                run_scenario();
                return 0;
            case IDC_BTN_PLOT:
                generate_graphs();
                return 0;
            case IDC_BTN_OPEN:
                open_last_execution();
                return 0;
            case IDC_BTN_OPEN_RESULTS_ROOT:
                open_results_root();
                return 0;
            case IDC_BTN_OPEN_HISTORY:
                open_scenario_history();
                return 0;
            case IDC_BTN_OPEN_NEURON_CSV:
                open_neuron_csv();
                return 0;
            case IDC_BTN_PLOT_NEURON:
                generate_neuron_graph();
                return 0;
            case IDC_BTN_OPEN_NEURON_PNG:
                open_neuron_plot();
                return 0;
            case IDC_BTN_COMPARE_RUNS:
                compare_runs_from_studio();
                return 0;
            case IDC_BTN_OPEN_COMPARISON:
                open_comparison_results();
                return 0;
            case IDC_BTN_GENERATE_DIAGNOSTICS:
                generate_diagnostics();
                return 0;
            case IDC_BTN_OPEN_METRICS:
                open_metrics();
                return 0;
            case IDC_BTN_OPEN_DIAGNOSTICS:
                open_diagnostics();
                return 0;
            case IDC_BTN_OPTIONS:
                open_topology_options();
                return 0;
            case IDC_BTN_PLASTICITY:
                open_plasticity_options();
                return 0;
            case IDC_BTN_HOMEOSTASIS:
                open_homeostasis_options();
                return 0;
            case IDC_BTN_REWARD:
                open_reward_options();
                return 0;
            case IDC_BTN_PLOT_PLASTICITY:
                generate_plasticity_graph();
                return 0;
            case IDC_BTN_OPEN_WEIGHTS:
                open_weights();
                return 0;
            case IDC_BTN_OPEN_STDP:
                open_stdp_plot();
                return 0;
            case IDC_BTN_PLOT_HOMEOSTASIS:
                generate_homeostasis_graph();
                return 0;
            case IDC_BTN_OPEN_HOMEOSTASIS:
                open_homeostasis_report();
                return 0;
            case IDC_BTN_PLOT_REWARD:
                generate_reward_graph();
                return 0;
            case IDC_BTN_OPEN_REWARD:
                open_reward_report();
                return 0;
            default:
                break;
            }
        }
        break;

    case WM_CTLCOLORSTATIC:
        return handle_color((HDC)wparam, (HWND)lparam);

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        return handle_edit_color((HDC)wparam);

    case WM_DRAWITEM:
        draw_button((const DRAWITEMSTRUCT *)lparam);
        return TRUE;

    case WM_ERASEBKGND:
    {
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect((HDC)wparam, &rect, g_app.background_brush);
        return 1;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC hdc = BeginPaint(hwnd, &paint);
        RECT rect;
        HGDIOBJ old_pen;
        HGDIOBJ old_brush;
        int width;
        int height;

        GetClientRect(hwnd, &rect);
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;

        FillRect(hdc, &rect, g_app.background_brush);
        old_pen = SelectObject(hdc, g_app.panel_pen);
        old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(
            hdc,
            STUDIO_LEFT_PANEL_LEFT,
            STUDIO_PANEL_TOP,
            STUDIO_LEFT_PANEL_RIGHT,
            height - STUDIO_PANEL_BOTTOM_MARGIN);
        Rectangle(
            hdc,
            STUDIO_MIDDLE_PANEL_LEFT,
            STUDIO_PANEL_TOP,
            STUDIO_MIDDLE_PANEL_RIGHT,
            height - STUDIO_PANEL_BOTTOM_MARGIN);
        Rectangle(
            hdc,
            right_panel_left(width),
            STUDIO_PANEL_TOP - 8,
            width - STUDIO_RIGHT_PANEL_MARGIN,
            height - STUDIO_PANEL_BOTTOM_MARGIN);
        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
        EndPaint(hwnd, &paint);
        return 0;
    }

    case WM_DESTROY:
        if (g_app.title_font != NULL)
            destroy_studio_font(g_app.title_font);
        if (g_app.normal_font != NULL)
            destroy_studio_font(g_app.normal_font);
        if (g_app.edit_font != NULL)
            destroy_studio_font(g_app.edit_font);
        if (g_app.summary_font != NULL)
            destroy_studio_font(g_app.summary_font);
        if (g_app.background_brush != NULL)
            DeleteObject(g_app.background_brush);
        if (g_app.edit_brush != NULL)
            DeleteObject(g_app.edit_brush);
        if (g_app.button_pen != NULL)
            DeleteObject(g_app.button_pen);
        if (g_app.button_focus_pen != NULL)
            DeleteObject(g_app.button_focus_pen);
        if (g_app.button_primary_pen != NULL)
            DeleteObject(g_app.button_primary_pen);
        if (g_app.panel_pen != NULL)
            DeleteObject(g_app.panel_pen);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE previous_instance,
    LPSTR command_line,
    int show_command)
{
    WNDCLASSA window_class;
    HWND hwnd;
    MSG message;
    int window_width;
    int window_height;

    (void)previous_instance;
    (void)command_line;

    memset(&g_app, 0, sizeof(g_app));
    memset(&window_class, 0, sizeof(window_class));

    if (!setup_project_root())
    {
        MessageBoxA(
            NULL,
            "Erro interno: nao foi possivel acessar a raiz do projeto.",
            APP_TITLE,
            MB_ICONERROR | MB_OK);
        return 1;
    }

    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = "MiniSNNStudioWindow";
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&window_class))
        return 1;

    client_size_to_window_size(
        STUDIO_INITIAL_CLIENT_WIDTH,
        STUDIO_INITIAL_CLIENT_HEIGHT,
        &window_width,
        &window_height);

    hwnd = CreateWindowExA(
        0,
        window_class.lpszClassName,
        APP_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window_width,
        window_height,
        NULL,
        NULL,
        instance,
        NULL);

    if (hwnd == NULL)
        return 1;

    enable_dark_title_bar(hwnd);

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    while (GetMessageA(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    return (int)message.wParam;
}
