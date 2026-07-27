#include "d1_b_robustness_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    D1_B_SEEN_NEURONS = 1 << 0,
    D1_B_SEEN_SEED = 1 << 1,
    D1_B_SEEN_LIF_STEPS = 1 << 2,
    D1_B_SEEN_ADEX_STEPS = 1 << 3,
    D1_B_SEEN_HH_STEPS = 1 << 4,
    D1_B_SEEN_STRUCTURAL_STEPS = 1 << 5,
    D1_B_SEEN_C7_TICKS = 1 << 6,
    D1_B_SEEN_BRAIN_STEPS = 1 << 7
};

static void write_error(char *buffer, size_t size, const char *message)
{
    if (buffer != NULL && size > 0U)
        snprintf(buffer, size, "%s", message != NULL ? message : "erro de configuracao");
}

static char *trim_ascii(char *text)
{
    char *end;

    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
        text++;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
    return text;
}

static int parse_u64_strict(const char *text, uint64_t *out_value)
{
    char *end;
    unsigned long long value;

    if (text == NULL || out_value == NULL || text[0] == '\0' || text[0] == '-')
        return 0;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || *trim_ascii(end) != '\0')
        return 0;
    *out_value = (uint64_t)value;
    return 1;
}

int d1_b_robustness_config_is_valid(const D1BRobustnessConfig *config)
{
    return config != NULL && config->neuron_count >= 2U &&
        config->neuron_count <= 128U && config->lif_steps > 0U &&
        config->adex_steps > 0U && config->hodgkin_huxley_steps > 0U &&
        config->structural_steps > 0U && config->c7_ticks > 0U &&
        config->brain_steps_per_tick > 0U && config->brain_steps_per_tick <= 64U;
}

