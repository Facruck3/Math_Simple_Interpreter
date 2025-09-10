#include "parser.h"
#include "symbolTable.h"
#include "debug.h"
#include <assert.h>
#include <ctype.h>
#include <gmp.h>
#include <mpfr.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

// ##############################
// #####     TOKENIZER      ##### 
// ##############################

static bool lookupTable[256];
char bufferNames[TOKEN_LEXEME_LEN_LIMIT];
const char* TokenNamesConsts[20] = {
    "TOK_NUM", "TOK_VAR", "TOK_ASSING", "TOK_ADD", "TOK_SUB",
    "TOK_DIVIDE", "TOK_MODULE", "TOK_MULT", "TOK_POWER", "TOK_SQUARE",
    "TOK_LPAR", "TOK_RPAR", "TOK_COMM", "TOK_LCOR", "TOK_RCOR",
    "TOK_INVALID"
};

static inline bool token_buffer_add(TokenBuffer* buff, TokenType type, const char* start, uint8_t len) {
    DEBUG_FUNCTION_ENTER();
    
    if (buff->count >= buff->size) {
        size_t new_size = buff->size + TOKEN_BUFFER_SIZE;
        Token* new_buff = realloc(buff->token_buff, new_size * sizeof(Token));
        if (!new_buff) {
            ERROR_PRINT("Out of memory for tokens (requested: %zu bytes)\n", new_size * sizeof(Token));
            DEBUG_FUNCTION_EXIT();
            return false;
        }
        buff->token_buff = new_buff;
        buff->size = new_size;
        DEBUG_TOKENIZE("Token buffer resized: %d -> %zu\n", buff->size - TOKEN_BUFFER_SIZE, new_size);
    }
    
    Token* tok = &buff->token_buff[buff->count++];
    tok->lexeme = start;
    tok->type = type;
    tok->len = len;
    tok->negative = false;
    
    DEBUG_TOKEN(type, start, len);
    DEBUG_FUNCTION_EXIT();
    return true;
}

static inline const char* token_adjust_lexeme(Token* tok) {
    if (tok->type > TOK_VAR) return tok->lexeme;
    snprintf(bufferNames, tok->len + 1 + tok->negative, "%s%s", 
             tok->negative ? "-" : "", tok->lexeme);
    return bufferNames;
}

TokenBuffer* token_buffer_create() {
    DEBUG_FUNCTION_ENTER();
    
    TokenBuffer* buff = malloc(sizeof(TokenBuffer));
    CHECK_NULL(buff, ERROR_RETURN_NULL("Failed to allocate TokenBuffer"));
    
    buff->token_buff = calloc(TOKEN_BUFFER_SIZE, sizeof(Token));
    CHECK_NULL(buff->token_buff, {
        free(buff);
        ERROR_RETURN_NULL("Failed to allocate token buffer");
    });
    
    buff->size = TOKEN_BUFFER_SIZE;
    buff->count = 0;

    // Initialize lookup table
    for (uint8_t i = 'a'; i <= 'z'; i++) lookupTable[i] = true;
    for (uint8_t i = 'A'; i <= 'Z'; i++) lookupTable[i] = true;
    lookupTable['_'] = true;

    DEBUG_TOKENIZE("Token buffer created: size=%d\n", TOKEN_BUFFER_SIZE);
    DEBUG_FUNCTION_EXIT();
    return buff;
}

void token_buffer_destroy(TokenBuffer* tokBuff) {
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(tokBuff, return);
    
    DEBUG_TOKENIZE("Destroying token buffer: count=%d\n", tokBuff->count);
    free(tokBuff->token_buff);
    free(tokBuff);
    
    DEBUG_FUNCTION_EXIT();
}

void token_buffer_show(TokenBuffer* tokBuff) {
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(tokBuff, return);
    
    printf("Tokens (%d):\n", tokBuff->count);
    for (uint16_t i = 0; i < tokBuff->count; i++) {
        Token* currTok = &tokBuff->token_buff[i];
        printf("  %2d: %-12s [%s] (len: %d%s)\n", 
               i, TokenNamesConsts[currTok->type], 
               token_adjust_lexeme(currTok), currTok->len,
               currTok->negative ? ", negative" : "");
    }
    DEBUG_FUNCTION_EXIT();
}


