#include "instructions.h"
#include "symbolTable.h"
#include "debug.h"  // <-- Añadir esta línea
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ##########################################
// ######       Special Functions       #####
// ##########################################

static void exit_command(void* args) {
    DEBUG_FUNCTION_ENTER();
    App *app = (App *)args;
    app->run = false;
    DEBUG_INSTR("Exit command executed\n");
    DEBUG_FUNCTION_EXIT();
}

static void clear_command(void* args) {
    DEBUG_FUNCTION_ENTER();
    DEBUG_INSTR("Clear command executed\n");
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
    DEBUG_FUNCTION_EXIT();
}

static void help_command(void* args) {
    DEBUG_FUNCTION_ENTER();
    DEBUG_INSTR("Help command executed\n");
    printf("|(Explanation about apps) and commands :                            |\n");
    printf("| -exit : to escape from app                                        |\n");
    printf("| -clear : to clean terminal                                        |\n");
    printf("| -clear-vars : to delete all variables, if not you can re define it|\n");
    printf("| -help : to see the commands                                       |\n");
    printf("| -show : to see the current variables                              |\n");
    printf("| -info : information and characteristics of the app                |\n");
    printf("=====================================================================\n");
    DEBUG_FUNCTION_EXIT();
}

static void clear_vars_command(void* args) {
    DEBUG_FUNCTION_ENTER();
    App* app = (App*) args;
    DEBUG_INSTR("Clear variables command executed\n");
    symbol_table_empty(app->parser->symTable);
    DEBUG_INSTR("Cleared variables from symbol table\n");
    DEBUG_FUNCTION_EXIT();
}

static void info_command(void* args) {
    DEBUG_FUNCTION_ENTER();
    DEBUG_INSTR("Info command executed\n");
    printf("=== Math Interpreter Information ===\n");
    printf("Version: 1.0\n");
    printf("Precision: %d bits\n", PRECISION_ROUNDING_BITS);
    printf("MPFR Version: %s\n", mpfr_get_version());
    printf("Features: Variables, Arithmetic, Functions\n");
    printf("=====================================\n");
    DEBUG_FUNCTION_EXIT();
}

static void show_command(void* args) {
    DEBUG_FUNCTION_ENTER();
    App *app = (App *)args;
    DEBUG_INSTR("Show command executed\n");
    symbol_table_show(app->parser->symTable);
    DEBUG_FUNCTION_EXIT();
}

// ##########################################
// ######       Instruction Map         #####
// ##########################################

static inline uint32_t string_hash(const char* str, size_t len) {
    DEBUG_FUNCTION_ENTER();
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= str[i];
        hash *= 16777619;
    }
    DEBUG_INSTR("Hash for '%.*s': %u\n", (int)len, str, hash);
    DEBUG_FUNCTION_EXIT();
    return hash;
}

static inline void instruction_map_add(InstructionMap *map, const char* inst, void (*func_name) (void* args)) {
    DEBUG_FUNCTION_ENTER();
    size_t len = strlen(inst);
    uint32_t hash = string_hash(inst, len) % INSTRUCTION_COUNT;
    
    DEBUG_INSTR_ADD(inst, func_name);
    DEBUG_INSTR("Instruction: %s, Hash: %u, Bucket: %u\n", inst, hash, hash % INSTRUCTION_COUNT);
    
    Instruction* curr = &map->inst_map[hash];
    
    // Handle collisions with chaining
    if (curr->str) {
        DEBUG_INSTR("Hash collision detected, adding to chain\n");
        while (curr->next != NULL) {
            curr = curr->next;
        }
        Instruction* newInst = malloc(sizeof(Instruction));
        CHECK_NULL(newInst, {
            ERROR_PRINT("Failed to allocate new instruction node\n");
            DEBUG_FUNCTION_EXIT();
            return;
        });
        curr->next = newInst;
        curr = newInst;
    }

    // Store function pointer and hash
    curr->func = func_name;
    curr->hash = hash;
    curr->next = NULL;
    
    // Store string in chunk
    if (map->chunk_offset + len + 1 > INSTRUCTION_COUNT_SIZE) {
        ERROR_PRINT("Instruction chunk overflow\n");
        DEBUG_FUNCTION_EXIT();
        return;
    }
    
    char* stored_str = map->chunk + map->chunk_offset;
    memcpy(stored_str, inst, len);
    stored_str[len] = '\0';
    map->chunk_offset += len + 1;
    
    curr->str = stored_str;
    
    DEBUG_INSTR("Instruction added successfully: %s -> %p\n", stored_str, (void*)func_name);
    DEBUG_FUNCTION_EXIT();
}

