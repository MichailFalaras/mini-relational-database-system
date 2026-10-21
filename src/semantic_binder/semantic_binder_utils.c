#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../../include/semantic_binder.h"
#include "semantic_binder_utils.h"
#include "../../include/ast.h"
#include "../../include/database.h"
#include "../../include/expressions.h"
#include "../data_types/data_types_utils.h"
#include "../../include/table.h"
#include "../../include/schema.h"   


/* ---------- Binding of ExpressionNode ---------- */

bool bind_expression(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type) {
    if (!expr || expr->type < EXPR_LITERAL || expr->type > EXPR_FUNCTIONS) {
        printf("bind_expression: Invalid input expression.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_expression: Invalid input context.\n");
        return false;
    }

    switch (expr->type) {
        case EXPR_LITERAL:
            return bind_literal(expr, context, out_type);
        case EXPR_COLUMN_REF:
            return bind_column_ref(expr, context, out_type);
        case EXPR_UNARY:
            return bind_unary(expr, context, out_type);
        case EXPR_BINARY:
            return bind_binary(expr, context, out_type);
        case EXPR_IS_NULL:
            return bind_is_null(expr, context, out_type);
        case EXPR_IS_NOT_NULL:
            return bind_is_not_null(expr, context, out_type);
        case EXPR_IN:
            return bind_in(expr, context, out_type);
        case EXPR_BETWEEN:
            return bind_between(expr, context, out_type);
        case EXPR_FUNCTIONS:
            return bind_function(expr, context, out_type);
        default:
            printf("bind_expression: Invalid expression type.\n");
            return false;
    }
}

// Bind a literal expression
bool bind_literal(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type) {
    if (!expr || expr->type != EXPR_LITERAL) {
        printf("bind_literal: Invalid input expression.\n");
        return false;
    }

    if (!context || !context->db || !out_type) {
        printf("bind_literal: Invalid input context or output.\n");
        return false;
    }

    Literal *literal_data = &expr->expression_data.literal_value;

    // Check for a NULL literal
    if (literal_data->is_untyped_null) {
        if (literal_data->literal != NULL) {
            printf("bind_literal: Untyped NULL has an unexpected value.\n");
            return false;
        }

        out_type->is_untyped_null = true;
        return true;
    }

    // Validate non-NULL value
    Value *value = literal_data->literal;

    if (!value) {
        printf("bind_literal: Expression literal is NULL.\n");
        return false;
    }

    if (value->type < INTEGER || value->type > JSONB) {
        printf("bind_literal: Invalid literal data type.\n");
        return false;
    }

    out_type->type = value->type;
    out_type->is_untyped_null = false;

    return true;
}

// Bind a column reference expression
bool bind_column_ref(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type) {
    if (!expr || expr->type != EXPR_COLUMN_REF) {
        printf("bind_column_ref: Invalid input expression.\n");
        return false;
    }

    if (!context || !context->db || 
        !context->relations || context->num_relations == 0 || !out_type) {
        printf("bind_column_ref: Invalid input context or output.\n");
        return false;
    }

    ColumnRef *column_ref = &expr->expression_data.column_value;

    // Validate column name
    if (column_ref->column_name[0] == '\0') {
        printf("bind_column_ref: Empty column name.\n");
        return false;
    }

    // Initially, column and relation indexes are -1
    if (column_ref->column_index != -1 || column_ref->relation_index != -1) {
        printf("bind_column_ref: Invalid input column reference.\n");
        return false;
    }

    uint32_t column_matches = 0;
    int32_t relation_index = -1;
    int32_t column_index = -1;

    if (column_ref->table_name[0] == '\0') {
        // Unqualified column reference
        // Must search all relations and find the matching column in only 1 of them

        for (uint32_t i = 0; i < context->num_relations; i++) {
            Table *table = context->relations[i].table;

            if (!table) {
                printf("bind_column_ref: Invalid binding context structure.\n");
                return false;
            }

            if (!table->table_schema || !table->table_schema->columns) {
                printf("bind_column_ref: Invalid table schema structure.\n");
                return false;
            }

            for (uint32_t j = 0; j < table->table_schema->num_columns; j++) {
                Column *column = table->table_schema->columns[j];
                
                if (!column || column->name[0] == '\0') {
                    printf("bind_column_ref: Invalid table schema structure.\n");
                    return false;
                }

                // Check for matching column
                if (!strcmp(column_ref->column_name, column->name)) {
                    column_matches++;
                    relation_index = i;
                    column_index = j;

                    out_type->type = column->type;
                    out_type->is_untyped_null = false;
                }
            }
        }
    }
    else {
        // Qualified column reference
        // Must find the exact owner table through its name or alias
        for (uint32_t i = 0; i < context->num_relations; i++) {
            Table *table = context->relations[i].table; 
            
            if (!table) {
                printf("bind_column_ref: Invalid binding context structure.\n");
                return false;
            }

            char *table_name = table->name;
            char *table_alias = context->relations[i].alias;

            if (table_name[0] == '\0' && table_alias[0] == '\0') {
                printf("bind_column_ref: Invalid relation with no table identifiers.\n");
                return false;
            }

            // If we found the matching table, search for the column (if it exists)
            if ((table_alias[0] != '\0' && !strcmp(column_ref->table_name, table_alias)) || 
                (table_alias[0] == '\0' && !strcmp(column_ref->table_name, table_name))) {

                if (!table->table_schema || !table->table_schema->columns) {
                    printf("bind_column_ref: Invalid table schema structure.\n");
                    return false;
                }
                
                for (uint32_t j = 0; j < table->table_schema->num_columns; j++) {
                    Column *column = table->table_schema->columns[j];

                    if (!column || column->name[0] == '\0') {
                        printf("bind_column_ref: Invalid table schema structure.\n");
                        return false;
                    }

                    // Check for matching column
                    if (!strcmp(column_ref->column_name, column->name)) {
                        column_matches++;
                        relation_index = i;
                        column_index = j;

                        out_type->type = column->type;
                        out_type->is_untyped_null = false;
                    }
                }

                break;
            }
        }
    }

    if (column_matches > 1) {
        printf("bind_column_ref: Ambiguous column reference.\n");
        return false;
    }

    if (column_matches == 0) {
        printf("bind_column_ref: Column not found.\n");
        return false;
    }

    column_ref->relation_index = relation_index;
    column_ref->column_index = column_index;

    return true;
}