bool tokenize(TokenBuffer* tokBuff, const char* buff) {
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(tokBuff, ERROR_RETURN(false, "TokenBuffer is NULL"));
    CHECK_NULL(buff, ERROR_RETURN(false, "Input buffer is NULL"));
    
    DEBUG_TOKENIZE("Tokenizing: '%s'\n", buff);
    
    tokBuff->count = 0;
    const char* p = buff;
    int token_count = 0;

    while (*p) {
        if (isspace(*p)) {
            p++; 
            continue;
        }

        if (isdigit(*p)) {
            const char* n = p++;
            bool isFloat = false;

            while (1) {
                if (isdigit(*p)) p++;
                else if (!isFloat && (*p == '.' || *p == ',')) {
                    isFloat = true; 
                    p++;
                } else break;
            }

            if (p - n > TOKEN_LEXEME_LEN_LIMIT) {
                ERROR_PRINT("Number too long: '%.*s...' (max %d)\n", 
                           TOKEN_LEXEME_LEN_LIMIT, n, TOKEN_LEXEME_LEN_LIMIT);
                DEBUG_FUNCTION_EXIT();
                return false;
            }
            
            if (!token_buffer_add(tokBuff, TOK_NUM, n, p - n)) {
                DEBUG_FUNCTION_EXIT();
                return false;
            }
            token_count++;
            continue;
        }

        if (isascii(*p) && lookupTable[*p]) {
            const char* n = p++;
            while (isascii(*p) && lookupTable[*p]) p++;

            if (p - n > TOKEN_LEXEME_LEN_LIMIT) {
                ERROR_PRINT("Identifier too long: '%.*s...' (max %d)\n", 
                           TOKEN_LEXEME_LEN_LIMIT, n, TOKEN_LEXEME_LEN_LIMIT);
                DEBUG_FUNCTION_EXIT();
                return false;
            }

            if (p - n == 4 && strncmp(n, "sqrt", 4) == 0) {
                if (!token_buffer_add(tokBuff, TOK_SQUARE, "sqrt", 4)) {
                    DEBUG_FUNCTION_EXIT();
                    return false;
                }
                token_count++;
                continue;
            }

            if (!token_buffer_add(tokBuff, TOK_VAR, n, p - n)) {
                DEBUG_FUNCTION_EXIT();
                return false;
            }
            token_count++;
            continue;
        }

        switch (*p) {
            case '=': 
                if (!token_buffer_add(tokBuff, TOK_ASSING, "=", 1)) {
                    DEBUG_FUNCTION_EXIT();
                    return false;
                }
                break;
            case '+': 
                if (!token_buffer_add(tokBuff, TOK_ADD, "+", 1)) {
                    DEBUG_FUNCTION_EXIT();
                    return false;
                }
                break;
            case '-': {
                if (tokBuff->count == 0 || 
                    (tokBuff->token_buff[tokBuff->count-1].type != TOK_NUM &&
                     tokBuff->token_buff[tokBuff->count-1].type != TOK_RPAR &&
                     tokBuff->token_buff[tokBuff->count-1].type != TOK_VAR)) {
                    p++;
                    while (*p == ' ') p++;
                    const char* n = p;
                    while (isdigit(*p)) p++;
                    if (!token_buffer_add(tokBuff, TOK_NUM, n, p - n)) {
                        DEBUG_FUNCTION_EXIT();
                        return false;
                    }
                    tokBuff->token_buff[tokBuff->count-1].negative = true;
                    DEBUG_TOKEN_NEGATIVE(TOK_NUM, n, (int) (p - n));
                    p--;
                } else {
                    if (!token_buffer_add(tokBuff, TOK_SUB, "-", 1)) {
                        DEBUG_FUNCTION_EXIT();
                        return false;
                    }
                }
                break;
            }
            case '/': 
                if (!token_buffer_add(tokBuff, TOK_DIVIDE, "/", 1)) {
                    DEBUG_FUNCTION_EXIT();
                    return false;
                }
                break;
            case '*': 
                if (!token_buffer_add(tokBuff, TOK_MULT, "*", 1)) {
                    DEBUG_FUNCTION_EXIT();
                    return false;
                }
                break;
            case '(': 
                if (!token_buffer_add(tokBuff, TOK_LPAR, "(", 1)) {
                    DEBUG_FUNCTION_EXIT();
                    return false;
                }
                break;
            case ')': 
                if (!token_buffer_add(tokBuff, TOK_RPAR, ")", 1)) {
                    DEBUG_FUNCTION_EXIT();
                    return false;
                }
                break;
            case '[': 
                if (!token_buffer_add(tokBuff, TOK_LCOR, "[", 1)) {
                    DEBUG_FUNCTION_EXIT();
                    return false;
                }
                break;
            case ']': 
                if (!token_buffer_add(tokBuff, TOK_RCOR, "]", 1)) {
                    DEBUG_FUNCTION_EXIT();
                    return false;
                }
                break;
            case '%': 
                if (!token_buffer_add(tokBuff, TOK_MODULE, "%", 1)) {
                    DEBUG_FUNCTION_EXIT();
                    return false;
                }
                break;
            case '^': 
                if (!token_buffer_add(tokBuff, TOK_POWER, "^", 1)) {
                    DEBUG_FUNCTION_EXIT();
                    return false;
                }
                break;
            case ',': 
                if (!token_buffer_add(tokBuff, TOK_COMM, ",", 1)) {
                    DEBUG_FUNCTION_EXIT();
                    return false;
                }
                break;
            default: {
                ERROR_PRINT("Unrecognized character: '%c' (0x%02x)\n", *p, *p);
                DEBUG_FUNCTION_EXIT();
                return false;
            }
        }
        token_count++;
        p++;
    }

    DEBUG_TOKENIZE("Tokenization completed: %d tokens\n", token_count);
    DEBUG_FUNCTION_EXIT();
    return true;
}

