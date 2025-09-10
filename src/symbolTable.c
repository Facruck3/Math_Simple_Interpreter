#include "symbolTable.h"
#include "debug.h"
#include <assert.h>
#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static inline uint32_t hash_string(const char* str, size_t len) {
    DEBUG_FUNCTION_ENTER();
    
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= str[i];
        hash *= 16777619;
    }
    
    DEBUG_HASH(str, len, hash, hash % SYMBOL_MAP_BASE_SIZE);
    DEBUG_FUNCTION_EXIT();
    return hash;
}

static void symbol_table_debug_bucket(SymbolTable* table, uint16_t bucket_index);
static void symbol_table_debug_show(SymbolTable* table);
static void symbol_table_debug_stats(SymbolTable* table);

#define DEBUG_SHOW_TABLE(table) symbol_table_debug_show(table);
#define DEBUG_SHOW_TABLE_STATS(table) symbol_table_debug_stats(table);
#define DEBUG_SHOW_BUCKET(table,idx) symbol_table_debug_bucket(table,idx);


#ifdef DEBUG
static void symbol_table_debug_bucket(SymbolTable* table, uint16_t bucket_index) {
    DEBUG_PRINT("=== BUCKET %d DEBUG ===\n", bucket_index);
    
    if (bucket_index >= table->capacity) {
        ERROR_PRINT("Bucket index %d out of range (capacity: %d)\n", 
                   bucket_index, table->capacity);
        return;
    }
    
    Symbol* current = table->buckets[bucket_index];
    if (current == NULL) {
        DEBUG_PRINT("Bucket %d is empty\n", bucket_index);
        return;
    }
    
    int position = 0;
    while (current) {
        char value_str[100];
        mpfr_sprintf(value_str, "%.15Rf", current->num);
        DEBUG_PRINT("Position %d: %s = %s\n", 
                   position, current->name, value_str);
        DEBUG_PRINT("  Hash: %u, Length: %d, Next: %p\n",
                   hash_string(current->name, current->len) % table->capacity,
                   current->len, (void*)current->next);
        current = current->next;
        position++;
    }
    DEBUG_PRINT("========================\n");
}
static void symbol_table_debug_show(SymbolTable* table) {
    DEBUG_PRINT("=== SYMBOL TABLE DUMP ===\n");
    DEBUG_PRINT("Capacity: %d, Count: %d, Load factor: %.2f%%\n", 
                table->capacity, table->count, 
                (float)table->count / table->capacity * 100);
    
    int total_buckets_used = 0;
    for (uint16_t i = 0; i < table->capacity; i++) {
        if (table->buckets[i]) {
            total_buckets_used++;
            DEBUG_PRINT("Bucket %3d: ", i);
            Symbol* current = table->buckets[i];
            int chain_length = 0;
            while (current) {
                char value_str[100];
                mpfr_sprintf(value_str, "%.10Rf", current->num);
                DEBUG_PRINT("%s=%s", current->name, value_str);
                current = current->next;
                chain_length++;
                if (current) DEBUG_PRINT(" → ");
            }
            DEBUG_PRINT(" [chain: %d]\n", chain_length);
        }
    }
    
    DEBUG_PRINT("Buckets used: %d/%d (%.1f%%)\n", 
                total_buckets_used, table->capacity,
                (float)total_buckets_used / table->capacity * 100);
    DEBUG_PRINT("=========================\n");
}
static void symbol_table_debug_stats(SymbolTable* table) {
    DEBUG_PRINT("=== SYMBOL TABLE STATISTICS ===\n");
    
    int empty_buckets = 0;
    int max_chain_length = 0;
    int total_chain_length = 0;
    int non_empty_buckets = 0;
    int total_symbols = 0;
    
    for (uint16_t i = 0; i < table->capacity; i++) {
        if (table->buckets[i] == NULL) {
            empty_buckets++;
        } else {
            non_empty_buckets++;
            int chain_length = 0;
            Symbol* current = table->buckets[i];
            while (current) {
                chain_length++;
                total_symbols++;
                current = current->next;
            }
            total_chain_length += chain_length;
            if (chain_length > max_chain_length) {
                max_chain_length = chain_length;
            }
        }
    }
    
    DEBUG_PRINT("Capacity: %d\n", table->capacity);
    DEBUG_PRINT("Total symbols: %d\n", total_symbols);
    DEBUG_PRINT("Table count: %d\n", table->count);
    DEBUG_PRINT("Load factor: %.2f%%\n", (float)table->count / table->capacity * 100);
    DEBUG_PRINT("Empty buckets: %d (%.1f%%)\n", empty_buckets, 
                (float)empty_buckets / table->capacity * 100);
    DEBUG_PRINT("Non-empty buckets: %d\n", non_empty_buckets);
    DEBUG_PRINT("Average chain length: %.2f\n", non_empty_buckets > 0 ? 
                (float)total_chain_length / non_empty_buckets : 0);
    DEBUG_PRINT("Max chain length: %d\n", max_chain_length);
    DEBUG_PRINT("Memory usage: ~%.2f KB\n", 
                (table->capacity * sizeof(Symbol*) + table->count * sizeof(Symbol)) / 1024.0);
    DEBUG_PRINT("===============================\n");
}
#else
static void symbol_table_debug_bucket(SymbolTable* table, uint16_t bucket_index){}
static void symbol_table_debug_show(SymbolTable* table){}
static void symbol_table_debug_stats(SymbolTable* table){}
#endif



