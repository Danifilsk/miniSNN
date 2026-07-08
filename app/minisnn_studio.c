#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

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
#define PYTHON_COMMAND_BUFFER_SIZE 1200
#define PYTHON_MESSAGE_BUFFER_SIZE 2200
#define STUDIO_FIELD_COUNT 21

#define STUDIO_MIN_CLIENT_WIDTH 1180
#define STUDIO_MIN_CLIENT_HEIGHT 760
#define STUDIO_INITIAL_CLIENT_WIDTH STUDIO_MIN_CLIENT_WIDTH
#define STUDIO_INITIAL_CLIENT_HEIGHT STUDIO_MIN_CLIENT_HEIGHT

#define STUDIO_LEFT_X 28
#define STUDIO_MIDDLE_X 445
#define STUDIO_TOP_Y 120
#define STUDIO_LABEL_WIDTH 220
#define STUDIO_FIELD_WIDTH 130
#define STUDIO_TOPOLOGY_FIELD_WIDTH 190
#define STUDIO_ROW_HEIGHT 36
#define STUDIO_LABEL_HEIGHT 24
#define STUDIO_LABEL_OFFSET_Y 4
#define STUDIO_FIELD_HEIGHT 28

#define STUDIO_PANEL_TOP 94
#define STUDIO_PANEL_BOTTOM_MARGIN 24
#define STUDIO_LEFT_PANEL_LEFT 18
#define STUDIO_LEFT_PANEL_RIGHT 448
#define STUDIO_MIDDLE_PANEL_LEFT 452
#define STUDIO_MIDDLE_PANEL_RIGHT 800
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

#define IDC_BTN_NEW 2001
#define IDC_BTN_LOAD 2002
#define IDC_BTN_SAVE 2003
#define IDC_BTN_RUN 2004
#define IDC_BTN_PLOT 2005
#define IDC_BTN_OPEN 2006

#define IDC_STATUS 3001
#define IDC_SUMMARY 3002

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
    HWND status_label;
    HWND summary_box;
    HWND execution_section_label;
    HWND buttons[6];

    ScenarioConfig current_config;
    ScenarioRunResult last_result;
    int has_result;

    char project_root[MAX_PATH];
    char pixel_font_face[LF_FACESIZE];
    int resolved_python_uses_py_launcher;
} StudioState;

static StudioState g_app;

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
        strcmp(topology, "random_balanced") == 0);
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

    update_density_enabled();
}