// ##############################
// #####       PARSER       ##### 
// ##############################

static inline Token* peek(Parser* parser) {
    if (parser->curr_tok >= parser->tokens->count) return NULL;
    return &parser->tokens->token_buff[parser->curr_tok];
}

static inline Token* peek_next(Parser* parser) {
    if (parser->curr_tok + 1 >= parser->tokens->count) return NULL;
    return &parser->tokens->token_buff[parser->curr_tok + 1];
}

static inline Token* consume(Parser* parser) {
    if (parser->curr_tok >= parser->tokens->count) return NULL;
    Token* token = &parser->tokens->token_buff[parser->curr_tok++];
    parser->nodesBuffer[parser->curr_node].token = token;
    DEBUG_PARSE("Consumed: %s [%s]\n", 
               TokenNamesConsts[token->type], token_adjust_lexeme(token));
    return token;
}

static inline bool match(Parser* parser, TokenType type) {
    Token* token = peek(parser);
    return token && token->type == type;
}

static inline bool expect(Parser* parser, TokenType type) {
    Token* token = peek(parser);
    if (token && token->type == type) return true;
    
    ERROR_PRINT("Expected %s, got %s [%s]\n", 
               TokenNamesConsts[type],
               token ? TokenNamesConsts[token->type] : "EOF",
               token ? token_adjust_lexeme(token) : "EOF");
    DEBUG_EXPECT(type, token ? token->type : TOK_INVALID);
    return false;
}

static inline ASTNode* create_leaf_node(Parser* p, Token* token) {
    if (p->curr_node >= p->size) {
        ERROR_PRINT("AST node buffer overflow (size: %d)\n", p->size);
        return NULL;
    }
    
    ASTNode* node = &p->nodesBuffer[p->curr_node++];
    node->token = token;
    node->left = node->right = NULL;
    
    DEBUG_AST_NODE(node, "leaf");
    return node;
}

static inline ASTNode* create_binary_node(Parser* p, Token* op, ASTNode* left, ASTNode* right) {
    if (p->curr_node >= p->size) {
        ERROR_PRINT("AST node buffer overflow (size: %d)\n", p->size);
        return NULL;
    }
    
    ASTNode* node = &p->nodesBuffer[p->curr_node++];
    node->token = op;
    node->left = left;
    node->right = right;
    
    DEBUG_AST_NODE(node, "binary");
    DEBUG_PARSE("  Left: %s, Right: %s\n",
               left ? TokenNamesConsts[left->token->type] : "NULL",
               right ? TokenNamesConsts[right->token->type] : "NULL");
    return node;
}


static ASTNode* parse_statement(Parser* p);     // Level 0
static ASTNode* parse_assignment(Parser* p);    // Level 1
static ASTNode* parse_expression(Parser* p);    // Level 2
static ASTNode* parse_term(Parser* p);          // Level 3
static ASTNode* parse_power(Parser* p);         // Level 4
static ASTNode* parse_primary(Parser* p);       // Level 5