// Bind a table reference expression
bool bind_table_ref(ExpressionNode *expr, BindingContext *context, Table **out_table) {
    if (!expr || expr->type != EXPR_TABLE_REF) {
        printf("bind_table_ref: Invalid input expression.\n");
        return false;
    }

    if (!context || !context->db || !out_table) {
        printf("bind_table_ref: Invalid input context or output.\n");
        return false;
    }

    *out_table = NULL;

    const char *table_name = expr->expression_data.table_value.table_name;

    if (table_name[0] == '\0') {
        printf("bind_table_ref: Empty table name.\n");
        return false;
    }

    Table *table = database_find_table(context->db, table_name);

    if (!table) {
        printf("bind_table_ref: Table not found.\n");
        return false;
    }
    
    *out_table = table;
    return true;
}

// Helper that rejects table references inside scalar expressions
bool bind_scalar_expression(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type) {
    if (!expr || expr->type == EXPR_TABLE_REF) {
        printf("bind_scalar_expression: Invalid scalar expression.\n");
        return false;
    }

    return bind_expression(expr, context, out_type);
}

// Bind a unary expression
bool bind_unary(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type) {
    if (!expr || expr->type != EXPR_UNARY) {
        printf("bind_unary: Invalid input expression.\n");
        return false;
    }

    if (!context || !context->db || !out_type) {
        printf("bind_unary: Invalid input context or output.\n");
        return false;
    }

    Unary *unary = &expr->expression_data.unary_expr;
    
    // Validate unary expression components
    if (unary->op != OP_ADD && unary->op != OP_SUB && 
        unary->op != OP_NOT && unary->op != OP_BITWISE_NOT) {
        printf("bind_unary: Unsupported unary operator type.\n");
        return false;
    }

    if (!unary->operand) {
        printf("bind_unary: Invalid unary operand.\n");
        return false;
    }

    // Bind inner operand
    BoundExpressionType operand_type = {0};

    if (!bind_scalar_expression(unary->operand, context, &operand_type)) {
        printf("bind_unary: Inner expression could not be bound.\n");
        return false;   
    }

    if (operand_type.is_untyped_null) {
        // Special "NOT NULL" case
        if (unary->op == OP_NOT) {
            out_type->type = BOOL;
            out_type->is_untyped_null = false;
            return true;
        }

        // This case rejects unary operations of untyped NULL -> "+NULL" and "-NULL"
        printf("bind_unary: Inner expression type cannot be NULL.\n");
        return false; 
    }

    if (unary->op == OP_ADD || unary->op == OP_SUB) {
        if (operand_type.type != INTEGER && operand_type.type != UNSIGNED_INTEGER &&
            operand_type.type != FLOAT && operand_type.type != DOUBLE &&
            operand_type.type != NUMERIC) {
            printf("bind_unary: Invalid numeric operand type.\n");
            return false; 
        }

        if (unary->op == OP_SUB && operand_type.type == UNSIGNED_INTEGER) {
            printf("bind_unary: Cannot negate an unsigned integer.\n");
            return false;
        }
    }
    else if (unary->op == OP_NOT) {
        if (operand_type.type != BOOL) {
            printf("bind_unary: Invalid boolean operand type.\n");
            return false;
        }
    }
    else if (unary->op == OP_BITWISE_NOT) {
        if (operand_type.type != INTEGER &&
            operand_type.type != UNSIGNED_INTEGER) {
            printf("bind_unary: Invalid inner expression type for bitwise NOT operator.\n");
            return false;
        }
    }

    out_type->type = unary->op == OP_NOT ? BOOL : operand_type.type;
    out_type->is_untyped_null = false;

    return true;
}

// Bind a binary expression
bool bind_binary(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type) {
    if (!expr || expr->type != EXPR_BINARY) {
        printf("bind_binary: Invalid input expression.\n");
        return false;
    }

    if (!context || !context->db || !out_type) {
        printf("bind_binary: Invalid input context or output.\n");
        return false;
    }

    Binary *binary = &expr->expression_data.binary_expr;

    // Validate binary expression components
    if (!binary->left_operand || !binary->right_operand) {
        printf("bind_binary: Invalid binary operands.\n");
        return false;
    }

    // Bind inner operands
    BoundExpressionType left_operand_type = {0}, right_operand_type = {0};

    if (!bind_scalar_expression(binary->left_operand, context, &left_operand_type)) {
        printf("bind_binary: Inner left expression could not be bound.\n");
        return false; 
    }

    if (!bind_scalar_expression(binary->right_operand, context, &right_operand_type)) {
        printf("bind_binary: Inner right expression could not be bound.\n");
        return false; 
    }

    bool left_is_null = left_operand_type.is_untyped_null;
    bool right_is_null = right_operand_type.is_untyped_null;

    // Determine operator category
    bool is_arithmetic_operator = (
        binary->op == OP_ADD || binary->op == OP_SUB ||
        binary->op == OP_MUL || binary->op == OP_DIV ||
        binary->op == OP_MODULO
    );

    bool is_comparison_operator = (
        binary->op == OP_EQ || binary->op == OP_NEQ ||
        binary->op == OP_LT || binary->op == OP_LTE ||
        binary->op == OP_GT || binary->op == OP_GTE
    );

    bool is_logical_operator = binary->op == OP_AND || binary->op == OP_OR;

    // Validate supported binary operators
    if (!is_arithmetic_operator && !is_comparison_operator && !is_logical_operator) {
        printf("bind_binary: Unsupported binary operator.\n");
        return false;
    }

    // Handle two untyped NULL operands
    if (left_is_null && right_is_null) {
        if (is_comparison_operator || is_logical_operator) {
            out_type->type = BOOL;
            out_type->is_untyped_null = false;
            return true;
        }

        printf("bind_binary: Cannot infer numeric type from NULL operands.\n");
        return false;
    }

    // Infer the missing operand type from the known operand
    if (left_is_null) {
        left_operand_type.type = right_operand_type.type;
    }
    else if (right_is_null) {
        right_operand_type.type = left_operand_type.type;
    }

    DataType left_type = left_operand_type.type;
    DataType right_type = right_operand_type.type;

    // Arithmetic operators
    if (is_arithmetic_operator) {
        if (!is_numeric_type(left_type) || !is_numeric_type(right_type)) {
            printf("bind_binary: Invalid numeric operand types.\n");
            return false;
        }

        // Modulo is supported only for integer operands
        if (binary->op == OP_MODULO && (!is_integer_type(left_type) || !is_integer_type(right_type))) {
            printf("bind_binary: Modulo requires integer operands.\n");
            return false;
        }

        // Mixed numeric types require explicit promotion rules
        if (left_type != right_type) {
            printf("bind_binary: Incompatible numeric operand types.\n");
            return false;
        }

        out_type->type = left_type;
    }
    else if (is_logical_operator) {
        // Logical operators
        if (left_type != BOOL || right_type != BOOL) {
            printf("bind_binary: Logical operators require boolean operands.\n");
            return false;
        }

        out_type->type = BOOL;
    }
    else if (is_comparison_operator) {
        // Comparison operators
        bool compatible = false;

        if (binary->op == OP_EQ || binary->op == OP_NEQ) {
            compatible = are_equality_compatible(left_type, right_type);
        }
        else {
            compatible = are_ordering_compatible(left_type, right_type);
        }

        if (!compatible) {
            printf("bind_binary: Incompatible comparison operand types.\n");
            return false;
        }

        out_type->type = BOOL;
    }

    out_type->is_untyped_null = false;

    return true;
}