static int controls_to_config(
    ScenarioConfig *config,
    char *error_message,
    size_t error_message_size)
{
    double inhibitory_percent;

    scenario_config_default(config);

    GetWindowTextA(
        field_control(IDC_RUN_NAME),
        config->run_name,
        sizeof(config->run_name));

    GetWindowTextA(
        g_app.topology_combo,
        config->topology,
        sizeof(config->topology));

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
        !parse_int_field(IDC_RECORD_NEURON, "Neuronio gravado", &config->record_neuron, error_message, error_message_size) ||
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
    char text[STATUS_BUFFER_SIZE];

    snprintf(
        text,
        sizeof(text),
        "STATUS: SIMULACAO CONCLUIDA\r\n"
        "\r\n"
        "Nome da execucao: %s\r\n"
        "Topologia: %s\r\n"
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
        config->topology,
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

static void reset_to_default(void)
{
    scenario_config_default(&g_app.current_config);
    config_to_controls(&g_app.current_config);
    SetWindowTextA(summary_control(), "STATUS: PRONTO PARA EXECUTAR\r\n");
    g_app.has_result = 0;
    EnableWindow(g_app.buttons[4], FALSE);
    EnableWindow(g_app.buttons[5], FALSE);
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
    EnableWindow(g_app.buttons[4], FALSE);
    EnableWindow(g_app.buttons[5], FALSE);

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
    EnableWindow(g_app.buttons[4], TRUE);
    EnableWindow(g_app.buttons[5], TRUE);
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

static void open_results(void)
{
    char output_path[MAX_PATH];
    HINSTANCE result;

    if (!g_app.has_result)
    {
        show_error("Sem resultados", "Rode uma simulacao antes de abrir a pasta.");
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

    set_status("PASTA DE RESULTADOS ABERTA");
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
    SendMessageA(g_app.topology_combo, CB_ADDSTRING, 0, (LPARAM)"random_balanced");
    SendMessageA(g_app.topology_combo, CB_SETDROPPEDWIDTH, (WPARAM)topology_w, 0);
    g_app.fields[g_app.field_count].control_hwnd = g_app.topology_combo;
    g_app.field_count++;

    add_field(hwnd, IDC_NEURONS, "Neuronios", x1, y + 2 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_INHIBITORY_PERCENT, "Proporcao inibitoria (%)", x1, y + 3 * STUDIO_ROW_HEIGHT, label_w, control_w);
    add_field(hwnd, IDC_CONNECTION_PROBABILITY, "Densidade de conexao", x1, y + 4 * STUDIO_ROW_HEIGHT, label_w, control_w);
    create_static(
        hwnd,
        "Usada apenas em random_balanced.",
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
    add_field(hwnd, IDC_RECORD_NEURON, "Neuronio gravado", x2, y + 4 * STUDIO_ROW_HEIGHT, label_w, control_w);

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

    g_app.execution_section_label = create_static(
        hwnd,
        "[ EXECUCAO E RESULTADOS ]",
        right_x,
        92,
        340,
        24,
        0);
    SendMessageA(g_app.execution_section_label, WM_SETFONT, (WPARAM)g_app.summary_font, TRUE);

    g_app.buttons[0] = create_button(hwnd, "NOVO PADRAO", IDC_BTN_NEW, right_x, 116, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[1] = create_button(hwnd, "CARREGAR CENARIO", IDC_BTN_LOAD, right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[2] = create_button(hwnd, "SALVAR CENARIO", IDC_BTN_SAVE, right_x, 116 + STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[3] = create_button(hwnd, "RODAR SIMULACAO", IDC_BTN_RUN, right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[4] = create_button(hwnd, "GERAR GRAFICOS", IDC_BTN_PLOT, right_x, 116 + 2 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT);
    g_app.buttons[5] = create_button(hwnd, "ABRIR RESULTADOS", IDC_BTN_OPEN, right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 2 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT);
    g_app.status_label = create_static(hwnd, "PRONTO PARA EXECUTAR", right_x, 276, 340, 58, IDC_STATUS);
    g_app.summary_box = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        right_x,
        340,
        340,
        345,
        hwnd,
        (HMENU)(INT_PTR)IDC_SUMMARY,
        GetModuleHandleA(NULL),
        NULL);
    SendMessageA(g_app.summary_box, WM_SETFONT, (WPARAM)g_app.summary_font, TRUE);

    EnableWindow(g_app.buttons[4], FALSE);
    EnableWindow(g_app.buttons[5], FALSE);
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
    summary_height = height - 360;

    if (summary_height < 180)
        summary_height = 180;

    MoveWindow(g_app.execution_section_label, right_x, 92, content_width, 24, TRUE);
    MoveWindow(g_app.status_label, right_x, 276, content_width, 58, TRUE);
    MoveWindow(g_app.summary_box, right_x, 340, content_width, summary_height, TRUE);

    MoveWindow(g_app.buttons[0], right_x, 116, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[1], right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[2], right_x, 116 + STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[3], right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[4], right_x, 116 + 2 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_LEFT, STUDIO_BUTTON_HEIGHT, TRUE);
    MoveWindow(g_app.buttons[5], right_x + STUDIO_BUTTON_WIDTH_LEFT + STUDIO_BUTTON_GAP, 116 + 2 * STUDIO_BUTTON_ROW_HEIGHT, STUDIO_BUTTON_WIDTH_RIGHT, STUDIO_BUTTON_HEIGHT, TRUE);

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
    (void)hwnd;
    SetBkColor(hdc, g_app.background_color);
    SetTextColor(hdc, g_app.text_color);
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
                open_results();
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
