#ifndef PARSER_H
#define PARSER_H

#include "symbolTable.h"
#include <mpfr.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum TokenType{
    TOK_NUM,
    TOK_VAR,

    TOK_ASSING, // =
    TOK_ADD, // +
    TOK_SUB, // -
    TOK_DIVIDE, // /
    TOK_MODULE, // %
    TOK_MULT, // *
    TOK_POWER, // ^
    TOK_SQUARE, // √ (sqrt)
    // sin , cos , tan , etc

    TOK_LPAR, // (
    TOK_RPAR, // )
    TOK_COMM, // ,
    TOK_LCOR, // [
    TOK_RCOR, // ]
    
    TOK_INVALID
} TokenType;

typedef struct Token{
    const char* lexeme;
    TokenType type;
    uint8_t len;
    bool negative;
} Token;

#define TOKEN_BUFFER_SIZE 128
#define TOKEN_LEXEME_LEN_LIMIT 255

typedef struct TokenBuffer{
    Token* token_buff;
    uint16_t size,count;
} TokenBuffer;

TokenBuffer* token_buffer_create();
void token_buffer_destroy(TokenBuffer* tokBuff);
bool tokenize(TokenBuffer* tokBuff, const char* buff);
void token_buffer_show(TokenBuffer* tokBuff);

typedef struct ASTNode {
    Token *token;
    struct ASTNode* left;
    struct ASTNode* right;
} ASTNode;

#define MPRF_BUFFER_SIZE 128

typedef struct MprfBUffer{
    mpfr_t* buffer;
    uint16_t size,count;
} MpfrBuffer;

typedef struct Parser{
    TokenBuffer* tokens;
    ASTNode* nodesBuffer;
    MpfrBuffer* mpfrBuffer;
    SymbolTable* symTable;
    uint16_t curr_tok;
    uint16_t curr_node;
    uint16_t size;
} Parser;

Parser* parser_create(TokenBuffer* tokens);
void parser_destroy(Parser* parser);
void parser_show(Parser* parser);

ASTNode* parse(Parser* parser);
bool evaluate_expression(Parser* parser, ASTNode* head, mpfr_t* ans);

#endif