static ASTNode* parse_primary(Parser* p) {
    DEBUG_PARSE_LEVEL(5, "Entering parse_primary()\n");
    Token* curr = peek(p);
    CHECK_NULL(curr, ERROR_RETURN_NULL("Unexpected EOF in primary expression"));
    
    switch (curr->type) {
        case TOK_NUM: {
            ASTNode* node = create_leaf_node(p, consume(p));
            DEBUG_PARSE_LEVEL(5, "Number literal: %s\n", token_adjust_lexeme(curr));
            DEBUG_FUNCTION_EXIT();
            return node;
        }
        case TOK_VAR: {
            ASTNode* node = create_leaf_node(p, consume(p));
            DEBUG_PARSE_LEVEL(5, "Variable: %s\n", token_adjust_lexeme(curr));
            DEBUG_FUNCTION_EXIT();
            return node;
        }
        case TOK_LPAR: {
            DEBUG_PARSE_LEVEL(5, "Parenthesized expression\n");
            consume(p); // Consume '('
            ASTNode* expr = parse_expression(p);
            if (!expr) {
                ERROR_RETURN_NULL("Failed to parse expression inside parentheses");
            }
            if (!expect(p, TOK_RPAR)) {
                ERROR_RETURN_NULL("Missing closing parenthesis");
            }
            consume(p); // Consume ')'
            DEBUG_PARSE_LEVEL(5, "Parenthesized expression completed\n");
            DEBUG_FUNCTION_EXIT();
            return expr;
        }
        case TOK_SQUARE: {
            DEBUG_PARSE_LEVEL(5, "Square root function\n");
            ASTNode* sqrt = create_leaf_node(p, consume(p));
            if (!expect(p, TOK_LPAR)) {
                ERROR_RETURN_NULL("Expected '(' after sqrt");
            }
            consume(p);
            sqrt->left = parse_expression(p);
            if (!sqrt->left) {
                ERROR_RETURN_NULL("Invalid sqrt argument");
            }
            if (!expect(p, TOK_RPAR)) {
                ERROR_RETURN_NULL("Missing closing parenthesis for sqrt");
            }
            consume(p);
            DEBUG_PARSE_LEVEL(5, "Square root function completed\n");
            DEBUG_FUNCTION_EXIT();
            return sqrt;
        }
        default: {
            ERROR_PRINT("Unexpected token in primary expression: %s [%s]\n",
                       TokenNamesConsts[curr->type], token_adjust_lexeme(curr));
            DEBUG_FUNCTION_EXIT();
            return NULL;
        }
    }
}

static ASTNode* parse_power(Parser* p) {
    DEBUG_PARSE_LEVEL(4, "Entering parse_power()\n");
    ASTNode* left = parse_primary(p);
    CHECK_NULL(left, ERROR_RETURN_NULL("Failed to parse left operand for power"));
    
    if (match(p, TOK_POWER)) {
        DEBUG_PARSE_LEVEL(4, "Power operator found\n");
        Token* op = consume(p);
        ASTNode* right = parse_power(p);
        CHECK_NULL(right, ERROR_RETURN_NULL("Failed to parse right operand for power"));
        
        ASTNode* node = create_binary_node(p, op, left, right);
        DEBUG_PARSE_LEVEL(4, "Power expression completed\n");
        DEBUG_FUNCTION_EXIT();
        return node;
    }
    
    DEBUG_PARSE_LEVEL(4, "No power operator, returning primary\n");
    DEBUG_FUNCTION_EXIT();
    return left;
}

static ASTNode* parse_term(Parser* p) {
    DEBUG_PARSE_LEVEL(3, "Entering parse_term()\n");
    ASTNode* left = parse_power(p);
    CHECK_NULL(left, ERROR_RETURN_NULL("Failed to parse left operand for term"));
    
    while (match(p, TOK_MULT) || match(p, TOK_DIVIDE) || match(p, TOK_MODULE)) {
        TokenType op_type = peek(p)->type;
        DEBUG_PARSE_LEVEL(3, "Operator found: %s\n", TokenNamesConsts[op_type]);
        
        Token* op = consume(p);
        ASTNode* right = parse_power(p);
        CHECK_NULL(right, {
            ERROR_PRINT("Failed to parse right operand for %s\n", TokenNamesConsts[op_type]);
            return NULL;
        });
        
        left = create_binary_node(p, op, left, right);
        DEBUG_PARSE_LEVEL(3, "Updated term with %s operation\n", TokenNamesConsts[op_type]);
    }
    
    DEBUG_PARSE_LEVEL(3, "Term parsing completed\n");
    DEBUG_FUNCTION_EXIT();
    return left;
}

