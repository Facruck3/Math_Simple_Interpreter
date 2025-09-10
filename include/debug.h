#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <time.h>

#define ERROR_LOGGING

#define WARNING_LOGGING

// ==================== MACROS GLOBAL ====================

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) do { \
    time_t now = time(NULL); \
    char timestr[20]; \
    strftime(timestr, sizeof(timestr), "%H:%M:%S", localtime(&now)); \
    printf("\033[36m[DEBUG] [%s] [%s:%d]:\033[0m " fmt, \
           timestr, __FILE__, __LINE__, ##__VA_ARGS__); \
} while(0)

#define DEBUG_FUNCTION_ENTER() DEBUG_PRINT("→ Entering %s()\n", __func__)
#define DEBUG_FUNCTION_EXIT() DEBUG_PRINT("← Exiting %s()\n", __func__)

#define DEBUG_SHOW_MEMORY(ptr, size) do { \
    DEBUG_PRINT("Memory at %p: ", ptr); \
    for(size_t i = 0; i < (size < 16 ? size : 16); i++) { \
        printf("%02x ", ((unsigned char*)ptr)[i]); \
    } \
    printf("\n"); \
} while(0)

#else
#define DEBUG_PRINT(fmt, ...)
#define DEBUG_FUNCTION_ENTER()
#define DEBUG_FUNCTION_EXIT()
#define DEBUG_SHOW_MEMORY(ptr, size)
#endif

// ==================== MACROS ERROR ====================

#ifdef ERROR_LOGGING
#define ERROR_PRINT(fmt, ...) do { \
    time_t now = time(NULL); \
    char timestr[20]; \
    strftime(timestr, sizeof(timestr), "%H:%M:%S", localtime(&now)); \
    fprintf(stderr, "\033[31m[ERROR] [%s] [%s:%d in %s]:\033[0m " fmt, \
            timestr, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
} while(0)

#define ERROR_RETURN(code, fmt, ...) do { \
    ERROR_PRINT(fmt, ##__VA_ARGS__); \
    return code; \
} while(0)

#define ERROR_RETURN_NULL(fmt, ...) do { \
    ERROR_PRINT(fmt, ##__VA_ARGS__); \
    return NULL; \
} while(0)

#else
#define ERROR_PRINT(fmt, ...)
#define ERROR_RETURN(code, fmt, ...) return code
#define ERROR_RETURN_NULL(fmt, ...) return NULL
#endif

// ==================== MACROS WARNING ====================

#ifdef WARNING_LOGGING
#define WARNING_PRINT(fmt, ...) do { \
    time_t now = time(NULL); \
    char timestr[20]; \
    strftime(timestr, sizeof(timestr), "%H:%M:%S", localtime(&now)); \
    printf("\033[33m[WARNING] [%s] [%s:%d]:\033[0m " fmt, \
           timestr, __FILE__, __LINE__, ##__VA_ARGS__); \
} while(0)
#else
#define WARNING_PRINT(fmt, ...)
#endif

// ==================== MACROS VERIFICATION ====================

#define CHECK_NULL(ptr, action) do { \
    if ((ptr) == NULL) { \
        ERROR_PRINT("Null pointer: %s\n", #ptr); \
        action; \
    } \
} while(0)

#define CHECK_CONDITION(cond, action, fmt, ...) do { \
    if (!(cond)) { \
        ERROR_PRINT("Condition failed: %s - " fmt, #cond, ##__VA_ARGS__); \
        action; \
    } \
} while(0)

// ==================== MACROS MPFR ====================

#ifdef DEBUG
#define DEBUG_MPFR_VALUE(var, name) do { \
    if (mpfr_nan_p(var)) { \
        DEBUG_PRINT("%s = NaN\n", name); \
    } else if (mpfr_inf_p(var)) { \
        DEBUG_PRINT("%s = %sInfinity\n", name, mpfr_signbit(var) ? "-" : "+"); \
    } else { \
        char buffer[100]; \
        mpfr_sprintf(buffer, "%.10Rf", var); \
        DEBUG_PRINT("%s = %s\n", name, buffer); \
    } \
} while(0)
#else
#define DEBUG_MPFR_VALUE(var, name)
#endif

