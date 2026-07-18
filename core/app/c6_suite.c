#include "c6_suite.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <windows.h>

#include "minisnn.h"
#include "scenario_config.h"
#include "scenario_runner.h"
#include "scenario_runtime.h"

#define C6_SUITE_OUTPUT_DIRECTORY "results/scenarios/c6_suite"

typedef struct
{
    const char *protocol;
    const char *config_path;
    const char *control_type;
    const char *persistence_expected;
    const char *persistence_observed;
    const char *limitation;
    ScenarioConfig config;
    ScenarioRunResult result;
    double score;
    double control;
    double margin;
    int deterministic;
    int passed;
} C6SuiteRow;

static void set_error(char *error_message, size_t error_message_size,
                      const char *message)
{
    if (error_message != NULL && error_message_size > 0)
        snprintf(error_message, error_message_size, "%s", message);
}

static int ensure_directory(const char *path)
{
    DWORD attributes;

    if (path == NULL)
        return 0;
    attributes = GetFileAttributesA(path);
    if (attributes != INVALID_FILE_ATTRIBUTES)
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return CreateDirectoryA(path, NULL) != 0 ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

static int load_and_run(C6SuiteRow *row, char *error_message,
                        size_t error_message_size)
{
    if (row == NULL ||
        !scenario_config_load_file(row->config_path, &row->config,
                                   error_message, error_message_size) ||
        !scenario_runner_execute(&row->config, row->config_path, &row->result,
                                 error_message, error_message_size))
    {
        return 0;
    }

    if (strcmp(row->protocol, "working_memory") == 0)
    {
        row->score = row->result.working_memory_recall_accuracy;
        row->control = row->result.working_memory_control_accuracy;
        row->margin = row->result.working_memory_retention_margin;
        row->passed = row->score >= 0.80 && row->margin >= 0.25;
    }
    else if (strcmp(row->protocol, "associative_memory") == 0)
    {
        row->score = row->result.associative_memory_recall_accuracy;
        row->control = row->result.associative_memory_control_accuracy;
        row->margin = row->result.associative_memory_association_margin;
        row->passed = row->score >= 0.80 && row->margin >= 0.25;
    }
    else
    {
        row->score = row->result.sequence_prediction_next_pattern_accuracy;
        row->control =
            row->result.sequence_prediction_last_symbol_only_control_accuracy;
        row->margin = row->result.sequence_prediction_context_margin;
        row->passed = row->score >= 0.80 && row->margin >= 0.25;
    }
    return 1;
}

static int nearly_equal(double left, double right)
{
    return fabs(left - right) < 1e-12;
}

static int rows_are_semantically_equal(
    const C6SuiteRow *first,
    const C6SuiteRow *second)
{
    MiniSNNConfig first_config;
    MiniSNNConfig second_config;

    if (first == NULL || second == NULL ||
        !scenario_runtime_make_minisnn_config(&first->config, &first_config) ||
        !scenario_runtime_make_minisnn_config(&second->config, &second_config) ||
        first->config.neuron_model != second->config.neuron_model ||
        minisnn_config_neuron_model_signature(&first_config) !=
            minisnn_config_neuron_model_signature(&second_config) ||
        first->result.topology_signature != second->result.topology_signature ||
        first->result.connection_count != second->result.connection_count ||
        !nearly_equal(first->score, second->score) ||
        !nearly_equal(first->control, second->control) ||
        !nearly_equal(first->margin, second->margin))
    {
        return 0;
    }

    if (strcmp(first->protocol, "working_memory") == 0)
    {
        return first->result.working_memory_trial_count ==
            second->result.working_memory_trial_count;
    }
    if (strcmp(first->protocol, "associative_memory") == 0)
    {
        return first->result.associative_memory_trial_count ==
            second->result.associative_memory_trial_count;
    }
    return first->result.sequence_prediction_trial_count ==
        second->result.sequence_prediction_trial_count;
}

static int write_outputs(const C6SuiteRow *rows, size_t row_count,
                         char *error_message, size_t error_message_size)
{
    char csv_path[512];
    char summary_path[512];
    char report_path[512];
    FILE *csv;
    FILE *summary;
    FILE *report;
    int all_passed = 1;

    if (!ensure_directory("results") || !ensure_directory("results/scenarios") ||
        !ensure_directory(C6_SUITE_OUTPUT_DIRECTORY) ||
        snprintf(csv_path, sizeof(csv_path), "%s/c6_suite_summary.csv",
                 C6_SUITE_OUTPUT_DIRECTORY) >= (int)sizeof(csv_path) ||
        snprintf(summary_path, sizeof(summary_path), "%s/c6_suite_summary.txt",
                 C6_SUITE_OUTPUT_DIRECTORY) >= (int)sizeof(summary_path) ||
        snprintf(report_path, sizeof(report_path), "%s/c6_suite_report.html",
                 C6_SUITE_OUTPUT_DIRECTORY) >= (int)sizeof(report_path))
    {
        set_error(error_message, error_message_size,
                  "erro ao preparar saidas da suite C6");
        return 0;
    }

    csv = fopen(csv_path, "w");
    summary = fopen(summary_path, "w");
    report = fopen(report_path, "w");
    if (csv == NULL || summary == NULL || report == NULL ||
        fprintf(csv,
                "protocol,neuron_model,neuron_model_config_signature,topology,seed,training_enabled,plasticity_frozen_during_evaluation,network_reconstructed_before_evaluation,checkpoint_loaded,control_type,result,control,margin,persistence_expected,persistence_observed,deterministic,status,limitation\n") < 0 ||
        fputs("<!doctype html><html><head><meta charset=\"utf-8\"><title>Suite C6</title><style>body{background:#101418;color:#e8edf2;font-family:system-ui,monospace;max-width:1100px;margin:2rem auto;padding:0 1rem}table{border-collapse:collapse;width:100%}td,th{border:1px solid #53606b;padding:.45rem;text-align:left}th{background:#1b242c}.ok{color:#8ee39b}.bad{color:#ff9b9b}</style></head><body><h1>Suite integrada C6</h1><p>Protocolos experimentais controlados. Os resultados nao formam um QI, nem demonstram inteligencia ou memoria geral.</p><table><thead><tr><th>protocolo</th><th>modelo</th><th>assinatura</th><th>topologia / seed</th><th>resultado</th><th>controle</th><th>margem</th><th>deterministico</th><th>persistencia</th><th>status</th></tr></thead><tbody>", report) == EOF)
    {
        if (csv != NULL)
            fclose(csv);
        if (summary != NULL)
            fclose(summary);
        if (report != NULL)
            fclose(report);
        set_error(error_message, error_message_size,
                  "erro ao abrir saidas da suite C6");
        return 0;
    }

    for (size_t index = 0; index < row_count; index++)
    {
        MiniSNNConfig minisnn_config;
        const C6SuiteRow *row = &rows[index];
        unsigned long long signature;

        scenario_runtime_make_minisnn_config(&row->config, &minisnn_config);
        signature = minisnn_config_neuron_model_signature(&minisnn_config);
        if (fprintf(csv,
                    "%s,%s,%016llx,%s,%u,%s,%s,%s,%s,%s,%.17g,%.17g,%.17g,%s,%s,%s,%s,%s\n",
                    row->protocol,
                    minisnn_neuron_model_name(row->config.neuron_model),
                    signature, row->config.topology,
                    row->config.sequence_prediction_enabled ?
                        row->config.sequence_prediction_seed :
                        row->config.associative_memory_enabled ?
                            row->config.associative_memory_seed :
                            row->config.working_memory_seed,
                    row->config.plasticity_enabled ? "true" : "false",
                    (row->config.associative_memory_enabled &&
                     row->config.associative_memory_freeze_plasticity_during_recall) ||
                    (row->config.sequence_prediction_enabled &&
                     row->config.sequence_prediction_freeze_plasticity_during_evaluation) ?
                        "true" : "NA",
                    row->protocol[0] == 'w' ? "NA" : "true",
                    "false", row->control_type, row->score, row->control,
                    row->margin, row->persistence_expected,
                    row->persistence_observed,
                    row->deterministic ? "true" : "false",
                    row->passed ? "PASSOU" : "FALHOU", row->limitation) < 0 ||
            fprintf(summary,
                    "%s: resultado=%.6f controle=%.6f margem=%.6f deterministico=%s persistencia=%s status=%s\n",
                    row->protocol, row->score, row->control, row->margin,
                    row->deterministic ? "true" : "false",
                    row->persistence_observed,
                    row->passed ? "PASSOU" : "FALHOU") < 0 ||
            fprintf(report,
                    "<tr><td>%s</td><td>%s</td><td>%016llx</td><td>%s / %u</td><td>%.6f</td><td>%.6f (%s)</td><td>%.6f</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td></tr>",
                    row->protocol,
                    minisnn_neuron_model_name(row->config.neuron_model),
                    signature, row->config.topology,
                    row->config.sequence_prediction_enabled ?
                        row->config.sequence_prediction_seed :
                        row->config.associative_memory_enabled ?
                            row->config.associative_memory_seed :
                            row->config.working_memory_seed,
                    row->score, row->control, row->control_type, row->margin,
                    row->deterministic ? "sim" : "nao",
                    row->persistence_observed,
                    row->passed ? "ok" : "bad",
                    row->passed ? "PASSOU" : "FALHOU") < 0)
        {
            fclose(csv);
            fclose(summary);
            fclose(report);
            set_error(error_message, error_message_size,
                      "erro ao escrever saidas da suite C6");
            return 0;
        }
        if (!row->passed)
            all_passed = 0;
    }
    if (fputs("</tbody></table><h2>Limitacoes</h2><p>Memoria de trabalho depende do estado dinamico e e apagada por reset. Memoria associativa e predicao sequencial persistem somente pelos pesos e sao avaliadas em redes limpas. Nenhum protocolo implementa memoria episodica, semantica, planejamento ou linguagem.</p></body></html>\n", report) == EOF ||
        fclose(csv) != 0 || fclose(summary) != 0 || fclose(report) != 0)
    {
        set_error(error_message, error_message_size,
                  "erro ao fechar saidas da suite C6");
        return 0;
    }
    if (!all_passed)
    {
        set_error(error_message, error_message_size,
                  "a suite C6 encontrou protocolo nao deterministico ou reprovado");
        return 0;
    }
    return 1;
}

int c6_suite_execute(char *error_message, size_t error_message_size)
{
    C6SuiteRow rows[] = {
        {.protocol = "working_memory",
         .config_path = "configs/working_memory_demo.ini",
         .control_type = "chance",
         .persistence_expected = "dinamica",
         .persistence_observed = "apagada por reset",
         .limitation = "nao e memoria geral"},
        {.protocol = "associative_memory",
         .config_path = "configs/associative_memory_demo.ini",
         .control_type = "sem treino/alvo embaralhado",
         .persistence_expected = "sinaptica",
         .persistence_observed = "reconstrucao limpa",
         .limitation = "depende de teacher pulse no treino"},
        {.protocol = "sequence_context",
         .config_path = "configs/sequence_prediction_context_demo.ini",
         .control_type = "somente ultimo simbolo",
         .persistence_expected = "sinaptica",
         .persistence_observed = "reconstrucao limpa",
         .limitation = "nao demonstra predicao geral"}
    };

    for (size_t index = 0; index < sizeof(rows) / sizeof(rows[0]); index++)
    {
        C6SuiteRow repeat = {
            .protocol = rows[index].protocol,
            .config_path = rows[index].config_path
        };

        if (!load_and_run(&rows[index], error_message, error_message_size) ||
            !load_and_run(&repeat, error_message, error_message_size))
        {
            return 0;
        }
        rows[index].deterministic = rows_are_semantically_equal(
            &rows[index], &repeat);
        if (!rows[index].deterministic)
            rows[index].passed = 0;
    }
    return write_outputs(rows, sizeof(rows) / sizeof(rows[0]), error_message,
                         error_message_size);
}
