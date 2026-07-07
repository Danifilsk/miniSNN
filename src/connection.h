#ifndef CONNECTION_H
#define CONNECTION_H

typedef struct
{
    // Índice do neurônio de destino
    int target;

    // Peso sináptico
    double weight;

    int delay;

} Connection;

typedef struct
{
    // Lista de conexões que saem deste neurônio
    Connection *list;

    // Número de conexões
    int count;

} ConnectionList;

#endif
