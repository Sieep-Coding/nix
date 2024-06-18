#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <math.h>

#define STACK_CAP 256
#define STACKFRAME_CAP 512
#define PROGRAM_CAP 2048
#define MAX_WORD_SIZE 256
#define MAX_STRING_LEN 1024
#define MAX_TABLE_ROWS 1024
#define MAX_TABLE_COLS 64

#define PANIC_ON_ERR(cond, err_type, ...)  {                                            \
    if(cond) {                                                                          \
         printf("Nix: Error: %s | Error Code: %i\n", #err_type, (int32_t)err_type);     \
         printf(__VA_ARGS__);                                                           \
         printf("\n");                                                                  \
    }                                                                                   \
}                                                                                       \

typedef enum {
    INST_STACK_PUSH, INST_STACK_PREV, INST_STACK_POP,
    INST_PLUS, INST_MINUS, INST_MUL, INST_DIV, INST_MOD,
    INST_EQ, INST_NEQ,
    INST_GT, INST_LT, INST_GEQ, INST_LEQ,
    INST_LOGICAL_AND, INST_LOGICAL_OR,
    INST_IF, INST_ELSE, INST_ELIF, INST_THEN, INST_ENDIF,
    INST_WHILE, INST_RUN_WHILE, INST_END_WHILE,
    INST_PRINT, INST_PRINTLN,
    INST_JUMP,
    INST_ADD_VAR_TO_STACKFRAME, INST_ASSIGN, INST_VAR_USAGE, INST_VAR_REASSIGN,
    INST_HEAP_ALLOC, INST_HEAP_FREE, INST_PTR_GET_I, INST_PTR_SET_I,
    INST_INT_TYPE, INST_FLOAT_TYPE, INST_DOUBLE_TYPE, INST_CHAR_TYPE, INST_STR_TYPE,
    INST_MACRO, INST_MACRO_DEF, INST_END_MACRO, INST_MACRO_USAGE,
    INST_FUNC_DEF, INST_FUNC_CALL, INST_FUNC_RET,
    INST_STRUCT_DEF, INST_STRUCT_INIT, INST_STRUCT_ACCESS,
    INST_TABLE_CREATE, INST_TABLE_INSERT, INST_TABLE_SELECT, INST_TABLE_UPDATE, INST_TABLE_DELETE,
    INST_SQL_QUERY,
    INST_CONCAT_STR,
    INST_CHART_PLOT,
    INST_EXPORT_DATA,
    INST_STAT_MEAN, INST_STAT_MEDIAN, INST_STAT_MODE, INST_STAT_STD_DEV,
    INST_REGRESSION, INST_CLUSTER, INST_TIME_SERIES,
    INST_API_REQUEST,
    INST_DB_CONNECT, INST_DB_QUERY,
    INST_ETL_EXTRACT, INST_ETL_TRANSFORM, INST_ETL_LOAD,
    INST_DATA_VALIDATE,
    INST_SCRIPT_EXECUTE, INST_JOB_SCHEDULE,
    INST_CUSTOM_AGGREGATE, INST_CUSTOM_TRANSFORM,
    INST_PARALLEL_EXEC, INST_ASYNC_EXEC,
    INST_ACCESS_CONTROL, INST_ENCRYPT_DATA
} Instruction;

typedef enum {
    VAR_TYPE_INT,
    VAR_TYPE_FLOAT,
    VAR_TYPE_DOUBLE,
    VAR_TYPE_CHAR,
    VAR_TYPE_STR,
    VAR_TYPE_TABLE,
    VAR_TYPE_STRUCT
} VariableType;

typedef enum {
    ERR_STACK_OVERFLOW,
    ERR_STACK_UNDERFLOW,
    ERR_INVALID_JUMP,
    ERR_INVALID_STACK_ACCESS,
    ERR_INVALID_DATA_TYPE,
    ERR_ILLEGAL_INSTRUCTION,
    ERR_SYNTAX_ERROR,
    ERR_INVALID_PTR,
    ERR_INVALID_TABLE_OPERATION,
    ERR_INVALID_SQL_QUERY,
    ERR_INVALID_CHART_TYPE,
    ERR_EXPORT_FAILED,
    ERR_STAT_INSUFFICIENT_DATA,
    ERR_INVALID_API_RESPONSE,
    ERR_DB_CONNECTION_FAILED,
    ERR_ETL_OPERATION_FAILED,
    ERR_DATA_VALIDATION_FAILED,
    ERR_SCRIPT_EXECUTION_FAILED,
    ERR_JOB_SCHEDULING_FAILED,
    ERR_CUSTOM_FUNCTION_FAILED,
    ERR_PARALLEL_EXECUTION_FAILED,
    ERR_ASYNC_EXECUTION_FAILED,
    ERR_ACCESS_DENIED,
    ERR_ENCRYPTION_FAILED
} Error;

typedef struct {
    size_t data;
    VariableType var_type;
    bool heap_ptr;
} RuntimeValue;

typedef struct {
    void* data;
    VariableType var_type;
} HeapValue;

typedef struct {
    Instruction inst;
    RuntimeValue val;
} Token;

typedef struct {
    RuntimeValue val;
    uint32_t frame_index;
} StackFrameValue;