// Bind an IS NULL expression
bool bind_is_null(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type) {
    if (!expr || expr->type != EXPR_IS_NULL) {
        printf("bind_is_null: Invalid input expression.\n");
        return false;
    }

    if (!context || !context->db || !out_type) {
        printf("bind_is_null: Invalid input context or output.\n");
        return false;
    }

    IsNull *is_null = &expr->expression_data.is_null_expr;

    // Validate IS NULL expression components
    if (!is_null->operand) {
        printf("bind_is_null: Invalid IS NULL operand.\n");
        return false;
    }

    // Bind inner expression
    BoundExpressionType operand_type = {0};

    if (!bind_scalar_expression(is_null->operand, context, &operand_type)) {
        printf("bind_is_null: Inner expression could not be bound.\n");
        return false;
    }

    // IS NULL always returns a boolean
    out_type->type = BOOL;
    out_type->is_untyped_null = false;

    return true;
}

// Bind an IS NOT NULL expression
bool bind_is_not_null(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type) {
    if (!expr || expr->type != EXPR_IS_NOT_NULL) {
        printf("bind_is_not_null: Invalid input expression.\n");
        return false;
    }

    if (!context || !context->db || !out_type) {
        printf("bind_is_not_null: Invalid input context or output.\n");
        return false;
    }

    IsNotNull *is_not_null = &expr->expression_data.is_not_null_expr;

    // Validate IS NULL expression components
    if (!is_not_null->operand) {
        printf("bind_is_not_null: Invalid IS NULL operand.\n");
        return false;
    }

    // Bind inner expression
    BoundExpressionType operand_type = {0};

    if (!bind_scalar_expression(is_not_null->operand, context, &operand_type)) {
        printf("bind_is_not_null: Inner expression could not be bound.\n");
        return false;
    }

    // IS NOT NULL always returns a boolean
    out_type->type = BOOL;
    out_type->is_untyped_null = false;

    return true;
}

// Bind an IN expression
bool bind_in(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type) {
    if (!expr || expr->type != EXPR_IN) {
        printf("bind_in: Invalid input expression.\n");
        return false;
    }

    if (!context || !context->db || !out_type) {
        printf("bind_in: Invalid input context or output.\n");
        return false;
    }

    In *in = &expr->expression_data.in_expr;

    // Validate IN expression components
    if (!in->operand) {
        printf("bind_in: Invalid IN operand.\n");
        return false;
    }

    if (!in->set_options || in->option_count == 0) {
        printf("bind_in: Invalid set of IN options.\n");
        return false;
    }

    // Bind inner expression
    BoundExpressionType operand_type = {0};

    if (!bind_scalar_expression(in->operand, context, &operand_type)) {
        printf("bind_in: Inner expression could not be bound.\n");
        return false;
    }

    // Track a known comparison type
    bool has_known_type = !operand_type.is_untyped_null;
    DataType comparison_type = {0};

    if (has_known_type) {
        comparison_type = operand_type.type;
    }

    // Validate options and their data types
    for (uint32_t i = 0; i < in->option_count; i++) {
        ExpressionNode *option = in->set_options[i];

        if (!option) {
            printf("bind_in: Invalid options set.\n");
            return false;
        }

        // Bind inner option
        BoundExpressionType option_type = {0};

        if (!bind_scalar_expression(option, context, &option_type)) {
            printf("bind_in: Inner option expression could not be bound.\n");
            return false;
        }

        // Untyped NULL options are always acceptable
        if (option_type.is_untyped_null) {
            continue;
        }

        // First known type, when left operand is NULL
        if (!has_known_type) {
            comparison_type = option_type.type;
            has_known_type = true;
            continue;
        }

        // Validate equality compatibility
        if (!are_equality_compatible(comparison_type, option_type.type)) {
            printf("bind_in: Incompatible IN option data type.\n");
            return false;
        }
    }

    // Validate that a known comparison type supports equality
    if (has_known_type && !are_equality_compatible(comparison_type, comparison_type)) {
        printf("bind_in: Unsupported IN comparison data type.\n");
        return false;
    }

    // IN always produces a boolean expression
    out_type->type = BOOL;
    out_type->is_untyped_null = false;

    return true;
}