uint64_t d1_b_robustness_config_signature(const D1BRobustnessConfig *config)
{
    const uint64_t values[] =
    {
        config != NULL ? config->neuron_count : 0U,
        config != NULL ? config->seed : 0U,
        config != NULL ? config->lif_steps : 0U,
        config != NULL ? config->adex_steps : 0U,
        config != NULL ? config->hodgkin_huxley_steps : 0U,
        config != NULL ? config->structural_steps : 0U,
        config != NULL ? config->c7_ticks : 0U,
        config != NULL ? config->brain_steps_per_tick : 0U
    };
    uint64_t hash = UINT64_C(14695981039346656037);

    if (!d1_b_robustness_config_is_valid(config))
        return 0U;
    for (size_t index = 0U; index < sizeof(values) / sizeof(values[0]); index++)
    {
        for (unsigned int byte = 0U; byte < 8U; byte++)
        {
            hash ^= (values[index] >> (byte * 8U)) & UINT64_C(0xff);
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

void d1_b_robustness_config_default(D1BRobustnessConfig *config)
{
    if (config == NULL)
        return;
    memset(config, 0, sizeof(*config));
    config->neuron_count = 8U;
    config->seed = UINT64_C(20261001);
    config->lif_steps = UINT64_C(1000000);
    config->adex_steps = UINT64_C(250000);
    config->hodgkin_huxley_steps = UINT64_C(100000);
    config->structural_steps = UINT64_C(250000);
    config->c7_ticks = UINT64_C(10000);
    config->brain_steps_per_tick = 3U;
}

int d1_b_robustness_config_load_file(
    const char *filename,
    D1BRobustnessConfig *out_config,
    char *error_message,
    size_t error_message_size)
{
    FILE *file;
    D1BRobustnessConfig parsed;
    unsigned int seen = 0U;
    unsigned int line_number = 0U;
    char line[256];

    if (filename == NULL || out_config == NULL || error_message == NULL ||
        error_message_size == 0U)
        return 0;
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        write_error(error_message, error_message_size, "arquivo de auditoria nao pode ser aberto");
        return 0;
    }
    d1_b_robustness_config_default(&parsed);
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *text;
        char *equals;
        char *key;
        char *value;
        uint64_t parsed_value;
        unsigned int bit;

        line_number++;
        if (strchr(line, '\n') == NULL && !feof(file))
        {
            fclose(file);
            snprintf(error_message, error_message_size, "linha %u excede 255 bytes", line_number);
            return 0;
        }
        text = trim_ascii(line);
        if (text[0] == '\0' || text[0] == '#' || text[0] == ';' || text[0] == '[')
            continue;
        equals = strchr(text, '=');
        if (equals == NULL)
            goto format_error;
        *equals = '\0';
        key = trim_ascii(text);
        value = trim_ascii(equals + 1U);
        if (strcmp(key, "neuron_count") == 0)
            bit = D1_B_SEEN_NEURONS;
        else if (strcmp(key, "seed") == 0)
            bit = D1_B_SEEN_SEED;
        else if (strcmp(key, "lif_steps") == 0)
            bit = D1_B_SEEN_LIF_STEPS;
        else if (strcmp(key, "adex_steps") == 0)
            bit = D1_B_SEEN_ADEX_STEPS;
        else if (strcmp(key, "hodgkin_huxley_steps") == 0)
            bit = D1_B_SEEN_HH_STEPS;
        else if (strcmp(key, "structural_steps") == 0)
            bit = D1_B_SEEN_STRUCTURAL_STEPS;
        else if (strcmp(key, "c7_ticks") == 0)
            bit = D1_B_SEEN_C7_TICKS;
        else if (strcmp(key, "brain_steps_per_tick") == 0)
            bit = D1_B_SEEN_BRAIN_STEPS;
        else
        {
            fclose(file);
            snprintf(error_message, error_message_size, "chave desconhecida na linha %u", line_number);
            return 0;
        }
        if ((seen & bit) != 0U || !parse_u64_strict(value, &parsed_value))
            goto format_error;
        seen |= bit;
        if (bit == D1_B_SEEN_NEURONS || bit == D1_B_SEEN_BRAIN_STEPS)
        {
            if (parsed_value > UINT32_MAX)
                goto format_error;
            if (bit == D1_B_SEEN_NEURONS)
                parsed.neuron_count = (uint32_t)parsed_value;
            else
                parsed.brain_steps_per_tick = (uint32_t)parsed_value;
        }
        else if (bit == D1_B_SEEN_SEED)
            parsed.seed = parsed_value;
        else if (bit == D1_B_SEEN_LIF_STEPS)
            parsed.lif_steps = parsed_value;
        else if (bit == D1_B_SEEN_ADEX_STEPS)
            parsed.adex_steps = parsed_value;
        else if (bit == D1_B_SEEN_HH_STEPS)
            parsed.hodgkin_huxley_steps = parsed_value;
        else if (bit == D1_B_SEEN_STRUCTURAL_STEPS)
            parsed.structural_steps = parsed_value;
        else
            parsed.c7_ticks = parsed_value;
        continue;

format_error:
        fclose(file);
        snprintf(error_message, error_message_size, "valor invalido ou duplicado na linha %u", line_number);
        return 0;
    }
    {
        const int read_failed = ferror(file);
        const int close_failed = fclose(file) != 0;

        if (read_failed || close_failed || !d1_b_robustness_config_is_valid(&parsed))
        {
            write_error(error_message, error_message_size, "configuracao D1-B invalida");
            return 0;
        }
    }
    *out_config = parsed;
    write_error(error_message, error_message_size, "");
    return 1;
}

int d1_b_robustness_config_write_file(
    const char *filename,
    const D1BRobustnessConfig *config,
    char *error_message,
    size_t error_message_size)
{
    FILE *file;

    if (filename == NULL || error_message == NULL || error_message_size == 0U ||
        !d1_b_robustness_config_is_valid(config))
        return 0;
    file = fopen(filename, "wb");
    if (file == NULL)
    {
        write_error(error_message, error_message_size, "config_used.ini nao pode ser aberto");
        return 0;
    }
    {
        const int written = fprintf(file,
                                    "[d1_b_robustness]\n"
                                    "neuron_count=%u\nseed=%llu\nlif_steps=%llu\nadex_steps=%llu\n"
                                    "hodgkin_huxley_steps=%llu\nstructural_steps=%llu\nc7_ticks=%llu\n"
                                    "brain_steps_per_tick=%u\n",
                                    config->neuron_count, (unsigned long long)config->seed,
                                    (unsigned long long)config->lif_steps,
                                    (unsigned long long)config->adex_steps,
                                    (unsigned long long)config->hodgkin_huxley_steps,
                                    (unsigned long long)config->structural_steps,
                                    (unsigned long long)config->c7_ticks,
                                    config->brain_steps_per_tick) >= 0;
        const int closed = fclose(file) == 0;

        if (!written || !closed)
        {
            write_error(error_message, error_message_size, "falha ao gravar config_used.ini");
            return 0;
        }
    }
    write_error(error_message, error_message_size, "");
    return 1;
}