// ==================== MACROS SYMBOL TABLE ====================

#ifdef DEBUG
#define DEBUG_SYMBOL_OP(op, name, value) do { \
    char val_str[100]; \
    mpfr_sprintf(val_str, "%.10Rf", value); \
    DEBUG_PRINT("Symbol %s: %s = %s\n", op, name, val_str); \
} while(0)

#define DEBUG_HASH(name, len, hash, index) \
    DEBUG_PRINT("Hash: '%.*s' -> hash=%u, index=%u\n", len, name, hash, index)
#else
#define DEBUG_SYMBOL_OP(op, name, value)
#define DEBUG_HASH(name, len, hash, index)
#endif

// ==================== MACROS TOKENIZER ====================
#ifdef DEBUG
#define DEBUG_TOKENIZE(fmt, ...) DEBUG_PRINT("[TOKENIZER] " fmt, ##__VA_ARGS__)
#define DEBUG_TOKEN(type, lexeme, len) \
    DEBUG_TOKENIZE("Token: %s [%.*s] (len: %d)\n", \
                  TokenNamesConsts[type], len, lexeme, len)
#define DEBUG_TOKEN_NEGATIVE(type, lexeme, len) \
    DEBUG_TOKENIZE("Token: %s [-%.*s] (negative, len: %d)\n", \
                  TokenNamesConsts[type], len, lexeme, len)
#else
#define DEBUG_TOKENIZE(fmt, ...)
#define DEBUG_TOKEN(type, lexeme, len)
#define DEBUG_TOKEN_NEGATIVE(type, lexeme, len)
#endif

// ==================== MACROS PARSER ====================
#ifdef DEBUG
#define DEBUG_PARSE(fmt, ...) DEBUG_PRINT("[PARSER] " fmt, ##__VA_ARGS__)
#define DEBUG_PARSE_LEVEL(level, fmt, ...) \
    DEBUG_PARSE("[Level %d] " fmt, level, ##__VA_ARGS__)
#define DEBUG_AST_NODE(node, operation) \
    DEBUG_PARSE("AST %s: %s [%s]\n", operation, \
               TokenNamesConsts[node->token->type], \
               token_adjust_lexeme(node->token))
#define DEBUG_EXPECT(expected, actual) \
    DEBUG_PARSE("Expect: %s, Got: %s\n", \
               TokenNamesConsts[expected], \
               TokenNamesConsts[actual])
#else
#define DEBUG_PARSE(fmt, ...)
#define DEBUG_PARSE_LEVEL(level, fmt, ...)
#define DEBUG_AST_NODE(node, operation)
#define DEBUG_EXPECT(expected, actual)
#endif

// ==================== MACROS EVALUATION ====================
#ifdef DEBUG
#define DEBUG_EVAL(fmt, ...) DEBUG_PRINT("[EVAL] " fmt, ##__VA_ARGS__)
#define DEBUG_EVAL_NODE(node, result) \
    DEBUG_EVAL("Node %s [%s] = ", \
              TokenNamesConsts[node->token->type], \
              token_adjust_lexeme(node->token)); \
    DEBUG_MPFR_VALUE(*result, "")
#else
#define DEBUG_EVAL(fmt, ...)
#define DEBUG_EVAL_NODE(node, result)
#endif

// ==================== MACROS INSTRUCCION ====================
#ifdef DEBUG
#define DEBUG_INSTR(fmt, ...) DEBUG_PRINT("[INSTR] " fmt, ##__VA_ARGS__)
#define DEBUG_INSTR_ADD(name, func) \
    DEBUG_INSTR("Added instruction: %s -> %p\n", name, (void*)func)
#define DEBUG_INSTR_EXEC(name, success) \
    DEBUG_INSTR("Execute: %s -> %s\n", name, success ? "SUCCESS" : "NOT_FOUND")
#else
#define DEBUG_INSTR(fmt, ...)
#define DEBUG_INSTR_ADD(name, func)
#define DEBUG_INSTR_EXEC(name, success)
#endif

#endif // DEBUG_H