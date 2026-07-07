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
#define IDC_BTN_SELECT_PYTHON 2007

#define IDC_STATUS 3001
#define IDC_SUMMARY 3002

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
    HFONT font;
    HBRUSH background_brush;
    HBRUSH panel_brush;
    HBRUSH edit_brush;
    COLORREF background_color;
    COLORREF panel_color;
    COLORREF edit_color;
    COLORREF text_color;
    COLORREF accent_color;

    StudioField fields[STUDIO_FIELD_COUNT];
    int field_count;

    HWND topology_combo;
    HWND status_label;
    HWND summary_box;
    HWND buttons[7];

    ScenarioConfig current_config;
    ScenarioRunResult last_result;
    int has_result;

    char project_root[MAX_PATH];
    char selected_python[MAX_PATH];
    int has_selected_python;
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
            return g_app.fields[i].control_hwnd;
    }

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

static int filename_is_python_exe(const char *path)
{
    const char *name = strrchr(path, '\\');
    const char expected[] = "python.exe";
    size_t i;

    if (name == NULL)
        name = strrchr(path, '/');

    if (name == NULL)
        name = path;
    else
        name++;

    for (i = 0; expected[i] != '\0' && name[i] != '\0'; i++)
    {
        if (tolower((unsigned char)name[i]) != expected[i])
            return 0;
    }

    return expected[i] == '\0' && name[i] == '\0';
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
            "O Python selecionado por py -3 precisa de pandas e matplotlib.",
            py_path);
        return 0;
    }

    return 1;
}