// Bind a BETWEEN expression
bool bind_between(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type) {
    if (!expr || expr->type != EXPR_BETWEEN) {
        printf("bind_between: Invalid input expression.\n");
        return false;
    }

    if (!context || !context->db || !out_type) {
        printf("bind_between: Invalid input context or output.\n");
        return false;
    }

    Between *between = &expr->expression_data.between_expr;

    // Validate BETWEEN expression components
    if (!between->lower || !between->upper || !between->operand) {
        printf("bind_between: Invalid BETWEEN expression.\n");
        return false;
    }

    // Bind inner operand expression
    BoundExpressionType operand_type = {0};

    if (!bind_scalar_expression(between->operand, context, &operand_type)) {
        printf("bind_between: Inner operand could not be bound.\n");
        return false;
    }

    // Bind inner lower bound expression
    BoundExpressionType lower_type = {0};

    if (!bind_scalar_expression(between->lower, context, &lower_type)) {
        printf("bind_between: Inner lower bound expression could not be bound.\n");
        return false;
    }

    // Bind inner upper bound expression
    BoundExpressionType upper_type = {0};

    if (!bind_scalar_expression(between->upper, context, &upper_type)) {
        printf("bind_between: Inner upper bound expression could not be bound.\n");
        return false;
    }

    // Determine which expressions have known data types
    bool operand_is_null = operand_type.is_untyped_null;
    bool lower_is_null = lower_type.is_untyped_null;
    bool upper_is_null = upper_type.is_untyped_null;

    bool has_known_type = false;
    DataType comparison_type = {0};

    // Use the operand type if it is known
    if (!operand_is_null) {
        comparison_type = operand_type.type;
        has_known_type = true;
    }

    // Validate the lower bound
    if (!lower_is_null) {
        if (!has_known_type) {
            comparison_type = lower_type.type;
            has_known_type = true;
        }
        else if (!are_ordering_compatible(comparison_type, lower_type.type)) {
            printf("bind_between: Incompatible lower bound data type.\n");
            return false;
        }
    }

    // Validate the upper bound
    if (!upper_is_null) {
        if (!has_known_type) {
            comparison_type = upper_type.type;
            has_known_type = true;
        }
        else if (!are_ordering_compatible(comparison_type, upper_type.type)) {
            printf("bind_between: Incompatible upper bound data type.\n");
            return false;
        }
    }

    // Ensure the inferred type supports ordering
    if (has_known_type && !are_ordering_compatible(comparison_type, comparison_type)) {
        printf("bind_between: Unsupported BETWEEN operand data type.\n");
        return false;
    }


    out_type->type = BOOL;
    out_type->is_untyped_null = false;

    return true;
}

// Bind a function expression
bool bind_function(ExpressionNode *expr, BindingContext *context, BoundExpressionType *out_type) {
    if (!expr || expr->type != EXPR_FUNCTIONS) {
        printf("bind_function: Invalid input expression.\n");
        return false;
    }

    if (!context || !context->db || !out_type) {
        printf("bind_function: Invalid input context.\n");
        return false;
    }

    if (context->current_clause == BIND_CLAUSE_WHERE ||
        context->current_clause == BIND_CLAUSE_JOIN_ON ||
        context->current_clause == BIND_CLAUSE_GROUP_BY) {
        printf("bind_function: Aggregate functions are not allowed in this clause.\n");
        return false;
    }

    if (context->aggregate_depth > 0) {
        printf("bind_function: Nested aggregate functions are not allowed.\n");
        return false;
    }

    AggregateFunction *function = &expr->expression_data.aggregate_func_expr;

    // Validate aggregate function type
    if (function->type < EXPR_AGGREGATE_SUM || function->type >= EXPR_AGGREGATE_ERROR) {
        printf("bind_function: Unsupported aggregate function.\n");
        return false;
    }

    // Validate wildcard usage
    if (function->wildcard) {
        if (function->type != EXPR_AGGREGATE_COUNT || function->expression) {
            printf("bind_function: Invalid aggregate wildcard usage.\n");
            return false;
        }

        // COUNT(*) does not require an expression
        out_type->type = UNSIGNED_INTEGER;
        out_type->is_untyped_null = false;
        return true;
    }

    // All non-wildcard aggregates require an expression
    if (!function->expression) {
        printf("bind_function: Missing aggregate argument.\n");
        return false;
    }

    // Bind argument type
    BoundExpressionType argument_type = {0};

    context->aggregate_depth++;

    bool success = bind_scalar_expression(function->expression, context, &argument_type);

    context->aggregate_depth--;
    
    if (!success) {
        printf("bind_function: Aggregate argument could not be bound.\n");
        return false;
    }

    // COUNT accepts all scalar types, including untyped NULL's
    if (function->type == EXPR_AGGREGATE_COUNT) {
        out_type->type = UNSIGNED_INTEGER;
        out_type->is_untyped_null = false;
        return true;
    }

    // Other aggregates require a known argument type
    if (argument_type.is_untyped_null) {
        printf("bind_function: Cannot infer aggregate argument type from NULL.\n");
        return false;
    }

    // Determine aggregate result type
    switch (function->type) {
        case EXPR_AGGREGATE_SUM:
            if (!is_numeric_type(argument_type.type)) {
                printf("bind_function: SUM requires a numeric argument.\n");
                return false;
            }

            out_type->type = argument_type.type;
            break;

        case EXPR_AGGREGATE_AVG:
            if (!is_numeric_type(argument_type.type)) {
                printf("bind_function: AVG requires a numeric argument.\n");
                return false;
            }

            out_type->type = DOUBLE;
            break;

        case EXPR_AGGREGATE_MIN:
        case EXPR_AGGREGATE_MAX:
            if (!supports_ordering(argument_type.type)) {
                printf("bind_function: MIN/MAX requires an orderable argument.\n");
                return false;
            }

            out_type->type = argument_type.type;
            break;

        default:
            printf("bind_function: Unsupported aggregate function.\n");
            return false;
    }

    out_type->is_untyped_null = false;
    return true;
}

/* ---------- Binding of each ASTNodeType ---------- */


