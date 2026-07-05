#ifndef CONFIG_H
#define CONFIG_H

//========================================
// Simulação
//========================================

// Apenas um neurônio para validação
#define N_NEURONS 10

// Tempo suficiente para observar vários disparos
#define T_MAX 1000

//========================================
// Modelo LIF
//========================================

#define DT       0.1
#define TAU      20.0

#define V_REST   -65.0
#define V_RESET  -65.0
#define V_THRESH -50.0

#define R        1.0

//========================================
// Estímulos
//========================================

// Teste diferentes valores:
// 5.0, 10.0, 15.0, 20.0, 30.0, 50.0...
#define I_EXT    20.0

// Não é usado neste teste, mas mantemos para evitar
// alterações em outros arquivos.
#define W        200.0

// Decaimento da corrente sináptica
#define SYN_DECAY 0.95

// Também não é usado por enquanto.
#define DELAY    1

#endif