InstructionMap* instruction_map_creation() {
    DEBUG_FUNCTION_ENTER();
    
    InstructionMap* insMap = malloc(sizeof(InstructionMap));
    CHECK_NULL(insMap, ERROR_RETURN_NULL("Failed to allocate InstructionMap"));
    
    insMap->chunk = malloc(INSTRUCTION_COUNT_SIZE);
    CHECK_NULL(insMap->chunk, {
        free(insMap);
        ERROR_RETURN_NULL("Failed to allocate instruction chunk");
    });
    
    insMap->inst_map = calloc(INSTRUCTION_COUNT, sizeof(Instruction));
    CHECK_NULL(insMap->inst_map, {
        free(insMap->chunk);
        free(insMap);
        ERROR_RETURN_NULL("Failed to allocate instruction map");
    });
    
    insMap->chunk_offset = 0;
    
    // Add all instructions
    instruction_map_add(insMap, "-clear-vars", clear_vars_command);
    instruction_map_add(insMap, "-help", help_command);
    instruction_map_add(insMap, "-info", info_command);
    instruction_map_add(insMap, "-exit", exit_command);
    instruction_map_add(insMap, "-clear", clear_command);
    instruction_map_add(insMap, "-show", show_command);
    
    DEBUG_INSTR("Instruction map created with %d buckets\n", INSTRUCTION_COUNT);
    DEBUG_INSTR("Chunk usage: %d/%d bytes\n", insMap->chunk_offset, INSTRUCTION_COUNT_SIZE);
    
    DEBUG_FUNCTION_EXIT();
    return insMap;
}

void instruction_map_destroy(InstructionMap* map) {
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(map, return);
    
    DEBUG_INSTR("Destroying instruction map\n");
    
    free(map->chunk);
    
    int nodes_freed = 0;
    for (uint16_t i = 0; i < INSTRUCTION_COUNT; i++) {
        Instruction* curr = map->inst_map[i].next;
        Instruction* next = NULL;
        while (curr) {
            next = curr->next;
            free(curr);
            curr = next;
            nodes_freed++;
        }
    }
    
    free(map->inst_map);
    free(map);
    
    DEBUG_INSTR("Instruction map destroyed. Freed %d chain nodes\n", nodes_freed);
    DEBUG_FUNCTION_EXIT();
}

bool instruction_map_execute(InstructionMap* map, const char* inst, void *args) {
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(map, ERROR_RETURN(false, "Instruction map is NULL"));
    CHECK_NULL(inst, ERROR_RETURN(false, "Instruction string is NULL"));
    
    DEBUG_INSTR("Attempting to execute: %s\n", inst);
    
    char *token = strtok((char*)inst, " ");
    if (!token) {
        DEBUG_INSTR("No token found in instruction string\n");
        DEBUG_FUNCTION_EXIT();
        return false;
    }
    
    size_t len = strlen(token);
    uint32_t hash = string_hash(token, len) % INSTRUCTION_COUNT;
    
    DEBUG_INSTR("Looking up: %s (hash: %u, bucket: %u)\n", token, hash, hash % INSTRUCTION_COUNT);
    
    Instruction* curr = &map->inst_map[hash];
    int chain_position = 0;
    
    while (curr) {
        if (curr->str && strcmp(curr->str, token) == 0) {
            DEBUG_INSTR("Found instruction at bucket %u, chain position %d\n", hash, chain_position);
            DEBUG_INSTR_EXEC(token, true);
            
            curr->func(args);
            DEBUG_FUNCTION_EXIT();
            return true;
        }
        curr = curr->next;
        chain_position++;
    }
    
    DEBUG_INSTR("Instruction not found: %s\n", token);
    DEBUG_INSTR_EXEC(token, false);
    DEBUG_FUNCTION_EXIT();
    return false;
}