bool bind_select(ASTNode *root, BindingContext *context, BoundProjection *out_projection) {
    if (!root || root->type != AST_SELECT) {
        printf("bind_select: Invalid AST root node.\n");
        return false;
    }

    if (!context || !context->db || !out_projection) {
        printf("bind_select: Invalid context or output.\n");
        return false;
    }

    // The caller must provide an empty output and a fresh context.
    if (out_projection->columns || out_projection->num_columns != 0 ||
        context->relations || context->num_relations != 0 ||
        context->aggregate_depth != 0) {
        printf("bind_select: Expected fresh binding state.\n");
        return false;
    }

    SelectNode *select = &root->node_contents.select;
    BoundProjection projection_result = {0};
    bool success = false;

    // Bind FROM clause
    context->current_clause = BIND_CLAUSE_SELECT;

    if (!bind_from(select->from, context)) {
        printf("bind_select: FROM binding failed.\n");
        goto cleanup;
    }

    // Bind optional multiple JOIN ... ON clauses
    if (select->num_joins > 0) {
        if (!select->joins || !bind_joins(select->joins, select->num_joins, context)) {
            printf("bind_select: JOIN binding failed.\n");
            goto cleanup;
        }
    }
    else if (select->joins) {
        printf("bind_select: Unexpected JOIN array.\n");
        goto cleanup;
    }

    // Bind optional WHERE clause
    if (select->where) {
        context->current_clause = BIND_CLAUSE_WHERE;

        if (!bind_where(select->where, context)) {
            printf("bind_select: WHERE binding failed.\n");
            goto cleanup;
        }
    }

    // Bind optional GROUP BY clause
    if (select->group_by) {
        context->current_clause = BIND_CLAUSE_GROUP_BY;

        if (!bind_group_by(select->group_by, context)) {
            printf("bind_select: GROUP BY binding failed.\n");
            goto cleanup;
        }
    }

    // Bind SELECT projection clause
    context->current_clause = BIND_CLAUSE_SELECT;

    if (!bind_projection(select->projection, context, &projection_result)) {
        printf("bind_select: Projection binding failed.\n");
        goto cleanup;
    }

    // Bind optional HAVING clause
    if (select->having) {
        context->current_clause = BIND_CLAUSE_HAVING;

        if (!bind_having(select->having, context)) {
            printf("bind_select: HAVING binding failed.\n");
            goto cleanup;
        }
    }

    // Bind optional ORDER BY clause
    if (select->order_by) {
        context->current_clause = BIND_CLAUSE_ORDER_BY;

        if (!bind_order_by(select->order_by, context)) {
            printf("bind_select: ORDER BY binding failed.\n");
            goto cleanup;
        }
    }

    // Determine whether the query requires grouping rules
    bool has_aggregates = false;

    for (uint32_t i = 0; i < select->projection->num_expressions; i++) {
        if (expression_contains_aggregate(select->projection->expressions[i])) {
            has_aggregates = true;
            break;
        }
    }

    if (select->having && expression_contains_aggregate(select->having->expression)) {
        has_aggregates = true;
    }

    if (select->order_by) {
        for (uint32_t i = 0; i < select->order_by->num_column_refs; i++) {
            if (expression_contains_aggregate(select->order_by->column_refs[i])) {
                has_aggregates = true;
                break;
            }
        }
    }

    bool requires_group_validation = 
        select->group_by != NULL || select->having != NULL || has_aggregates;

    // Cross-clause grouping validation
    if (requires_group_validation) {
        if (!validate_grouped_projection(select->projection, select->group_by, context)) {
            printf("bind_select: Invalid grouped projection.\n");
            goto cleanup;
        }

        if (select->having && !validate_grouped_expression(select->having->expression, select->group_by)) {
            printf("bind_select: Invalid grouped HAVING.\n");
            goto cleanup;
        }

        if (select->order_by && !validate_grouped_order_by(select->order_by, select->group_by)) {
            printf("bind_select: Invalid grouped ORDER BY.\n");
            goto cleanup;
        }
    }

    // Finalize projection output
    *out_projection = projection_result;
    success = true;

cleanup:
    if (!success) {
        free(projection_result.columns);
    }

    context->current_clause = BIND_CLAUSE_SELECT;
    return success;
}


bool bind_insert(ASTNode *root, BindingContext *context);

bool bind_update(ASTNode *root, BindingContext *context);

bool bind_delete(ASTNode *root, BindingContext *context);

bool bind_create_table(ASTNode *root, BindingContext *context);

bool bind_drop_table(ASTNode *root, BindingContext *context);

bool bind_alter_table(ASTNode *root, BindingContext *context);

bool bind_truncate_table(ASTNode *root, BindingContext *context);

bool bind_create_index(ASTNode *root, BindingContext *context);

bool bind_drop_index(ASTNode *root, BindingContext *context);

/* ---------- Parsing of Inner Query Components ---------- */

// Bind multiple JOIN clauses
bool bind_joins(JoinNode **joins, uint32_t num_joins, BindingContext *context) {
    if (!joins || num_joins == 0) {
        printf("bind_joins: Invalid Join AST nodes pointer array.\n");
        return false;
    }

    if (!context || !context->db || !context->relations || context->num_relations == 0) {
        printf("bind_joins: Invalid binding context.\n");
        return false;
    }

    for (uint32_t i = 0; i < num_joins; i++) {
        JoinNode *join = joins[i];

        if (!join || join->secondary_table_name[0] == '\0') {
            printf("bind_joins: Invalid JOIN node.\n");
            return false;
        }

        // Validate JOIN type
        if (join->join_type != INNER && join->join_type != LEFT &&
            join->join_type != RIGHT && join->join_type != OUTER) {
            printf("bind_joins: Unsupported JOIN type.\n");
            return false;
        }

        // Require an ON condition for supported JOINs
        if (!join->on || !join->on->expression) {
            printf("bind_joins: Missing JOIN ON condition.\n");
            return false;
        }

        if (!register_binding_relation(join->secondary_table_name, join->alias.alias, context)) {
            printf("bind_joins: Joined table could not be registered.\n");
            return false;
        }

        BindingClause previous_clause = context->current_clause;

        context->current_clause = BIND_CLAUSE_JOIN_ON;

        bool success = bind_on(join->on, context);

        context->current_clause = previous_clause;

        if (!success) {
            printf("bind_joins: JOIN ON condition could not be bound.\n");
            return false;
        }
    }

    return true;
}

// Bind ON clause
bool bind_on(OnNode *on, BindingContext *context) {
    if (!on || !on->expression) {
        printf("bind_on: Invalid On AST node.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_on: Invalid binding context.\n");
        return false;
    }

    BoundExpressionType result_type = {0};

    if (!bind_scalar_expression(on->expression, context, &result_type)) {
        printf("bind_on: ON expression could not be bound.\n");
        return false;
    }

    if (result_type.is_untyped_null || result_type.type != BOOL) {
        printf("bind_on: ON expression must have a boolean type.\n");
        return false;
    }

    return true;
}