static ASTNode* parse_expression(Parser* p) {
    DEBUG_PARSE_LEVEL(2, "Entering parse_expression()\n");
    ASTNode* left = parse_term(p);
    CHECK_NULL(left, ERROR_RETURN_NULL("Failed to parse left operand for expression"));
    
    while (match(p, TOK_ADD) || match(p, TOK_SUB)) {
        TokenType op_type = peek(p)->type;
        DEBUG_PARSE_LEVEL(2, "Operator found: %s\n", TokenNamesConsts[op_type]);
        
        Token* op = consume(p);
        ASTNode* right = parse_term(p);
        CHECK_NULL(right, {
            ERROR_PRINT("Failed to parse right operand for %s\n", TokenNamesConsts[op_type]);
            return NULL;
        });
        
        left = create_binary_node(p, op, left, right);
        DEBUG_PARSE_LEVEL(2, "Updated expression with %s operation\n", TokenNamesConsts[op_type]);
    }
    
    DEBUG_PARSE_LEVEL(2, "Expression parsing completed\n");
    DEBUG_FUNCTION_EXIT();
    return left;
}

static ASTNode* parse_assignment(Parser* p) {
    DEBUG_PARSE_LEVEL(1, "Entering parse_assignment()\n");
    
    // Solo variables pueden ser asignadas
    if (peek(p)->type != TOK_VAR) {
        DEBUG_PARSE_LEVEL(1, "Not a variable, falling back to expression\n");
        return parse_expression(p);
    }
    
    ASTNode* left = create_leaf_node(p, consume(p));
    CHECK_NULL(left, ERROR_RETURN_NULL("Failed to parse variable for assignment"));
    
    if (match(p, TOK_ASSING)) {
        DEBUG_PARSE_LEVEL(1, "Assignment operator found\n");
        Token* op = consume(p);
        ASTNode* right = parse_expression(p);
        CHECK_NULL(right, ERROR_RETURN_NULL("Failed to parse right side of assignment"));
        
        ASTNode* node = create_binary_node(p, op, left, right);
        DEBUG_PARSE_LEVEL(1, "Assignment parsing completed\n");
        DEBUG_FUNCTION_EXIT();
        return node;
    }
    
    DEBUG_PARSE_LEVEL(1, "No assignment operator, returning variable\n");
    DEBUG_FUNCTION_EXIT();
    return left;
}

static ASTNode* parse_statement(Parser* p) {
    DEBUG_PARSE_LEVEL(0, "Entering parse_statement()\n");
    
    // Verifica si es asignación
    if (peek(p) && peek(p)->type == TOK_VAR && peek_next(p) && peek_next(p)->type == TOK_ASSING) {
        DEBUG_PARSE_LEVEL(0, "Assignment statement detected\n");
        ASTNode* var = create_leaf_node(p, consume(p));
        CHECK_NULL(var, ERROR_RETURN_NULL("Failed to parse variable in assignment"));
        
        Token* op = consume(p);
        ASTNode* right = parse_expression(p);
        CHECK_NULL(right, ERROR_RETURN_NULL("Failed to parse assignment value"));
        
        ASTNode* node = create_binary_node(p, op, var, right);
        DEBUG_PARSE_LEVEL(0, "Assignment statement completed\n");
        DEBUG_FUNCTION_EXIT();
        return node;
    } else {
        DEBUG_PARSE_LEVEL(0, "Expression statement detected\n");
        ASTNode* expr = parse_expression(p);
        DEBUG_PARSE_LEVEL(0, "Expression statement completed\n");
        DEBUG_FUNCTION_EXIT();
        return expr;
    }
}

