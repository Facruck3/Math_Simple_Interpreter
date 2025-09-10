#ifndef SYMBOL_TABLE
#define SYMBOL_TABLE

#include <mpfr.h>
#include <stdbool.h>
#include <stdint.h>

#define PRECISION_ROUNDING_BITS 256
#define SYMBOL_MAP_BASE_SIZE 64 
#define THRESHOLD_MAP 0.6 // 60% of map -> resize 

typedef struct Symbol{
    mpfr_t num;
    char* name;
    struct Symbol* next;
    uint8_t len;
} Symbol;

typedef struct SymbolTable {
    Symbol** buckets;     
    uint16_t capacity;
    uint16_t count;   
} SymbolTable;


SymbolTable* symbol_table_create();
void symbol_table_destroy(SymbolTable* symTable);
void symbol_table_show(SymbolTable* symTable);
void symbol_table_empty(SymbolTable* symTable);


mpfr_t* symbol_table_insert(SymbolTable* symTable, const char* name, const mpfr_t* num , uint8_t nameLen);
mpfr_t* symbol_table_get(SymbolTable* symTable, const char* name , uint8_t nameLen);

void print_friendly_mpfr(mpfr_t value, const char* label);

#endif

