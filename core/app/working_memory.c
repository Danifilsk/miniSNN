#include "working_memory.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORKING_MEMORY_PATH_MAX 512

static void set_error(
    char *error_message,
    size_t error_message_size,
    const char *message)
{
    if (error_message != NULL && error_message_size > 0)
        snprintf(error_message, error_message_size, "%s", message);
}

static uint32_t working_memory_next_random(uint32_t *state)
{
    uint32_t value = *state;

    if (value == 0U)
        value = 0x6d2b79f5U;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static int working_memory_expected_pattern(
    const ScenarioConfig *config,
    int trial,
    uint32_t *random_state)
{
    if (strcmp(config->working_memory_cue_pattern, "seeded") == 0)
    {
        return (int)(working_memory_next_random(random_state) %
                     (uint32_t)config->working_memory_readout_count);
    }

    return trial % config->working_memory_readout_count;
}

static int working_memory_control_expected_pattern(
    int trial,
    int readout_count)
{
    /* A balanced, deterministic reordering independent of cue identity. */
    return (trial / readout_count) % readout_count;
}

static int working_memory_inputs_are_zero(const double *inputs, int count)
{
    for (int index = 0; index < count; index++)
    {
        if (inputs[index] != 0.0)
            return 0;
    }
    return 1;
}

static int make_path(
    char *out_path,
    size_t out_path_size,
    const char *directory,
    const char *filename)
{
    return out_path != NULL && directory != NULL && filename != NULL &&
           snprintf(out_path, out_path_size, "%s/%s", directory, filename) <
               (int)out_path_size;
}

static void write_html_text(FILE *file, const char *text)
{
    for (; *text != '\0'; text++)
    {
        if (*text == '&')
            fputs("&amp;", file);
        else if (*text == '<')
            fputs("&lt;", file);
        else if (*text == '>')
            fputs("&gt;", file);
        else if (*text == '"')
            fputs("&quot;", file);
        else
            fputc((unsigned char)*text, file);
    }
}

void working_memory_result_destroy(WorkingMemoryResult *result)
{
    if (result == NULL)
        return;

    free(result->trials);
    memset(result, 0, sizeof(*result));
}

int working_memory_decode_readout(
    const int *readout_spike_counts,
    int readout_count,
    int expected_pattern,
    double *out_score,
    int *out_recalled_pattern,
    double *out_mean_absolute_error)
{
    int total_spikes = 0;
    int maximum_count = -1;
    int recalled_pattern = -1;
    double absolute_error_sum = 0.0;

    if (readout_spike_counts == NULL || readout_count < 2 ||
        expected_pattern < 0 || expected_pattern >= readout_count ||
        out_score == NULL || out_recalled_pattern == NULL ||
        out_mean_absolute_error == NULL)
    {
        return 0;
    }

    for (int index = 0; index < readout_count; index++)
    {
        if (readout_spike_counts[index] < 0)
            return 0;
        total_spikes += readout_spike_counts[index];
        if (readout_spike_counts[index] > maximum_count)
        {
            maximum_count = readout_spike_counts[index];
            recalled_pattern = index;
        }
    }

    if (total_spikes == 0)
    {
        recalled_pattern = -1;
        absolute_error_sum = 1.0;
    }
    else
    {
        for (int index = 0; index < readout_count; index++)
        {
            double observed = (double)readout_spike_counts[index] /
                              (double)total_spikes;
            double expected = index == expected_pattern ? 1.0 : 0.0;
            absolute_error_sum += fabs(observed - expected);
        }
    }

    *out_mean_absolute_error = absolute_error_sum / (double)readout_count;
    *out_score = 1.0 - *out_mean_absolute_error;
    *out_recalled_pattern = recalled_pattern;
    return isfinite(*out_score) && isfinite(*out_mean_absolute_error);
}

int working_memory_execute(
    const ScenarioConfig *config,
    const ScenarioBlueprint *blueprint,
    WorkingMemoryResult *out_result,
    char *error_message,
    size_t error_message_size)
{
    WorkingMemoryResult result;
    MiniSNN *snn = NULL;
    int *readout_spike_counts = NULL;
    double inputs[SCENARIO_RUNTIME_MAX_NEURONS];
    uint32_t random_state;
    int absolute_step = 0;
    int response_count = 0;
    int control_correct_trials = 0;

    if (config == NULL || blueprint == NULL || out_result == NULL ||
        !config->working_memory_enabled ||
        !scenario_config_validate(config, error_message, error_message_size))
    {
        if (error_message != NULL && error_message_size > 0 &&
            error_message[0] == '\0')
        {
            set_error(error_message, error_message_size,
                      "memoria de trabalho nao esta habilitada");
        }
        return 0;
    }

    memset(&result, 0, sizeof(result));
    result.trial_count = config->working_memory_trials;
    result.mean_response_latency = -1.0;
    result.trials = calloc((size_t)result.trial_count, sizeof(*result.trials));
    readout_spike_counts = calloc(
        (size_t)config->working_memory_readout_count,
        sizeof(*readout_spike_counts));
    if (result.trials == NULL || readout_spike_counts == NULL)
    {
        free(readout_spike_counts);
        working_memory_result_destroy(&result);
        set_error(error_message, error_message_size,
                  "memoria insuficiente para memoria de trabalho");
        return 0;
    }

    random_state = config->working_memory_seed;
    for (int trial_index = 0; trial_index < result.trial_count; trial_index++)
    {
        WorkingMemoryTrial *trial = &result.trials[trial_index];
        int cue_pattern = working_memory_expected_pattern(
            config, trial_index, &random_state);
        int trial_steps = config->working_memory_cue_steps +
                          config->working_memory_delay_steps +
                          config->working_memory_probe_steps;
        double mean_absolute_error;
        double control_score;
        double control_mean_absolute_error;
        int control_recalled_pattern;
        int control_expected_pattern;

        if (snn == NULL || config->working_memory_reset_between_trials)
        {
            minisnn_destroy(&snn);
            if (!scenario_runtime_create_from_blueprint(
                    config, blueprint, &snn, error_message, error_message_size) ||
                !scenario_runtime_configure_modules(
                    snn, config, error_message, error_message_size))
            {
                minisnn_destroy(&snn);
                free(readout_spike_counts);
                working_memory_result_destroy(&result);
                return 0;
            }
            if (config->working_memory_reset_between_trials)
                absolute_step = 0;
        }

        memset(readout_spike_counts, 0,
               (size_t)config->working_memory_readout_count *
                   sizeof(*readout_spike_counts));
        trial->trial = trial_index;
        trial->cue_pattern = cue_pattern;
        trial->expected_pattern = cue_pattern;
        trial->delay_steps = config->working_memory_delay_steps;
        trial->first_response_step = -1;
        trial->delay_inputs_zero = 1;
        trial->probe_inputs_zero = 1;

        for (int trial_step = 0; trial_step < trial_steps; trial_step++)
        {
            ScenarioRuntimeStep runtime_step;
            int in_cue = trial_step < config->working_memory_cue_steps;
            int in_probe = trial_step >= config->working_memory_cue_steps +
                                          config->working_memory_delay_steps;

            memset(inputs, 0, (size_t)config->neurons * sizeof(*inputs));
            if (in_cue)
            {
                int cue_start = config->working_memory_cue_start +
                                cue_pattern *
                                    config->working_memory_cue_group_size;

                for (int member = 0;
                     member < config->working_memory_cue_group_size;
                     member++)
                {
                    inputs[cue_start + member] = config->input_current;
                }
            }
            else if (in_probe)
            {
                trial->probe_inputs_zero &=
                    working_memory_inputs_are_zero(inputs, config->neurons);
            }
            else
            {
                trial->delay_inputs_zero &=
                    working_memory_inputs_are_zero(inputs, config->neurons);
            }

            if (!scenario_runtime_step_with_inputs(
                    snn, config, blueprint->inhibitory_count, absolute_step,
                    inputs, config->neurons, &runtime_step, error_message,
                    error_message_size))
            {
                minisnn_destroy(&snn);
                free(readout_spike_counts);
                working_memory_result_destroy(&result);
                return 0;
            }

            if (in_probe)
            {
                int readout_spikes_this_step = 0;
                for (int readout_index = 0;
                     readout_index < config->working_memory_readout_count;
                     readout_index++)
                {
                    int readout_start = config->working_memory_readout_start +
                                        readout_index *
                                            config->working_memory_readout_group_size;

                    for (int member = 0;
                         member < config->working_memory_readout_group_size;
                         member++)
                    {
                        int neuron_id = readout_start + member;

                        readout_spike_counts[readout_index] +=
                            runtime_step.spikes[neuron_id];
                        readout_spikes_this_step += runtime_step.spikes[neuron_id];
                    }
                }
                if (readout_spikes_this_step > 0 &&
                    trial->first_response_step < 0)
                {
                    trial->first_response_step =
                        trial_step - config->working_memory_cue_steps -
                        config->working_memory_delay_steps;
                }
            }
            absolute_step++;
        }

        if (!working_memory_decode_readout(
                readout_spike_counts, config->working_memory_readout_count,
                trial->expected_pattern, &trial->recall_score,
                &trial->recalled_pattern, &mean_absolute_error))
        {
            minisnn_destroy(&snn);
            free(readout_spike_counts);
            working_memory_result_destroy(&result);
            set_error(error_message, error_message_size,
                      "erro ao decodificar readout de memoria de trabalho");
            return 0;
        }

        trial->recall_correct =
            trial->recalled_pattern == trial->expected_pattern &&
            trial->recall_score >= config->working_memory_recall_threshold &&
            mean_absolute_error <= config->working_memory_recall_tolerance;
        control_expected_pattern = working_memory_control_expected_pattern(
            trial->trial, config->working_memory_readout_count);
        if (!working_memory_decode_readout(
                readout_spike_counts, config->working_memory_readout_count,
                control_expected_pattern,
                &control_score, &control_recalled_pattern,
                &control_mean_absolute_error))
        {
            minisnn_destroy(&snn);
            free(readout_spike_counts);
            working_memory_result_destroy(&result);
            set_error(error_message, error_message_size,
                      "erro ao calcular controle de memoria de trabalho");
            return 0;
        }
        trial->control_correct =
            control_recalled_pattern == control_expected_pattern &&
            control_score >= config->working_memory_recall_threshold &&
            control_mean_absolute_error <=
                config->working_memory_recall_tolerance;
        for (int readout_index = 0;
             readout_index < config->working_memory_readout_count;
             readout_index++)
        {
            trial->mean_readout_activity +=
                (double)readout_spike_counts[readout_index];
        }
        trial->mean_readout_activity /=
            (double)(config->working_memory_readout_count *
                     config->working_memory_readout_group_size *
                     config->working_memory_probe_steps);
        result.mean_recall_score += trial->recall_score;
        if (trial->recall_correct)
            result.correct_trials++;
        if (trial->control_correct)
            control_correct_trials++;
        if (trial->first_response_step >= 0)
        {
            result.mean_response_latency +=
                (double)trial->first_response_step;
            response_count++;
        }
    }

    minisnn_destroy(&snn);
    free(readout_spike_counts);
    result.recall_accuracy =
        (double)result.correct_trials / (double)result.trial_count;
    result.chance_accuracy =
        1.0 / (double)config->working_memory_readout_count;
    result.control_accuracy =
        (double)control_correct_trials / (double)result.trial_count;
    result.retention_margin =
        result.recall_accuracy - result.control_accuracy;
    result.mean_recall_score /= (double)result.trial_count;
    for (int trial_index = 0; trial_index < result.trial_count; trial_index++)
    {
        double difference = result.trials[trial_index].recall_score -
                            result.mean_recall_score;
        result.recall_score_stddev += difference * difference;
    }
    result.recall_score_stddev = sqrt(
        result.recall_score_stddev / (double)result.trial_count);
    if (response_count > 0)
    {
        result.mean_response_latency =
            (result.mean_response_latency + 1.0) / (double)response_count;
    }

    *out_result = result;
    return 1;
}

static int write_trials_csv(
    const WorkingMemoryResult *result,
    const char *path)
{
    FILE *file = fopen(path, "w");

    if (file == NULL ||
        fprintf(file,
                "trial,cue_pattern,expected_pattern,recalled_pattern,recall_score,recall_correct,control_correct,delay_steps,first_response_step,mean_readout_activity,delay_inputs_zero,probe_inputs_zero\n") < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    for (int index = 0; index < result->trial_count; index++)
    {
        const WorkingMemoryTrial *trial = &result->trials[index];
        if (fprintf(file, "%d,%d,%d,%d,%.17g,%d,%d,%d,%d,%.17g,%d,%d\n",
                    trial->trial, trial->cue_pattern, trial->expected_pattern,
                    trial->recalled_pattern, trial->recall_score,
                    trial->recall_correct, trial->control_correct,
                    trial->delay_steps, trial->first_response_step,
                    trial->mean_readout_activity, trial->delay_inputs_zero,
                    trial->probe_inputs_zero) < 0)
        {
            fclose(file);
            return 0;
        }
    }
    return fclose(file) == 0;
}

static int write_summary(
    const WorkingMemoryResult *result,
    const char *path)
{
    FILE *file = fopen(path, "w");

    if (file == NULL)
        return 0;
    if (fprintf(file,
                "trial_count=%d\ncorrect_trials=%d\nrecall_accuracy=%.17g\n"
                "mean_recall_score=%.17g\nrecall_score_stddev=%.17g\n"
                "mean_response_latency=%.17g\nchance_accuracy=%.17g\n"
                "control_accuracy=%.17g\nretention_margin=%.17g\n",
                result->trial_count, result->correct_trials,
                result->recall_accuracy, result->mean_recall_score,
                result->recall_score_stddev,
                result->mean_response_latency, result->chance_accuracy,
                result->control_accuracy, result->retention_margin) < 0)
    {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int write_html_report(
    const ScenarioConfig *config,
    const WorkingMemoryResult *result,
    const char *path)
{
    FILE *file = fopen(path, "w");

    if (file == NULL)
        return 0;
    if (fputs("<!doctype html><html><head><meta charset=\"utf-8\">"
              "<title>Memoria de trabalho</title><style>"
              "body{background:#101418;color:#e8edf2;font-family:system-ui,monospace;max-width:1100px;margin:2rem auto;padding:0 1rem}"
              "table{border-collapse:collapse;width:100%}td,th{border:1px solid #53606b;padding:.45rem;text-align:right}"
              "th{background:#1b242c}code{color:#9fd3ff}</style></head><body>"
              "<h1>Memoria de trabalho temporal</h1><p>"
              "Protocolo: cue, delay sem estimulo e probe de readout.</p><h2>Configuracao</h2><ul><li>Modelo: <code>", file) == EOF)
    {
        fclose(file);
        return 0;
    }
    write_html_text(file, minisnn_neuron_model_name(config->neuron_model));
    if (fprintf(file,
                "</code></li><li>Trials: %d; cue: %d passos; delay: %d; probe: %d</li>"
                "<li>Padrao: <code>",
                config->working_memory_trials,
                config->working_memory_cue_steps,
                config->working_memory_delay_steps,
                config->working_memory_probe_steps) < 0)
    {
        fclose(file);
        return 0;
    }
    write_html_text(file, config->working_memory_cue_pattern);
    if (fprintf(file,
                "</code></li><li>Cue: neuronios %d a %d, grupos de %d</li>"
                "<li>Readout: %d grupos de %d neuronios, iniciando em %d</li>"
                "<li>Seed: %u; reset entre trials: %s</li>"
                "<li>Tolerancia: %.6f; limiar: %.6f</li></ul>"
                "<h2>Resumo</h2><p>Accuracy: %.6f; score medio: %.6f; "
                "desvio: %.6f; latencia media: %.6f passos. "
                "Acaso: %.6f; controle de rotulos embaralhados: %.6f; "
                "margem de retencao: %.6f.</p>"
                "<h2>Trials</h2><table><thead><tr>"
                "<th>trial</th><th>cue</th><th>esperado</th><th>recordado</th>"
                "<th>score</th><th>correto</th><th>controle</th><th>delay</th><th>latencia</th><th>atividade</th><th>delay sem entrada</th><th>probe sem entrada</th>"
                "</tr></thead><tbody>",
                config->working_memory_cue_start,
                config->working_memory_cue_start +
                    config->working_memory_readout_count *
                        config->working_memory_cue_group_size - 1,
                config->working_memory_cue_group_size,
                config->working_memory_readout_count,
                config->working_memory_readout_group_size,
                config->working_memory_readout_start,
                config->working_memory_seed,
                config->working_memory_reset_between_trials ? "sim" : "nao",
                config->working_memory_recall_tolerance,
                config->working_memory_recall_threshold,
                result->recall_accuracy, result->mean_recall_score,
                result->recall_score_stddev,
                result->mean_response_latency, result->chance_accuracy,
                result->control_accuracy, result->retention_margin) < 0)
    {
        fclose(file);
        return 0;
    }
    for (int index = 0; index < result->trial_count; index++)
    {
        const WorkingMemoryTrial *trial = &result->trials[index];
        if (fprintf(file,
                    "<tr><td>%d</td><td>%d</td><td>%d</td><td>%d</td>"
                    "<td>%.6f</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%.6f</td><td>%s</td><td>%s</td></tr>",
                    trial->trial, trial->cue_pattern, trial->expected_pattern,
                    trial->recalled_pattern, trial->recall_score,
                    trial->recall_correct ? "sim" : "nao",
                    trial->control_correct ? "sim" : "nao",
                    trial->delay_steps, trial->first_response_step,
                    trial->mean_readout_activity,
                    trial->delay_inputs_zero ? "sim" : "nao",
                    trial->probe_inputs_zero ? "sim" : "nao") < 0)
        {
            fclose(file);
            return 0;
        }
    }
    if (fputs("</tbody></table><h2>Limitacoes</h2>"
              "<p>O score e calculado somente a partir dos spikes do grupo de readout durante o probe. "
              "O cue nao e copiado para a resposta: o padrao recordado e obtido pelo maior canal de atividade. "
              "O controle usa rotulos ciclicamente embaralhados apenas na avaliacao, sem alterar a dinamica. "
              "Accuracy alta neste protocolo depende da configuracao recorrente do demo e nao prova memoria geral.</p>"
              "<p><a href=\"working_memory_trials.csv\">CSV por trial</a> | "
              "<a href=\"working_memory_summary.txt\">resumo textual</a></p></body></html>\n", file) == EOF)
    {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

int working_memory_write_outputs(
    const ScenarioConfig *config,
    const WorkingMemoryResult *result,
    const char *output_directory,
    char *error_message,
    size_t error_message_size)
{
    char trials_path[WORKING_MEMORY_PATH_MAX];
    char summary_path[WORKING_MEMORY_PATH_MAX];
    char report_path[WORKING_MEMORY_PATH_MAX];

    if (config == NULL || result == NULL || output_directory == NULL ||
        result->trials == NULL || !make_path(trials_path, sizeof(trials_path),
                                              output_directory,
                                              "working_memory_trials.csv") ||
        !make_path(summary_path, sizeof(summary_path), output_directory,
                   "working_memory_summary.txt") ||
        !make_path(report_path, sizeof(report_path), output_directory,
                   "working_memory_report.html") ||
        !write_trials_csv(result, trials_path) ||
        !write_summary(result, summary_path) ||
        !write_html_report(config, result, report_path))
    {
        set_error(error_message, error_message_size,
                  "erro ao escrever saidas de memoria de trabalho");
        return 0;
    }

    return 1;
}
