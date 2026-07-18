#include <stdio.h>

#include "c6_suite.h"

int main(void)
{
    char error_message[256] = {0};

    if (!c6_suite_execute(error_message, sizeof(error_message)))
    {
        fprintf(stderr, "Suite C6 falhou: %s\n", error_message);
        return 1;
    }
    printf("Suite C6 concluida: results/scenarios/c6_suite/c6_suite_report.html\n");
    return 0;
}