// Bind SELECT Projection expressions 
bool bind_projection(ProjectionNode *projection, BindingContext *context, BoundProjection *out_projection) {
    if (!projection || 
        !projection->expressions || 
        !projection->aliases ||
        projection->num_expressions == 0) {
        printf("bind_projection: Invalid Projection AST node.\n");
        return false;
    }

    if (!context || !context->db || !out_projection) {
        printf("bind_projection: Invalid binding context or output.\n");
        return false;   
    }

    if (out_projection->columns || out_projection->num_columns != 0) {
        printf("bind_projection: Output must be initially empty.\n");
        return false;
    }

    if ((size_t)projection->num_expressions > SIZE_MAX / sizeof(BoundProjectionColumn)) {
        printf("bind_projection: Projection allocation overflow.\n");
        return false;
    }

    BoundProjectionColumn *columns = (BoundProjectionColumn *) calloc(
        projection->num_expressions,
        sizeof(BoundProjectionColumn)
    );

    if (!columns) {
        printf("bind_projection: Could not allocate output columns.\n");
        return false;
    }

    for (uint32_t i = 0; i < projection->num_expressions; i++) {
        ExpressionNode *expr = projection->expressions[i];
        AsNode *alias = &projection->aliases[i];

        if (!expr) {
            printf("bind_projection: Invalid projection expression\n");
            free(columns);
            return false;
        }

        BoundExpressionType result_type = {0};

        if (!bind_scalar_expression(expr, context, &result_type)) {
            printf("bind_projection: Projection expression could not be bound.\n");
            free(columns);
            return false;
        }   

        columns[i].expression = expr;
        columns[i].result_type = result_type;

        // Explicit alias takes precedence
        if (alias->alias[0] != '\0') {
            strncpy(columns[i].output_name, alias->alias, sizeof(columns[i].output_name) - 1);
        }
        else if (expr->type == EXPR_COLUMN_REF) {
            strncpy(
                columns[i].output_name,
                expr->expression_data.column_value.column_name,
                sizeof(columns[i].output_name) - 1
            );
        }

        // Other expressions without aliases retain an empty name
        // until a display-name policy is implemented.
    }

    out_projection->columns = columns;
    out_projection->num_columns = projection->num_expressions;

    return true;
}

// Bind FROM clause
bool bind_from(FromNode *from, BindingContext *context) {
    if (!from || !from->expressions || !from->aliases || from->num_expressions == 0) {
        printf("bind_from: Invalid FROM AST node.\n");
        return false;
    }

    if (!context || !context->db || (context->num_relations > 0 && !context->relations)) {
        printf("bind_from: Invalid binding context.\n");
        return false;
    }

    for (uint32_t i = 0; i < from->num_expressions; i++) {
        ExpressionNode *expr = from->expressions[i];
        AsNode *alias = &from->aliases[i];

        if (!expr || expr->type != EXPR_TABLE_REF) {
            printf("bind_from: Invalid table reference expression.\n");
            return false;
        }

        const char *table_name = expr->expression_data.table_value.table_name;

        if (!register_binding_relation(table_name, alias->alias, context)) {
            printf("bind_from: Table could not be registered.\n");
            return false;
        }
    }

    return true;
}

// Bind WHERE clause
bool bind_where(WhereNode *where, BindingContext *context) {
    if (!where || !where->expression) {
        printf("bind_where: Invalid WHERE AST node.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_where: Invalid binding context.\n");
        return false;
    }

    BoundExpressionType result_type = {0};

    if (!bind_scalar_expression(where->expression, context, &result_type)) {
        printf("bind_where: WHERE expression could not be bound.\n");
        return false;
    }

    if (result_type.is_untyped_null || result_type.type != BOOL) {
        printf("bind_where: WHERE expression must have a boolean type.\n");
        return false;
    }

    return true;
}

// Bind GROUP BY clause
bool bind_group_by(GroupByNode *group_by, BindingContext *context) {
    if (!group_by || !group_by->column_refs || group_by->num_column_refs == 0) {
        printf("bind_group_by: Invalid GROUP BY AST node.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_group_by: Invalid binding context.\n");
        return false;
    }

    for (uint32_t i = 0; i < group_by->num_column_refs; i++) {
        ExpressionNode *expr = group_by->column_refs[i];

        if (!expr) {
            printf("bind_group_by: Invalid GROUP BY expression.\n");
            return false;
        }

        BoundExpressionType result_type = {0};

        if (!bind_scalar_expression(expr, context, &result_type)) {
            printf("bind_group_by: GROUP BY expression could not be bound.\n");
            return false;
        }

        // Grouping expressions must support equality comparison
        // Untyped NULL is allowed as a constant grouping expression.
        if (!result_type.is_untyped_null && 
            !are_equality_compatible(result_type.type, result_type.type)) {
            printf("bind_group_by: Unsupported GROUP BY expression type.\n");
            return false;
        }
    }

    return true;
}

// Bind HAVING clause
bool bind_having(HavingNode *having, BindingContext *context) {
    if (!having || !having->expression) {
        printf("bind_having: Invalid HAVING AST node.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_having: Invalid binding context.\n");
        return false;
    }

    BoundExpressionType result_type = {0};

    if (!bind_scalar_expression(having->expression, context, &result_type)) {
        printf("bind_having: HAVING expression could not be bound.\n");
        return false;
    }

    if (result_type.is_untyped_null || result_type.type != BOOL) {
        printf("bind_having: HAVING expression must have a boolean type.\n");
        return false;
    }

    return true;
}

// Bind ORDER BY clause
bool bind_order_by(OrderByNode *order_by, BindingContext *context) {
    if (!order_by ||
        !order_by->column_refs ||
        !order_by->types ||
        order_by->num_column_refs == 0) {
        printf("bind_order_by: Invalid ORDER BY AST node.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_order_by: Invalid binding context.\n");
        return false;
    }

    for (uint32_t i = 0; i < order_by->num_column_refs; i++) {
        ExpressionNode *expr = order_by->column_refs[i];
        OrderByType direction = order_by->types[i];

        if (!expr) {
            printf("bind_order_by: Invalid ORDER BY expression.\n");
            return false;
        }

        // Validate sorting direction
        if (direction != ASC && direction != DESC) {
            printf("bind_order_by: Unsupported ORDER BY direction.\n");
            return false;
        }

        BoundExpressionType result_type = {0};

        if (!bind_scalar_expression(expr, context, &result_type)) {
            printf("bind_order_by: ORDER BY expression could not be bound.\n");
            return false;
        }

        // Validate ordering compatibility
        if (!result_type.is_untyped_null && !supports_ordering(result_type.type)) {
            printf("bind_order_by: ORDER BY expression type does not support ordering.\n");
            return false;
        }
    }

    return true;
}

bool bind_into(IntoNode *into, BindingContext *context);

bool bind_values(ValuesNode *values, BindingContext *context);

bool bind_set(SetNode *set, BindingContext *context);

bool bind_columns(ColumnsNode *columns, BindingContext *context);

bool bind_column_def(ColumnDefNode *column_def, BindingContext *context);

bool bind_constraint(ConstraintNode *constraint, BindingContext *context);

bool bind_primary_key_constraint(ConstraintNode *constraint, BindingContext *context);

