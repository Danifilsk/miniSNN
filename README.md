# miniSNN

**miniSNN** é um projeto experimental em C para estudar **Spiking Neural Networks (SNNs)** e utilizá-las como cérebros de agentes em mundos simulados.

O projeto começou como uma implementação pequena e compreensível de redes neurais pulsantes, mas evoluiu para uma arquitetura modular composta por uma biblioteca neural independente e uma plataforma de simulação determinística.

O objetivo não é criar apenas um jogo.

A ideia é construir uma plataforma onde seja possível observar redes neurais controlando organismos, interagindo com ambientes, competindo por recursos e, futuramente, apresentando comportamentos cada vez mais complexos.

---

# Visão geral

O projeto é dividido fisicamente em componentes independentes:

```text
miniSNN
│
├── core/
│   └── miniSNN Core
│
└── worlds/
    ├── kernel/
    │   └── Worlds Kernel
    │
    ├── domain/
    │   └── Worlds Domain
    │
    ├── brain_bridge/
    │   └── Brain Bridge
    │
    └── app/
        └── Visual App (futuro)
```

A arquitetura foi feita para evitar que conceitos de uma camada vazem para outra.

Em resumo:

```text
Core
não sabe o que é um mundo.

Kernel
não sabe o que é uma criatura.

Domain
não sabe o que é um neurônio.

Brain Bridge
é a camada que conecta cérebro e mundo.

Visual App
apenas observa e representa a simulação.
```

---

# miniSNN Core

Localização:

```text
core/
```

O **miniSNN Core** é a biblioteca neural do projeto.

Ele implementa as estruturas e operações necessárias para trabalhar com Spiking Neural Networks sem conhecer absolutamente nada sobre criaturas, comida, mapas ou jogos.

O Core trabalha com conceitos como:

* neurônios;
* spikes;
* potenciais de membrana;
* sinapses;
* redes;
* entradas e saídas neurais;
* topologias;
* execução determinística;
* métricas;
* experimentos.

O modelo neural base utiliza neurônios do tipo **LIF — Leaky Integrate-and-Fire**.

Para o Core, um cérebro utilizado por uma criatura no Worlds é simplesmente uma rede recebendo valores de entrada e produzindo atividade de saída.

O Core não conhece conceitos como:

```text
fish
food
hunger
position
movement
world
terrain
```

Essa separação permite utilizar o miniSNN Core também fora do Worlds.

---

# miniSNN Worlds

Localização:

```text
worlds/
```

**miniSNN Worlds** é a plataforma de simulação construída ao redor do Core.

Ela é dividida em várias camadas.

---

# Worlds Kernel

Localização:

```text
worlds/kernel/
```

O **Worlds Kernel** é o runtime genérico da simulação.

Ele é responsável pelos mecanismos fundamentais necessários para manter um mundo determinístico.

Entre suas responsabilidades estão:

* tempo lógico baseado em ticks;
* entidades;
* comandos;
* eventos;
* transforms espaciais;
* movimento;
* occupancy;
* colisões;
* vínculos espaciais entre entidades;
* PRNG determinístico;
* hashing canônico do estado;
* snapshots;
* save/load;
* command logs;
* replay determinístico.

O Kernel é propositalmente genérico.

Ele não sabe o que é:

```text
organism
fish
shark
food
species
brain
hunger
health
```

Uma entidade do Kernel é apenas uma entidade.

Essa separação permite que as regras físicas e temporais da simulação permaneçam independentes da lógica biológica do mundo.

---

# Worlds Domain

Localização:

```text
worlds/domain/
```

O **Worlds Domain** adiciona significado às estruturas genéricas fornecidas pelo Kernel.

É aqui que conceitos de mundo passam a existir.

O domínio mínimo atualmente trabalha com conceitos como:

```text
ORGANISM
FOOD
Species
Energy
Hunger
Perception
```

E ações como:

```text
WAIT
MOVE
EAT
```

Por exemplo:

O Kernel sabe que:

```text
Entity 42
está na posição (1000, 3000)
```

O Domain pode saber que:

```text
Entity 42
é um ORGANISM
da espécie X
com determinada quantidade de energia
```

O Domain utiliza apenas a API pública do Kernel.

O Kernel nunca depende do Domain.

---

# Percepção

Criaturas não recebem uma explicação completa sobre o mundo.

O objetivo é fornecer **sensações**, não comportamentos prontos.

Por exemplo, uma percepção inicial pode conter:

```text
energy
hunger

nearest_food_present
nearest_food_dx
nearest_food_dy
nearest_food_distance
```

Isso não significa que o organismo automaticamente saiba:

```text
"Existe comida à direita.
Portanto devo andar para a direita."
```

Esses valores precisam ser processados pelo cérebro.

No futuro, sensores poderão se tornar progressivamente mais indiretos e biologicamente interessantes, incluindo conceitos como:

* visão por setores;
* proximidade;
* contato;
* temperatura;
* luz;
* cheiro;
* som;
* dor;
* orientação;
* velocidade.

Uma regra importante do projeto é:

> **O mundo fornece causas e sensações; não fornece explicações.**

---

# Brain Bridge

Localização planejada:

```text
worlds/brain_bridge/
```

O **Brain Bridge** conecta o Worlds Domain ao miniSNN Core.

Esta é a única camada autorizada a conhecer simultaneamente:

```text
miniSNN Core
+
Worlds Domain
```

Seu fluxo conceitual é:

```text
DomainPerception
        │
        ▼
Brain Bridge Encoder
        │
        ▼
miniSNN Core
        │
        ▼
atividade neural
        │
        ▼
Brain Bridge Decoder
        │
        ▼
DomainAction
```

Por exemplo:

```text
energia
fome
posição relativa da comida
```

podem ser transformadas em canais de entrada neural.

A atividade produzida por neurônios de saída pode então ser traduzida para intenções como:

```text
WAIT
MOVE +X
MOVE -X
MOVE +Y
MOVE -Y
EAT
```

Isso cria uma separação importante:

> O cérebro escolhe o que tentar fazer.
> O Domain decide o que realmente acontece.

Se o cérebro tentar andar contra um obstáculo, por exemplo, ele pode simplesmente ter seu movimento rejeitado.

A rede neural não recebe automaticamente a solução correta.

---

# Ações e comportamento

As criaturas possuem um conjunto limitado de ações compatíveis com seu corpo.

Inicialmente essas ações são discretas:

```text
WAIT
MOVE
EAT
```

Isso não significa que seu comportamento seja programado manualmente.

O Brain Bridge apenas traduz atividade neural para ações disponíveis.

O projeto não deve esconder controllers tradicionais dentro do Bridge, como:

```text
if food_is_right:
    move_right()
```

A decisão deve surgir da rede neural.

No futuro, ações poderão evoluir para atuadores mais contínuos, por exemplo:

```text
TURN
THRUST
BITE
```

ou até sinais semelhantes a controle muscular.

---

# Determinismo

Determinismo é uma propriedade central do miniSNN Worlds.

Dadas as mesmas:

```text
configurações
seeds
condições iniciais
ações
```

a simulação deve produzir exatamente a mesma história.

O Kernel possui mecanismos de:

```text
State Hash
Snapshot
Restore
Command Log
Replay
```

que permitem verificar isso.

Isso é importante tanto para debugging quanto para experimentação científica.

---

# Persistência e Replay

O Worlds Kernel possui suporte para snapshots e replay.

Isso permite:

```text
executar
↓
salvar checkpoint
↓
continuar
```

ou:

```text
restaurar checkpoint
↓
reaplicar comandos
↓
reproduzir exatamente a mesma simulação
```

A persistência semântica completa do Domain e dos cérebros será adicionada progressivamente.

O objetivo futuro é permitir snapshots integrados contendo:

```text
Kernel
+
Domain
+
Brain state
```

---

# Visual App

Uma aplicação visual será adicionada ao Worlds sem alterar a simulação headless.

A arquitetura deverá permanecer:

```text
Simulation
    │
    ▼
Visual App
```

e nunca:

```text
Visual App
    │
    ▼
regras da simulação
```

Isso significa que executar o Worlds sem interface gráfica deverá continuar produzindo exatamente os mesmos resultados.

O app servirá para observar coisas como:

* mapa;
* criaturas;
* alimentos;
* obstáculos;
* movimentos;
* energia;
* sensores;
* atividade neural;
* informações de debug.

---

# Sprites e Assets

Sprites pertencem exclusivamente à camada visual.

Eles não fazem parte do estado semântico da simulação.

A organização planejada é semelhante a:

```text
assets/
└── sprites/
    ├── creatures/
    │   ├── fish.png
    │   ├── shark.png
    │   └── ...
    │
    ├── terrain/
    │   ├── water.png
    │   ├── sand.png
    │   ├── rock.png
    │   └── ...
    │
    ├── food/
    ├── objects/
    └── effects/
```

Assim será possível adicionar ou substituir sprites sem modificar o código da simulação.

Por exemplo:

```text
Domain:
Species = FISH

Visual App:
FISH → assets/sprites/creatures/fish.png
```