ASTNode* parse(Parser* parser) {
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(parser, ERROR_RETURN_NULL("Parser is NULL"));
    CHECK_NULL(parser->tokens, ERROR_RETURN_NULL("Token buffer is NULL"));
    
    parser->curr_node = 0;
    parser->curr_tok = 0;
    
    // Ensure enough space for AST nodes
    if (parser->size < parser->tokens->count + 1) {
        size_t new_size = parser->tokens->size;
        ASTNode* newNodes = realloc(parser->nodesBuffer, new_size * sizeof(ASTNode));
        CHECK_NULL(newNodes, ERROR_RETURN_NULL("Failed to reallocate AST node buffer"));
        parser->nodesBuffer = newNodes;
        parser->size = new_size;
        DEBUG_PARSE("AST node buffer resized to %zu\n", new_size);
    }
    
    ASTNode* ans = parse_statement(parser);
    
    DEBUG_PARSE("Parsing completed - Tokens consumed: %d/%d, Nodes created: %d\n",
               parser->curr_tok, parser->tokens->count, parser->curr_node);
    
    if (ans) {
        DEBUG_PARSE("Root node: %s [%s]\n",
                   TokenNamesConsts[ans->token->type],
                   token_adjust_lexeme(ans->token));
    } else {
        ERROR_PRINT("Failed to parse statement\n");
    }
    
    DEBUG_FUNCTION_EXIT();
    return ans;
}