bool bind_unique_constraint(ConstraintNode *constraint, BindingContext *context);

bool bind_not_null_constraint(ConstraintNode *constraint, BindingContext *context);

bool bind_foreign_key_constraint(ConstraintNode *constraint, BindingContext *context);

bool bind_check_constraint(ConstraintNode *constraint, BindingContext *context);

bool bind_default_constraint(ConstraintNode *constraint, BindingContext *context);

bool bind_alter_action(AlterActionNode *action, BindingContext *context);

bool bind_alter_add_col(AlterActionNode *action, BindingContext *context);

bool bind_alter_drop_col(AlterActionNode *action, BindingContext *context);

bool bind_alter_rename_table(AlterActionNode *action, BindingContext *context);

bool bind_alter_rename_col(AlterActionNode *action, BindingContext *context);

bool bind_alter_modify_col(AlterActionNode *action, BindingContext *context);

bool bind_alter_add_constraint(AlterActionNode *action, BindingContext *context);

bool bind_alter_drop_constraint(AlterActionNode *action, BindingContext *context);


/* ---------- Cross-Clause Validation Helpers ---------- */

// Register a table in the binding context, used by FROM and JOIN binding
bool register_binding_relation(const char *table_name, const char *alias, BindingContext *context) {
    if (!table_name || table_name[0] == '\0' || !alias) {
        printf("register_binding_relation: Invalid table name or alias.\n");
        return false;
    }

    if (!context || !context->db ||
        (context->num_relations > 0 && !context->relations)) {
        printf("register_binding_relation: Invalid binding context.\n");
        return false;
    }

    // Resolve the actual table
    Table *table = database_find_table(context->db, table_name);

    if (!table) {
        printf("register_binding_relation: Table not found.\n");
        return false;
    }

    // Determine visible relation identifier
    const char *identifier = alias[0] != '\0' ? alias : table->name;

    // Reject duplicate visible identifiers
    for (uint32_t i = 0; i < context->num_relations; i++) {
        BindingRelation *existing = &context->relations[i];

        if (!existing->table) {
            printf("register_binding_relation: Invalid existing relation.\n");
            return false;
        }

        const char *existing_identifier =
            existing->alias[0] != '\0' ? existing->alias : existing->table->name;

        if (!strcmp(identifier, existing_identifier)) {
            printf("register_binding_relation: Duplicate relation identifier.\n");
            return false;
        }
    }

    // Ensure relation indexes fit into int32_t
    if (context->num_relations >= INT32_MAX) {
        printf("register_binding_relation: Maximum number of relations reached.\n");
        return false;
    }

    if ((size_t)context->num_relations >= SIZE_MAX / sizeof(BindingRelation)) {
        printf("register_binding_relation: Maximum relations reached.\n");
        return false;
    }

    // Extend context's relations array
    BindingRelation *new_relations = realloc(
        context->relations,
        ((size_t)context->num_relations + 1) * sizeof(BindingRelation)
    );

    if (!new_relations) {
        printf("register_binding_relation: Relations array could not be expanded.\n");
        return false;
    }

    context->relations = new_relations;

    BindingRelation *relation = &context->relations[context->num_relations];
    *relation = (BindingRelation){0};
    relation->table = table;

    if (alias[0] != '\0') {
        strncpy(relation->alias, alias, sizeof(relation->alias) - 1);
    }

    context->num_relations++;

    return true;
}

