#ifndef INSTRUCCTIONS_H
#define INSTRUCCTIONS_H

#include <assert.h>
#include <mpfr.h>
#include <stdbool.h>
#include <stdint.h>
#include "parser.h"

#define INSTRUCTION_COUNT 24
#define INSTRUCTION_COUNT_SIZE 256
#define INPUT_BUFFER 512

typedef struct Instruction{
    void (*func) (void* args);
    struct Instruction *next;
    char* str;
    uint32_t hash;
} Instruction;

typedef struct InstructionMap{
    Instruction* inst_map;
    char* chunk;
    uint16_t chunk_offset; 
} InstructionMap;

typedef struct App{
    char buffer[INPUT_BUFFER];
    mpfr_t result;
    Parser* parser;
    bool run;
} App;


InstructionMap* instruction_map_creation();
void instruction_map_destroy(InstructionMap* map);
bool instruction_map_exist(InstructionMap* map, const char* inst);
bool instruction_map_execute(InstructionMap* map, const char* inst, void* args);

#endif