SymbolTable* symbol_table_create() {
    DEBUG_FUNCTION_ENTER();
    
    SymbolTable* sym = malloc(sizeof(SymbolTable));
    CHECK_NULL(sym, ERROR_RETURN_NULL("Failed to allocate SymbolTable"));
    
    sym->capacity = SYMBOL_MAP_BASE_SIZE;
    sym->count = 0;
    sym->buckets = calloc(SYMBOL_MAP_BASE_SIZE, sizeof(Symbol*));
    CHECK_NULL(sym->buckets, {
        free(sym);
        ERROR_RETURN_NULL("Failed to allocate buckets array");
    });
    
    DEBUG_PRINT("Symbol table created successfully\n");
    DEBUG_PRINT("Capacity: %d, Initial count: 0\n", SYMBOL_MAP_BASE_SIZE);
    DEBUG_FUNCTION_EXIT();
    return sym;
}

void symbol_table_destroy(SymbolTable* symTable) {
    DEBUG_FUNCTION_ENTER();
    symbol_table_empty(symTable);
    
    free(symTable->buckets);
    free(symTable);
    mpfr_free_cache();
    
    DEBUG_FUNCTION_EXIT();
}

static inline void symbol_table_resize(SymbolTable* table, size_t new_capacity) {
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(table, return);
    
    DEBUG_PRINT("Resizing required - Current: %d, New: %zu\n", 
                table->capacity, new_capacity);
    DEBUG_PRINT("Count: %d, Threshold: %.0f%%\n", 
                table->count, THRESHOLD_MAP * 100);
    
    Symbol** new_buckets = calloc(new_capacity, sizeof(Symbol*));
    CHECK_NULL(new_buckets, {
        ERROR_PRINT("Failed to allocate new buckets array\n");
        return;
    });
    
    int rehashed_symbols = 0;
    for (size_t i = 0; i < table->capacity; i++) {
        Symbol* current = table->buckets[i];
        while (current) {
            Symbol* next = current->next;
            uint32_t new_index = hash_string(current->name, current->len) % new_capacity;
            
            DEBUG_PRINT("Rehashing '%s' from bucket %zu to %u\n", 
                       current->name, i, new_index);
            
            current->next = new_buckets[new_index];
            new_buckets[new_index] = current;
            current = next;
            rehashed_symbols++;
        }
    }
    
    free(table->buckets);
    table->buckets = new_buckets;
    table->capacity = new_capacity;
    
    DEBUG_PRINT("Resize completed. Rehashed %d symbols\n", rehashed_symbols);
    DEBUG_PRINT("New capacity: %d\n", table->capacity);
    DEBUG_SHOW_TABLE(table);
    DEBUG_FUNCTION_EXIT();
}

mpfr_t* symbol_table_insert(SymbolTable* table, const char* name, const mpfr_t* num, uint8_t nameLen) {
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(table, ERROR_RETURN_NULL("Table is NULL"));
    CHECK_NULL(name, ERROR_RETURN_NULL("Variable name is NULL"));
    CHECK_NULL(num, ERROR_RETURN_NULL("MPFR value is NULL"));
    
    DEBUG_PRINT("Insert operation: '%s' (length: %d)\n", name, nameLen);
    DEBUG_MPFR_VALUE(*num, "Input value");
    
    // Check if resizing is needed
    if (table->count > table->capacity * THRESHOLD_MAP) {
        DEBUG_PRINT("Load threshold exceeded (%.1f%%), resizing...\n", 
                   (float)table->count / table->capacity * 100);
        symbol_table_resize(table, table->capacity << 1);
    }
    
    uint32_t index = hash_string(name, nameLen) % table->capacity;
    DEBUG_PRINT("Hash index calculated: %u\n", index);
    
    Symbol* current = table->buckets[index];
    Symbol* prev = NULL;
    uint16_t position = 0;
    
    // Search for existing symbol
    while (current) {
        if (current->len == nameLen && strcmp(current->name, name) == 0) {
            DEBUG_PRINT("Found existing symbol at bucket %d, position %d\n", index, position);
            
            char old_value[100], new_value[100];
            mpfr_sprintf(old_value, "%.15Rf", current->num);
            mpfr_sprintf(new_value, "%.15Rf", *num);
            
            DEBUG_PRINT("Updating value: %s -> %s\n", old_value, new_value);
            
            mpfr_clear(current->num);
            mpfr_init2(current->num, PRECISION_ROUNDING_BITS);
            mpfr_set(current->num, *num, MPFR_RNDN);
            
            DEBUG_SYMBOL_OP("updated", name, current->num);
            DEBUG_FUNCTION_EXIT();
            return &current->num;
        }
        prev = current;
        current = current->next;
        position++;
    }
    
    DEBUG_PRINT("Creating new symbol '%s' in bucket %d\n", name, index);
    
    // Create new symbol
    Symbol* new_symbol = malloc(sizeof(Symbol));
    CHECK_NULL(new_symbol, ERROR_RETURN_NULL("Failed to allocate new symbol"));
    
    new_symbol->name = strdup(name);
    CHECK_NULL(new_symbol->name, {
        free(new_symbol);
        ERROR_RETURN_NULL("Failed to duplicate symbol name");
    });
    
    new_symbol->len = nameLen;
    mpfr_init2(new_symbol->num, PRECISION_ROUNDING_BITS);
    mpfr_set(new_symbol->num, *num, MPFR_RNDN);
    
    // Insert at head of bucket chain
    new_symbol->next = table->buckets[index];
    table->buckets[index] = new_symbol;
    table->count++;
    
    DEBUG_SYMBOL_OP("inserted", name, new_symbol->num);
    DEBUG_PRINT("Total symbols now: %d\n", table->count);
    DEBUG_SHOW_TABLE(table);
    
    DEBUG_FUNCTION_EXIT();
    return &new_symbol->num;
}