static int install_plot_dependencies(
    const char *python_path,
    char *error_message,
    size_t error_message_size)
{
    char command[PYTHON_COMMAND_BUFFER_SIZE];
    char confirmation[PYTHON_MESSAGE_BUFFER_SIZE];
    DWORD exit_code = 1;
    int answer;

    if (python_path == NULL || !file_exists(python_path))
    {
        snprintf(error_message, error_message_size, "Python invalido para instalacao.");
        return 0;
    }

    if (snprintf(
            command,
            sizeof(command),
            "\"%s\" -m pip install pandas matplotlib",
            python_path) >= (int)sizeof(command))
    {
        snprintf(error_message, error_message_size, "Comando de instalacao muito longo.");
        return 0;
    }

    snprintf(
        confirmation,
        sizeof(confirmation),
        "O Studio vai executar:\n\n%s\n\nDeseja continuar?",
        command);

    answer = MessageBoxA(
        g_app.window,
        confirmation,
        "Instalar bibliotecas",
        MB_ICONQUESTION | MB_YESNO);

    if (answer != IDYES)
    {
        snprintf(error_message, error_message_size, "Instalacao cancelada pelo usuario.");
        return 0;
    }

    if (!run_hidden_process(command, &exit_code) || exit_code != 0)
    {
        snprintf(
            error_message,
            error_message_size,
            "Falha ao instalar pandas e matplotlib neste Python:\n%s",
            python_path);
        return 0;
    }

    return python_has_plot_dependencies(
        python_path,
        error_message,
        error_message_size);
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

    if (g_app.has_selected_python &&
        file_exists(g_app.selected_python) &&
        python_has_plot_dependencies(
            g_app.selected_python,
            validation_error,
            sizeof(validation_error)) &&
        copy_path(out_path, out_path_size, g_app.selected_python))
    {
        return 1;
    }

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

static int choose_python_executable(int explain_first)
{
    OPENFILENAMEA ofn;
    char filename[MAX_PATH] = "";
    char error[512];

    if (explain_first)
    {
        show_info(
            "Python necessario",
            "Nao foi possivel detectar Python automaticamente.\n\n"
            "Escolha um python.exe com pandas e matplotlib instalados.");
    }

    for (;;)
    {
        filename[0] = '\0';

        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_app.window;
        ofn.lpstrFilter = "python.exe\0python.exe\0Executaveis (*.exe)\0*.exe\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = sizeof(filename);
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameA(&ofn))
        {
            SetCurrentDirectoryA(g_app.project_root);
            return 0;
        }

        SetCurrentDirectoryA(g_app.project_root);

        if (!filename_is_python_exe(filename) || !file_exists(filename))
        {
            show_error(
                "Python invalido",
                "Escolha um arquivo chamado python.exe.");
            continue;
        }

        if (python_has_plot_dependencies(filename, error, sizeof(error)))
        {
            if (!copy_path(
                    g_app.selected_python,
                    sizeof(g_app.selected_python),
                    filename))
            {
                show_error("Python invalido", "Caminho do Python muito longo.");
                return 0;
            }

            g_app.has_selected_python = 1;
            g_app.resolved_python_uses_py_launcher = 0;

            {
                char status[STATUS_BUFFER_SIZE];
                snprintf(
                    status,
                    sizeof(status),
                    "Python selecionado manualmente: %s",
                    g_app.selected_python);
                set_status(status);
            }

            show_info("Python valido", "Python selecionado e validado com pandas e matplotlib.");
            return 1;
        }

        {
            char message[PYTHON_MESSAGE_BUFFER_SIZE];
            int answer;

            snprintf(
                message,
                sizeof(message),
                "%s\n\n"
                "O Python selecionado nao possui pandas e matplotlib.\n"
                "Ele nao sera usado pelo Studio.\n\n"
                "Sim: escolher outro Python.\n"
                "Nao: instalar bibliotecas neste Python.\n"
                "Cancelar: voltar ao Studio.",
                error);

            answer = MessageBoxA(
                g_app.window,
                message,
                "Python sem dependencias",
                MB_ICONWARNING | MB_YESNOCANCEL);

            if (answer == IDYES)
                continue;

            if (answer == IDNO)
            {
                if (install_plot_dependencies(filename, error, sizeof(error)))
                {
                    if (!copy_path(
                            g_app.selected_python,
                            sizeof(g_app.selected_python),
                            filename))
                    {
                        show_error("Python invalido", "Caminho do Python muito longo.");
                        return 0;
                    }

                    g_app.has_selected_python = 1;
                    g_app.resolved_python_uses_py_launcher = 0;

                    {
                        char status[STATUS_BUFFER_SIZE];
                        snprintf(
                            status,
                            sizeof(status),
                            "Python selecionado manualmente: %s",
                            g_app.selected_python);
                        set_status(status);
                    }

                    show_info("Bibliotecas instaladas", "Python validado apos instalar pandas e matplotlib.");
                    return 1;
                }

                show_error("Falha na instalacao", error);
                return 0;
            }

            return 0;
        }
    }
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

static void set_edit_double(int id, double value)
{
    char text[TEXT_BUFFER_SIZE];
    snprintf(text, sizeof(text), "%.17g", value);
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
    SetWindowTextA(field_control(IDC_RUN_NAME), config->run_name);

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

    SetWindowTextA(g_app.summary_box, text);
}

static void reset_to_default(void)
{
    scenario_config_default(&g_app.current_config);
    config_to_controls(&g_app.current_config);
    SetWindowTextA(g_app.summary_box, "");
    g_app.has_result = 0;
    EnableWindow(g_app.buttons[4], FALSE);
    EnableWindow(g_app.buttons[5], FALSE);
    set_status("Status: configuracao padrao carregada.");
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
    SetWindowTextA(g_app.summary_box, "");
    g_app.has_result = 0;
    EnableWindow(g_app.buttons[4], FALSE);
    EnableWindow(g_app.buttons[5], FALSE);

    {
        char status[STATUS_BUFFER_SIZE];
        snprintf(status, sizeof(status), "Status: cenario carregado de %s.", filename);
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
    set_status("Status: cenario salvo.");
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

    set_status("Status: simulacao em execucao...");
    UpdateWindow(g_app.window);

    if (!SetCurrentDirectoryA(g_app.project_root))
    {
        show_error(
            "Erro interno",
            "Erro interno: nao foi possivel acessar a raiz do projeto.");
        set_status("Status: erro ao acessar raiz do projeto.");
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
        set_status("Status: erro ao executar simulacao.");
        return;
    }

    g_app.current_config = config;
    g_app.last_result = result;
    g_app.has_result = 1;

    update_summary(&config, &result);
    EnableWindow(g_app.buttons[4], TRUE);
    EnableWindow(g_app.buttons[5], TRUE);
    set_status("Status: simulacao concluida. Executar o mesmo nome sobrescreve a pasta de resultados.");
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
        int answer = MessageBoxA(
            g_app.window,
            "Nenhum Python com pandas e matplotlib foi encontrado.\n\n"
            "Deseja selecionar um python.exe agora?",
            "Python nao encontrado",
            MB_ICONWARNING | MB_OKCANCEL);

        if (answer != IDOK || !choose_python_executable(0))
        {
            set_status("Status: Python nao configurado.");
            return;
        }

        if (!resolve_python_executable(python_path, sizeof(python_path)))
        {
            show_error("Erro ao localizar Python", "Nao foi possivel usar o Python selecionado.");
            set_status("Status: erro ao localizar Python.");
            return;
        }
    }

    if (!project_path(script_path, sizeof(script_path), "scripts\\plot_scenario.py") ||
        !project_path(output_path, sizeof(output_path), g_app.last_result.output_directory))
    {
        show_error(
            "Erro ao gerar graficos",
            "Nao foi possivel montar caminhos absolutos para script ou resultados.");
        set_status("Status: erro ao montar caminhos dos graficos.");
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
        g_app.has_selected_python && !g_app.resolved_python_uses_py_launcher ?
            "Python selecionado manualmente: %s" :
            "Python valido detectado: %s",
        python_path);
    set_status(status);
    UpdateWindow(g_app.window);

    snprintf(
        status,
        sizeof(status),
        "Status: gerando graficos com %s",
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
        set_status("Status: erro ao gerar graficos.");
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
        set_status("Status: erro ao gerar graficos.");
        return;
    }

    show_info(
        "Graficos gerados",
        "Arquivos criados:\n- population_activity.png\n- mean_state.png\n- raster.png");
    snprintf(
        status,
        sizeof(status),
        "Status: graficos gerados. Python usado: %s",
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

    set_status("Status: pasta de resultados aberta.");
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
        WS_CHILD | WS_VISIBLE,
        x,
        y,
        w,
        h,
        parent,
        (HMENU)(INT_PTR)id,
        GetModuleHandleA(NULL),
        NULL);

    SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_app.font, TRUE);
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

    SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_app.font, TRUE);
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

    SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_app.font, TRUE);
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
    field->label_hwnd = create_static(parent, label, x, y + 4, label_w, 22, 0);
    field->control_hwnd = create_edit(parent, id, x + label_w, y, control_w, 24);
}