static mpfr_t* evaluate_node(Parser* p, ASTNode* node) {
    DEBUG_EVAL("Entering evaluate_node(): %s [%s]\n",
              TokenNamesConsts[node->token->type],
              token_adjust_lexeme(node->token));
    
    if (p->mpfrBuffer->count >= p->mpfrBuffer->size) {
        size_t new_size = p->mpfrBuffer->size * 2;
        mpfr_t* new_buff = realloc(p->mpfrBuffer->buffer, new_size * sizeof(mpfr_t));
        CHECK_NULL(new_buff, ERROR_RETURN_NULL("Failed to reallocate MPFR buffer"));
        
        // Initialize new MPFR variables
        for (uint16_t i = p->mpfrBuffer->size; i < new_size; i++) {
            mpfr_init2(new_buff[i], PRECISION_ROUNDING_BITS);
        }
        
        p->mpfrBuffer->buffer = new_buff;
        p->mpfrBuffer->size = new_size;
        DEBUG_EVAL("MPFR buffer resized to %zu\n", new_size);
    }
    
    mpfr_t* result = &p->mpfrBuffer->buffer[p->mpfrBuffer->count++];
    mpfr_set_d(*result, 0, MPFR_RNDN);

    switch (node->token->type) {
        case TOK_NUM: {
            int status = mpfr_set_str(*result, token_adjust_lexeme(node->token), 10, MPFR_RNDN);
            CHECK_CONDITION(status == 0, {
                ERROR_PRINT("Failed to convert number: %s\n", token_adjust_lexeme(node->token));
                mpfr_set_nan(*result);
            }, "MPFR set_str failed");
            break;
        }
        case TOK_VAR: {
            mpfr_t* var_value = symbol_table_get(p->symTable,
                token_adjust_lexeme(node->token), node->token->len);
            if (var_value) {
                mpfr_set(*result, *var_value, MPFR_RNDN);
            } else {
                ERROR_PRINT("Undefined variable: '%s'\n", token_adjust_lexeme(node->token));
                mpfr_set_nan(*result);
            }
            break;
        }
        case TOK_ADD: {
            mpfr_t* left_val = evaluate_node(p, node->left);
            mpfr_t* right_val = evaluate_node(p, node->right);
            CHECK_NULL(left_val, ERROR_RETURN_NULL("Left operand evaluation failed"));
            CHECK_NULL(right_val, ERROR_RETURN_NULL("Right operand evaluation failed"));
            mpfr_add(*result, *left_val, *right_val, MPFR_RNDN);
            break;
        }
        case TOK_SUB: {
            mpfr_t* left_val = evaluate_node(p, node->left);
            mpfr_t* right_val = evaluate_node(p, node->right);
            CHECK_NULL(left_val, ERROR_RETURN_NULL("Left operand evaluation failed"));
            CHECK_NULL(right_val, ERROR_RETURN_NULL("Right operand evaluation failed"));
            mpfr_sub(*result, *left_val, *right_val, MPFR_RNDN);
            break;
        }
        case TOK_DIVIDE: {
            mpfr_t* left_val = evaluate_node(p, node->left);
            mpfr_t* right_val = evaluate_node(p, node->right);
            CHECK_NULL(left_val, ERROR_RETURN_NULL("Left operand evaluation failed"));
            CHECK_NULL(right_val, ERROR_RETURN_NULL("Right operand evaluation failed"));
            
            if (mpfr_zero_p(*right_val)) {
                ERROR_PRINT("Division by zero\n");
                mpfr_set_nan(*result);
            } else {
                mpfr_div(*result, *left_val, *right_val, MPFR_RNDN);
            }
            break;
        }
        case TOK_MODULE: {
            mpfr_t* left_val = evaluate_node(p, node->left);
            mpfr_t* right_val = evaluate_node(p, node->right);
            CHECK_NULL(left_val, ERROR_RETURN_NULL("Left operand evaluation failed"));
            CHECK_NULL(right_val, ERROR_RETURN_NULL("Right operand evaluation failed"));
            
            if (mpfr_zero_p(*right_val)) {
                ERROR_PRINT("Modulo by zero\n");
                mpfr_set_nan(*result);
            } else {
                mpfr_modf(*result, *left_val, *right_val, MPFR_RNDN);
            }
            break;
        }
        case TOK_MULT: {
            mpfr_t* left_val = evaluate_node(p, node->left);
            mpfr_t* right_val = evaluate_node(p, node->right);
            CHECK_NULL(left_val, ERROR_RETURN_NULL("Left operand evaluation failed"));
            CHECK_NULL(right_val, ERROR_RETURN_NULL("Right operand evaluation failed"));
            mpfr_mul(*result, *left_val, *right_val, MPFR_RNDN);
            break;
        }
        case TOK_POWER: {
            mpfr_t* left_val = evaluate_node(p, node->left);
            mpfr_t* right_val = evaluate_node(p, node->right);
            CHECK_NULL(left_val, ERROR_RETURN_NULL("Left operand evaluation failed"));
            CHECK_NULL(right_val, ERROR_RETURN_NULL("Right operand evaluation failed"));
            mpfr_pow(*result, *left_val, *right_val, MPFR_RNDN);
            break;
        }
        case TOK_SQUARE: {
            mpfr_t* arg = evaluate_node(p, node->left);
            CHECK_NULL(arg, ERROR_RETURN_NULL("Square root argument evaluation failed"));
            
            if (mpfr_cmp_d(*arg, 0) < 0) {
                ERROR_PRINT("Square root of negative number\n");
                mpfr_set_nan(*result);
            } else {
                mpfr_sqrt(*result, *arg, MPFR_RNDN);
            }
            break;
        }
        case TOK_ASSING: {
            mpfr_t* right_val = evaluate_node(p, node->right);
            CHECK_NULL(right_val, ERROR_RETURN_NULL("Assignment value evaluation failed"));
            
            mpfr_t* stored = symbol_table_insert(p->symTable,
                token_adjust_lexeme(node->left->token),
                right_val,
                node->left->token->len);
            CHECK_NULL(stored, ERROR_RETURN_NULL("Failed to store variable in symbol table"));
            
            mpfr_set(*result, *right_val, MPFR_RNDN);
            DEBUG_EVAL("Assignment: %s = ", token_adjust_lexeme(node->left->token));
            DEBUG_MPFR_VALUE(*result, "");
            break;
        }
        default: {
            ERROR_PRINT("Unsupported token type in evaluation: %s\n",
                       TokenNamesConsts[node->token->type]);
            DEBUG_FUNCTION_EXIT();
            return NULL;
        }
    }
    
    DEBUG_EVAL_NODE(node, result);
    DEBUG_FUNCTION_EXIT();
    return result;
}

bool evaluate_expression(Parser* parser, ASTNode* head, mpfr_t* ans) {
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(parser, ERROR_RETURN(false, "Parser is NULL"));
    CHECK_NULL(head, ERROR_RETURN(false, "AST head is NULL"));
    CHECK_NULL(ans, ERROR_RETURN(false, "Result pointer is NULL"));
    
    DEBUG_EVAL("Starting expression evaluation\n");
    parser->mpfrBuffer->count = 0;
    
    mpfr_t* result = evaluate_node(parser, head);
    if (!result) {
        ERROR_PRINT("Expression evaluation failed\n");
        DEBUG_FUNCTION_EXIT();
        return false;
    }
    
    mpfr_set(*ans, *result, MPFR_RNDN);
    DEBUG_EVAL("Evaluation completed successfully\n");
    DEBUG_MPFR_VALUE(*ans, "Final result");
    DEBUG_FUNCTION_EXIT();
    return true;
}