// Validate Projection expressions against GROUP BY expressions
bool validate_grouped_projection(ProjectionNode *projection, GroupByNode *group_by,
    BindingContext *context) {
    if (!projection ||
        !projection->expressions ||
        projection->num_expressions == 0) {
        printf("validate_grouped_projection: Invalid projection AST node.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("validate_grouped_projection: Invalid binding context.\n");
        return false;
    }

    for (uint32_t i = 0; i < projection->num_expressions; i++) {
        ExpressionNode *expr = projection->expressions[i];

        if (!expr) {
            printf("validate_grouped_projection: Invalid projection expression.\n");
            return false;
        }

        if (!validate_grouped_expression(expr, group_by)) {
            printf("validate_grouped_projection: Invalid grouped projection.\n");
            return false;
        }
    }

    return true;
}

// Validate that an expression is legal in a grouped query
bool validate_grouped_expression(ExpressionNode *expr, GroupByNode *group_by) {
    if (!expr) {
        printf("validate_grouped_expression: Invalid arguments.\n");
        return false;
    }

    if (group_by) {
        if (!group_by->column_refs || group_by->num_column_refs == 0) {
            printf("validate_grouped_expression: Invalid GROUP BY structure.\n");
            return false;
        }

        for (uint32_t i = 0; i < group_by->num_column_refs; i++) {
            ExpressionNode *group_expr = group_by->column_refs[i];

            if (!group_expr) {
                printf("validate_grouped_expression: Invalid grouping expression.\n");
                return false;
            }

            // Checking if current expression matches a GROUP BY expression
            if (grouped_expressions_equal(expr, group_expr)) {
                return true;
            }
        }
    }

    // At this point there's no match with GROUP BY
    switch(expr->type) {
        // Constants (EXPR_LITERAL) are independent of individual rows
        case EXPR_LITERAL:
            return true;
        
        // A bare column (EXPR_COLUMN_REF) not matched by GROUP BY is invalid
        case EXPR_COLUMN_REF:
            return false;

        // Aggregate arguments were already bound and type-checked. 
        // Nested aggregates must be rejected separately
        case EXPR_FUNCTIONS:
            return true;

        case EXPR_UNARY: {
            Unary *unary = &expr->expression_data.unary_expr;

            return validate_grouped_expression(unary->operand, group_by);
        }

        case EXPR_BINARY: {
            Binary *binary = &expr->expression_data.binary_expr;

            return (
                validate_grouped_expression(binary->left_operand, group_by) &&
                validate_grouped_expression(binary->right_operand, group_by)
            );
        }

        case EXPR_IS_NULL: {
            IsNull *is_null = &expr->expression_data.is_null_expr;
            
            return validate_grouped_expression(is_null->operand, group_by);
        }

        case EXPR_IS_NOT_NULL: {
            IsNotNull *is_not_null = &expr->expression_data.is_not_null_expr;

            return validate_grouped_expression(is_not_null->operand, group_by);
        }

        case EXPR_IN: {
            In *in = &expr->expression_data.in_expr;

            if (!validate_grouped_expression(in->operand, group_by)) {
                return false;
            }

            if (!in->set_options || in->option_count == 0) {
                return false;
            }

            for (uint32_t i = 0; i < in->option_count; i++) {
                if (!validate_grouped_expression(in->set_options[i], group_by)) {
                    return false;
                }
            }

            return true;
        }

        case EXPR_BETWEEN: {
            Between *between = &expr->expression_data.between_expr;

            return (
                validate_grouped_expression(between->lower, group_by) &&
                validate_grouped_expression(between->operand, group_by) &&
                validate_grouped_expression(between->upper, group_by)
            );
        }

        default: 
            printf("validate_grouped_expression: Unsupported expression type.\n");
            return false;
    }
}

// Determine whether 2 expressions represent the same grouping expression
bool grouped_expressions_equal(const ExpressionNode *left, const ExpressionNode *right) {
    if (!left || !right) {
        return false;
    }

    // Both expressions must have the same AST type
    if (left->type != right->type) {
        return false;
    }

    switch(left->type) {
        case EXPR_LITERAL: {
            const Literal *left_literal = &left->expression_data.literal_value;
            const Literal *right_literal = &right->expression_data.literal_value;

            if (left_literal->is_untyped_null || right_literal->is_untyped_null) {
                return left_literal->is_untyped_null && right_literal->is_untyped_null;
            }

            if (!left_literal->literal || !right_literal->literal) {
                return false;
            }

            if (left_literal->literal->type != right_literal->literal->type) {
                return false;
            }

            return values_equal(left_literal->literal, right_literal->literal);
        }

        case EXPR_COLUMN_REF: {
            const ColumnRef *left_col = &left->expression_data.column_value;
            const ColumnRef *right_col = &right->expression_data.column_value;
            
            return (
                left_col->relation_index >= 0 && 
                left_col->column_index >= 0 &&
                left_col->relation_index == right_col->relation_index &&
                left_col->column_index == right_col->column_index
            );
        }

        case EXPR_UNARY: {
            const Unary *left_un = &left->expression_data.unary_expr;
            const Unary *right_un = &right->expression_data.unary_expr;

            return (
                left_un->op == right_un->op &&
                grouped_expressions_equal(left_un->operand, right_un->operand)
            );
        }

        case EXPR_BINARY: {
            const Binary *left_bin = &left->expression_data.binary_expr;
            const Binary *right_bin = &right->expression_data.binary_expr;

            return (
                left_bin->op == right_bin->op &&
                grouped_expressions_equal(left_bin->left_operand, right_bin->left_operand) &&
                grouped_expressions_equal(left_bin->right_operand, right_bin->right_operand)
            );
        }

        case EXPR_IS_NULL: {
            const IsNull *left_is = &left->expression_data.is_null_expr;
            const IsNull *right_is = &right->expression_data.is_null_expr;

            return grouped_expressions_equal(left_is->operand, right_is->operand);
        }

        case EXPR_IS_NOT_NULL: {
            const IsNotNull *left_is_not = &left->expression_data.is_not_null_expr;
            const IsNotNull *right_is_not = &right->expression_data.is_not_null_expr;

            return grouped_expressions_equal(left_is_not->operand, right_is_not->operand);
        }

        case EXPR_IN: {
            const In *left_in = &left->expression_data.in_expr;
            const In *right_in = &right->expression_data.in_expr;

            if (left_in->option_count != right_in->option_count ||
                left_in->option_count == 0 ||
                !left_in->set_options ||
                !right_in->set_options) {
                return false;
            }

            if (!grouped_expressions_equal(left_in->operand, right_in->operand)) {
                return false;
            }

            for (uint32_t i = 0; i < left_in->option_count; i++) {
                if (!grouped_expressions_equal(left_in->set_options[i], right_in->set_options[i])) {
                    return false;
                }
            }

            return true;
        }

        case EXPR_BETWEEN: {
            const Between *left_between = &left->expression_data.between_expr;
            const Between *right_between = &right->expression_data.between_expr;

            return (
                grouped_expressions_equal(left_between->operand, right_between->operand) &&
                grouped_expressions_equal(left_between->lower, right_between->lower) &&
                grouped_expressions_equal(left_between->upper, right_between->upper)
            );
        }

        default:
            return false;
    }
}


// Validate if a expression contains a nested aggregate expression
bool expression_contains_aggregate(const ExpressionNode *expr) {
    if (!expr) {
        printf("expression_contains_aggregate: Invalid input expression.\n");
        return false;
    }


    switch (expr->type) {
        case EXPR_FUNCTIONS:
            return true;

        case EXPR_LITERAL:
        case EXPR_COLUMN_REF:
            return false;

        case EXPR_UNARY:
            return expression_contains_aggregate(expr->expression_data.unary_expr.operand);

        case EXPR_BINARY:
            return (
                expression_contains_aggregate(expr->expression_data.binary_expr.left_operand) ||
                expression_contains_aggregate(expr->expression_data.binary_expr.right_operand)
            );
        
        case EXPR_IS_NULL:
            return expression_contains_aggregate(expr->expression_data.is_null_expr.operand);

        case EXPR_IS_NOT_NULL:
            return expression_contains_aggregate(expr->expression_data.is_not_null_expr.operand);

        case EXPR_IN: {
            In *in = &expr->expression_data.in_expr;
            
            if (expression_contains_aggregate(in->operand)) {
                return true;
            }

            for (uint32_t i = 0; i < in->option_count; i++) {
                if (expression_contains_aggregate(in->set_options[i])) {
                    return true;
                }
            }

            return false;
        }

        case EXPR_BETWEEN: {
            Between *between = &expr->expression_data.between_expr;

            return (
                expression_contains_aggregate(between->lower) ||
                expression_contains_aggregate(between->operand) ||
                expression_contains_aggregate(between->upper)
            );
        }

        default:
            return false;
    }
}

// Validate ORDER BY clause in a grouped query
bool validate_grouped_order_by(OrderByNode *order_by, GroupByNode *group_by) {
    if (!order_by || !order_by->column_refs || order_by->num_column_refs == 0) {
        printf("validate_grouped_order_by: Invalid ORDER BY node.\n");
        return false;
    }

    for (uint32_t i = 0; i < order_by->num_column_refs; i++) {
        if (!validate_grouped_expression(order_by->column_refs[i], group_by)) {
            printf("validate_grouped_order_by: Invalid grouped ORDER BY expression.\n");
            return false;
        }
    }

    return true;
}