static void create_controls(HWND hwnd)
{
    int label_w = 190;
    int control_w = 150;
    int x1 = 28;
    int x2 = 410;
    int y = 120;

    create_static(hwnd, "miniSNN Studio", 24, 18, 400, 28, 0);
    create_static(hwnd, "Laboratorio de cenarios configuraveis", 24, 48, 450, 22, 0);
    create_static(hwnd, "Edite, execute e compare redes sem alterar o nucleo da miniSNN.", 24, 74, 760, 22, 0);

    create_static(hwnd, "Cenario e topologia", x1, 100, 260, 20, 0);
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
    g_app.fields[g_app.field_count].label_hwnd = create_static(hwnd, "Topologia", x1, y + 36 + 4, label_w, 22, 0);
    g_app.topology_combo = CreateWindowExA(
        0,
        "COMBOBOX",
        "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        x1 + label_w,
        y + 36,
        control_w,
        160,
        hwnd,
        (HMENU)(INT_PTR)IDC_TOPOLOGY,
        GetModuleHandleA(NULL),
        NULL);
    SendMessageA(g_app.topology_combo, WM_SETFONT, (WPARAM)g_app.font, TRUE);
    SendMessageA(g_app.topology_combo, CB_ADDSTRING, 0, (LPARAM)"chain");
    SendMessageA(g_app.topology_combo, CB_ADDSTRING, 0, (LPARAM)"ring");
    SendMessageA(g_app.topology_combo, CB_ADDSTRING, 0, (LPARAM)"all_to_all");
    SendMessageA(g_app.topology_combo, CB_ADDSTRING, 0, (LPARAM)"random_balanced");
    g_app.fields[g_app.field_count].control_hwnd = g_app.topology_combo;
    g_app.field_count++;

    add_field(hwnd, IDC_NEURONS, "Neuronios", x1, y + 72, label_w, control_w);
    add_field(hwnd, IDC_INHIBITORY_PERCENT, "Proporcao inibitoria (%)", x1, y + 108, label_w, control_w);
    add_field(hwnd, IDC_CONNECTION_PROBABILITY, "Densidade de conexao", x1, y + 144, label_w, control_w);
    create_static(hwnd, "Usada apenas em random_balanced.", x1 + label_w, y + 171, 250, 20, 0);
    add_field(hwnd, IDC_SEED, "Seed", x1, y + 204, label_w, control_w);
    add_field(hwnd, IDC_DELAY, "Delay", x1, y + 240, label_w, control_w);
    add_field(hwnd, IDC_MAX_DELAY, "Delay maximo", x1, y + 276, label_w, control_w);

    create_static(hwnd, "Pesos e entrada", x2, 100, 260, 20, 0);
    add_field(hwnd, IDC_EXC_WEIGHT, "Peso excitatorio", x2, y, label_w, control_w);
    add_field(hwnd, IDC_INH_WEIGHT, "Peso inibitorio", x2, y + 36, label_w, control_w);
    add_field(hwnd, IDC_SOURCE_COUNT, "Neuronios com entrada", x2, y + 72, label_w, control_w);
    add_field(hwnd, IDC_INPUT_CURRENT, "Corrente externa", x2, y + 108, label_w, control_w);
    add_field(hwnd, IDC_RECORD_NEURON, "Neuronio gravado", x2, y + 144, label_w, control_w);

    create_static(hwnd, "Simulacao", x2, y + 196, 260, 20, 0);
    add_field(hwnd, IDC_STEPS, "Passos", x2, y + 224, label_w, control_w);
    add_field(hwnd, IDC_DT, "dt", x2, y + 260, label_w, control_w);
    add_field(hwnd, IDC_TAU, "tau", x2, y + 296, label_w, control_w);
    add_field(hwnd, IDC_V_REST, "V_rest", x2, y + 332, label_w, control_w);
    add_field(hwnd, IDC_V_RESET, "V_reset", x2, y + 368, label_w, control_w);
    add_field(hwnd, IDC_V_THRESHOLD, "V_threshold", x2, y + 404, label_w, control_w);
    add_field(hwnd, IDC_RESISTANCE, "Resistencia", x2, y + 440, label_w, control_w);
    add_field(hwnd, IDC_SYNAPTIC_DECAY, "Decaimento sinaptico", x2, y + 476, label_w, control_w);

    g_app.buttons[0] = create_button(hwnd, "Novo padrao", IDC_BTN_NEW, 810, 116, 150, 34);
    g_app.buttons[1] = create_button(hwnd, "Carregar cenario", IDC_BTN_LOAD, 970, 116, 160, 34);
    g_app.buttons[2] = create_button(hwnd, "Salvar cenario", IDC_BTN_SAVE, 810, 160, 150, 34);
    g_app.buttons[3] = create_button(hwnd, "Rodar simulacao", IDC_BTN_RUN, 970, 160, 160, 34);
    g_app.buttons[4] = create_button(hwnd, "Gerar graficos", IDC_BTN_PLOT, 810, 204, 150, 34);
    g_app.buttons[5] = create_button(hwnd, "Abrir resultados", IDC_BTN_OPEN, 970, 204, 160, 34);
    g_app.buttons[6] = create_button(hwnd, "Selecionar Python", IDC_BTN_SELECT_PYTHON, 810, 248, 320, 34);

    g_app.status_label = create_static(hwnd, "Status: pronto para executar.", 810, 300, 330, 48, IDC_STATUS);
    g_app.summary_box = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        810,
        360,
        330,
        315,
        hwnd,
        (HMENU)(INT_PTR)IDC_SUMMARY,
        GetModuleHandleA(NULL),
        NULL);
    SendMessageA(g_app.summary_box, WM_SETFONT, (WPARAM)g_app.font, TRUE);

    EnableWindow(g_app.buttons[4], FALSE);
    EnableWindow(g_app.buttons[5], FALSE);
    reset_to_default();
}