bool instruction_map_exist(InstructionMap* map, const char* inst) {
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(map, ERROR_RETURN(false, "Instruction map is NULL"));
    CHECK_NULL(inst, ERROR_RETURN(false, "Instruction string is NULL"));
    
    size_t len = strlen(inst);
    uint32_t hash = string_hash(inst, len) % INSTRUCTION_COUNT;
    
    DEBUG_INSTR("Checking existence: %s (hash: %u)\n", inst, hash);
    
    Instruction* curr = &map->inst_map[hash];
    int chain_position = 0;
    
    while (curr) {
        if (curr->str && strcmp(curr->str, inst) == 0) {
            DEBUG_INSTR("Instruction exists at bucket %u, chain position %d\n", hash, chain_position);
            DEBUG_FUNCTION_EXIT();
            return true;
        }
        curr = curr->next;
        chain_position++;
    }
    
    DEBUG_INSTR("Instruction does not exist: %s\n", inst);
    DEBUG_FUNCTION_EXIT();
    return false;
}

// ##########################################
// ######             Main              #####
// ##########################################

int main() {
    DEBUG_FUNCTION_ENTER();
    
    printf("This is a simple math interpreter of math equations. Here you can:\n"
           "1- Get the response of a math equation.\n"
           "2- Create variables with numeric values.\n\n");
    
    App *app = malloc(sizeof(App));
    CHECK_NULL(app, ERROR_RETURN(1, "Failed to allocate App"));
    
    InstructionMap *instructions = instruction_map_creation();
    CHECK_NULL(instructions, {
        free(app);
        ERROR_RETURN(1, "Failed to create instruction map");
    });
    
    TokenBuffer* token_buff = token_buffer_create();
    CHECK_NULL(token_buff, {
        instruction_map_destroy(instructions);
        free(app);
        ERROR_RETURN(1, "Failed to create token buffer");
    });
    
    app->parser = parser_create(token_buff);
    CHECK_NULL(app->parser, {
        token_buffer_destroy(token_buff);
        instruction_map_destroy(instructions);
        free(app);
        ERROR_RETURN(1, "Failed to create parser");
    });
    
    app->run = true;
    
    mpfr_init2(app->result, PRECISION_ROUNDING_BITS);
    
    DEBUG_INSTR("Application initialized successfully\n");
    DEBUG_INSTR("Precision: %d bits\n", PRECISION_ROUNDING_BITS);
    
    while (app->run) {
        printf(">> ");
        if (!fgets(app->buffer, sizeof(app->buffer), stdin)) {
            DEBUG_INSTR("EOF detected, exiting\n");
            break;
        }
        
        app->buffer[strcspn(app->buffer, "\n")] = '\0';
        if (app->buffer[0] == '\0') {
            DEBUG_INSTR("Empty input, skipping\n");
            continue;
        }
        
        DEBUG_INSTR("Processing input: %s\n", app->buffer);
        
        // Check for commands first
        if (app->buffer[0] == '-') {
            DEBUG_INSTR("Detected command prefix\n");
            if (instruction_map_execute(instructions, app->buffer, app)) {
                DEBUG_INSTR("Command executed successfully\n");
                continue;
            } else {
                DEBUG_INSTR("No matching command found\n");
            }
        }
        
        // Tokenize and parse mathematical expression
        if (!tokenize(token_buff, app->buffer)) {
            ERROR_PRINT("Tokenization failed for: %s\n", app->buffer);
            continue;
        }
        
        #ifdef DEBUG
            token_buffer_show(token_buff);
        #endif
        
        ASTNode* head = parse(app->parser);
        if (!head) {
            ERROR_PRINT("Parsing failed for: %s\n", app->buffer);
            continue;
        }
        
        #ifdef DEBUG
            parser_show(app->parser);
        #endif
        
        if (evaluate_expression(app->parser, head, &app->result)) {
            print_friendly_mpfr(app->result, "Result: ");
            symbol_table_insert(app->parser->symTable, "last", &app->result, 4);
        } else {
            ERROR_PRINT("Evaluation failed for: %s\n", app->buffer);
        }
    }
    
    DEBUG_INSTR("Shutting down application\n");
    
    parser_destroy(app->parser);
    instruction_map_destroy(instructions);
    mpfr_clear(app->result);
    free(app);
    
    DEBUG_INSTR("Application terminated successfully\n");
    DEBUG_FUNCTION_EXIT();
    return 0;
}