Parser* parser_create(TokenBuffer* tokens) {
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(tokens, ERROR_RETURN_NULL("Token buffer is NULL"));
    
    Parser* parser = malloc(sizeof(Parser));
    CHECK_NULL(parser, ERROR_RETURN_NULL("Failed to allocate Parser"));
    
    parser->size = tokens->size;
    parser->nodesBuffer = calloc(tokens->size, sizeof(ASTNode));
    CHECK_NULL(parser->nodesBuffer, {
        free(parser);
        ERROR_RETURN_NULL("Failed to allocate AST node buffer");
    });
    
    parser->curr_node = 0;
    parser->curr_tok = 0;
    parser->tokens = tokens;
    
    parser->symTable = symbol_table_create();
    CHECK_NULL(parser->symTable, {
        free(parser->nodesBuffer);
        free(parser);
        ERROR_RETURN_NULL("Failed to create symbol table");
    });
    
    parser->mpfrBuffer = malloc(sizeof(MpfrBuffer));
    CHECK_NULL(parser->mpfrBuffer, {
        symbol_table_destroy(parser->symTable);
        free(parser->nodesBuffer);
        free(parser);
        ERROR_RETURN_NULL("Failed to allocate MPFR buffer");
    });
    
    parser->mpfrBuffer->buffer = calloc(MPRF_BUFFER_SIZE, sizeof(mpfr_t));
    CHECK_NULL(parser->mpfrBuffer->buffer, {
        free(parser->mpfrBuffer);
        symbol_table_destroy(parser->symTable);
        free(parser->nodesBuffer);
        free(parser);
        ERROR_RETURN_NULL("Failed to allocate MPFR variables");
    });
    
    parser->mpfrBuffer->size = MPRF_BUFFER_SIZE;
    parser->mpfrBuffer->count = 0;
    
    for (uint16_t i = 0; i < MPRF_BUFFER_SIZE; i++) {
        mpfr_init2(parser->mpfrBuffer->buffer[i], PRECISION_ROUNDING_BITS);
    }
    
    DEBUG_PARSE("Parser created successfully\n");
    DEBUG_FUNCTION_EXIT();
    return parser;
}

void parser_destroy(Parser* parser) {
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(parser, return);
    
    DEBUG_PARSE("Destroying parser\n");
    
    free(parser->nodesBuffer);
    
    if (parser->tokens) {
        token_buffer_destroy(parser->tokens);
    }
    
    if (parser->symTable) {
        symbol_table_destroy(parser->symTable);
    }
    
    if (parser->mpfrBuffer) {
        if (parser->mpfrBuffer->buffer) {
            for (uint16_t i = 0; i < parser->mpfrBuffer->size; i++) {
                mpfr_clear(parser->mpfrBuffer->buffer[i]);
            }
            free(parser->mpfrBuffer->buffer);
        }
        free(parser->mpfrBuffer);
    }
    
    mpfr_free_cache();
    free(parser);
    
    DEBUG_PARSE("Parser destroyed\n");
    DEBUG_FUNCTION_EXIT();
}

void parser_show(Parser* parser) {
    DEBUG_FUNCTION_ENTER();
    CHECK_NULL(parser, return);
    
    if (parser->tokens == NULL || parser->tokens->count == 0) {
        printf("Empty AST or invalid parser\n");
        DEBUG_FUNCTION_EXIT();
        return;
    }
    
    printf("\n🌳 Abstract Syntax Tree:\n");
    printf("========================================\n");
    for (uint16_t i = 0; i < parser->curr_node; i++) {
        ASTNode* curr = &parser->nodesBuffer[i];
        char currBuff[256];
        snprintf(currBuff, sizeof(currBuff), "%.*s",
                curr->token->len, curr->token->lexeme);
        
        printf("Node %3d: %-12s [%-15s] left:%-3d right:%-3d\n",
               i,
               TokenNamesConsts[curr->token->type],
               currBuff,
               curr->left ? (int)(curr->left - &parser->nodesBuffer[0]) : -1,
               curr->right ? (int)(curr->right - &parser->nodesBuffer[0]) : -1);
    }
    printf("========================================\n");
    DEBUG_FUNCTION_EXIT();
}