O Domain não armazena:

```text
fish.png
texture handles
rendering coordinates
GPU resources
```

Da mesma forma, tiles podem possuir representações visuais:

```text
WATER → water.png
SAND  → sand.png
ROCK  → rock.png
```

mas propriedades físicas continuam pertencendo à simulação.

Por exemplo:

```text
ROCK
→ bloqueia movimento
```

é uma propriedade do mundo.

```text
rock.png
```

é apenas sua aparência.

No futuro, arquivos de configuração poderão permitir trocar associações visuais sem recompilar o programa.

---

# Headless primeiro

A simulação é desenvolvida inicialmente em modo headless.

Isso permite testar comportamento através de:

* hashes;
* logs;
* snapshots;
* testes automatizados;
* artefatos determinísticos;
* replay.

A interface gráfica será uma representação desse estado, e não a autoridade sobre ele.

---

# Organização arquitetural

A direção geral das dependências é:

```text
             miniSNN Core
                  ▲
                  │
             Brain Bridge
                  │
                  ▼
            Worlds Domain
                  │
                  ▼
            Worlds Kernel
```

O Visual App observa as APIs apropriadas sem se tornar parte da lógica determinística.

Regras importantes:

```text
Core       ↛ Domain
Core       ↛ Kernel
Kernel     ↛ Domain
Kernel     ↛ Core
Domain     ↛ Core
```

O Brain Bridge existe justamente para evitar uma dependência direta entre Domain e Core.

---

# Estado atual

A fundação principal atualmente inclui:

```text
miniSNN Core
└── Core 1.0.0-rc.1

Worlds Kernel
├── K0 — deterministic runtime
├── K1 — spatial foundation
└── K2 — persistence and replay

Worlds Domain
- WD0 - minimal semantic domain
- WD1 - semantic Domain persistence
```

O próximo estágio de integração é o Brain Bridge, responsável pela primeira conexão formal entre percepção do mundo e execução neural.

---

# Roadmap simplificado

O caminho atual é aproximadamente:

```text
Core
│
├── Neural foundation
│
▼
Worlds Kernel
│
├── deterministic runtime
├── spatial simulation
├── persistence/replay
│
▼
Worlds Domain
│
├── organisms
├── food
├── energy
├── perception
│
▼
Brain Bridge
│
├── sensors → SNN
├── SNN → actions
│
▼
First neural organism
│
▼
Visual App
│
▼
Minimal ecosystem
│
▼
Predator / prey
│
▼
richer senses and bodies
│
▼
learning / adaptation
│
▼
genetics and evolution
│
▼
communication
│
▼
social behavior
│
▼
tools and technology
│
▼
emergent societies
```

Esse roadmap é experimental e pode mudar conforme novas necessidades arquiteturais aparecem.

---

# Filosofia do projeto

O miniSNN tenta manter algumas regras simples:

### Não programar comportamento quando ele pode emergir

Uma criatura deve receber sensores, possuir um corpo e produzir ações.

O objetivo não é escrever diretamente:

```text
if hungry:
    find_food()
```

mas observar se sistemas neurais conseguem desenvolver comportamentos úteis a partir das condições do mundo.

### Separar mecanismo de significado

O Kernel fornece mecanismos.

O Domain fornece significado.

O Brain Bridge fornece tradução.

O Core fornece processamento neural.

### Determinismo antes de complexidade

Uma simulação reproduzível é muito mais fácil de estudar, testar e entender.

### Headless antes de visual

A interface mostra o mundo.

Ela não define o mundo.

### Complexidade progressiva

O objetivo não é começar tentando simular uma civilização inteira.

O projeto cresce aproximadamente de:

```text
neurônio
→ rede
→ cérebro
→ organismo
→ ecossistema
→ comportamento social
→ sociedades
```

Cada camada deve existir sobre uma fundação testável.

---

# Objetivo de longo prazo

O objetivo final do miniSNN Worlds é funcionar como um laboratório para explorar questões como:

* Como comportamento emerge de redes neurais simples?
* Como organismos aprendem a interagir com ambientes?
* Como diferentes arquiteturas neurais afetam sobrevivência?
* Como percepção limitada altera comportamento?
* Como competição e cooperação podem surgir?
* Como comunicação pode aparecer?
* Como aprendizado social pode funcionar?
* Como ferramentas e tecnologias podem surgir?
* Até que ponto sistemas relativamente simples podem produzir estruturas sociais complexas?

O resultado final não precisa seguir um roteiro previamente determinado.

Essa é justamente a graça.

A ideia é construir o mundo, construir os cérebros e então observar:

> **o que acontece?**