static void layout_controls(HWND hwnd)
{
    RECT rect;
    int width;
    int height;
    int summary_height;

    GetClientRect(hwnd, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    summary_height = height - 380;

    if (summary_height < 180)
        summary_height = 180;

    MoveWindow(g_app.status_label, width - 370, 300, 340, 50, TRUE);
    MoveWindow(g_app.summary_box, width - 370, 360, 340, summary_height, TRUE);

    MoveWindow(g_app.buttons[0], width - 370, 116, 155, 34, TRUE);
    MoveWindow(g_app.buttons[1], width - 205, 116, 175, 34, TRUE);
    MoveWindow(g_app.buttons[2], width - 370, 160, 155, 34, TRUE);
    MoveWindow(g_app.buttons[3], width - 205, 160, 175, 34, TRUE);
    MoveWindow(g_app.buttons[4], width - 370, 204, 155, 34, TRUE);
    MoveWindow(g_app.buttons[5], width - 205, 204, 175, 34, TRUE);
    MoveWindow(g_app.buttons[6], width - 370, 248, 340, 34, TRUE);
}

static void draw_button(const DRAWITEMSTRUCT *item)
{
    HBRUSH brush;
    HPEN pen;
    RECT rect = item->rcItem;
    char text[TEXT_BUFFER_SIZE];
    UINT state = item->itemState;
    COLORREF fill = RGB(36, 42, 54);
    COLORREF border = RGB(96, 170, 210);
    COLORREF text_color = RGB(242, 246, 250);

    if ((state & ODS_DISABLED) != 0)
    {
        fill = RGB(28, 30, 36);
        border = RGB(58, 62, 70);
        text_color = RGB(120, 124, 132);
    }
    else if ((state & ODS_SELECTED) != 0)
    {
        fill = RGB(58, 78, 96);
    }

    brush = CreateSolidBrush(fill);
    pen = CreatePen(PS_SOLID, 1, border);

    FillRect(item->hDC, &rect, brush);
    SelectObject(item->hDC, pen);
    SelectObject(item->hDC, GetStockObject(NULL_BRUSH));
    Rectangle(item->hDC, rect.left, rect.top, rect.right, rect.bottom);

    GetWindowTextA(item->hwndItem, text, sizeof(text));
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, text_color);
    SelectObject(item->hDC, g_app.font);
    DrawTextA(
        item->hDC,
        text,
        -1,
        &rect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    DeleteObject(brush);
    DeleteObject(pen);
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
        g_app.background_color = RGB(14, 18, 24);
        g_app.panel_color = RGB(21, 26, 34);
        g_app.edit_color = RGB(24, 29, 38);
        g_app.text_color = RGB(240, 244, 248);
        g_app.accent_color = RGB(96, 170, 210);
        g_app.background_brush = CreateSolidBrush(g_app.background_color);
        g_app.panel_brush = CreateSolidBrush(g_app.panel_color);
        g_app.edit_brush = CreateSolidBrush(g_app.edit_color);
        g_app.font = CreateFontA(
            18,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            ANSI_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            FIXED_PITCH | FF_MODERN,
            "Terminal");
        create_controls(hwnd);
        layout_controls(hwnd);
        return 0;

    case WM_SIZE:
        layout_controls(hwnd);
        return 0;

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
            case IDC_BTN_SELECT_PYTHON:
                choose_python_executable(0);
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

    case WM_DESTROY:
        if (g_app.font != NULL)
            DeleteObject(g_app.font);
        if (g_app.background_brush != NULL)
            DeleteObject(g_app.background_brush);
        if (g_app.panel_brush != NULL)
            DeleteObject(g_app.panel_brush);
        if (g_app.edit_brush != NULL)
            DeleteObject(g_app.edit_brush);
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

    hwnd = CreateWindowExA(
        0,
        window_class.lpszClassName,
        APP_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1180,
        760,
        NULL,
        NULL,
        instance,
        NULL);

    if (hwnd == NULL)
        return 1;

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    while (GetMessageA(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    return (int)message.wParam;
}