typedef struct {
    RuntimeValue stack[STACK_CAP];
    int32_t stack_size;

    HeapValue* heap;
    uint32_t heap_size;

    StackFrameValue stackframe[STACKFRAME_CAP];
    uint32_t stackframe_size;
    uint32_t stackframe_index;

    uint32_t inst_ptr;
    uint32_t program_size;

    uint32_t call_positions[STACK_CAP];
    uint32_t call_positions_count;

    uint32_t macro_positions[PROGRAM_CAP];
    uint32_t macro_count;
    bool found_solution_for_if_block;

    Token program[PROGRAM_CAP]; // Add this line
} ProgramState;

RuntimeValue stack_top(ProgramState* state) {
    return state->stack[state->stack_size - 1];
}

RuntimeValue stack_pop(ProgramState* state) {
    state->stack_size--;
    return state->stack[state->stack_size];
}

RuntimeValue stack_peak(ProgramState* state, uint32_t index) {
    return state->stack[state->stack_size - index];
}

void stack_push(ProgramState* state, RuntimeValue val) {
    state->stack_size++;
    state->stack[state->stack_size - 1] = val;
}

//helper
bool is_str_int(const char *str) {
    if (*str == '-' || *str == '+') str++;
    while (*str) {
        if (!isdigit(*str)) return false;
        str++;
    }
    return true;
}

bool is_str_literal(const char *str) {
    return str[0] == '"' && str[strlen(str) - 1] == '"';
}

//table
typedef struct {
    char *headers[MAX_TABLE_COLS];
    char *rows[MAX_TABLE_ROWS];
    uint32_t row_count;
    uint32_t col_count;
} Table;

//operations for table
void table_create(Table *table, const char **headers, uint32_t col_count) {
    table->col_count = col_count;
    for (uint32_t i = 0; i < col_count; i++) {
        table->headers[i] = strdup(headers[i]);
    }
    table->row_count = 0;
}

void table_insert(Table *table, const char **row) {
    if (table->row_count < MAX_TABLE_ROWS) {
        table->rows[table->row_count] = (char *)malloc(MAX_TABLE_COLS * MAX_STRING_LEN);
        for (uint32_t i = 0; i < table->col_count; i++) {
            strcpy(&table->rows[table->row_count][i * MAX_STRING_LEN], row[i]);
        }
        table->row_count++;
    } else {
        printf("Nix: Error: %s | Error Code: %i\n", "ERR_INVALID_TABLE_OPERATION", (int32_t)ERR_INVALID_TABLE_OPERATION);
        printf("Table has reached its maximum capacity.\n");
    }
}

void table_select(Table *table, uint32_t row_index) {
    if (row_index < table->row_count) {
        for (uint32_t i = 0; i < table->col_count; i++) {
            printf("%s: %s\n", table->headers[i], &table->rows[row_index][i * MAX_STRING_LEN]);
        }
    } else {
        printf("Nix: Error: %s | Error Code: %i\n", "ERR_INVALID_TABLE_OPERATION", (int32_t)ERR_INVALID_TABLE_OPERATION);
        printf("Row index out of bounds.\n");
    }
}

void table_update(Table *table, uint32_t row_index, const char **new_row) {
    if (row_index < table->row_count) {
        for (uint32_t i = 0; i < table->col_count; i++) {
            strcpy(&table->rows[row_index][i * MAX_STRING_LEN], new_row[i]);
        }
    } else {
        printf("Nix: Error: %s | Error Code: %i\n", "ERR_INVALID_TABLE_OPERATION", (int32_t)ERR_INVALID_TABLE_OPERATION);
        printf("Row index out of bounds.\n");
    }
}

void table_delete(Table *table, uint32_t row_index) {
    if (row_index < table->row_count) {
        free(table->rows[row_index]);
        for (uint32_t i = row_index; i < table->row_count - 1; i++) {
            table->rows[i] = table->rows[i + 1];
        }
        table->row_count--;
    } else {
        printf("Nix: Error: %s | Error Code: %i\n", "ERR_INVALID_TABLE_OPERATION", (int32_t)ERR_INVALID_TABLE_OPERATION);
        printf("Row index out of bounds.\n");
    }
}

// Load program from file
bool load_program_from_file(const char* filename, ProgramState* state);

void exec_program(ProgramState* state) {
    while (state->inst_ptr < state->program_size) {
        Token current_token = state->program[state->inst_ptr];
        switch (current_token.inst) {
            case INST_STACK_PUSH:
                stack_push(state, current_token.val);
                break;
            case INST_STACK_POP:
                stack_pop(state);
                break;
            // Add more instructions here
            default:
                printf("Nix: [Error]: Illegal instruction encountered.\n");
                break;
        }
        state->inst_ptr++;
    }
}

// Main function
int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Nix: [Error]: Too few arguments specified. Usage: ./nix <filepath>\n");
        return 1;
    }
    if (argc > 2) {
        printf("Nix: [Error]: Too many arguments specified. Usage: ./nix <filepath>\n");
        return 1;
    }

    ProgramState program_state;
    memset(&program_state, 0, sizeof(ProgramState));

    if (!load_program_from_file(argv[1], &program_state)) {
        return 1;
    }

    exec_program(&program_state);

    return 0;
}

// Load program from file
bool load_program_from_file(const char* filename, ProgramState* state) {
    (void)state;
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Nix: [Error]: Could not open file %s\n", filename);
        return false;
    }
    
    // Read file content into state->program
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Tokenize and parse each line to fill state->program
        // Assume you have a tokenizer and parser (not implemented here)
        // Example: tokenize_line(line, state);
    }
    fclose(file);
    return true;
}
