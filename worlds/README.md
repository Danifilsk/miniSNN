# miniSNN Worlds

`worlds/` abriga produtos futuros separados do miniSNN Core. O produto presente
e o **miniSNN Worlds Kernel**, em `worlds/kernel/`, iniciado em K0-A e
estendido por K0-B e K0-C.

O Kernel e headless, deterministico e generico. Ele nao depende do Core, nao
conhece Domain, Brain Bridge ou interface grafica. O Core permanece um produto
independente em `core/`.

K0-A estabelece lifecycle, configuracao, tempo logico e diagnostico minimo.
K0-B estabelece entidades, comandos futuros e eventos deterministas. K0-C
estabelece streams PRNG, hash canonico e observabilidade. K0-D estabelece um
runner configuravel, artefatos tecnicos e auditoria de stress fora da
biblioteca. K0 esta concluido; K1 e o proximo bloco para entidades genericas,
transforms, espaco, ocupacao, barreiras, deslocamento e links.