mpfr_t* symbol_table_get(SymbolTable* table, const char* name, uint8_t nameLen) {
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(table, ERROR_RETURN_NULL("Table is NULL"));
    CHECK_NULL(name, ERROR_RETURN_NULL("Variable name is NULL"));
    
    DEBUG_PRINT("Lookup operation: '%s' (length: %d)\n", name, nameLen);
    
    uint32_t index = hash_string(name, nameLen) % table->capacity;
    DEBUG_PRINT("Hash index calculated: %u\n", index);
    
    Symbol* current = table->buckets[index];
    int position = 0;
    
    while (current) {
        if (current->len == nameLen && strcmp(current->name, name) == 0) {
            DEBUG_PRINT("Found symbol at bucket %d, position %d\n", index, position);
            DEBUG_SYMBOL_OP("retrieved", name, current->num);
            DEBUG_FUNCTION_EXIT();
            return &current->num;
        }
        current = current->next;
        position++;
    }
    
    DEBUG_PRINT("Symbol '%s' not found in table\n", name);
    WARNING_PRINT("Undefined variable: '%s'\n", name);
    DEBUG_FUNCTION_EXIT();
    return NULL;
}

void print_friendly_mpfr(mpfr_t value, const char* label) {
    if (label) printf("%s: ", label);
    
    if (mpfr_nan_p(value)) {
        printf("NaN\n");
        return;
    }
    if (mpfr_inf_p(value)) {
        printf("%sInfinity\n", mpfr_signbit(value) ? "-" : "");
        return;
    }
    
    // Choose the best format automatically
    mpfr_exp_t exponent;
    char* str = mpfr_get_str(NULL, &exponent, 10, 0, value, MPFR_RNDN);
    
    if (str && strlen(str) > 0) {
        if (exponent < -3 || exponent > 6) {
            // Use scientific notation for very small or very large numbers
            mpfr_printf("%.10Re\n", value);
        } else {
            // Use fixed point for normal numbers
            int digits_after_decimal = 10 - exponent;
            if (digits_after_decimal < 0) digits_after_decimal = 0;
            if (digits_after_decimal > 10) digits_after_decimal = 10;
            
            mpfr_printf("%.*Rf\n", digits_after_decimal, value);
        }
        mpfr_free_str(str);
    } else {
        mpfr_printf("%Rf\n", value);
    }
}

void symbol_table_show(SymbolTable* symTable){
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(symTable, return;);
    CHECK_NULL(symTable->buckets, return;);

    printf("=== === === Variables === === ===\n");
    for(uint16_t i = 0; i<symTable->capacity ;i++){
        if(symTable->buckets[i]){
            Symbol* sym = symTable->buckets[i];
            while (sym) {
                printf("-- %s : ",sym->name);            
                print_friendly_mpfr(sym->num, NULL);
                sym = sym->next;
            }
        }
    }
    printf("==== === === === === === === ====\n");

    DEBUG_FUNCTION_EXIT();
}

void symbol_table_empty(SymbolTable* symTable){
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(symTable, return);
    
    DEBUG_PRINT("Destroying symbol table - Capacity: %d, Count: %d\n", 
                symTable->capacity, symTable->count);
    
    uint16_t symbols_freed = 0;
    for (uint16_t i = 0; i < symTable->capacity; i++) {
        if (symTable->buckets[i]) {
            Symbol* current = symTable->buckets[i];
            while (current) {
                Symbol* next = current->next;
                DEBUG_PRINT("Freeing symbol: '%s' (bucket %d)\n", current->name, i);
                mpfr_clear(current->num);
                free(current->name);
                free(current);
                current = next;
                symbols_freed++;
            }
        }
    }

    DEBUG_PRINT("Symbol table destroyed. Freed %d symbols\n", symbols_freed);
    DEBUG_FUNCTION_EXIT();
}