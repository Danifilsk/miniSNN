# miniSNN Worlds

`worlds/` abriga produtos futuros separados do miniSNN Core. O produto presente
e o **miniSNN Worlds Kernel**, em `worlds/kernel/`, iniciado em K0-A e
estendido por K0-B.

O Kernel e headless, deterministico e generico. Ele nao depende do Core, nao
conhece Domain, Brain Bridge ou interface grafica. O Core permanece um produto
independente em `core/`.

K0-A estabelece lifecycle, configuracao, tempo logico e diagnostico minimo.
K0-B estabelece entidades, comandos futuros e eventos deterministas. K0-A e
K0-B estao concluidos; K0 permanece em andamento e K0-C e o proximo
sub-bloco apos a auditoria K0-B bem-sucedida.
