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

// Error handling macro
#define PANIC_ON_ERR(cond, err_type, ...)                                          \
    if (cond)                                                                      \
    {                                                                              \
        printf("Nix: Error: %s | Error Code: %i\n", #err_type, (int32_t)err_type); \
        printf(__VA_ARGS__);                                                       \
        printf("\n");                                                              \
        exit(1);                                                                   \
    }

// Enums and structs
typedef enum
{
    INST_STACK_PUSH,
    INST_STACK_PREV,
    INST_STACK_POP,
    INST_PLUS,
    INST_MINUS,
    INST_MUL,
    INST_DIV,
    INST_MOD,
    INST_EQ,
    INST_NEQ,
    INST_GT,
    INST_LT,
    INST_GEQ,
    INST_LEQ,
    INST_LOGICAL_AND,
    INST_LOGICAL_OR,
    INST_IF,
    INST_ELSE,
    INST_ELIF,
    INST_THEN,
    INST_ENDIF,
    INST_WHILE,
    INST_RUN_WHILE,
    INST_END_WHILE,
    INST_PRINT,
    INST_PRINTLN,
    INST_JUMP,
    INST_ADD_VAR_TO_STACKFRAME,
    INST_ASSIGN,
    INST_VAR_USAGE,
    INST_VAR_REASSIGN,
    INST_HEAP_ALLOC,
    INST_HEAP_FREE,
    INST_PTR_GET_I,
    INST_PTR_SET_I,
    INST_INT_TYPE,
    INST_FLOAT_TYPE,
    INST_DOUBLE_TYPE,
    INST_CHAR_TYPE,
    INST_STR_TYPE,
    INST_MACRO,
    INST_MACRO_DEF,
    INST_END_MACRO,
    INST_MACRO_USAGE,
    INST_FUNC_DEF,
    INST_FUNC_CALL,
    INST_FUNC_RET,
    INST_STRUCT_DEF,
    INST_STRUCT_INIT,
    INST_STRUCT_ACCESS,
    INST_TABLE_CREATE,
    INST_TABLE_INSERT,
    INST_TABLE_SELECT,
    INST_TABLE_UPDATE,
    INST_TABLE_DELETE,
    INST_SQL_QUERY,
    INST_CONCAT_STR,
    INST_CHART_PLOT,
    INST_EXPORT_DATA,
    INST_STAT_MEAN,
    INST_STAT_MEDIAN,
    INST_STAT_MODE,
    INST_STAT_STD_DEV,
    INST_REGRESSION,
    INST_CLUSTER,
    INST_TIME_SERIES,
    INST_API_REQUEST,
    INST_DB_CONNECT,
    INST_DB_QUERY,
    INST_ETL_EXTRACT,
    INST_ETL_TRANSFORM,
    INST_ETL_LOAD,
    INST_DATA_VALIDATE,
    INST_SCRIPT_EXECUTE,
    INST_JOB_SCHEDULE,
    INST_CUSTOM_AGGREGATE,
    INST_CUSTOM_TRANSFORM,
    INST_PARALLEL_EXEC,
    INST_ASYNC_EXEC,
    INST_ACCESS_CONTROL,
    INST_ENCRYPT_DATA
} Instruction;

typedef enum
{
    VAR_TYPE_INT,
    VAR_TYPE_FLOAT,
    VAR_TYPE_DOUBLE,
    VAR_TYPE_CHAR,
    VAR_TYPE_STR,
    VAR_TYPE_TABLE,
    VAR_TYPE_STRUCT
} VariableType;

typedef enum
{
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

typedef struct
{
    size_t data;
    VariableType var_type;
    bool heap_ptr;
} RuntimeValue;

typedef struct
{
    void *data;
    VariableType var_type;
} HeapValue;

typedef struct
{
    Instruction inst;
    RuntimeValue val;
} Token;

typedef struct
{
    RuntimeValue val;
    uint32_t frame_index;
} StackFrameValue;

typedef struct
{
    RuntimeValue stack[STACK_CAP];
    int32_t stack_size;

    HeapValue *heap;
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

    Token program[PROGRAM_CAP];
    uint32_t program_size;
    uint32_t inst_ptr;

} ProgramState;

// Stack operations
RuntimeValue stack_top(ProgramState *state)
{
    PANIC_ON_ERR(state->stack_size == 0, ERR_STACK_UNDERFLOW, "Stack is empty");
    return state->stack[state->stack_size - 1];
}

RuntimeValue stack_pop(ProgramState *state)
{
    PANIC_ON_ERR(state->stack_size == 0, ERR_STACK_UNDERFLOW, "Stack is empty");
    state->stack_size--;
    return state->stack[state->stack_size];
}

RuntimeValue stack_peak(ProgramState *state, uint32_t index)
{
    PANIC_ON_ERR(state->stack_size <= index, ERR_INVALID_STACK_ACCESS, "Index out of bounds");
    return state->stack[state->stack_size - index - 1];
}

void stack_push(ProgramState *state, RuntimeValue val)
{
    PANIC_ON_ERR(state->stack_size == STACK_CAP, ERR_STACK_OVERFLOW, "Stack is full");
    state->stack[state->stack_size++] = val;
}

// Helper functions
bool is_str_int(const char *str)
{
    if (*str == '-' || *str == '+')
        str++;
    while (*str)
    {
        if (!isdigit(*str))
            return false;
        str++;
    }
    return true;
}

bool is_str_literal(const char *str)
{
    return str[0] == '"' && str[strlen(str) - 1] == '"';
}

// Table operations
typedef struct
{
    char *headers[MAX_TABLE_COLS];
    char *rows[MAX_TABLE_ROWS];
    uint32_t row_count;
    uint32_t col_count;
} Table;

void table_create(Table *table, const char **headers, uint32_t col_count)
{
    PANIC_ON_ERR(table == NULL, ERR_INVALID_TABLE_OPERATION, "Invalid table pointer");
    PANIC_ON_ERR(col_count > MAX_TABLE_COLS, ERR_INVALID_TABLE_OPERATION, "Too many columns");

    table->col_count = col_count;
    for (uint32_t i = 0; i < col_count; i++)
    {
        table->headers[i] = strdup(headers[i]);
    }
    table->row_count = 0;
}

void table_destroy(Table *table)
{
    PANIC_ON_ERR(table == NULL, ERR_INVALID_TABLE_OPERATION, "Invalid table pointer");

    for (uint32_t i = 0; i < table->col_count; i++)
    {
        free(table->headers[i]);
    }

    for (uint32_t i = 0; i < table->row_count; i++)
    {
        free(table->rows[i]);
    }
}

void table_insert(Table *table, const char **row)
{
    PANIC_ON_ERR(table == NULL, ERR_INVALID_TABLE_OPERATION, "Invalid table pointer");
    PANIC_ON_ERR(table->row_count == MAX_TABLE_ROWS, ERR_INVALID_TABLE_OPERATION, "Table is full");

    table->rows[table->row_count] = (char *)malloc(MAX_TABLE_COLS * MAX_STRING_LEN);
    PANIC_ON_ERR(table->rows[table->row_count] == NULL, ERR_INVALID_TABLE_OPERATION, "Memory allocation failed");

    for (uint32_t i = 0; i < table->col_count; i++)
    {
        strcpy(&table->rows[table->row_count][i * MAX_STRING_LEN], row[i]);
    }
    table->row_count++;
}

void table_select(Table *table, uint32_t row_index)
{
    PANIC_ON_ERR(table == NULL, ERR_INVALID_TABLE_OPERATION, "Invalid table pointer");
    PANIC_ON_ERR(row_index >= table->row_count, ERR_INVALID_TABLE_OPERATION, "Row index out of bounds");

    for (uint32_t i = 0; i < table->col_count; i++)
    {
        printf("%s: %s\n", table->headers[i], &table->rows[row_index][i * MAX_STRING_LEN]);
    }
}

void table_update(Table *table, uint32_t row_index, const char **new_row)
{
    PANIC_ON_ERR(table == NULL, ERR_INVALID_TABLE_OPERATION, "Invalid table pointer");
    PANIC_ON_ERR(row_index >= table->row_count, ERR_INVALID_TABLE_OPERATION, "Row index out of bounds");

    for (uint32_t i = 0; i < table->col_count; i++)
    {
        strcpy(&table->rows[row_index][i * MAX_STRING_LEN], new_row[i]);
    }
}

void table_delete(Table *table, uint32_t row_index)
{
    PANIC_ON_ERR(table == NULL, ERR_INVALID_TABLE_OPERATION, "Invalid table pointer");
    PANIC_ON_ERR(row_index >= table->row_count, ERR_INVALID_TABLE_OPERATION, "Row index out of bounds");

    free(table->rows[row_index]);
    for (uint32_t i = row_index; i < table->row_count - 1; i++)
    {
        table->rows[i] = table->rows[i + 1];
    }
    table->row_count--;
}

// Heap operations
void heap_alloc(ProgramState *state, VariableType var_type, size_t size)
{
    PANIC_ON_ERR(state->heap_size == STACK_CAP, ERR_STACK_OVERFLOW, "Heap is full");

    state->heap[state->heap_size].var_type = var_type;
    switch (var_type)
    {
    case VAR_TYPE_INT:
        state->heap[state->heap_size].data = malloc(sizeof(int));
        break;
    case VAR_TYPE_FLOAT:
        state->heap[state->heap_size].data = malloc(sizeof(float));
        break;
    case VAR_TYPE_DOUBLE:
        state->heap[state->heap_size].data = malloc(sizeof(double));
        break;
    case VAR_TYPE_CHAR:
        state->heap[state->heap_size].data = malloc(sizeof(char));
        break;
    case VAR_TYPE_STR:
        state->heap[state->heap_size].data = malloc(size);
        break;
    default:
        PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type");
    }

    RuntimeValue ptr_val = {
        .data = state->heap_size,
        .var_type = VAR_TYPE_INT,
        .heap_ptr = true};
    stack_push(state, ptr_val);
    state->heap_size++;
}

void heap_free(ProgramState *state, RuntimeValue ptr_val)
{
    PANIC_ON_ERR(!ptr_val.heap_ptr, ERR_INVALID_PTR, "Value is not a heap pointer");

    uint32_t ptr_index = (uint32_t)ptr_val.data;
    PANIC_ON_ERR(ptr_index >= state->heap_size, ERR_INVALID_PTR, "Invalid heap pointer");

    free(state->heap[ptr_index].data);
    state->heap[ptr_index].data = NULL;
}

void ptr_get_i(ProgramState *state, RuntimeValue ptr_val, uint32_t index)
{
    PANIC_ON_ERR(!ptr_val.heap_ptr, ERR_INVALID_PTR, "Value is not a heap pointer");

    uint32_t ptr_index = (uint32_t)ptr_val.data;
    PANIC_ON_ERR(ptr_index >= state->heap_size, ERR_INVALID_PTR, "Invalid heap pointer");

    VariableType var_type = state->heap[ptr_index].var_type;
    void *data = state->heap[ptr_index].data;

    switch (var_type)
    {
    case VAR_TYPE_INT:
    {
        int *int_data = (int *)data;
        RuntimeValue val = {
            .data = (size_t)int_data[index],
            .var_type = VAR_TYPE_INT,
            .heap_ptr = false};
        stack_push(state, val);
        break;
    }
    case VAR_TYPE_FLOAT:
    {
        float *float_data = (float *)data;
        RuntimeValue val = {
            .data = (size_t)float_data[index],
            .var_type = VAR_TYPE_FLOAT,
            .heap_ptr = false};
        stack_push(state, val);
        break;
    }
    case VAR_TYPE_DOUBLE:
    {
        double *double_data = (double *)data;
        RuntimeValue val = {
            .data = (size_t)double_data[index],
            .var_type = VAR_TYPE_DOUBLE,
            .heap_ptr = false};
        stack_push(state, val);
        break;
    }
    case VAR_TYPE_CHAR:
    {
        char *char_data = (char *)data;
        RuntimeValue val = {
            .data = (size_t)char_data[index],
            .var_type = VAR_TYPE_CHAR,
            .heap_ptr = false};
        stack_push(state, val);
        break;
    }
    case VAR_TYPE_STR:
    {
        char *str_data = (char *)data;
        RuntimeValue val = {
            .data = (size_t)&str_data[index * MAX_STRING_LEN],
            .var_type = VAR_TYPE_STR,
            .heap_ptr = false};
        stack_push(state, val);
        break;
    }
    default:
        PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type");
    }
}

void ptr_set_i(ProgramState *state, RuntimeValue ptr_val, uint32_t index, RuntimeValue new_val)
{
    PANIC_ON_ERR(!ptr_val.heap_ptr, ERR_INVALID_PTR, "Value is not a heap pointer");

    uint32_t ptr_index = (uint32_t)ptr_val.data;
    PANIC_ON_ERR(ptr_index >= state->heap_size, ERR_INVALID_PTR, "Invalid heap pointer");

    VariableType var_type = state->heap[ptr_index].var_type;
    void *data = state->heap[ptr_index].data;

    switch (var_type)
    {
    case VAR_TYPE_INT:
    {
        int *int_data = (int *)data;
        int new_int_val = (int)new_val.data;
        int_data[index] = new_int_val;
        break;
    }
    case VAR_TYPE_FLOAT:
    {
        float *float_data = (float *)data;
        float new_float_val = (float)new_val.data;
        float_data[index] = new_float_val;
        break;
    }
    case VAR_TYPE_DOUBLE:
    {
        double *double_data = (double *)data;
        double new_double_val = (double)new_val.data;
        double_data[index] = new_double_val;
        break;
    }
    case VAR_TYPE_CHAR:
    {
        char *char_data = (char *)data;
        char new_char_val = (char)new_val.data;
        char_data[index] = new_char_val;
        break;
    }
    case VAR_TYPE_STR:
    {
        char *str_data = (char *)data;
        char *new_str_val = (char *)new_val.data;
        strcpy(&str_data[index * MAX_STRING_LEN], new_str_val);
        break;
    }
    default:
        PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type");
    }
}

// Type operations
VariableType get_var_type(const char *var_name)
{
    if (strcmp(var_name, "int") == 0)
        return VAR_TYPE_INT;
    else if (strcmp(var_name, "float") == 0)
        return VAR_TYPE_FLOAT;
    else if (strcmp(var_name, "double") == 0)
        return VAR_TYPE_DOUBLE;
    else if (strcmp(var_name, "char") == 0)
        return VAR_TYPE_CHAR;
    else if (strcmp(var_name, "str") == 0)
        return VAR_TYPE_STR;
    else if (strcmp(var_name, "table") == 0)
        return VAR_TYPE_TABLE;
    else if (strcmp(var_name, "struct") == 0)
        return VAR_TYPE_STRUCT;
    else
        PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid variable type");
}

// Variable operations
void add_var_to_stackframe(ProgramState *state, RuntimeValue val)
{
    PANIC_ON_ERR(state->stackframe_index == STACKFRAME_CAP, ERR_STACK_OVERFLOW, "Stackframe is full");

    StackFrameValue frame_val = {
        .val = val,
        .frame_index = state->stackframe_index};
    state->stackframe[state->stackframe_index++] = frame_val;
}

void assign_var(ProgramState *state, RuntimeValue val, RuntimeValue new_val)
{
    if (val.heap_ptr)
    {
        ptr_set_i(state, val, 0, new_val);
    }
    else
    {
        state->stack[state->stack_size - 1] = new_val;
    }
}

RuntimeValue get_var_value(ProgramState *state, RuntimeValue val)
{
    if (val.heap_ptr)
    {
        ptr_get_i(state, val, 0);
        return stack_top(state);
    }
    else
    {
        return val;
    }
}

// Main execution loop
void execute_program(ProgramState *state)
{
    while (state->inst_ptr < state->program_size)
    {
        Token current_token = state->program[state->inst_ptr++];

        switch (current_token.inst)
        {
        case INST_STACK_PUSH:
            stack_push(state, current_token.val);
            break;
        case INST_STACK_PREV:
            stack_push(state, stack_peak(state, 0));
            break;
        case INST_STACK_POP:
            stack_pop(state);
            break;
        case INST_PLUS:
        {
            RuntimeValue val2 = stack_pop(state);
            RuntimeValue val1 = stack_pop(state);
            RuntimeValue result;

            switch (val1.var_type)
            {
            case VAR_TYPE_INT:
                result.data = val1.data + val2.data;
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_FLOAT:
                result.data = val1.data + val2.data;
                result.var_type = VAR_TYPE_FLOAT;
                break;
            case VAR_TYPE_DOUBLE:
                result.data = val1.data + val2.data;
                result.var_type = VAR_TYPE_DOUBLE;
                break;
            case VAR_TYPE_CHAR:
                result.data = val1.data + val2.data;
                result.var_type = VAR_TYPE_CHAR;
                break;
            case VAR_TYPE_STR:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Cannot add strings");
            default:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type for addition");
            }

            stack_push(state, result);
            break;
        }
        case INST_MINUS:
        {
            RuntimeValue val2 = stack_pop(state);
            RuntimeValue val1 = stack_pop(state);
            RuntimeValue result;

            switch (val1.var_type)
            {
            case VAR_TYPE_INT:
                result.data = val1.data - val2.data;
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_FLOAT:
                result.data = val1.data - val2.data;
                result.var_type = VAR_TYPE_FLOAT;
                break;
            case VAR_TYPE_DOUBLE:
                result.data = val1.data - val2.data;
                result.var_type = VAR_TYPE_DOUBLE;
                break;
            case VAR_TYPE_CHAR:
                result.data = val1.data - val2.data;
                result.var_type = VAR_TYPE_CHAR;
                break;
            default:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type for subtraction");
            }

            stack_push(state, result);
            break;
        }
        case INST_MUL:
        {
            RuntimeValue val2 = stack_pop(state);
            RuntimeValue val1 = stack_pop(state);
            RuntimeValue result;

            switch (val1.var_type)
            {
            case VAR_TYPE_INT:
                result.data = val1.data * val2.data;
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_FLOAT:
                result.data = val1.data * val2.data;
                result.var_type = VAR_TYPE_FLOAT;
                break;
            case VAR_TYPE_DOUBLE:
                result.data = val1.data * val2.data;
                result.var_type = VAR_TYPE_DOUBLE;
                break;
            case VAR_TYPE_CHAR:
                result.data = val1.data * val2.data;
                result.var_type = VAR_TYPE_CHAR;
                break;
            default:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type for multiplication");
            }

            stack_push(state, result);
            break;
        }
        case INST_DIV:
        {
            RuntimeValue val2 = stack_pop(state);
            RuntimeValue val1 = stack_pop(state);
            RuntimeValue result;

            switch (val1.var_type)
            {
            case VAR_TYPE_INT:
                PANIC_ON_ERR(val2.data == 0, ERR_INVALID_DATA_TYPE, "Division by zero");
                result.data = val1.data / val2.data;
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_FLOAT:
                PANIC_ON_ERR(val2.data == 0, ERR_INVALID_DATA_TYPE, "Division by zero");
                result.data = val1.data / val2.data;
                result.var_type = VAR_TYPE_FLOAT;
                break;
            case VAR_TYPE_DOUBLE:
                PANIC_ON_ERR(val2.data == 0, ERR_INVALID_DATA_TYPE, "Division by zero");
                result.data = val1.data / val2.data;
                result.var_type = VAR_TYPE_DOUBLE;
                break;
            case VAR_TYPE_CHAR:
                PANIC_ON_ERR(val2.data == 0, ERR_INVALID_DATA_TYPE, "Division by zero");
                result.data = val1.data / val2.data;
                result.var_type = VAR_TYPE_CHAR;
                break;
            default:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type for division");
            }

            stack_push(state, result);
            break;
        }
        case INST_MOD:
        {
            RuntimeValue val2 = stack_pop(state);
            RuntimeValue val1 = stack_pop(state);
            RuntimeValue result;

            PANIC_ON_ERR(val2.data == 0, ERR_INVALID_DATA_TYPE, "Modulo by zero");

            switch (val1.var_type)
            {
            case VAR_TYPE_INT:
                result.data = val1.data % val2.data;
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_FLOAT:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Modulo operation not supported for float");
            case VAR_TYPE_DOUBLE:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Modulo operation not supported for double");
            case VAR_TYPE_CHAR:
                result.data = val1.data % val2.data;
                result.var_type = VAR_TYPE_CHAR;
                break;
            default:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type for modulo operation");
            }

            stack_push(state, result);
            break;
        }
        case INST_EQ:
        {
            RuntimeValue val2 = stack_pop(state);
            RuntimeValue val1 = stack_pop(state);
            RuntimeValue result;
            result.data = (val1.data == val2.data);
            result.var_type = VAR_TYPE_INT;
            stack_push(state, result);
            break;
        }
        case INST_NEQ:
        {
            RuntimeValue val2 = stack_pop(state);
            RuntimeValue val1 = stack_pop(state);
            RuntimeValue result;
            result.data = (val1.data != val2.data);
            result.var_type = VAR_TYPE_INT;
            stack_push(state, result);
            break;
        }
        case INST_GT:
        {
            RuntimeValue val2 = stack_pop(state);
            RuntimeValue val1 = stack_pop(state);
            RuntimeValue result;

            switch (val1.var_type)
            {
            case VAR_TYPE_INT:
                result.data = (val1.data > val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_FLOAT:
                result.data = (val1.data > val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_DOUBLE:
                result.data = (val1.data > val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_CHAR:
                result.data = (val1.data > val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            default:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type for comparison");
            }

            stack_push(state, result);
            break;
        }
        case INST_LT:
        {
            RuntimeValue val2 = stack_pop(state);
            RuntimeValue val1 = stack_pop(state);
            RuntimeValue result;

            switch (val1.var_type)
            {
            case VAR_TYPE_INT:
                result.data = (val1.data < val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_FLOAT:
                result.data = (val1.data < val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_DOUBLE:
                result.data = (val1.data < val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_CHAR:
                result.data = (val1.data < val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            default:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type for comparison");
            }

            stack_push(state, result);
            break;
        }
        case INST_GEQ:
        {
            RuntimeValue val2 = stack_pop(state);
            RuntimeValue val1 = stack_pop(state);
            RuntimeValue result;

            switch (val1.var_type)
            {
            case VAR_TYPE_INT:
                result.data = (val1.data >= val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_FLOAT:
                result.data = (val1.data >= val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_DOUBLE:
                result.data = (val1.data >= val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_CHAR:
                result.data = (val1.data >= val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            default:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type for comparison");
            }

            stack_push(state, result);
            break;
        }
        case INST_LEQ:
        {
            RuntimeValue val2 = stack_pop(state);
            RuntimeValue val1 = stack_pop(state);
            RuntimeValue result;

            switch (val1.var_type)
            {
            case VAR_TYPE_INT:
                result.data = (val1.data <= val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_FLOAT:
                result.data = (val1.data <= val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_DOUBLE:
                result.data = (val1.data <= val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            case VAR_TYPE_CHAR:
                result.data = (val1.data <= val2.data);
                result.var_type = VAR_TYPE_INT;
                break;
            default:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type for comparison");
            }

            stack_push(state, result);
            break;
        }
        case INST_LOGICAL_AND:
        {
            RuntimeValue val2 = stack_pop(state);
            RuntimeValue val1 = stack_pop(state);
            RuntimeValue result;
            result.data = (val1.data && val2.data);
            result.var_type = VAR_TYPE_INT;
            stack_push(state, result);
            break;
        }
        case INST_LOGICAL_OR:
        {
            RuntimeValue val2 = stack_pop(state);
            RuntimeValue val1 = stack_pop(state);
            RuntimeValue result;
            result.data = (val1.data || val2.data);
            result.var_type = VAR_TYPE_INT;
            stack_push(state, result);
            break;
        }
        case INST_IF:
        {
            RuntimeValue cond = stack_pop(state);
            if (cond.data != 0)
            {
                state->found_solution_for_if_block = true;
            }
            else
            {
                state->found_solution_for_if_block = false;
                uint32_t count = 0;
                for (int i = state->inst_ptr; i < state->program_size; i++)
                {
                    if (state->program[i].inst == INST_ELSE || state->program[i].inst == INST_ELIF || state->program[i].inst == INST_ENDIF)
                    {
                        count--;
                    }
                    if (state->program[i].inst == INST_IF || state->program[i].inst == INST_ELIF)
                    {
                        count++;
                    }
                    if (count == -1)
                    {
                        state->inst_ptr = i + 1;
                        break;
                    }
                }
            }
            break;
        }
        case INST_ELSE:
        {
            if (!state->found_solution_for_if_block)
            {
                uint32_t count = 0;
                for (int i = state->inst_ptr; i < state->program_size; i++)
                {
                    if (state->program[i].inst == INST_ELSE || state->program[i].inst == INST_ELIF || state->program[i].inst == INST_ENDIF)
                    {
                        count--;
                    }
                    if (state->program[i].inst == INST_IF || state->program[i].inst == INST_ELIF)
                    {
                        count++;
                    }
                    if (count == -1)
                    {
                        state->inst_ptr = i + 1;
                        break;
                    }
                }
            }
            break;
        }
        case INST_ELIF:
        {
            if (!state->found_solution_for_if_block)
            {
                uint32_t count = 0;
                for (int i = state->inst_ptr; i < state->program_size; i++)
                {
                    if (state->program[i].inst == INST_ELSE || state->program[i].inst == INST_ELIF || state->program[i].inst == INST_ENDIF)
                    {
                        count--;
                    }
                    if (state->program[i].inst == INST_IF || state->program[i].inst == INST_ELIF)
                    {
                        count++;
                    }
                    if (count == -1)
                    {
                        state->inst_ptr = i + 1;
                        break;
                    }
                }
            }
            break;
        }
        case INST_ENDIF:
            break;
        case INST_WHILE:
        {
            RuntimeValue cond = stack_pop(state);
            if (cond.data == 0)
            {
                uint32_t count = 0;
                for (int i = state->inst_ptr; i >= 0; i--)
                {
                    if (state->program[i].inst == INST_END_WHILE || state->program[i].inst == INST_WHILE || state->program[i].inst == INST_RUN_WHILE)
                    {
                        count--;
                    }
                    if (state->program[i].inst == INST_WHILE)
                    {
                        count++;
                    }
                    if (count == -1)
                    {
                        state->inst_ptr = i + 1;
                        break;
                    }
                }
            }
            break;
        }
        case INST_RUN_WHILE:
            break;
        case INST_END_WHILE:
        {
            uint32_t count = 0;
            for (int i = state->inst_ptr; i >= 0; i--)
            {
                if (state->program[i].inst == INST_END_WHILE || state->program[i].inst == INST_WHILE || state->program[i].inst == INST_RUN_WHILE)
                {
                    count--;
                }
                if (state->program[i].inst == INST_WHILE)
                {
                    count++;
                }
                if (count == -1)
                {
                    state->inst_ptr = i + 1;
                    break;
                }
            }
            break;
        case INST_PRINT:
        {
            RuntimeValue val = stack_pop(state);

            switch (val.var_type)
            {
            case VAR_TYPE_INT:
                printf("%d", (int)val.data);
                break;
            case VAR_TYPE_FLOAT:
                printf("%f", *(float *)&val.data);
                break;
            case VAR_TYPE_DOUBLE:
                printf("%lf", *(double *)&val.data);
                break;
            case VAR_TYPE_CHAR:
                printf("%c", (char)val.data);
                break;
            case VAR_TYPE_STR:
                printf("%s", (char *)val.data);
                break;
            default:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type for printing");
            }

            break;
        }
        case INST_PRINTLN:
        {
            RuntimeValue val = stack_pop(state);

            switch (val.var_type)
            {
            case VAR_TYPE_INT:
                printf("%d\n", (int)val.data);
                break;
            case VAR_TYPE_FLOAT:
                printf("%f\n", *(float *)&val.data);
                break;
            case VAR_TYPE_DOUBLE:
                printf("%lf\n", *(double *)&val.data);
                break;
            case VAR_TYPE_CHAR:
                printf("%c\n", (char)val.data);
                break;
            case VAR_TYPE_STR:
                printf("%s\n", (char *)val.data);
                break;
            default:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type for printing");
            }

            break;
        }
        case INST_JUMP:
        {
            uint32_t jump_index = state->program[state->inst_ptr].val.data;
            PANIC_ON_ERR(jump_index >= state->program_size, ERR_INVALID_JUMP, "Invalid jump index");

            state->inst_ptr = jump_index;
            break;
        }
        case INST_ADD_VAR_TO_STACKFRAME:
        {
            RuntimeValue val = stack_pop(state);
            uint32_t frame_index = state->stackframe_index;

            state->stackframe[frame_index].val = val;
            state->stackframe[frame_index].frame_index = frame_index;

            state->stackframe_index++;
            break;
        }
        case INST_ASSIGN:
        {
            RuntimeValue val = stack_pop(state);
            RuntimeValue *var_ptr = &state->stackframe[state->stackframe_index - 1].val;

            switch (var_ptr->var_type)
            {
            case VAR_TYPE_INT:
                var_ptr->data = val.data;
                break;
            case VAR_TYPE_FLOAT:
                *(float *)&var_ptr->data = *(float *)&val.data;
                break;
            case VAR_TYPE_DOUBLE:
                *(double *)&var_ptr->data = *(double *)&val.data;
                break;
            case VAR_TYPE_CHAR:
                var_ptr->data = val.data;
                break;
            case VAR_TYPE_STR:
                if (var_ptr->heap_ptr)
                {
                    heap_free(state, *var_ptr);
                }
                var_ptr->data = val.data;
                var_ptr->heap_ptr = false;
                break;
            default:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type for assignment");
            }
            break;
        }
        case INST_VAR_USAGE:
        {
            uint32_t var_index = state->stackframe[state->stackframe_index - 1].frame_index;
            RuntimeValue val = state->stackframe[var_index].val;
            stack_push(state, val);
            break;
        }
        case INST_VAR_REASSIGN:
        {
            uint32_t var_index = state->stackframe[state->stackframe_index - 1].frame_index;
            RuntimeValue val = stack_pop(state);

            switch (val.var_type)
            {
            case VAR_TYPE_INT:
                state->stackframe[var_index].val.data = val.data;
                break;
            case VAR_TYPE_FLOAT:
                *(float *)&state->stackframe[var_index].val.data = *(float *)&val.data;
                break;
            case VAR_TYPE_DOUBLE:
                *(double *)&state->stackframe[var_index].val.data = *(double *)&val.data;
                break;
            case VAR_TYPE_CHAR:
                state->stackframe[var_index].val.data = val.data;
                break;
            case VAR_TYPE_STR:
                if (state->stackframe[var_index].val.heap_ptr)
                {
                    heap_free(state, state->stackframe[var_index].val);
                }
                state->stackframe[var_index].val.data = val.data;
                state->stackframe[var_index].val.heap_ptr = false;
                break;
            default:
                PANIC_ON_ERR(true, ERR_INVALID_DATA_TYPE, "Invalid data type for variable reassignment");
            }
            break;
        }
        case INST_HEAP_ALLOC:
        {
            RuntimeValue size_val = stack_pop(state);
            VariableType var_type = state->program[state->inst_ptr].val.var_type;
            heap_alloc(state, var_type, size_val.data);
            break;
        }
        case INST_HEAP_FREE:
        {
            RuntimeValue ptr_val = stack_pop(state);
            heap_free(state, ptr_val);
            break;
        }
        case INST_PTR_GET_I:
        {
            RuntimeValue ptr_val = stack_pop(state);
            RuntimeValue index_val = stack_pop(state);
            ptr_get_i(state, ptr_val, index_val.data);
            break;
        }
        case INST_PTR_SET_I:
        {
            RuntimeValue ptr_val = stack_pop(state);
            RuntimeValue index_val = stack_pop(state);
            RuntimeValue data_val = stack_pop(state);
            ptr_set_i(state, ptr_val, index_val.data, data_val);
            break;
        }
        case INST_INT_TYPE:
        {
            int int_val = state->program[state->inst_ptr].val.data;
            RuntimeValue val = {.data = (size_t)int_val, .var_type = VAR_TYPE_INT};
            stack_push(state, val);
            break;
        }
        case INST_FLOAT_TYPE:
        {
            float float_val = *(float *)&state->program[state->inst_ptr].val.data;
            RuntimeValue val = {.data = (size_t)float_val, .var_type = VAR_TYPE_FLOAT};
            stack_push(state, val);
            break;
        }
        case INST_DOUBLE_TYPE:
        {
            double double_val = *(double *)&state->program[state->inst_ptr].val.data;
            RuntimeValue val = {.data = (size_t)double_val, .var_type = VAR_TYPE_DOUBLE};
            stack_push(state, val);
            break;
        }
        case INST_CHAR_TYPE:
        {
            char char_val = (char)state->program[state->inst_ptr].val.data;
            RuntimeValue val = {.data = (size_t)char_val, .var_type = VAR_TYPE_CHAR};
            stack_push(state, val);
            break;
        }
        case INST_STR_TYPE:
        {
            char *str_val = (char *)state->program[state->inst_ptr].val.data;
            size_t size = strlen(str_val);
            heap_alloc(state, VAR_TYPE_STR, size);
            memcpy(state->heap[state->heap_size - 1].data, str_val, size);
            break;
        }
        case INST_MACRO:
        {
            uint32_t macro_index = state->program[state->inst_ptr].val.data;
            state->macro_positions[state->macro_count++] = macro_index;
            break;
        }
        case INST_MACRO_DEF:
        {
            break;
        }
        case INST_END_MACRO:
        {
            break;
        }
        case INST_MACRO_USAGE:
        {
            break;
        }
        case INST_FUNC_DEF:
        {
            break;
        }
        case INST_FUNC_CALL:
        {
            break;
        }
        case INST_FUNC_RET:
        {
            break;
        }
        case INST_STRUCT_DEF:
        {
            break;
        }
        case INST_STRUCT_INIT:
        {
            break;
        }
        case INST_STRUCT_ACCESS:
        {
            break;
        }
        case INST_TABLE_CREATE:
        {
            break;
        }
        case INST_TABLE_INSERT:
        {
            break;
        }
        case INST_TABLE_SELECT:
        {
            break;
        }
        case INST_TABLE_UPDATE:
        {
            break;
        }
        case INST_TABLE_DELETE:
        {
            break;
        }
        case INST_SQL_QUERY:
        {
            break;
        }
        case INST_CONCAT_STR:
        {
            break;
        }
        case INST_CHART_PLOT:
        {
            break;
        }
        case INST_EXPORT_DATA:
        {
            break;
        }
        case INST_STAT_MEAN:
        {
            break;
        }
        case INST_STAT_MEDIAN:
        {
            break;
        }
        case INST_STAT_MODE:
        {
            break;
        }
        case INST_STAT_STD_DEV:
        {
            break;
        }
        case INST_REGRESSION:
        {
            break;
        }
        case INST_CLUSTER:
        {
            break;
        }
        case INST_TIME_SERIES:
        {
            break;
        }
        case INST_API_REQUEST:
        {
            break;
        }
        case INST_DB_CONNECT:
        {
            break;
        }
        case INST_DB_QUERY:
        {
            break;
        }
        case INST_ETL_EXTRACT:
        {
            break;
        }
        case INST_ETL_TRANSFORM:
        {
            break;
        }
        case INST_ETL_LOAD:
        {
            break;
        }
        case INST_DATA_VALIDATE:
        {
            break;
        }
        case INST_SCRIPT_EXECUTE:
        {
            break;
        }
        case INST_JOB_SCHEDULE:
        {
            break;
        }
        case INST_CUSTOM_AGGREGATE:
        {
            break;
        }
        case INST_CUSTOM_TRANSFORM:
        {
            break;
        }
        case INST_PARALLEL_EXEC:
        {
            break;
        }
        case INST_ASYNC_EXEC:
        {
            break;
        }
        case INST_ACCESS_CONTROL:
        {
            break;
        }
        case INST_ENCRYPT_DATA:
        {
            break;
        }
        default:
            PANIC_ON_ERR(true, ERR_ILLEGAL_INSTRUCTION, "Illegal instruction encountered");
        }

        state->inst_ptr++;
    }
}

// Main function to execute the program
int main()
{
    ProgramState state;
    memset(&state, 0, sizeof(ProgramState));

    // Example program - replace this with your actual program initialization logic
    Token program[] = {
        {INST_INT_TYPE, {.data = 42}},
        {INST_FLOAT_TYPE, {.data = *(size_t *)&3.14}},
        {INST_DOUBLE_TYPE, {.data = *(size_t *)&2.71828}},
        {INST_CHAR_TYPE, {.data = 'A'}},
        {INST_STR_TYPE, {.data = (size_t)"Hello, World!"}},
        {INST_PRINTLN, {.data = 0}},
        {INST_PRINTLN, {.data = 1}},
        {INST_PRINTLN, {.data = 2}},
        {INST_PRINTLN, {.data = 3}},
        {INST_PRINTLN, {.data = 4}},
        {INST_JUMP, {.data = 0}},
    };

    // Assign the program to the state
    state.program_size = sizeof(program) / sizeof(Token);
    memcpy(state.program, program, sizeof(program));

    // Execute the program
    execute_program(&state);

    return 0;
}