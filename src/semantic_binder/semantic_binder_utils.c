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
#include "../../include/index.h"
#include "../../include/schema.h"
#include "../../include/constraints.h"
#include "../src/constraints/constraints_utils.h"
#include "../src/btree/btree_utils.h"


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

    if (context->current_clause == BIND_CLAUSE_INSERT_VALUES ||
        context->current_clause == BIND_CLAUSE_DEFAULT) {
        printf("bind_column_ref: Column references are not allowed in VALUES.\n");
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

    // Validate Column Reference in a CHECK constraint context
    if (context->current_clause == BIND_CLAUSE_CHECK) {
        if (context->num_relations != 1) {
            printf("bind_column_ref: Invalid number of relations during CHECK constraint binding.\n");
            return false;
        }

        const BindingRelation *relation = &context->relations[0];
        const CreateTableNode *create_table = relation->create_table;

        // CHECK in CREATE TABLE
        if (create_table) {
            if (relation->proposed_schema || relation->proposed_column || relation->table) {
                printf("bind_column_ref: Invalid ALTER TABLE metadata for CHECK binding in CREATE TABLE.\n");
                return false;
            }

            if (column_ref->table_name[0] != '\0' && 
                strcmp(column_ref->table_name, create_table->table_name)) {
                printf("bind_column_ref: Invalid CHECK table qualifier in CREATE TABLE.\n");
                return false;
            }

            uint32_t index;

            if (!resolve_create_table_column(create_table, column_ref->column_name, &index)) {
                printf("bind_column_ref: CHECK column resolution in CREATE TABLE failed.\n");
                return false;
            }

            if (index > INT32_MAX) {
                printf("bind_column_ref: column index exceeds maximum limit in CREATE TABLE.\n");
                return false;
            }

            ColumnDefNode *column = create_table->columns->column_defs[index];

            if (!column) {
                printf("bind_column_ref: Invalid Column definition node in CREATE TABLE.\n");
                return false;
            }

            out_type->type = column->type;
            out_type->is_untyped_null = false;

            return true;
        }

        // CHECK in ALTER TABLE
        if (!relation->table || !relation->proposed_schema ||
            !relation->proposed_schema->columns) {
            printf("bind_column_ref: Invalid ALTER TABLE CHECK metadata.\n");
            return false;
        }

        if (column_ref->table_name[0] != '\0' &&
            strcmp(column_ref->table_name, binding_relation_current_name(relation))) {
            printf("bind_column_ref: Invalid CHECK table qualifier.\n");
            return false;
        }

        uint32_t index = 0;
        DataType type = {0};
        bool resolved = false;

        if (relation->proposed_column) {
            // ADD COLUMN: resolve against existing columns and the proposed new column
            resolved = resolve_alter_candidate_column(
                relation->proposed_schema,
                relation->proposed_column,
                column_ref->column_name,
                &index,
                &type
            );
        }
        else {
            // ADD CONSTRAINT: resolve against working_schema only.
            int32_t found = schema_find_column_index(relation->proposed_schema, column_ref->column_name);

            if (found >= 0) {
                index = (uint32_t)found;

                const Column *column = relation->proposed_schema->columns[index];

                if (!column) {
                    return false;
                }

                type = column->type;
                resolved = true;
            }
        }

        if (!resolved || index > INT32_MAX) {
            printf("bind_column_ref: CHECK column resolution failed.\n");
            return false;
        }

        column_ref->relation_index = 0;
        column_ref->column_index = (int32_t)index;

        out_type->type = type;
        out_type->is_untyped_null = false;

        return true;
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
            if (unary->operand->type != EXPR_LITERAL) {
                printf("bind_unary: Negation of non-literal unsigned expressions is not supported.\n");
                return false;
            }

            Value *value = unary->operand->expression_data.literal_value.literal;

            if (!value || value->type != UNSIGNED_INTEGER || value->null_val) {
                return false;
            }

            uint32_t magnitude = value->value.uint32_val;

            if (magnitude > (uint32_t)INT32_MAX + 1U) {
                printf("bind_unary: Negated integer exceeds signed 32-bit range.\n");
                return false;
            }

            out_type->type = INTEGER;
            out_type->is_untyped_null = false;
            return true; 
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
            printf("bind_binary: Arithmetic operands must be numeric.\n");
            return false;
        }

        // In this 1st version only integer operands are allowed to be used with modulo
        if (binary->op == OP_MODULO &&
            (!is_integer_type(left_type) || !is_integer_type(right_type))) {
            printf("bind_binary: Modulo requires integer operands.\n");
            return false;
        }

        DataType result_type;

        if ((left_type == INTEGER && right_type == UNSIGNED_INTEGER) ||
            (left_type == UNSIGNED_INTEGER && right_type == INTEGER)) {
            result_type = NUMERIC;
        }
        else if ((left_type == FLOAT && right_type == NUMERIC) ||
                 (left_type == NUMERIC && right_type == FLOAT)) {
            result_type = DOUBLE;
        }
        else if (left_type == right_type) {
            result_type = left_type;
        }
        else if (value_types_assignable(left_type, right_type)) {
            result_type = left_type;
        }
        else if (value_types_assignable(right_type, left_type)) {
            result_type = right_type;
        }
        else {
            printf("bind_binary: Incompatible numeric operand types.\n");
            return false;
        }

        out_type->type = result_type;
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
        context->current_clause == BIND_CLAUSE_GROUP_BY ||
        context->current_clause == BIND_CLAUSE_INSERT_VALUES ||
        context->current_clause == BIND_CLAUSE_SET ||
        context->current_clause == BIND_CLAUSE_CHECK ||
        context->current_clause == BIND_CLAUSE_DEFAULT) {
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

// Bind SELECT query
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

// Bind INSERT query
bool bind_insert(ASTNode *root, BindingContext *context, BoundInsertionValues *out_values) {
    if (!root || root->type != AST_INSERT) {
        printf("bind_insert: Invalid AST root node.\n");
        return false;
    }

    if (!context || !context->db || !out_values) {
        printf("bind_insert: Invalid context or output.\n");
        return false;
    }

    if (context->relations || context->num_relations != 0 || 
        context->aggregate_depth != 0 ||
        out_values->table || out_values->values || out_values->num_values != 0) {
        printf("bind_insert: Expected fresh binding state.\n");
        return false;
    }

    InsertNode *insert = &root->node_contents.insert;

    if (!insert->into || !insert->values ||
        !insert->values->values ||
        insert->values->num_values == 0) {
        printf("bind_insert: Invalid INSERT AST structure.\n");
        return false;
    }

    BoundExpressionType *value_types = NULL;
    bool success = false;

    // Bind INTO clause
    if (!bind_into(insert->into, context)) {
        printf("bind_insert: INTO binding failed.\n");
        return false;
    }

    if ((size_t)insert->values->num_values > SIZE_MAX / sizeof(BoundExpressionType)) {
        printf("bind_insert: Value type allocation overflow.\n");
        return false;
    }

    value_types = calloc(insert->values->num_values, sizeof(BoundExpressionType));

    if (!value_types) {
        printf("bind_insert: Data types of values array could not be allocated.\n");
        return false;
    }
    
     // Bind VALUES expressions under the appropriate clause
    BindingClause previous_clause = context->current_clause;
    context->current_clause = BIND_CLAUSE_INSERT_VALUES;

    success = bind_values(insert->values, context, value_types);

    context->current_clause = previous_clause;

    if (!success) {
        printf("bind_insert: VALUES binding failed.\n");
        free(value_types);
        return false;
    }

    // Validate assignments to explicitly specified columns
    if (!validate_insert_assignments(insert->into, insert->values, value_types, context)) {
        printf("bind_insert: Invalid compatibility between columns and assigned values.\n");
        free(value_types);
        return false;
    }

    // Construct complete schema-ordered insertion values
    if (!build_bound_insertion_values(insert->into, insert->values, value_types, context, out_values)) {
        printf("bind_insert: Insertion values could not be constructed.\n");
        free(value_types);
        return false;
    }

    free(value_types);
    return true;
}

// Bind UPDATE query
bool bind_update(ASTNode *root, BindingContext *context, BoundUpdateValues *out_values) {
    if (!root || root->type != AST_UPDATE) {
        printf("bind_update: Invalid AST root node.\n");
        return false;
    }

    if (!context || !context->db || !out_values) {
        printf("bind_update: Invalid context or output.\n");
        return false;
    }

    // Empty context is required
    if (context->relations || context->num_relations != 0 || 
        context->aggregate_depth != 0 ||
        out_values->table || out_values->values || out_values->num_values != 0) {
        printf("bind_update: Expected fresh binding state.\n");
        return false;
    }

    UpdateNode *update = &root->node_contents.update;

    if (!update->set || !update->set->assignments || update->set->num_assignments == 0) {
        printf("bind_update: Invalid SET clause.\n");
        return false;
    }

    BoundExpressionType *value_types = NULL;
    bool success = false;

    if ((size_t)update->set->num_assignments > SIZE_MAX / sizeof(BoundExpressionType)) {
        printf("bind_update: Value type allocation overflow.\n");
        return false;
    }

    value_types = calloc(update->set->num_assignments, sizeof(BoundExpressionType));

    if (!value_types) {
        printf("bind_update: Data types of values array could not be allocated.\n");
        return false;
    }
    
    // Bind table name and optional alias
    char *table_name = update->table_name;
    AsNode alias = update->alias;

    if (table_name[0] == '\0') {
        printf("bind_from: Invalid table reference expression.\n");
        free(value_types);
        return false;
    }

    if (!register_binding_relation(table_name, alias.alias, context)) {
        printf("bind_from: Table could not be registered.\n");
        free(value_types);
        return false;
    }

    BindingClause previous_clause = context->current_clause;
    context->current_clause = BIND_CLAUSE_SET;

    success = bind_set(update->set, context, value_types);

    context->current_clause = previous_clause;

    // Bind SET clause
    if (!success) {
        printf("bind_update: Set expressions could not be bound.\n");
        free(value_types);
        return false;
    }
    
    // Bind optional WHERE clause
    if (update->where) {
        context->current_clause = BIND_CLAUSE_WHERE;

        success = bind_where(update->where, context);

        context->current_clause = previous_clause;

        if (!success) {
            printf("bind_update: WHERE binding failed.\n");
            free(value_types);
            return false;
        }
    }

    // Validate update assignment types
    if (!validate_update_assignments(update->set, value_types, context)) {
        printf("bind_update: Invalid UPDATE assignments.\n");
        free(value_types);
        return false;
    }

    // Construct complete updated values
    if (!build_bound_update_values(update->set, value_types, context, out_values)) {
        printf("bind_update: Updated values could not be constructed.\n");
        free(value_types);
        return false;
    }


    free(value_types);
    return true;
}

// Bind DELETE query
bool bind_delete(ASTNode *root, BindingContext *context, BoundDeleteValues *out_values) {
    if (!root || root->type != AST_DELETE) {
        printf("bind_delete: Invalid AST root node.\n");
        return false;
    }

    if (!context || !context->db || !out_values) {
        printf("bind_delete: Invalid context or output.\n");
        return false;
    }

    // Empty context is required
    if (context->relations || context->num_relations != 0 || 
        context->aggregate_depth != 0 ||
        out_values->table) {
        printf("bind_delete: Expected fresh binding state.\n");
        return false;
    }

    DeleteNode *delete = &root->node_contents.delete;    
    
    if (!delete->from || !delete->from->expressions || delete->from->num_expressions != 1) {
        printf("bind_delete: Invalid DELETE FROM clause.\n");
        return false;
    }

    if (!delete->from || !delete->from->expressions || !delete->from->aliases ||
        delete->from->num_expressions != 1) {
        printf("bind_delete: Invalid DELETE FROM clause.\n");
        return false;
    }

    // Bind FROM clause
    if (!bind_from(delete->from, context)) {
        printf("bind_delete: FROM binding failed.\n");
        return false;
    }

    // Bind optional WHERE clause
    if (delete->where) {
        BindingClause previous_clause = context->current_clause;
        context->current_clause = BIND_CLAUSE_WHERE;

        bool success = bind_where(delete->where, context);

        context->current_clause = previous_clause;

        if (!success) {
            printf("bind_delete: WHERE binding failed.\n");
            return false;
        }
    }

    // Construct output
    if (!build_bound_delete_values(delete->where, context, out_values)) {
        printf("bind_delete: DELETE output could not be constructed.\n");
        return false;
    }

    return true;
}

// Bind CREATE TABLE
bool bind_create_table(ASTNode *root, BindingContext *context) {
    if (!root || root->type != AST_CREATE_TABLE) {
        printf("bind_create_table: Invalid AST root node.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_create_table: Invalid binding context.\n");
        return false;
    }

    // Empty context is required
    if (context->relations || context->num_relations != 0 || context->aggregate_depth != 0) {
        printf("bind_create_table: Expected fresh binding state.\n");
        return false;
    }

    CreateTableNode *create_table = &root->node_contents.create_table;

    if (!create_table || create_table->table_name[0] == '\0' || 
        !create_table->columns || !create_table->columns->column_defs ||
        create_table->columns->num_column_defs == 0 ||
        (create_table->num_constraints > 0 && !create_table->constraints)) {
        printf("bind_create_table: Invalid CREATE TABLE node.\n");
        return false;
    }

    // Check for potential duplicate table name
    if (database_find_table(context->db, create_table->table_name)) {
        printf("bind_create_table: Duplicate table name.\n");
        return false;
    }

    // Validate column definitions and column-level constraints
    if (!bind_columns(create_table->columns, create_table, context)) {
        printf("bind_create_table: Columns binding failed.\n");
        return false;
    }

    // Validate table-level constraints
    for (uint32_t i = 0; i < create_table->num_constraints; i++) {
        if (!bind_constraint(create_table->constraints[i], create_table, context, -1)) {
            printf("bind_create_table: Table-level constraint failed.\n");
            return false;
        }
    }

    // Validate rules spanning multiple columns
    if (!validate_primary_key_count(create_table)) {
        printf("bind_create_table: Multiple PRIMARY KEY constraints.\n");
        return false;
    }

    if (!validate_create_table_constraint_names(create_table)) {
        printf("bind_create_table: Duplicate constraint names.\n");
        return false;
    }

    if (!validate_create_table_defaults(create_table)) {
        printf("bind_create_table: Conflicting DEFAULT NULL among column-level and table-level constraints.\n");
        return false;
    }

    // Publish resolved indexes after all validation succeeds
    if (!commit_create_table_references(create_table, context->db)) {
        printf("bind_create_table: Reference finalization failed.\n");
        return false;
    }

    return true;
}

// Bind DROP TABLE query
bool bind_drop_table(ASTNode *root, BindingContext *context, BoundDropTable *drop) {
    if (!root || root->type != AST_DROP_TABLE) {
        printf("bind_drop_table: Invalid AST root node.\n");
        return false;
    }

    if (!context || !context->db || !drop) {
        printf("bind_drop_table: Invalid binding context or output.\n");
        return false;
    }

    // Empty context and bound statement are required
    if (context->relations || context->num_relations != 0 || context->aggregate_depth != 0) {
        printf("bind_drop_table: Expected fresh binding state.\n");
        return false;
    }

    if (drop->table || drop->drop_table) {
        printf("bind_drop_table: Invalid initialized output.\n");
        return false;
    }

    DropTableNode *drop_table = &root->node_contents.drop_table;

    if (!drop_table || drop_table->table_name[0] == '\0') {
        printf("bind_drop_table: Invalid drop table node or target table name.\n");
        return false;
    }

    if (!register_binding_relation(drop_table->table_name, "", context)) {
        printf("bind_drop_table: Table could not be registered.\n");
        return false;
    }

    Table *table = context->relations[0].table;

    if (!table || !table->table_schema) {
        printf("bind_drop_table: Invalid target table.\n");
        return false;
    }

    // Reject DROP TABLE if another table references it
    if (has_external_incoming_fk(context->db, table)) {
        printf("bind_drop_table: Target table has incoming FOREIGN KEY dependencies.\n");
        return false;
    }

    // Publish validated output
    drop->table = table;
    drop->drop_table = drop_table;

    return true;
}

// Bind TRUNCATE TABLE query
bool bind_truncate_table(ASTNode *root, BindingContext *context, BoundTruncateTable *truncate) {
    if (!root || root->type != AST_TRUNCATE_TABLE) {
        printf("bind_truncate_table: Invalid AST root node.\n");
        return false;
    }

    if (!context || !context->db || !truncate) {
        printf("bind_truncate_table: Invalid binding context or output.\n");
        return false;
    }

    // Empty context and bound statement are required
    if (context->relations || context->num_relations != 0 || context->aggregate_depth != 0) {
        printf("bind_truncate_table: Expected fresh binding state.\n");
        return false;
    }

    if (truncate->table || truncate->truncate_table) {
        printf("bind_truncate_table: Invalid assigned output.\n");
        return false;
    }

    TruncateTableNode *truncate_table = &root->node_contents.truncate_table;

    if (!truncate_table || truncate_table->table_name[0] == '\0') {
        printf("bind_truncate_table: Invalid drop table node or target table name.\n");
        return false;
    }

    if (!register_binding_relation(truncate_table->table_name, "", context)) {
        printf("bind_truncate_table: Table could not be registered.\n");
        return false;
    }

    Table *table = context->relations[0].table;

    if (has_external_incoming_fk(context->db, table)) {
        printf("bind_truncate_table: Target table has incoming FOREIGN KEY dependencies.\n");
        return false;
    }

    // Publish validated output
    truncate->table = table;
    truncate->truncate_table = truncate_table;

    return true;
}

// Bind ALTER TABLE
bool bind_alter_table(ASTNode *root, BindingContext *context, BoundAlterTable *out_alter) {
    if (!root || root->type != AST_ALTER_TABLE || !out_alter) {
        printf("bind_alter_table: Invalid AST root node or output.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_alter_table: Invalid binding context.\n");
        return false;
    }

    // Empty context is required
    if (context->relations || context->num_relations != 0 || context->aggregate_depth != 0) {
        printf("bind_alter_table: Expected fresh binding state.\n");
        return false;
    }

    AlterTableNode *alter_table = &root->node_contents.alter_table;

    if (!alter_table || alter_table->table_name[0] == '\0' ||
        !alter_table->actions || alter_table->num_actions == 0) {
        printf("bind_alter_table: Invalid ALTER TABLE node.\n");
        return false;
    }

    // Bind table name
    if (!register_binding_relation(alter_table->table_name, "", context)) {
        printf("bind_alter_table: Table binding relation registration failed.\n");
        return false;
    }

    Table *table = context->relations[0].table;
    
    if (!table || !table->table_schema) {
        printf("bind_alter_table: Invalid Table.\n");
        return false;
    }

    // Initialize proposed table name as the current table name
    BindingRelation *relation = &context->relations[0];

    if (strlen(table->name) >= sizeof(relation->proposed_table_name)) {
        return false;
    }

    strcpy(relation->proposed_table_name, table->name);

    // Deep copy table schema to update the copy in case of multiple alteration actions
    Schema *working_schema = schema_copy(table->table_schema);

    if (!working_schema) {
        return false;
    }

    // Initialize column mapping before a potential column addition/removal
    AlterColumnMap map = {0};

    if (!alter_column_map_init(&map, table->table_schema->num_columns)) {
        schema_free(working_schema);
        return false;
    }

    context->alter_column_map = &map;

    bool success = false;

    // Bind potentially multiple ALTER actions sequentially
    for (uint32_t i = 0; i < alter_table->num_actions; i++) {
        AlterActionNode *action = &alter_table->actions[i];

        if (!bind_alter_action(action, context, working_schema)) {
            printf("bind_alter_table: ALTER action binding failed.\n");
            goto cleanup;
        }

        // The proposed schema and mapping must remain synchronized
        if (map.num_columns != working_schema->num_columns) {
            printf("bind_alter_table: Column mapping mismatch.\n");
            goto cleanup;
        }
    }

    // Assign non-owning table pointer after validation is complete
    out_alter->table = table;
    out_alter->alter_table = alter_table;
    out_alter->result_schema = working_schema;

    success = true;

cleanup:
    // The context must never retain a pointer to this local map
    context->alter_column_map = NULL;
    alter_column_map_free(&map);

    if (!success) {
        schema_free(working_schema);
    }

    return success;
}

bool bind_create_index(ASTNode *root, BindingContext *context, BoundCreateIndex *create_index) {
    if (!root || root->type != AST_CREATE_INDEX || !create_index) {
        printf("bind_create_index: Invalid AST root node or output.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_create_index: Invalid binding context.\n");
        return false;
    }

    // Empty context and bound output are required
    if (context->relations || context->num_relations != 0 || context->aggregate_depth != 0) {
        printf("bind_create_index: Expected fresh binding state.\n");
        return false;
    }

    if (create_index->create_index || create_index->table || 
        create_index->column_indexes || create_index->num_columns != 0) {
        printf("bind_create_index: Invalid initialized output.\n");
        return false;
    }

    CreateIndexNode *index = &root->node_contents.create_index;

    if (!index || 
        index->index_name[0] == '\0' ||
        index->table_name[0] == '\0' ||
        !index->column_refs || index->num_column_refs == 0) {
        printf("bind_create_index: Invalid create index node.\n");
        return false;
    }

    uint32_t num_columns = index->num_column_refs;

    // Allocate the column index array
    if ((size_t)num_columns > SIZE_MAX / sizeof(uint32_t)) {
        printf("bind_create_index: Number of index key columns exceeds maximum size.\n");
        return false;
    }

    // Validate the table exists
    if (!register_binding_relation(index->table_name, "", context)) {
        printf("bind_create_index: Table could not be registered.\n");
        return false;
    }

    Table *table = context->relations[0].table;

    if (!table || table->name[0] == '\0' || !table->table_schema ||  
        table->is_deleted || !table->is_materialized ||
        !table->primary_index ||
        (table->total_secondary_indexes > 0 && !table->secondary_indexes)) {
        printf("bind_create_index: Invalid table.\n");
        return false;
    }

    if (table->total_secondary_indexes >= MAX_INDEXES) {
        printf("bind_create_index: Secondary index limit reached.\n");
        return false;
    }


    uint32_t *resolved_indexes = malloc((size_t)num_columns * sizeof(*resolved_indexes));

    if (!resolved_indexes) {
        printf("bind_create_index: Column index allocation failed.\n");
        return false;
    }

    // Resolve all indexed column references
    for (uint32_t i = 0; i < num_columns; i++) {
        ExpressionNode *column_expr = index->column_refs[i];

        if (!column_expr || column_expr->type != EXPR_COLUMN_REF) {
            printf("bind_create_index: Invalid column reference.\n");
            free(resolved_indexes);
            return false;
        }

        ColumnRef *column_ref = &column_expr->expression_data.column_value;

        // CREATE INDEX requires unqualified column names
        if (column_ref->table_name[0] != '\0') {
            printf("bind_create_index: Qualified column reference.\n");
            free(resolved_indexes);
            return false;
        }

        BoundExpressionType result_type = {0};

        if (!bind_scalar_expression(column_expr, context, &result_type)) {
            printf("bind_create_index: Column binding failed.\n");
            free(resolved_indexes);
            return false;
        }

        // The only registered relation is the indexed table.
        if (column_ref->relation_index != 0 || column_ref->column_index < 0 ||
            (uint32_t)column_ref->column_index >= table->table_schema->num_columns) {
            free(resolved_indexes);
            return false;
        }

        uint32_t column_id = (uint32_t)column_ref->column_index;

        // Reject duplicate columns in the same index key
        for (uint32_t j = 0; j < i; j++) {
            if (resolved_indexes[j] == column_id) {
                printf("bind_create_index: Duplicate indexed column.\n");
                free(resolved_indexes);
                return false;
            }
        }

        // Reject types that do not support index ordering
        if (result_type.is_untyped_null || !supports_ordering(result_type.type)) {
            printf("bind_create_index: Unsupported indexed type.\n");
            free(resolved_indexes);
            return false;
        }

        resolved_indexes[i] = column_id;
    }


    // Validate that no other index in the database has the same name
    Table *existing_owner = NULL;

    if (database_find_index(context->db, index->index_name, &existing_owner)) {
        printf("bind_create_index: Index name already exists in database.\n");
        free(resolved_indexes);
        return false;
    }

    // Validate that no existing index has the same key
    // Primary index
    if (index_key_matches_key(table->primary_index, resolved_indexes, num_columns)) {
        printf("bind_create_index: New index key matches the primary index.\n");
        free(resolved_indexes);
        return false;
    }

    // Secondary indexes
    for (uint32_t i = 0; i < table->total_secondary_indexes; i++) {
        Index *current_index = table->secondary_indexes[i];
        
        if (!current_index || 
            !current_index->key || 
            !current_index->key->column_index_array || 
            current_index->key->num_columns == 0) {
            printf("bind_create_index: Invalid secondary index.\n");
            free(resolved_indexes);
            return false;
        }

        if (index_key_matches_key(current_index, resolved_indexes, num_columns)) {
            printf("bind_create_index: New index key matches a secondary index.\n");
            free(resolved_indexes);
            return false;
        }
    }

    // Validate the new index key definition    
    IndexKey candidate_key = {0};

    candidate_key.column_index_array = resolved_indexes;
    candidate_key.num_columns = num_columns;

    Index candidate_index = {0};
    candidate_index.key = &candidate_key;
    candidate_index.type = SECONDARY_INDEX;

    if (!btree_index_definition_fits(&candidate_index, table->table_schema)) {
        printf("bind_create_index: Invalid index definition.\n");
        free(resolved_indexes);
        return false;
    }

    // Publish validated output
    create_index->table = table;
    create_index->create_index = index;
    create_index->column_indexes = resolved_indexes;
    create_index->num_columns = num_columns;

    return true;
}

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
        if (join->join_type != AST_JOIN_INNER && join->join_type != AST_JOIN_LEFT &&
            join->join_type != AST_JOIN_RIGHT && join->join_type != AST_JOIN_OUTER) {
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
        ASTOrderByTypes direction = order_by->types[i];

        if (!expr) {
            printf("bind_order_by: Invalid ORDER BY expression.\n");
            return false;
        }

        // Validate sorting direction
        if (direction != AST_ORDER_BY_ASC && direction != AST_ORDER_BY_DESC) {
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

// Bind INTO clause
bool bind_into(IntoNode *into, BindingContext *context) {
    if (!into || 
        into->table_name[0] == '\0' || 
        (into->num_column_refs > 0 && !into->column_refs) ||
        (into->num_column_refs == 0 && into->column_refs)) {
        printf("bind_into: Invalid INTO AST node.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_into: Invalid context.\n");
        return false;
    }

    if (!register_binding_relation(into->table_name, "", context)) {
        printf("bind_into: Into node's table could not be registered.\n");
        return false;
    }
    
    // Bind each column reference
    for (uint32_t i = 0; i < into->num_column_refs; i++) {
        ExpressionNode *column_expr = into->column_refs[i];

        if (!column_expr || column_expr->type != EXPR_COLUMN_REF) {
            printf("bind_into: Invalid column reference expression.\n");
            return false;
        }

        ColumnRef *current = &column_expr->expression_data.column_value;

        // INSERT destination columns must be unqualified
        if (current->table_name[0] != '\0') {
            printf("bind_into: Qualified destination columns are not allowed.\n");
            return false;
        }

        BoundExpressionType result_type = {0};

        if(!bind_scalar_expression(column_expr, context, &result_type)) {
            printf("bind_into: Column reference expression could not be bound.\n");
            return false;
        }

        // Detect potential duplicate columns
        for (uint32_t j = 0; j < i; j++) {
            ColumnRef *previous = &into->column_refs[j]->expression_data.column_value;

            if (current->column_index == previous->column_index) {
                printf("bind_into: Duplicate destination column.\n");
                return false;
            }
        }
    }

    return true;
}

// Bind VALUES clause
bool bind_values(ValuesNode *values, BindingContext *context, BoundExpressionType *out_types) {
    if (!values || !values->values || values->num_values == 0) {
        printf("bind_values: Invalid VALUES AST root node.\n");
        return false;
    }

    if (!context || !context->db || !out_types) {
        printf("bind_values: Invalid binding context or output.\n");
        return false;
    }

    // Bind VALUES
    for (uint32_t i = 0; i < values->num_values; i++) {
        ExpressionNode *value_expr = values->values[i];
        
        if (!value_expr) {
            printf("bind_values: Invalid value expression.\n");
            return false;
        }

        BoundExpressionType result_type = {0};

        if (!bind_scalar_expression(value_expr, context, &result_type)) {
            printf("bind_values: Value expression could not be bound.\n");
            return false;
        }

        out_types[i] = result_type;
    }

    return true;
}

// Bind SET clause
bool bind_set(SetNode *set, BindingContext *context, BoundExpressionType *out_types) {
    if (!set || !set->assignments || set->num_assignments == 0) {
        printf("bind_set: Invalid SET AST node.\n");
        return false;
    }

    if (!context || !context->db || !out_types ||
        !context->relations || context->num_relations != 1 ||
        !context->relations[0].table) {
        printf("bind_set: Invalid binding context or output.\n");
        return false;
    }

    for (uint32_t i = 0; i < set->num_assignments; i++) {
        ExpressionNode *column_expr = set->assignments[i].column_ref;
        ExpressionNode *value_expr = set->assignments[i].value;
        
        if (!column_expr || column_expr->type != EXPR_COLUMN_REF || !value_expr) {
            printf("bind_set: Invalid assignment.\n");
            return false;
        }

        ColumnRef *current = &column_expr->expression_data.column_value;

        // Bind destination column
        BoundExpressionType column_type = {0};

        if (!bind_column_ref(column_expr, context, &column_type)) {
            printf("bind_set: Destination column could not be bound.\n");
            return false;
        }

        if (current->relation_index != 0 || current->column_index < 0) {
            printf("bind_set: Invalid destination relation or column.\n");
            return false;
        }

        // Reject duplicate columns
        for (uint32_t j = 0; j < i; j++) {
            ColumnRef *previous = &set->assignments[j].column_ref->expression_data.column_value;

            if (current->column_index == previous->column_index) {
                printf("bind_set: Duplicate destination column.\n");
                return false;
            }
        }

        // Bind assigned expression
        BoundExpressionType result_type = {0};

        if (!bind_scalar_expression(value_expr, context, &result_type)) {
            printf("bind_set: Assigned expression could not be bound.\n");
            return false;
        }

        out_types[i] = result_type;
    }

    return true;
}

// Bind a Columns Node
bool bind_columns(ColumnsNode *columns, CreateTableNode *create_table, BindingContext *context) {
    if (!columns || !columns->column_defs || columns->num_column_defs == 0 || !create_table) {
        printf("bind_columns: Invalid Columns AST node.\n");
        return false;
    }


    if (!context || !context->db) {
        printf("bind_columns: Invalid binding context.\n");
        return false;
    }

    // Validate each column definition
    for (uint32_t i = 0; i < columns->num_column_defs; i++) {
        ColumnDefNode *curr_column_def = columns->column_defs[i];

        if (!curr_column_def || curr_column_def->column_name[0] == '\0') {
            printf("bind_columns: Invalid column definition.\n");
            return false;
        }

        // Compare with all previously encountered column names.
        for (uint32_t j = 0; j < i; j++) {
            ColumnDefNode *prev_column_def = columns->column_defs[j];

            if (!prev_column_def) {
                return false;
            }

            if (!strcmp(curr_column_def->column_name, prev_column_def->column_name)) {
                printf("bind_columns: Duplicate column name.\n");
                return false;
            }
        }
    }

    // Bind only after checking all column names for duplicates
    for (uint32_t i = 0; i < columns->num_column_defs; i++) {
        if (!bind_column_def(columns->column_defs[i], create_table, context, i)) {
            printf("bind_columns: Invalid column definition.\n");
            return false;
        }
    }

    return true;
}

// Bind a Column definition node
bool bind_column_def(ColumnDefNode *column_def, CreateTableNode *create_table,
    BindingContext *context, uint32_t owner_column_index) {
    if (!context || !context->db || !create_table) {
        printf("bind_column_def: Invalid binding context or input Create Table node.\n");
        return false;
    }

    if (!column_def || column_def->column_name[0] == '\0' ||
        (column_def->num_constraints > 0 && !column_def->constraints) ||
        column_def->type < INTEGER || column_def->type > JSONB) {
        printf("bind_column_def: Invalid column reference.\n");
        return false;
    }

    if ((column_def->type == CHAR && column_def->type_args.length == 0) ||
        (column_def->type == VARCHAR && column_def->type_args.length == 0) ||
        (column_def->type == NUMERIC && 
        (column_def->type_args.precision == 0 ||
         column_def->type_args.scale > column_def->type_args.precision))) {
        printf("bind_column_def: Invalid column data type arguments.\n");
        return false;
    }

    // Validate each column-level constraint
    for (uint32_t i = 0; i < column_def->num_constraints; i++) {
        ConstraintNode *constraint = column_def->constraints[i];

        if (!constraint || 
            constraint->type < AST_CONSTRAINT_PRIMARY_KEY ||
            constraint->type > AST_CONSTRAINT_DEFAULT) {
            printf("bind_column_def: Invalid column-level constraint.\n");
            return false;
        }

        if (!bind_constraint(
                column_def->constraints[i], 
                create_table, 
                context, 
                (uint32_t)owner_column_index)) {
            printf("bind_column_def: Column-level constraint binding failed.\n");
            return false;
        }
    }

    return true;
}

// Validate column-level and table-level constraints
bool bind_constraint(ConstraintNode *constraint, CreateTableNode *create_table, 
    BindingContext *context, int32_t owner_column_index) {
    if (!constraint || !create_table) {
        printf("bind_constraint: Invalid table and constraint metadata.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_constraint: Invalid binding context.\n");
        return false;
    }

    switch (constraint->type) {
        case AST_CONSTRAINT_PRIMARY_KEY:
            return bind_primary_key_constraint(constraint, create_table, context, owner_column_index);

        case AST_CONSTRAINT_FOREIGN_KEY:
            return bind_foreign_key_constraint(constraint, create_table, context, owner_column_index);

        case AST_CONSTRAINT_UNIQUE:
            return bind_unique_constraint(constraint, create_table, context, owner_column_index);

        case AST_CONSTRAINT_CHECK:
            return bind_check_constraint(constraint, create_table, context, owner_column_index);

        case AST_CONSTRAINT_NOT_NULL:
            return bind_not_null_constraint(constraint, create_table, context, owner_column_index);

        case AST_CONSTRAINT_DEFAULT:
            return bind_default_constraint(constraint, create_table, context, owner_column_index);

        default:
            printf("bind_constraint: Invalid constraint type.\n");
            return false;
    }
}

// Bind PRIMARY KEY constraint
bool bind_primary_key_constraint(ConstraintNode *constraint, CreateTableNode *create_table,
    BindingContext *context, int32_t owner_column_index) {
    if (!constraint || constraint->type != AST_CONSTRAINT_PRIMARY_KEY || !create_table) {
        printf("bind_primary_key_constraint: Invalid Constraint AST node.\n");
        return false;
    }
    
    if (!context || !context->db) {
        printf("bind_primary_key_constraint: Invalid binding context.\n");
        return false;
    }

    PrimaryKeyConstraintNode *primary = &constraint->constraint_data.primary_key;

    if (!primary || !primary->column_refs || primary->num_columns == 0) {
        printf("bind_primary_key_constraint: Invalid Primary Key constraint.\n");
        return false;
    }

    if (owner_column_index >= 0 && primary->num_columns != 1) {
        printf("bind_primary_key_constraint: Invalid column references in primary key.\n");
        return false;
    }

    if (!validate_constraint_columns(primary->column_refs, primary->num_columns, create_table)) {
        printf("bind_primary_key_constraint: Constraint column binding failed.\n");
        return false;
    }

    // If the PRIMARY KEY is a column-level constraint, validate its column reference
    if (owner_column_index >= 0) {
        uint32_t index;

        if (!resolve_create_table_column(
                create_table, 
                primary->column_refs[0]->expression_data.column_value.column_name,
                &index) ||
            index != (uint32_t)owner_column_index) {
            printf("bind_primary_key_constraint: Constraint column resolution failed or column index mismatch.\n");
            return false;
        }
    }   
    
    return true;
}

// Bind FOREIGN KEY constraint
bool bind_foreign_key_constraint(ConstraintNode *constraint, CreateTableNode *create_table,
    BindingContext *context, int32_t owner_column_index) {
    if (!constraint || constraint->type != AST_CONSTRAINT_FOREIGN_KEY || !create_table) {
        printf("bind_foreign_key_constraint: Invalid Constraint AST node.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_foreign_key_constraint: Invalid binding context.\n");
        return false;
    }

    ForeignKeyConstraintNode *foreign = &constraint->constraint_data.foreign_key;

    if (!foreign || foreign->referenced_table_name[0] == '\0' ||
        !foreign->local_column_refs || foreign->num_local_columns == 0 ||
        !foreign->referenced_column_refs || foreign->num_referenced_columns == 0 ||
        foreign->num_local_columns != foreign->num_referenced_columns) {
        printf("bind_foreign_key_constraint: Invalid Foreign Key constraint.\n");
        return false;
    }

    if (!validate_constraint_columns(
            foreign->local_column_refs, 
            foreign->num_local_columns,
            create_table)) {
        printf("bind_foreign_constraint: Constraint column binding failed.\n");
        return false;
    }

    // If the FOREIGN KEY is a column-level constraint, validate its column reference
    if (owner_column_index >= 0) {
        if (foreign->num_local_columns != 1) {
            printf("bind_foreign_constraint: Invalid multiple columns in column-level foreign key.\n");
            return false;
        }

        uint32_t index;

        if (!resolve_create_table_column(
                create_table,
                foreign->local_column_refs[0]->expression_data.column_value.column_name,
                &index) ||
            index != (uint32_t)owner_column_index) {
            printf("bind_foreign_key_constraint: Constraint column resolution failed or column index mismatch.\n");
            return false;
        }
    }

    // Determine if FOREIGN KEY references the same table
    bool self_reference = strcmp(foreign->referenced_table_name, create_table->table_name) == 0;

    Table *referenced_table = NULL;

    // If it references another existing table, find that table
    if (!self_reference) {
        referenced_table = database_find_table(context->db, foreign->referenced_table_name);

        if (!referenced_table || 
            !referenced_table->table_schema ||
            !referenced_table->table_schema->columns) {
            printf("bind_foreign_key_constraint: Referenced table not found.\n");
            return false;
        }
    }

    uint32_t *referenced_indexes = calloc(foreign->num_referenced_columns, sizeof(uint32_t));

    if (!referenced_indexes) {
        printf("bind_foreign_key_constraint: Referenced indexes array could not be allocated.\n");
        return false;
    }

    // Validate every local/referenced column pair
    for (uint32_t i = 0; i < foreign->num_local_columns; i++) {
        ExpressionNode *local_expr = foreign->local_column_refs[i];
        ExpressionNode *referenced_expr = foreign->referenced_column_refs[i];

        if (!local_expr || !referenced_expr ||
            local_expr->type != EXPR_COLUMN_REF || referenced_expr->type != EXPR_COLUMN_REF) {
            printf("bind_foreign_key_constraint: Invalid local/ref. column expressions.\n");
            free(referenced_indexes);
            return false;
        }

        ColumnRef *local_ref = &local_expr->expression_data.column_value;
        ColumnRef *referenced_ref = &referenced_expr->expression_data.column_value;

        uint32_t local_index;
        uint32_t referenced_index;
        DataType referenced_type;

        // Resolve local column index
        if (!resolve_create_table_column(create_table, local_ref->column_name, &local_index)) {
            printf("bind_foreign_key_constraint: Local column index resolution failed.\n");
            free(referenced_indexes);
            return false;
        }

        if (local_index > INT32_MAX) {
            printf("bind_foreign_key_constraint: Local column index exceeds max limit.\n");
            free(referenced_indexes);
            return false;
        }

        // Resolve referenced column index
        if (!resolve_fk_referenced_column(
                foreign, 
                create_table,
                referenced_table,
                referenced_ref,
                &referenced_index,
                &referenced_type)) {
            printf("bind_foreign_key_constraint: Local column index exceeds max limit.\n");
            free(referenced_indexes);
            return false;
        }

        referenced_indexes[i] = referenced_index;

        const ColumnDefNode *local_column = create_table->columns->column_defs[local_index];
        bool comparison_success = false;

        // Compare data types between local and corresponding referenced column
        if (self_reference) {
            const ColumnDefNode *referenced_column = create_table->columns->column_defs[referenced_index];

            comparison_success = fk_column_types_compatible(
                local_column,
                referenced_column,
                NULL
            );
        }
        else {
            const Column *referenced_column = referenced_table->table_schema->columns[referenced_index];

            comparison_success = fk_column_types_compatible(
                local_column,
                NULL,
                referenced_column
            );
        }

        if (!comparison_success) {
            printf("bind_foreign_key_constraint: Data type mismatch between local and referenced columns.\n");
            free(referenced_indexes);
            return false;
        }

        // Reject duplicate referenced column names
        for (uint32_t j = 0; j < i; j++) {
            const ColumnRef *previous = 
                &foreign->referenced_column_refs[j]->expression_data.column_value;

            if (!strcmp(previous->column_name, referenced_ref->column_name)) {
                printf("bind_foreign_key_constraint: Duplicate referenced column names.\n");
                free(referenced_indexes);
                return false;
            }
        }
    }

    // Verify that the referenced columns match a PRIMARY KEY or UNIQUE constraint's columns
    bool valid_referenced_key = false;

    if (self_reference) {
        valid_referenced_key = fk_matches_create_table_key(
            create_table,
            foreign->referenced_column_refs,
            foreign->num_referenced_columns
        );
    }
    else {
        valid_referenced_key = fk_matches_schema_key(
            referenced_table->table_schema,
            referenced_indexes,
            foreign->num_referenced_columns
        );
    }

    free(referenced_indexes);

    if (!valid_referenced_key) {
        printf("bind_foreign_key_constraint: Referenced columns are not a PRIMARY KEY or UNIQUE key.\n");
        return false;
    }

    return true;
}

// Bind UNIQUE constraint
bool bind_unique_constraint(ConstraintNode *constraint, CreateTableNode *create_table,
    BindingContext *context, int32_t owner_column_index) {
    if (!constraint || constraint->type != AST_CONSTRAINT_UNIQUE || !create_table) {
        printf("bind_unique_constraint: Invalid Constraint AST node.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_unique_constraint: Invalid binding context.\n");
        return false;
    }

    UniqueConstraintNode *unique = &constraint->constraint_data.unique;

    if (!unique || !unique->column_refs || unique->num_columns == 0) {
        printf("bind_unique_constraint: Invalid Unique constraint.\n");
        return false;
    }

    if (owner_column_index >= 0 && unique->num_columns != 1) {
        printf("bind_unique_constraint: Invalid column references in primary key.\n");
        return false;
    }

    if (!validate_constraint_columns(unique->column_refs, unique->num_columns, create_table)) {
        printf("bind_unique_constraint: Constraint column binding failed.\n");
        return false;
    }

    // If the UNIQUE is a column-level constraint, validate its column reference
    if (owner_column_index >= 0) {
        uint32_t index;

        if (!resolve_create_table_column(
                create_table, 
                unique->column_refs[0]->expression_data.column_value.column_name,
                &index) ||
            index != (uint32_t)owner_column_index) {
            printf("bind_unique_constraint: Constraint column resolution failed or column index mismatch.\n");
            return false;
        }
    }   

    return true;
}

// Bind CHECK constraint
bool bind_check_constraint(ConstraintNode *constraint, CreateTableNode *create_table,
    BindingContext *context, int32_t owner_column_index) {
    if (!constraint || constraint->type != AST_CONSTRAINT_CHECK || !create_table) {
        printf("bind_check_constraint: Invalid Constraint AST node.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_check_constraint: Invalid binding context.\n");
        return false;
    }

    CheckConstraintNode *check = &constraint->constraint_data.check;

    if (!check || !check->check_expr) {
        printf("bind_check_constraint: Invalid Check constraint.\n");
        return false;
    }

    BindingRelation relation = {0};
    relation.create_table = create_table;

    BindingContext check_context = {0};
    check_context.db = context->db;
    check_context.relations = &relation;
    check_context.num_relations = 1;
    check_context.current_clause = BIND_CLAUSE_CHECK;

    BoundExpressionType result_type = {0};

    if (!bind_scalar_expression(check->check_expr, &check_context, &result_type)) {
        printf("bind_check_constraint: CHECK expression binding failed.\n");
        return false;
    }

    if (result_type.is_untyped_null || result_type.type != BOOL) {
        printf("bind_check_constraint: CHECK expression must be boolean.\n");
        return false;
    }

    return true;
}

// Bind NOT NULL constraint
bool bind_not_null_constraint(ConstraintNode *constraint, CreateTableNode *create_table,
    BindingContext *context, int32_t owner_column_index) {
    if (!constraint || constraint->type != AST_CONSTRAINT_NOT_NULL || !create_table) {
        printf("bind_not_null_constraint: Invalid Constraint AST node.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_not_null_constraint: Invalid binding context.\n");
        return false;
    }

    NotNullConstraintNode *not_null = &constraint->constraint_data.not_null;

    if (!not_null || !not_null->column_ref || not_null->column_ref->type != EXPR_COLUMN_REF) {
        printf("bind_not_null_constraint: Invalid NOT NULL constraint.\n");
        return false;
    }

    ColumnRef *column_ref = &not_null->column_ref->expression_data.column_value;

    if (column_ref->column_name[0] == '\0' || column_ref->column_index != -1 ||
        column_ref->relation_index != -1) {
        printf("bind_not_null_constraint: Invalid initialized column reference.\n");
        return false;
    }

    if (column_ref->table_name[0] != '\0' &&
        strcmp(column_ref->table_name, create_table->table_name)) {
        printf("bind_not_null_constraint: Invalid qualified column reference.\n");
        return false;
    }

    uint32_t index;

    if (!resolve_create_table_column(create_table, column_ref->column_name, &index)) {
        printf("bind_not_null_constraint: Constraint column resolution failed.\n");
        return false;
    }

    if (index > INT32_MAX) {
        printf("bind_not_null_constraint: Column index exceeds INT32_MAX.\n");
        return false;
    }

    // Column-level NOT NULL must refer to its owner
    if (owner_column_index >= 0 && index != (uint32_t)owner_column_index) {
        printf("bind_not_null_constraint: Invalid column index.\n");
        return false;
    }

    return true;
}

// Bind DEFAULT constraint
bool bind_default_constraint(ConstraintNode *constraint, CreateTableNode *create_table,
    BindingContext *context, int32_t owner_column_index) {
    if (!constraint || constraint->type != AST_CONSTRAINT_DEFAULT || !create_table) {
        printf("bind_default_constraint: Invalid Constraint AST node.\n");
        return false;
    }

    if (!context || !context->db) {
        printf("bind_default_constraint: Invalid binding context.\n");
        return false;
    }

    DefaultConstraintNode *default_value = &constraint->constraint_data.default_value;

    if (!default_value || default_value->column_name[0] == '\0' || !default_value->default_expr) {
        printf("bind_default_constraint: Invalid Default constraint.\n");
        return false;
    }

    uint32_t index;

    if (!resolve_create_table_column(create_table, default_value->column_name, &index)) {
        printf("bind_default_constraint: Constraint column resolution failed.\n");
        return false;
    }

    if (owner_column_index >= 0 && index != (uint32_t)owner_column_index) {
        printf("bind_default_constraint: Invalid column index.\n");
        return false;
    }

    ColumnDefNode *column = create_table->columns->column_defs[index];

    // Bind DEFAULT scalar expression
    BindingClause previous_clause = context->current_clause;
    context->current_clause = BIND_CLAUSE_DEFAULT;

    BoundExpressionType result_type = {0};

    bool success = bind_scalar_expression(default_value->default_expr, context, &result_type);

    context->current_clause = previous_clause;

    if (!success) {
        printf("bind_default_constraint: DEFAULT expression binding failed.\n");
        return false;
    }

    // If the expression is a Literal,  
    // validate that its data type can be assigned to the corresponding column
    if (!result_type.is_untyped_null) {
        if (default_value->default_expr->type == EXPR_LITERAL) {
            Literal *literal = &default_value->default_expr->expression_data.literal_value;

            if (!literal->literal) {
                printf("bind_default_constraint: Invalid literal.\n");
                return false;
            }

            // Typed NULL literals are handled by the NULL validation pass
            if (!literal->literal->null_val &&
                !value_can_assign(column->type, literal->literal)) {
                printf("bind_default_constraint: Incompatible literal.\n");
                return false;
            }
        }
        else if (!value_types_assignable(column->type, result_type.type)) {
            printf("bind_default_constraint: Incompatible DEFAULT type.\n");
            return false;
        }
    }

    return true;
}

// Bind Alteration action
bool bind_alter_action(AlterActionNode *action, BindingContext *context, Schema *working_schema) {
    if (!action) {
        printf("bind_alter_action: Invalid action node.\n");
        return false;
    }

    if (!context) {
        printf("bind_alter_action: Invalid binding context.\n");
        return false;
    }

    if (!working_schema || !working_schema->columns || working_schema->num_columns == 0 ||
        (working_schema->num_constraints > 0 && !working_schema->constraints)) {
        printf("bind_alter_action: Invalid working schema.\n");
        return false;
    }

    // Dispatch action to the appropriate alter type binder
    switch (action->type) {
        case AST_ALTER_ADD_COLUMN:
            return bind_alter_add_col(action, context, working_schema);

        case AST_ALTER_DROP_COLUMN:
            return bind_alter_drop_col(action, context, working_schema);

        case AST_ALTER_RENAME_TABLE:
            return bind_alter_rename_table(action, context, working_schema);

        case AST_ALTER_RENAME_COLUMN:
            return bind_alter_rename_col(action, context, working_schema);

        case AST_ALTER_MODIFY_COLUMN:
            return bind_alter_modify_col(action, context, working_schema);

        case AST_ALTER_ADD_CONSTRAINT:
            return bind_alter_add_constraint(action, context, working_schema);

        case AST_ALTER_DROP_CONSTRAINT:
            return bind_alter_drop_constraint(action, context, working_schema);

        default:
            printf("bind_alter_table: Invalid alter action node type.\n");
            return false;
    }
}

// Bind ADD COLUMN alteration node
bool bind_alter_add_col(AlterActionNode *action, BindingContext *context, Schema *working_schema) {
    if (!action || action->type != AST_ALTER_ADD_COLUMN) {
        printf("bind_alter_add_col: Invalid ADD COLUMN action.\n");
        return false;
    }

    if (!context || !context->db ||
        !context->relations || context->num_relations != 1 ||
        !context->relations[0].table) {
        printf("bind_alter_add_col: Invalid binding context.\n");
        return false;
    }

    // Resolve table name in case it's been renamed by another alteration operation
    const char *current_table_name = binding_relation_current_name(&context->relations[0]);

    if (!current_table_name || current_table_name[0] == '\0') {
        return false;
    }

    Table *table = context->relations[0].table;
    ColumnDefNode *column = action->alter_contents.alter_add.column;

    if (!working_schema || !working_schema->columns || !column ||
        column->column_name[0] == '\0' ||
        (column->num_constraints > 0 && !column->constraints)) {
        printf("bind_alter_add_col: Invalid working schema or column.\n");
        return false;
    }

    if (working_schema->num_constraints > 0 && !working_schema->constraints) {
        printf("bind_alter_add_col: Invalid working schema constraints.\n");
        return false;
    }

    // The new column would occupy this index in the final schema
    if (working_schema->num_columns >= INT32_MAX) {
        printf("bind_alter_add_col: Column index limit reached.\n");
        return false;
    }

    // Validate column name uniqueness
    for (uint32_t i = 0; i < working_schema->num_columns; i++) {
        Column *existing = working_schema->columns[i];

        if (!existing || existing->name[0] == '\0') {
            printf("bind_alter_add_col: Invalid existing column.\n");
            return false;
        }

        if (!strcmp(existing->name, column->column_name)) {
            printf("bind_alter_add_col: Duplicate column name.\n");
            return false;
        }
    }

    // Validate datatype and datatype parameters
    if (column->type < INTEGER || column->type > JSONB) {
        printf("bind_alter_add_col: Invalid datatype.\n");
        return false;
    }

    if ((column->type == CHAR && column->type_args.length == 0) ||
        (column->type == VARCHAR && column->type_args.length == 0) ||
        (column->type == NUMERIC &&
         (column->type_args.precision == 0 ||
          column->type_args.scale > column->type_args.precision))) {
        printf("bind_alter_add_col: Invalid datatype arguments.\n");
        return false;
    }

    // Validate existing primary-key state
    bool existing_primary_key = false;
    
    for (uint32_t i = 0; i < working_schema->num_constraints; i++) {
        Constraint *existing = working_schema->constraints[i];

        if (!existing) {
            printf("bind_alter_add_col: Invalid existing constraint.\n");
            return false;
        }

        if (existing->type == PRIMARY_KEY) {
            existing_primary_key = true;
        }
    }

    bool new_primary_key = false;
    bool new_not_null = false;
    bool has_default = false;
    bool null_default = false;

    // Validate new column-level constraints
    for (uint32_t i = 0; i < column->num_constraints; i++) {
        ConstraintNode *constraint = column->constraints[i];

        if (!constraint ||
            constraint->type < AST_CONSTRAINT_PRIMARY_KEY ||
            constraint->type > AST_CONSTRAINT_DEFAULT) {
            printf("bind_alter_add_col: Invalid constraint type.\n");
            return false;
        }

        // Detect duplicate names among newly declared constraints
        if (constraint->constraint_name[0] != '\0') {
            for (uint32_t j = 0; j < i; j++) {
                ConstraintNode *previous = column->constraints[j];

                if (!previous) {
                    printf("bind_alter_add_col: Invalid constraint.\n");
                    return false;
                }

                if (previous->constraint_name[0] != '\0' &&
                    !strcmp(previous->constraint_name, constraint->constraint_name)) {
                    printf("bind_alter_add_col: Duplicate constraint name.\n");
                    return false;
                }
            }

            // Check constraint-name collisions against the working schema,
            // which includes the original constraints and all changes
            // successfully applied by preceding ALTER TABLE actions.
            for (uint32_t j = 0; j < working_schema->num_constraints; j++) {
                Constraint *existing_constraint = working_schema->constraints[j];

                if (!existing_constraint) {
                    printf("bind_alter_add_col: Invalid constraint.\n");
                    return false;
                }

                if (existing_constraint->constraint_name[0] != '\0' &&
                    !strcmp(existing_constraint->constraint_name, constraint->constraint_name)) {
                    printf("bind_alter_add_col: Duplicate constraint name.\n");
                    return false;
                }
            }
        }

        switch (constraint->type) {
            // Validate PRIMARY KEY and UNIQUE constraints
            case AST_CONSTRAINT_PRIMARY_KEY:
            case AST_CONSTRAINT_UNIQUE: {
                ExpressionNode **column_refs = NULL;
                uint32_t count = 0;

                if (constraint->type == AST_CONSTRAINT_PRIMARY_KEY) {
                    PrimaryKeyConstraintNode *pk = &constraint->constraint_data.primary_key;

                    // Check for a duplicate PRIMARY KEY
                    if (existing_primary_key || new_primary_key) {
                        printf("bind_alter_add_col: Duplicate PRIMARY KEY.\n");
                        return false;
                    }

                    new_primary_key = true;

                    column_refs = pk->column_refs;
                    count = pk->num_columns;
                }
                else {
                    UniqueConstraintNode *unique = &constraint->constraint_data.unique;

                    column_refs = unique->column_refs;
                    count = unique->num_columns;
                }

                // Only one column references should exist
                if (!column_refs || count != 1 || 
                    !column_refs[0] || 
                    column_refs[0]->type != EXPR_COLUMN_REF) {
                    printf("bind_alter_add_col: Invalid key columns.\n");
                    return false;
                }

                ColumnRef *ref = &column_refs[0]->expression_data.column_value;

                if (ref->relation_index != -1 || ref->column_index != -1) {
                    printf("bind_alter_add_col: Invalid initialized relation/column indexes.\n");
                    return false;
                }

                // The column reference contents must match the column definition contents
                if (strcmp(ref->column_name, column->column_name) ||
                    (ref->table_name[0] != '\0' && strcmp(ref->table_name, current_table_name))) {
                    printf("bind_alter_add_col: Invalid key owner.\n");
                    return false;
                }

                break;
            }

            case AST_CONSTRAINT_NOT_NULL: {
                // Validate NOT NULL constraint 
                ExpressionNode *expr = constraint->constraint_data.not_null.column_ref;

                if (!expr || expr->type != EXPR_COLUMN_REF) {
                    printf("bind_alter_add_col: Invalid NOT NULL expression.\n");
                    return false;
                }

                // Validate column reference expression
                ColumnRef *column_ref = &expr->expression_data.column_value;

                if (new_not_null ||
                    column_ref->column_index != -1 || column_ref->relation_index != -1 ||
                    strcmp(column_ref->column_name, column->column_name) ||
                    (column_ref->table_name[0] != '\0' && strcmp(column_ref->table_name, current_table_name))) {
                    printf("bind_alter_add_col: Invalid column reference inside NOT NULL.\n");
                    return false;
                }

                new_not_null = true;

                break;
            }

            case AST_CONSTRAINT_DEFAULT: {
                // Validate DEFAULT constraint 
                DefaultConstraintNode *def = &constraint->constraint_data.default_value;

                if (has_default || strcmp(def->column_name, column->column_name) ||
                    !def->default_expr) {
                    printf("bind_alter_add_col: Invalid DEFAULT constraint.\n");
                    return false;
                }

                // Check if DEFAULT expression validates to NULL
                has_default = true;
                null_default = expression_is_explicit_null_literal(def->default_expr);

                // Bind DEFAULT expression
                BindingClause previous = context->current_clause;
                context->current_clause = BIND_CLAUSE_DEFAULT;

                BoundExpressionType result_type = {0};

                bool bound = bind_scalar_expression(def->default_expr, context, &result_type);

                context->current_clause = previous;

                if (!bound) {
                    printf("bind_alter_add_col: DEFAULT expression binding failed.\n");
                    return false;
                }

                // Check if DEFAULT expression can be assigned to the appropriate column type
                if (!result_type.is_untyped_null && !null_default) {
                    if (def->default_expr->type == EXPR_LITERAL) {
                        Literal *literal = &def->default_expr->expression_data.literal_value;

                        if (!literal->literal || !value_can_assign(column->type, literal->literal)) {
                            printf("bind_alter_add_col: DEFAULT value cannot be assigned to column.\n");
                            return false;
                        }
                    }
                    else if (!value_types_assignable(column->type, result_type.type)) {
                        printf("bind_alter_add_col: DEFAULT value cannot be assigned to column.\n");
                        return false;
                    }
                }

                break;
            }

            case AST_CONSTRAINT_FOREIGN_KEY: {
                // Validate FOREIGN KEY constraint 
                if (!validate_alter_column_fk(
                        &constraint->constraint_data.foreign_key,
                        column,
                        table,
                        working_schema,
                        context)) {
                    printf("bind_alter_add_col: Invalid FOREIGN KEY constraint.\n");
                    return false;
                }
                break;
            }

            case AST_CONSTRAINT_CHECK:
                // Validate CHECK constraint 
                if (!validate_alter_column_check(
                        constraint,
                        column,
                        table,
                        working_schema,
                        context)) {
                    printf("bind_alter_add_col: Invalid CHECK constraint.\n");
                    return false;
                }
                break;

            default:
                return false;
        }
    }

    // Cross-constraint validation
    // DEFAULT NULL is not allowed along with a NOT NULL or PRIMARY KEY constraint
    if (null_default && (new_not_null || new_primary_key)) {
        printf("bind_alter_add_col: NULL DEFAULT on required column.\n");
        return false;
    }

    // Validate mapping before changing the working schema
    if (!context->alter_column_map ||
        !context->alter_column_map->working_to_original ||
        context->alter_column_map->num_columns != working_schema->num_columns) {
        printf("bind_alter_add_col: Invalid column mapping.\n");
        return false;
    }

    // Update the proposed schema
    if (!working_schema_add_column(working_schema, column, context, table)) {
        printf("bind_alter_add_col: Working schema update failed.\n");
        return false;
    }

    // The new column did not exist in the original table
    if (!alter_column_map_add(context->alter_column_map)) {
        printf("bind_alter_add_col: Column mapping update failed.\n");
        return false;
    }

    return true;
}


// Bind DROP COLUMN alteration node
bool bind_alter_drop_col(AlterActionNode *action, BindingContext *context, Schema *working_schema) {
    if (!action || action->type != AST_ALTER_DROP_COLUMN) {
        printf("bind_alter_drop_col: Invalid DROP COLUMN alteration node.\n");
        return false;
    }

    if (!context || !context->db || !context->relations || 
        context->num_relations != 1 || !context->relations[0].table) {
        printf("bind_alter_drop_col: Invalid binding context.\n");
        return false;
    }

    // Resolve table name in case it's been renamed by another alteration operation
    const char *current_table_name = binding_relation_current_name(&context->relations[0]);

    if (!current_table_name || current_table_name[0] == '\0') {
        return false;
    }

    if (!working_schema || !working_schema->columns ||
        (working_schema->num_constraints > 0 && !working_schema->constraints)) {
        printf("bind_alter_drop_col: Invalid working schema.\n");
        return false;
    }

    if (working_schema->num_columns <= 1) {
        printf("bind_alter_drop_col: Cannot drop the last column.\n");
        return false;
    }

    Table *table = context->relations[0].table;
    char *drop_column_name = action->alter_contents.alter_drop.column_name;

    if (!drop_column_name || drop_column_name[0] == '\0') {
        printf("bind_alter_drop_col: Invalid target column.\n");
        return false;
    }

    int32_t found = schema_find_column_index(working_schema, drop_column_name);

    if (found < 0) {
        printf("bind_alter_drop_col: Target column does not exist.\n");
        return false;
    }

    uint32_t column_index = (uint32_t)found;

    for (uint32_t i = 0; i < working_schema->num_columns; i++) {
        Column *column = working_schema->columns[i];

        if (!column || column->name[0] == '\0') {
            printf("bind_alter_drop_col: Invalid working schema column.\n");
            return false;
        }

        // Skip all non-matching columns
        if (strcmp(drop_column_name, column->name)) {
            continue;
        }

        // At this point we've found the matching column
        column_index = i;

        // We should verify if any constraints reference that column
        for (uint32_t j = 0; j < working_schema->num_constraints; j++) {
            Constraint *constraint = working_schema->constraints[j];

            if (!constraint) {
                printf("bind_alter_drop_col: Invalid working schema constraint.\n");
                return false;
            }

            switch (constraint->type) {
                case PRIMARY_KEY:
                case UNIQUE:
                case CHECK:
                    if (constraint_references_column(constraint, column_index)) {
                        printf("bind_alter_drop_col: Column participates in a PRIMARY KEY/UNIQUE constraint.\n");
                        return false;
                    }
                    break;

                case FOREIGN_KEY: {
                    bool local_dependency = foreign_key_uses_column(constraint, column_index);

                    bool self_reference = 
                        strcmp(constraint->constraint_data.foreign_key.referenced_table_name, current_table_name) == 0;

                    bool referenced_dependency =
                            self_reference &&
                            foreign_key_references_column(constraint, column_index);

                    if (local_dependency || referenced_dependency) {
                        printf("bind_alter_drop_col: Column is used by a FOREIGN KEY.\n");
                        return false;
                    }

                    break;
                }

                case DEFAULT:
                case NOT_NULL:
                    break;
                    
                default:
                    printf("bind_alter_drop_col: Invalid constraint type.\n");
                    return false;
            }
        }
    }

    uint32_t original_index;

    if (!alter_map_get_original(context->alter_column_map, column_index, &original_index)) {
        printf("bind_alter_drop_col: Failed to retrieve original column index.\n");
        return false;
    }

    if (original_index != ALTER_NEW_COLUMN &&
        has_incoming_fk_dependency(context->db, table, original_index)) {
        printf("bind_alter_drop_col: Target column has incoming FOREIGN KEY constraint.\n");
        return false;
    }

    // Validate if the primary index references the target column
    if (!table->primary_index || !table->primary_index->key) {
        printf("bind_alter_drop_col: Invalid canonical index.\n");
        return false;
    }

    if (original_index != ALTER_NEW_COLUMN &&
        index_key_has_column(table->primary_index, original_index)) {
        printf("bind_alter_drop_col: Column is part of canonical index.\n");
        return false;
    }

    if (table->total_secondary_indexes > 0 && !table->secondary_indexes) {
        printf("bind_alter_drop_col: Invalid secondary indexes.\n");
        return false;
    }

    // Validate if any secondary index references the target column
    for (uint32_t i = 0; i < table->total_secondary_indexes; i++) {
        Index *index = table->secondary_indexes[i];

        if (!index || !index->key) {
            printf("bind_alter_drop_col: Invalid secondary index.\n");
            return false;
        }

        if (original_index != ALTER_NEW_COLUMN && 
            index_key_has_column(index, original_index)) {
            printf("bind_alter_drop_col: Column is part of secondary index.\n");
            return false;
        }
    }
    
    // Validate mapping before changing the working schema
    if (!context->alter_column_map ||
        !context->alter_column_map->working_to_original ||
        context->alter_column_map->num_columns != working_schema->num_columns ||
        column_index >= context->alter_column_map->num_columns) {
        printf("bind_alter_drop_col: Invalid column mapping.\n");
        return false;
    }

    // Remove the column from the proposed schema
    if (!working_schema_drop_column(working_schema, column_index, current_table_name)) {
        printf("bind_alter_drop_col: Working schema update failed.\n");
        return false;
    }

    // Remove the corresponding position from the mapping
    if (!alter_column_map_drop(context->alter_column_map, column_index)) {
        printf("bind_alter_drop_col: Column mapping update failed.\n");
        return false;
    }

    return true;
}

// Bind RENAME TABLE alteration node
bool bind_alter_rename_table(AlterActionNode *action, BindingContext *context, Schema *working_schema) {
    if (!action || action->type != AST_ALTER_RENAME_TABLE) {
        printf("bind_alter_rename_table: Invalid RENAME TABLE action.\n");
        return false;
    }

    if (!context || !context->db ||
        !context->relations || context->num_relations != 1 ||
        !context->relations[0].table) {
        printf("bind_alter_rename_table: Invalid binding context.\n");
        return false;
    }
    

    if (!working_schema || !working_schema->columns ||
        working_schema->num_columns == 0 ||
        (working_schema->num_constraints > 0 && !working_schema->constraints)) {
        printf("bind_alter_rename_table: Invalid working schema.\n");
        return false;
    }

    BindingRelation *relation = &context->relations[0];

    const char *new_name = action->alter_contents.alter_rename_table.new_table_name;

    if (new_name[0] == '\0' || strlen(new_name) >= sizeof(relation->proposed_table_name)) {
        printf("bind_alter_rename_table: Invalid new table name.\n");
        return false;
    }

    if (!strcmp(new_name, relation->proposed_table_name)) {
        printf("bind_alter_rename_table: Table already has this name.\n");
        return false;
    }

    // Check against all existing tables in the database
    Table *existing = database_find_table(context->db, new_name);

    // The original table object retains its original name during binding
    // Allow a later rename back to that name, but reject any other table
    if (existing && existing != relation->table) {
        printf("bind_alter_rename_table: Table name already in use.\n");
        return false;
    }

    const char *old_name = relation->proposed_table_name;

    // First pass: validate all metadata that must be rewritten
    for (uint32_t i = 0; i < working_schema->num_constraints; i++) {
        Constraint *constraint = working_schema->constraints[i];

        if (!constraint) {
            return false;
        }

        if (constraint->type == FOREIGN_KEY) {
            ForeignKeyConstraint *fk = &constraint->constraint_data.foreign_key;

            if (!strcmp(fk->referenced_table_name, old_name) &&
                strlen(new_name) >= sizeof(fk->referenced_table_name)) {
                return false;
            }
        }

        if (constraint->type == CHECK) {
            if (!rewrite_check_identifiers(
                    constraint->constraint_data.check.constraint_expr,
                    -1, NULL,
                    old_name, new_name,
                    false)) {
                return false;
            }
        }
    }

    // Second pass: update the working schema metadata
    for (uint32_t i = 0; i < working_schema->num_constraints; i++) {
        Constraint *constraint = working_schema->constraints[i];

        if (constraint->type == FOREIGN_KEY) {
            ForeignKeyConstraint *fk = &constraint->constraint_data.foreign_key;

            if (!strcmp(fk->referenced_table_name, old_name)) {
                strcpy(fk->referenced_table_name, new_name);
            }
        }

        if (constraint->type == CHECK) {
            rewrite_check_identifiers(
                constraint->constraint_data.check.constraint_expr,
                -1, NULL,
                old_name, new_name,
                true
            );
        }
    }

    // Update the table name only the proposed name of the bound relation
    strcpy(relation->proposed_table_name, new_name);

    return true;
}


// Bind RENAME COLUMN alteration node
bool bind_alter_rename_col(AlterActionNode *action, BindingContext *context, Schema *working_schema) {
    if (!action || action->type != AST_ALTER_RENAME_COLUMN) {
        printf("bind_alter_rename_col: Invalid RENAME COLUMN action.\n");
        return false;
    }

    if (!context || !context->db ||
        !context->relations || context->num_relations != 1 ||
        !context->relations[0].table) {
        printf("bind_alter_rename_col: Invalid binding context.\n");
        return false;
    }

    if (!working_schema || !working_schema->columns ||
        working_schema->num_columns == 0 ||
        (working_schema->num_constraints > 0 && !working_schema->constraints)) {
        printf("bind_alter_rename_col: Invalid working schema.\n");
        return false;
    }

    const char *old_name = action->alter_contents.alter_rename_col.old_col_name;
    const char *new_name = action->alter_contents.alter_rename_col.new_col_name;

    if (old_name[0] == '\0' || new_name[0] == '\0') {
        printf("bind_alter_rename_col: Empty column name.\n");
        return false;
    }

    // Validate that the old column name exists
    int32_t old_index = schema_find_column_index(working_schema, old_name);

    if (old_index < 0) {
        printf("bind_alter_rename_col: Old column doesn't exist.\n");
        return false;
    }

    // Validate that the new column name doesn't exist
    if (schema_find_column_index(working_schema, new_name) >= 0) {
        printf("bind_alter_rename_col: New column name already in use.\n");
        return false;
    }

    Column *column = working_schema->columns[old_index];

    if (!column || strlen(new_name) >= sizeof(column->name)) {
        printf("bind_alter_rename_col: Invalid new column name.\n");
        return false;
    }

    // First pass: validate every affected CHECK expression
    for (uint32_t i = 0; i < working_schema->num_constraints; i++) {
        Constraint *constraint = working_schema->constraints[i];

        if (!constraint) {
            return false;
        }

        if (constraint->type != CHECK) {
            continue;
        }

        if (!rewrite_check_identifiers(
                constraint->constraint_data.check.constraint_expr,
                old_index, new_name, NULL, NULL, false)) {
            return false;
        }
    }

    // Second pass: apply the validated rewrites
    for (uint32_t i = 0; i < working_schema->num_constraints; i++) {
        Constraint *constraint = working_schema->constraints[i];

        if (constraint->type == CHECK) {
            if (!rewrite_check_identifiers(
                    constraint->constraint_data.check.constraint_expr,
                    old_index, new_name, NULL, NULL, true)) {
                return false;
            }
        }
    }

    // Update the column name only the working schema
    strcpy(column->name, new_name);

    return true;
}

// Bind MODIFY COLUMN alteration node
bool bind_alter_modify_col(AlterActionNode *action, BindingContext *context, Schema *working_schema) {
    if (!action || action->type != AST_ALTER_MODIFY_COLUMN) {
        printf("bind_alter_modify_col: Invalid MODIFY COLUMN action.\n");
        return false;
    }

    if (!context || !context->db ||
        !context->relations || context->num_relations != 1 ||
        !context->relations[0].table) {
        printf("bind_alter_modify_col: Invalid binding context.\n");
        return false;
    }

    // Resolve table name in case it's been renamed by another alteration operation
    const char *current_table_name = binding_relation_current_name(&context->relations[0]);

    if (!current_table_name || current_table_name[0] == '\0') {
        return false;
    }

    Table *table = context->relations[0].table;
    AlterModifyNode *modify_column = &action->alter_contents.alter_modify; 

    if (!working_schema || !working_schema->columns || 
        !modify_column || modify_column->column_name[0] == '\0') {
        printf("bind_alter_modify_col: Invalid schema or column.\n");
        return false;
    }

    if (working_schema->num_constraints > 0 && !working_schema->constraints) {
        printf("bind_alter_modify_col: Invalid schema constraints.\n");
        return false;
    }

    // Validate that the column exists
    Column *target_column = NULL;
    uint32_t column_index = 0;

    for (uint32_t i = 0; i < working_schema->num_columns; i++) {
        Column *existing = working_schema->columns[i];

        if (!existing || existing->name[0] == '\0') {
            printf("bind_alter_modify_col: Invalid existing column.\n");
            return false;
        }

        if (!strcmp(existing->name, modify_column->column_name)) {
            target_column = existing;
            column_index = i;
            break;   
        }
    }

    if (!target_column) {
        printf("bind_alter_modify_col: Target column name doesn't exist.\n");
        return false;
    }

    if (column_index >= INT32_MAX) {
        printf("bind_alter_modify_col: Target column index exceeds maximum integer limit.\n");
        return false;
    }

    // Validate new data type and data type arguments
    if (modify_column->new_type < INTEGER || modify_column->new_type > JSONB) {
        printf("bind_alter_modify_col: Invalid new data type.\n");
        return false;
    }

    // Validate whether the existing datatype can be converted to the requested datatype.
    if (!value_types_assignable(modify_column->new_type, target_column->type)) {
        printf("bind_alter_modify_col: Unsupported datatype conversion.\n");
        return false;
    }

    // Validate potential changes to data type arguments
    if ((modify_column->new_type == CHAR && modify_column->type_args.length == 0) ||
        (modify_column->new_type == VARCHAR && modify_column->type_args.length == 0) ||
        (modify_column->new_type == NUMERIC &&
        (modify_column->type_args.precision == 0 ||
         modify_column->type_args.scale > modify_column->type_args.precision))) {
        printf("bind_alter_modify_col: Invalid data type arguments.\n");
        return false;
    }

    // Retain the current column
    Column *old_column = working_schema->columns[column_index];

    // Construct the proposed replacement column
    Column *new_column = column_alloc(
        target_column->name, modify_column->new_type, modify_column->type_args,
        target_column->non_null_rows, target_column->null_rows
    ); 

    if (!new_column) {
        printf("bind_alter_modify_col: New column creation failed.\n");
        return false;
    }

    working_schema->columns[column_index] = new_column;

    // Validate affected constraints and indexes
    for (uint32_t i = 0; i < working_schema->num_constraints; i++) {
        Constraint *constraint = working_schema->constraints[i];

        if (!constraint) {
            printf("bind_alter_modify_col: Invalid existing working schema constraint.\n");
            goto rollback;
        }

        // If a constraint doesn't reference the target column, skip its validation
        bool affected = false;

        if (constraint->type == CHECK) {
            affected = true;
        }
        else if (constraint->type == FOREIGN_KEY) {
            affected = 
                foreign_key_uses_column(constraint, column_index) ||
                (!strcmp(
                    constraint->constraint_data.foreign_key.referenced_table_name,
                    current_table_name) &&
                foreign_key_references_column(constraint, column_index));
        }
        else {
            affected = constraint_has_column(constraint, column_index);
        }

        if (!affected) {
            continue;
        }

        switch (constraint->type) {
            case PRIMARY_KEY:
            case UNIQUE:
                // Verify that the proposed datatype can serve as a key in BaseQL's ordered B+ Tree
                if (!are_equality_compatible(modify_column->new_type, modify_column->new_type) ||
                    !supports_ordering(modify_column->new_type)) {
                    printf("bind_alter_modify_col: Unsupported key datatype.\n");
                    goto rollback;
                }

                // Converted-key uniqueness is checked by the executor
                break;

            case FOREIGN_KEY:
                // Validate both sides using the proposed schema
                if (!validate_modified_foreign_key(
                        constraint, table, working_schema, context->db, current_table_name)) {
                    printf("bind_alter_modify_col: FOREIGN KEY datatype mismatch.\n");
                    goto rollback;
                }
                break;

            case CHECK:
                // Check expression types, not stored row values.
                if (!validate_modified_check_expression(
                        constraint, table, working_schema, context->db, current_table_name)) {
                    printf("bind_alter_modify_col: CHECK expression incompatible with new type.\n");
                    goto rollback;
                }
                break;

            case DEFAULT:
                if (!validate_modified_default(
                        constraint, 
                        modify_column->new_type,
                        modify_column->type_args,
                        context)) {
                    printf("bind_alter_modify_col: DEFAULT incompatible with new datatype.\n");
                    goto rollback;
                }
                break;

            case NOT_NULL:
                // No datatype-specific semantic validation. The constraint remains unchanged
                break;

            default:
                printf("bind_alter_modify_col: Unknown constraint type.\n");
                goto rollback;
        }
    }

    // Validate potential incoming (from other tables) FOREIGN KEY constraints
    if (!validate_modify_incoming_foreign_keys(
            table, working_schema, context->db, context->alter_column_map)) {
        printf("bind_alter_modify_col: Incoming FOREIGN KEY incompatibility.\n");
        goto rollback;
    }

    // Validate affected secondary indexes
    if (!validate_modified_column_indexes(
            table, working_schema, column_index, context->alter_column_map)) {
        printf("bind_alter_modify_col: Index incompatibility.\n");
        goto rollback;
    }
    
    free(old_column);
    return true;

rollback:
    working_schema->columns[column_index] = old_column;
    free(new_column);
    return false;
}

// Bind ADD CONSTRAINT alteration node
bool bind_alter_add_constraint(AlterActionNode *action, BindingContext *context, Schema *working_schema) {
    if (!action || action->type != AST_ALTER_ADD_CONSTRAINT) {
        printf("bind_alter_add_constraint: Invalid ADD CONSTRAINT alteration node.\n");
        return false;
    }

    if (!context || !context->db || 
        !context->relations || context->num_relations != 1 || !context->relations[0].table) {
        printf("bind_alter_add_constraint: Invalid binding context.\n");
        return false;
    }

    // Resolve table name in case it's been renamed by another alteration operation
    const char *current_table_name = binding_relation_current_name(&context->relations[0]);

    if (!current_table_name || current_table_name[0] == '\0') {
        return false;
    }

    if (!working_schema || 
        !working_schema->columns || working_schema->num_columns == 0 || 
        (working_schema->num_constraints > 0 && !working_schema->constraints)) {
        printf("bind_alter_add_constraint: Invalid working schema.\n");
        return false;
    }

    if (working_schema->num_constraints == UINT32_MAX) {
        printf("bind_alter_add_constraint: Constraint count limit reached.\n");
        return false;
    }

    if (working_schema->num_columns > INT32_MAX) {
        printf("bind_alter_add_constraint: Column index limit exceeded.\n");
        return false;
    }

    for (uint32_t i = 0; i < working_schema->num_constraints; i++) {
        if (!working_schema->constraints[i]) {
            printf("bind_alter_add_constraint: Invalid existing constraint.\n");
            return false;
        }
    }

    Table *table = context->relations[0].table;
    ConstraintNode *new_constraint = action->alter_contents.alter_add_constraint.constraint;

    if (!table || !new_constraint) {
        printf("bind_alter_add_constraint: Invalid table or constraint.\n");
        return false;
    }

    // Validate and uniqueness of constraint name
    if (new_constraint->constraint_name[0] != '\0') {
        for (uint32_t i = 0; i < working_schema->num_constraints; i++) {
            Constraint *existing = working_schema->constraints[i];

            if (!existing) {
                printf("bind_alter_add_constraint: Invalid existing constraint.\n");
                return false;
            }

            if (!strcmp(existing->constraint_name, new_constraint->constraint_name)) {
                printf("bind_alter_add_constraint: Duplicate constraint name.\n");
                return false;   
            }
        }
    }

    // Prepare the constraint's runtime name
    char runtime_name[64] = {0};

    if (new_constraint->constraint_name[0] != '\0') {
        // Preserve the explicitly provided constraint name
        memcpy(runtime_name, new_constraint->constraint_name, sizeof(runtime_name));
        runtime_name[sizeof(runtime_name) - 1] = '\0';
    }
    else {
        // ADD CONSTRAINT has no new ColumnDefNode
        // Supply an empty column definition to the existing naming helper
        ColumnDefNode empty_column = {0};

        if (!generate_alter_constraint_name(
                new_constraint, &empty_column, table, working_schema, NULL, 0, runtime_name
            )) {
            printf("bind_alter_add_constraint: Constraint name generation failed.\n");
            return false;
        }
    }

    // Used to allocate the runtime constraint during binding
    Constraint *prepared = NULL;

    switch (new_constraint->type) {
        case AST_CONSTRAINT_PRIMARY_KEY:
        case AST_CONSTRAINT_UNIQUE: {
            bool primary = new_constraint->type == AST_CONSTRAINT_PRIMARY_KEY;

            ExpressionNode **refs = primary
                ? new_constraint->constraint_data.primary_key.column_refs
                : new_constraint->constraint_data.unique.column_refs;

            uint32_t count = primary
                ? new_constraint->constraint_data.primary_key.num_columns
                : new_constraint->constraint_data.unique.num_columns;

            if (!refs || !count || count > working_schema->num_columns ||
                (size_t)count > SIZE_MAX / sizeof(uint32_t)) {
                goto failed;
            }

            if (primary) {
                for (uint32_t i = 0; i < working_schema->num_constraints; i++) {
                    if (working_schema->constraints[i]->type == PRIMARY_KEY) {
                        printf("bind_alter_add_constraint: PRIMARY KEY already exists.\n");
                        goto failed;
                    }
                }
            }

            uint32_t *indexes = calloc(count, sizeof(*indexes));

            if (!indexes) {
                goto failed;
            }

            bool valid = resolve_constraint_column_refs(refs, count, working_schema, current_table_name, indexes);

            for (uint32_t i = 0; valid && i < count; i++) {
                DataType type = working_schema->columns[indexes[i]]->type;

                if (!supports_ordering(type) || !are_equality_compatible(type, type)) {
                    valid = false;
                }

                if (primary) {
                    // PRIMARY KEY columns cannot have DEFAULT NULL.
                    for (uint32_t j = 0; valid && j < working_schema->num_constraints; j++) {
                        Constraint *existing = working_schema->constraints[j];

                        if (existing->type == DEFAULT &&
                            constraint_has_column(existing, indexes[i]) &&
                            expression_is_explicit_null_literal(existing->constraint_data.default_value.default_expr)) {
                            valid = false;
                        }
                    }
                }
            }

            // Validate physical B+ Tree storage limits
            Index *temp_index = NULL;
            IndexKey *key = NULL;

            if (valid) {
                key = index_key_create(indexes, count);

                if (!key) {
                    printf("bind_alter_add_constraint: Temporary index key allocation failed.\n");
                    valid = false;
                }
            }

            if (valid) {
                temp_index = index_metadata_create(
                    primary ? "temp_pk_index" : "temp_uq_index",
                    primary ? PRIMARY_INDEX : SECONDARY_INDEX,
                    key,
                    UINT32_MAX,
                    true
                );

                if (!temp_index) {
                    printf("bind_alter_add_constraint: Temporary index allocation failed.\n");
                    valid = false;
                }
            }

            if (valid) {
                valid = btree_index_definition_fits(temp_index, working_schema);

                if (!valid) {
                    printf("bind_alter_add_constraint: Index exceeds physical storage limits.\n");
                }
            }

            // Build the runtime constraint after all validations succeed
            if (valid) {
                prepared = primary
                    ? constraint_create_primary_key(runtime_name, indexes, count)
                    : constraint_create_unique(runtime_name, indexes, count);
            }

            // Release all temporary allocations
            if (temp_index) {
                index_free(temp_index);
            }

            if (key) {
                index_key_free(key);
            }

            free(indexes);

            if (!valid || !prepared) {
                goto failed;
            }

            break;
        }

        case AST_CONSTRAINT_FOREIGN_KEY: {
            ForeignKeyConstraintNode *fk = &new_constraint->constraint_data.foreign_key;

            uint32_t count = fk->num_local_columns;

            if (!fk->referenced_table_name[0] || !fk->local_column_refs ||
                !fk->referenced_column_refs || !count || count != fk->num_referenced_columns ||
                (size_t)count > SIZE_MAX / sizeof(uint32_t)) {
                goto failed;
            }

            const Schema *ref_schema = NULL;

            if (!strcmp(fk->referenced_table_name, current_table_name)) {
                ref_schema = working_schema;
            }
            else {
                Table *referenced = database_find_table(context->db, fk->referenced_table_name);

                if (!referenced) {
                    goto failed;
                }

                ref_schema = referenced->table_schema;
            }

            if (!ref_schema || !ref_schema->columns ||
                count > ref_schema->num_columns) {
                goto failed;
            }

            uint32_t *local = calloc(count, sizeof(*local));
            uint32_t *remote = calloc(count, sizeof(*remote));

            if (!local || !remote) {
                free(local);
                free(remote);
                goto failed;
            }

            // Resolve local and referenced columns
            bool valid = resolve_constraint_column_refs(
                fk->local_column_refs,
                count,
                working_schema,
                current_table_name,
                local
            ) &&
            resolve_constraint_column_refs(
                fk->referenced_column_refs,
                count,
                ref_schema,
                fk->referenced_table_name,
                remote
            );

            // Validate each local/referenced datatype pair.
            for (uint32_t i = 0; valid && i < count; i++) {
                if (!fk_runtime_types_compatible(working_schema->columns[local[i]], ref_schema->columns[remote[i]])) {
                    valid = false;
                }
            }

            // Referenced columns must constitute a candidate key.
            if (valid) {
                valid = fk_matches_schema_key(ref_schema, remote, count);
            }

            if (valid) {
                prepared = constraint_create_foreign_keys(
                    runtime_name, local, count, fk->referenced_table_name, remote, count
                );
            }

            free(local);
            free(remote);

            if (!valid || !prepared) {
                printf("bind_alter_add_constraint: Invalid FOREIGN KEY definition.\n");
                goto failed;
            }

            break;
        }

        case AST_CONSTRAINT_CHECK: {
            ExpressionNode *expr = new_constraint->constraint_data.check.check_expr;

            if (!expr) {
                goto failed;
            }

            // Bind a copy rather than modifying the parser-owned AST
            ExpressionNode *copy = expression_node_copy(expr);

            if (!copy) {
                goto failed;
            }

            BindingRelation relation = {0};
            relation.table = table;
            relation.proposed_schema = working_schema;
            relation.proposed_column = NULL;

            const char *current_table_name = binding_relation_current_name(&context->relations[0]);

            if (!current_table_name ||
                strlen(current_table_name) >= sizeof(relation.proposed_table_name)) {
                expression_node_free(copy);
                goto failed;
            }

            strcpy(relation.proposed_table_name, current_table_name);

            BindingContext check_context = {0};
            check_context.db = context->db;
            check_context.relations = &relation;
            check_context.num_relations = 1;
            check_context.current_clause = BIND_CLAUSE_CHECK;

            BoundExpressionType result = {0};

            bool valid = 
                bind_scalar_expression(copy, &check_context, &result) &&
                !result.is_untyped_null &&
                result.type == BOOL;

            uint32_t *indexes = NULL;
            uint32_t used = 0;

            if (valid) {
                indexes = calloc(working_schema->num_columns, sizeof(*indexes));

                valid = indexes && collect_bound_check_refs(copy, indexes, working_schema->num_columns, &used);
            }

            if (valid) {
                prepared = constraint_create_check(runtime_name, copy, used ? indexes : NULL, used);
            }

            free(indexes);
            expression_node_free(copy);

            if (!valid || !prepared) {
                goto failed;
            }

            break;
        }

        case AST_CONSTRAINT_DEFAULT: {
            DefaultConstraintNode *def = &new_constraint->constraint_data.default_value;

            if (!def->column_name[0] || !def->default_expr) {
                goto failed;
            }

            int32_t column_index = schema_find_column_index(working_schema, def->column_name);

            if (column_index < 0) {
                goto failed;
            }

            // Reject an existing DEFAULT for this column
            for (uint32_t i = 0; i < working_schema->num_constraints; i++) {
                Constraint *existing = working_schema->constraints[i];

                if (existing->type == DEFAULT &&
                    constraint_has_column(existing, (uint32_t)column_index)) {
                    goto failed;
                }
            }

            ExpressionNode *copy = expression_node_copy(def->default_expr);

            if (!copy) {
                goto failed;
            }

            BindingContext default_context = {0};
            default_context.db = context->db;
            default_context.current_clause = BIND_CLAUSE_DEFAULT;

            BoundExpressionType result = {0};

            bool valid = bind_scalar_expression(copy, &default_context, &result);

            bool null_default = expression_is_explicit_null_literal(copy);

            Column *column = working_schema->columns[column_index];

            if (!column) {
                expression_node_free(copy);
                goto failed;
            }

            // Validate DEFAULT expression assignment
            if (valid && !null_default) {
                if (result.is_untyped_null) {
                    // Only a genuine NULL expression can be untyped NULL
                    valid = false;
                }
                else if (copy->type == EXPR_LITERAL) {
                    Literal *literal = &copy->expression_data.literal_value;

                    Value *value = literal->literal;

                    valid = value && !value->null_val && value_can_assign(column->type, value);

                    // Validate destination datatype parameters when
                    // the value has the same datatype as the column
                    if (valid && value->type == column->type) {
                        valid = value_fits_type_args(value, column->type, column->type_args);
                    }
                }
                else {
                    valid = value_types_assignable(column->type, result.type);
                }
            }

            // DEFAULT NULL conflicts with NOT NULL and PRIMARY KEY
            if (valid && null_default) {
                for (uint32_t i = 0;
                    i < working_schema->num_constraints;
                    i++) {

                    Constraint *existing = working_schema->constraints[i];

                    if ((existing->type == NOT_NULL || existing->type == PRIMARY_KEY) &&
                        constraint_has_column(existing, (uint32_t)column_index)) {
                        valid = false;
                        break;
                    }
                }
            }

            if (valid) {
                prepared = constraint_create_default(runtime_name, (uint32_t)column_index, copy);
            }

            expression_node_free(copy);

            if (!valid || !prepared) {
                goto failed;
            }

            break;
        }

        case AST_CONSTRAINT_NOT_NULL: {
            ExpressionNode *ref = new_constraint->constraint_data.not_null.column_ref;

            if (!ref) {
                goto failed;
            }

            uint32_t index = 0;

            if (!resolve_constraint_column_refs(&ref, 1, working_schema, current_table_name, &index)) {
                goto failed;
            }

            for (uint32_t i = 0; i < working_schema->num_constraints; i++) {
                Constraint *existing = working_schema->constraints[i];

                // Already effectively NOT NULL
                if ((existing->type == NOT_NULL || existing->type == PRIMARY_KEY) &&
                    constraint_has_column(existing, index)) {
                    goto failed;
                }

                // Reject conflicts with an existing DEFAULT NULL
                if (existing->type == DEFAULT &&
                    constraint_has_column(existing, index) &&
                    expression_is_explicit_null_literal(existing->constraint_data.default_value.default_expr)) {
                    goto failed;
                }
            }

            prepared = constraint_create_not_null(runtime_name, index);

            if (!prepared) {
                goto failed;
            }

            break;
        }

        default:
            printf("bind_alter_add_constraint: Unknown constraint type.\n");
            goto failed;
    }

    // Check for an existing constraint with the same effect
    if (schema_has_equivalent_constraint(working_schema, prepared)) {
        printf("bind_alter_add_constraint: Schema already has equivalent constraint.\n");
        goto failed;
    }

    // Append the validated constraint to the working schema
    size_t new_count = (size_t)working_schema->num_constraints + 1;

    if (new_count > SIZE_MAX / sizeof(Constraint *)) {
        goto failed;
    }

    Constraint **expanded_constraints = realloc(
        working_schema->constraints,
        new_count * sizeof(*expanded_constraints)
    );

    if (!expanded_constraints) {
        goto failed;
    }

    working_schema->constraints = expanded_constraints;
    working_schema->constraints[working_schema->num_constraints] = prepared;
    working_schema->num_constraints++;

    return true;

failed:
    if (prepared) {
        constraint_free(prepared);
    }
    return false;
}

// Bind DROP CONSTRAINT alteration node
bool bind_alter_drop_constraint(AlterActionNode *action, BindingContext *context, Schema *working_schema) {
    if (!action || action->type != AST_ALTER_DROP_CONSTRAINT) {
        printf("bind_alter_drop_constraint: Invalid DROP CONSTRAINT alteration node.\n");
        return false;
    }

    if (!context || !context->db || 
        !context->relations || context->num_relations != 1 || !context->relations[0].table) {
        printf("bind_alter_drop_constraint: Invalid binding context.\n");
        return false;
    }

    if (!working_schema || 
        !working_schema->columns || working_schema->num_columns == 0 || 
        (working_schema->num_constraints > 0 && !working_schema->constraints)) {
        printf("bind_alter_drop_col: Invalid working schema.\n");
        return false;
    }

    if (working_schema->num_columns > INT32_MAX) {
        printf("bind_alter_drop_constraint: Column index limit exceeded.\n");
        return false;
    }

    Table *table = context->relations[0].table;

    uint32_t constraint_index = UINT32_MAX;
    char *drop_constraint_name = action->alter_contents.alter_drop_constraint.constraint_name;

    if (drop_constraint_name[0] == '\0') {
        printf("bind_alter_drop_constraint: Invalid target constraint name.\n");
        return false;
    }

    for (uint32_t i = 0; i < working_schema->num_constraints; i++) {
        Constraint *constraint = working_schema->constraints[i];

        if (!constraint || constraint->constraint_name[0] == '\0') {
            printf("bind_alter_drop_constraint: Invalid existing constraint.\n");
            return false;
        }

        // Skip constraints that don't match
        if (strcmp(drop_constraint_name, constraint->constraint_name)) {
            continue;
        }

        // At this point we've found the matching constraint
        constraint_index = i;

        switch (constraint->type) {
            case PRIMARY_KEY:
            case UNIQUE: {
                bool referenced = false;

                if (!constraint_is_referenced_by_foreign_key(
                        context->db,
                        table,
                        working_schema,
                        binding_relation_current_name(&context->relations[0]),
                        context->alter_column_map,
                        constraint,
                        &referenced
                    )) {
                    printf("bind_alter_drop_constraint: FOREIGN KEY dependency inspection failed.\n");
                    return false;
                }

                if (referenced) {
                    printf("bind_alter_drop_constraint: Constraint is referenced by a FOREIGN KEY.\n");
                    return false;
                }

                break;
            }

            case FOREIGN_KEY:
            case CHECK:
            case DEFAULT:
            case NOT_NULL:
                // Binding doesn't require any other checks for those constraint types
                // And the any other tables are not affected by the removal of 
                // the current table's FOREIGN KEY toward them 
                break;

            default:
                printf("bind_alter_drop_constraint: Invalid constraint type.\n");
                return false;
        }

        break;
    }

    if (constraint_index == UINT32_MAX) {
        printf("bind_alter_drop_constraint: Target constraint doesn't exist in the working schema.\n");
        return false;
    }

    // Update working schema by deallocating the target constraint
    constraint_free(working_schema->constraints[constraint_index]);

    for (uint32_t i = constraint_index; i + 1 < working_schema->num_constraints; i++) {
        working_schema->constraints[i] = working_schema->constraints[i + 1];
    }

    working_schema->num_constraints--;
    working_schema->constraints[working_schema->num_constraints] = NULL;

    return true;
}


/* ---------- Binding Context Management ---------- */

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


/* ---------- SELECT Grouping and Aggregate Validation ---------- */

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

            // Handle typed NULL values
            if (left_literal->literal->null_val || right_literal->literal->null_val) {
                return left_literal->literal->null_val && right_literal->literal->null_val;
            }

            int comparison = 0;

            if (!value_compare(left_literal->literal, right_literal->literal, &comparison)) {
                return false;
            }

            return comparison == 0;
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
            const In *in = &expr->expression_data.in_expr;
            
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
            const Between *between = &expr->expression_data.between_expr;

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


/* ---------- INSERT Assignment Validation and Output Construction ---------- */

// Check assignment compatibility between columns and corresponding inserted values
bool validate_insert_assignments(IntoNode *into, ValuesNode *values, 
    const BoundExpressionType *value_types, BindingContext *context) {
    
    if (!into || 
        into->table_name[0] == '\0' || 
        (into->num_column_refs > 0 && !into->column_refs) ||
        (into->num_column_refs == 0 && into->column_refs)) {
        printf("validate_insert_assignments: Invalid INTO AST node.\n");
        return false;
    }

    if (!values || !values->values || values->num_values == 0) {
        printf("validate_insert_assignments: Invalid VALUES AST node.\n");
        return false;
    }

    if (!context || 
        !context->relations ||
        !context->relations[0].table || 
        context->num_relations != 1) {
        printf("validate_insert_assignments: Invalid context.\n");
        return false;
    }

    if (!value_types) {
        printf("validate_insert_assignments: Invalid value data types.\n");
        return false;
    }

    // Validate inserting table and its schema
    Table *table = context->relations[0].table;

    if (!table->table_schema || 
        !table->table_schema->columns ||
        table->table_schema->num_columns == 0) {
        printf("validate_insert_assignments: Invalid table schema.\n");
        return false;
    }

    Schema *schema = table->table_schema;

    // Validate total number of column-value pairs
    uint32_t num_target_columns = 
        into->num_column_refs > 0 ? into->num_column_refs : schema->num_columns;

    if (values->num_values != num_target_columns) {
        printf("validate_insert_assignments: Column/value count mismatch.\n");
        return false;
    }

    // Validate column-value pairs
    for (uint32_t i = 0; i < values->num_values; i++) {
        uint32_t column_index = i;

        if (into->num_column_refs > 0) {
            ExpressionNode *column_expr = into->column_refs[i];

            if (!column_expr || column_expr->type != EXPR_COLUMN_REF) {
                printf("validate_insert_assignments: Invalid destination column.\n");
                return false;
            }

            ColumnRef *column_ref = &column_expr->expression_data.column_value;

            if (column_ref->relation_index != 0 || 
                column_ref->column_index < 0 ||
                (uint32_t)column_ref->column_index >= schema->num_columns) {
                printf("validate_insert_assignments: Invalid bound column index.\n");
                return false;
            }

            column_index = (uint32_t) column_ref->column_index;
        }

        Column *column = schema->columns[column_index];

        if (!column) {
            printf("validate_insert_assignments: Invalid schema column.\n");
            return false;
        }

        ExpressionNode *value_expr = values->values[i];

        if (!value_expr) {
            printf("validate_insert_assignments: Invalid value expression.\n");
            return false;
        }

        BoundExpressionType value_type = value_types[i];

        // Determine whether the supplied expression is an explicit NULL
        bool is_explicit_null = value_type.is_untyped_null;

        if (value_expr->type == EXPR_LITERAL) {
            Literal *literal = &value_expr->expression_data.literal_value;

            if (!literal->is_untyped_null && literal->literal && literal->literal->null_val) {
                is_explicit_null = true;
            }
        }

        // Validate assignment compatibility for all typed expressions
        if (!is_explicit_null) {
            if (value_expr->type == EXPR_LITERAL) {
                Literal *literal = &value_expr->expression_data.literal_value;

                if (!literal->literal ||
                    !value_can_assign(column->type, literal->literal)) {
                    printf("validate_insert_assignments: Invalid literal assignment.\n");
                    return false;
                }
            }
            else if (!value_types_assignable(column->type, value_type.type)) {
                printf("validate_insert_assignments: Incompatible expression type.\n");
                return false;
            }
        }

        // Validate NULL constraints for explicit NULL assignments
        if (is_explicit_null) {
            if (schema->num_constraints > 0 && !schema->constraints) {
                printf("validate_insert_assignments: Invalid constraint array.\n");
                return false;
            }

            for (uint32_t j = 0; j < schema->num_constraints; j++) {
                Constraint *constraint = schema->constraints[j];

                if (!constraint) {
                    printf("validate_insert_assignments: Invalid constraint in table's schema.\n");
                    return false;
                }

                if (constraint->type != NOT_NULL && constraint->type != PRIMARY_KEY) {
                    continue;
                }

                if (constraint_has_column(constraint, column_index)) {
                    printf("validate_insert_assignments: NULL is not allowed in this column.\n");
                    return false;
                }
            }
        }
    }

    return true;
}

// Build insertion values tuple
bool build_bound_insertion_values(IntoNode *into, ValuesNode *values,
    const BoundExpressionType *value_types, BindingContext *context, BoundInsertionValues *out_values) {
    if (!into || 
        into->table_name[0] == '\0' || 
        (into->num_column_refs > 0 && !into->column_refs) ||
        (into->num_column_refs == 0 && into->column_refs)) {
        printf("build_bound_insertion_values: Invalid AST into node.\n");
        return false;
    }

    if (!values || !values->values || values->num_values == 0) {
        printf("build_bound_insertion_values: Invalid VALUES AST node.\n");
        return false;
    }

    if (!context || !context->db || 
        !context->relations || 
        context->num_relations != 1 ||
        !context->relations[0].table) {
        printf("build_bound_insertion_values: Invalid binding context.\n");
        return false;
    }

    if (!value_types) {
        printf("build_bound_insertion_values: Invalid value types.\n");
        return false;
    }

    if (!out_values) {
        printf("build_bound_insertion_values: Invalid output.\n");
        return false;
    }

    // Caller must provide an empty output
    if (out_values->table || out_values->values || out_values->num_values != 0) {
        printf("build_bound_insertion_values: Output must be empty.\n");
        return false;
    }

    Table *table = context->relations[0].table;
    Schema *schema = table->table_schema;

    if (!schema || !schema->columns || schema->num_columns == 0 ||
        (schema->num_constraints > 0 && !schema->constraints)) {
        printf("build_bound_insertion_values: Invalid table schema.\n");
        return false;
    }

    uint32_t num_columns = schema->num_columns;
    uint32_t expected_values = 
        into->num_column_refs > 0 ? into->num_column_refs : num_columns;

    if (values->num_values != expected_values) {
        printf("build_bound_insertion_values: Column/value count mismatch.\n");
        return false;
    }

    if ((size_t)num_columns > SIZE_MAX / sizeof(BoundInsertValue) ||
        (size_t)num_columns > SIZE_MAX / sizeof(bool)) {
        printf("build_bound_insertion_values: Allocation size overflow.\n");
        return false;
    }

    BoundInsertValue *bound_values = (BoundInsertValue *) calloc(num_columns, sizeof(BoundInsertValue));
    bool *assigned = (bool *) calloc(num_columns, sizeof(bool));

    if (!bound_values || !assigned) {
        printf("build_bound_insertion_values: Allocation failed.\n");
        free(bound_values);
        free(assigned);
        return false;
    }

    bool success = false;

    // Map explicitly supplied expressions into schema order
    for (uint32_t i = 0; i < values->num_values; i++) {
        uint32_t column_index = i;

        if (into->num_column_refs > 0) {
            ExpressionNode *column_expr = into->column_refs[i];

            if (!column_expr || column_expr->type != EXPR_COLUMN_REF) {
                printf("build_bound_insertion_values: Invalid destination column.\n");
                goto cleanup;
            }

            ColumnRef *column_ref = &column_expr->expression_data.column_value;

            if (column_ref->relation_index != 0 || column_ref->column_index < 0 ||
                (uint32_t)column_ref->column_index >= num_columns) {
                printf("build_bound_insertion_values: Invalid bound column.\n");
                goto cleanup;
            }

            column_index = (uint32_t)column_ref->column_index;
        }

        if (!schema->columns[column_index] || !values->values[i]) {
            printf("build_bound_insertion_values: Invalid column or value.\n");
            goto cleanup;
        }

        // Check for duplicate column assignment
        if (assigned[column_index]) {
            printf("build_bound_insertion_values: Duplicate destination column.\n");
            goto cleanup;
        }

        // Assign existing expression and data type for the corresponding column
        assigned[column_index] = true;

        bound_values[column_index].kind = BOUND_INSERT_EXPRESSION;
        bound_values[column_index].expression = values->values[i];
        bound_values[column_index].result_type = value_types[i];
    }

    // Resolve columns omitted from the explicit INSERT column list
    for (uint32_t i = 0; i < num_columns; i++) {
        if (assigned[i]) {
            continue;
        }

        Column *column = schema->columns[i];

        if (!column) {
            printf("build_bound_insertion_values: Invalid schema column.\n");
            goto cleanup;
        }

        Constraint *default_constraint = NULL;
        bool allows_null = true;

        // Check if any constraint applies to the current omitted column
        // and if it does check its type to verify if it allows null or has a default value
        for (uint32_t j = 0; j < schema->num_constraints; j++) {
            Constraint *constraint = schema->constraints[j];

            if (!constraint) {
                printf("build_bound_insertion_values: Invalid constraint.\n");
                goto cleanup;
            }

            if (!constraint_has_column(constraint, i)) {
                continue;
            }

            if (constraint->type == NOT_NULL || constraint->type == PRIMARY_KEY) {
                allows_null = false;
            }
            else if (constraint->type == DEFAULT) {
                if (default_constraint) {
                    printf("build_bound_insertion_values: Duplicate DEFAULT.\n");
                    goto cleanup;
                }

                default_constraint = constraint;
            }
        }

        // If omitted column has a DEFAULT expression, bind and validate it
        if (default_constraint) {
            ExpressionNode *default_expr =
                default_constraint->constraint_data.default_value.default_expr;

            if (!default_expr) {
                printf("build_bound_insertion_values: Invalid DEFAULT expression.\n");
                goto cleanup;
            }

            BoundExpressionType default_type = {0};

            BindingClause previous_clause = context->current_clause;
            context->current_clause = BIND_CLAUSE_DEFAULT;

            bool bound = bind_scalar_expression(default_expr, context, &default_type);

            context->current_clause = previous_clause;

            if (!bound) {
                printf("build_bound_insertion_values: DEFAULT expression binding failed.\n");
                goto cleanup;
            }

            // Validate type compatibility
            if (!default_type.is_untyped_null) {
                if (default_expr->type == EXPR_LITERAL) {
                    Literal *literal = &default_expr->expression_data.literal_value;

                    if (!literal->literal ||
                        !value_can_assign(column->type, literal->literal)) {
                        return false;
                    }
                }
                else if (!value_types_assignable(column->type, default_type.type)) {
                    return false;
                }
            }

            // A DEFAULT NULL is invalid for NOT NULL / PRIMARY KEY columns
            bool is_null_default = default_type.is_untyped_null;

            if (default_expr->type == EXPR_LITERAL) {
                Literal *literal = &default_expr->expression_data.literal_value;

                if (!literal->is_untyped_null && literal->literal && literal->literal->null_val) {
                    is_null_default = true;
                }
            }

            if (is_null_default && !allows_null) {
                printf("build_bound_insertion_values: NULL DEFAULT for required column.\n");
                goto cleanup;
            }

            bound_values[i].kind = BOUND_INSERT_DEFAULT;
            bound_values[i].expression = default_expr;
            bound_values[i].result_type = default_type;
        }
        else if (allows_null) {
            bound_values[i].kind = BOUND_INSERT_NULL;
            bound_values[i].expression = NULL;
            bound_values[i].result_type = (BoundExpressionType) {
                .type = column->type,
                .is_untyped_null = true
            };
        }
        else {
            printf("build_bound_insertion_values: Required column omitted.\n");
            goto cleanup;
        }
    }

    // Publish only after successful construction
    out_values->table = table;
    out_values->values = bound_values;
    out_values->num_values = num_columns;

    success = true;

cleanup:
    free(assigned);

    if (!success) {
        free(bound_values);
    }

    return success;
}


/* ---------- UPDATE Assignment Validation and Output Construction ---------- */

// Check assignment compatibility between columns and corresponding update values
bool validate_update_assignments(SetNode *set, const BoundExpressionType *value_types, 
    BindingContext *context) {
    
    if (!set || !set->assignments || set->num_assignments == 0) {
        printf("validate_update_assignments: Invalid SET AST node.\n");
        return false;
    }

    if (!context || !context->db ||
        !context->relations ||
        !context->relations[0].table || 
        context->num_relations != 1) {
        printf("validate_update_assignments: Invalid context.\n");
        return false;
    }

    if (!value_types) {
        printf("validate_update_assignments: Invalid value data types.\n");
        return false;
    }
    
    // Validate updated table and its schema
    Table *table = context->relations[0].table;
    Schema *schema = table->table_schema;

    if (!schema || !schema->columns || schema->num_columns == 0 ||
        (schema->num_constraints > 0 && !schema->constraints)) {
        printf("validate_update_assignments: Invalid table schema.\n");
        return false;
    }

    for (uint32_t i = 0; i < set->num_assignments; i++) {
        ExpressionNode *column_expr = set->assignments[i].column_ref;
        ExpressionNode *value_expr = set->assignments[i].value;

        if (!column_expr || column_expr->type != EXPR_COLUMN_REF ||
            !value_expr) {
            printf("validate_update_assignments: Invalid assignment.\n");
            return false;
        }

        ColumnRef *column_ref = &column_expr->expression_data.column_value;

        if (column_ref->relation_index != 0 ||
            column_ref->column_index < 0 ||
            (uint32_t)column_ref->column_index >= schema->num_columns) {
            printf("validate_update_assignments: Invalid bound column.\n");
            return false;
        }

        uint32_t column_index = column_ref->column_index;
        Column *column = schema->columns[column_index];

        if (!column) {
            printf("validate_update_assignments: Invalid schema column.\n");
            return false;
        }

        // Determine if assigned value in NULL
        BoundExpressionType value_type = value_types[i];
        bool is_explicit_null = value_type.is_untyped_null;

        if (value_expr->type == EXPR_LITERAL) {
            Literal *literal = &value_expr->expression_data.literal_value;

            if (!literal->is_untyped_null && literal->literal && literal->literal->null_val) {
                is_explicit_null = true;
            }
        }

        // Validate type compatibility if assigned value is not NULL
        if (!is_explicit_null) {
            if (value_expr->type == EXPR_LITERAL) {
                Literal *literal = &value_expr->expression_data.literal_value;

                if (!literal->literal || !value_can_assign(column->type, literal->literal)) {
                    printf("validate_update_assignments: Invalid literal assignment.\n");
                    return false;
                }
            }
            else if (!value_types_assignable(column->type, value_type.type)) {
                printf("validate_update_assignments: Incompatible expression type.\n");
                return false;
            }
        }

        // Validate explicit NULL assignments
        if (is_explicit_null) {
            for (uint32_t j = 0; j < schema->num_constraints; j++) {
                Constraint *constraint = schema->constraints[j];

                if (!constraint) {
                    printf("validate_update_assignments: Invalid constraint.\n");
                    return false;
                }

                if (constraint->type != NOT_NULL && constraint->type != PRIMARY_KEY) {
                    continue;
                }

                if (constraint_has_column(constraint, column_index)) {
                    printf("validate_update_assignments: NULL not allowed.\n");
                    return false;
                }   
            }
        }

    }

    return true;
}

// Build updated values tuple
bool build_bound_update_values(SetNode *set, const BoundExpressionType *value_types, 
    BindingContext *context, BoundUpdateValues *out_values) {

    if (!set || !set->assignments || set->num_assignments == 0) {
        printf("build_bound_update_values: Invalid SET AST node.\n");
        return false;
    }

    if (!context || !context->db || 
        !context->relations || 
        context->num_relations != 1 ||
        !context->relations[0].table) {
        printf("build_bound_update_values: Invalid binding context.\n");
        return false;
    }

    if (!value_types || !out_values) {
        printf("build_bound_update_values: Invalid output or value types.\n");
        return false;
    }

    // Caller must provide an empty output
    if (out_values->table || out_values->values || out_values->num_values != 0) {
        printf("build_bound_update_values: Output must be empty.\n");
        return false;
    }

    Table *table = context->relations[0].table;
    Schema *schema = table->table_schema;

    if (!schema || !schema->columns || schema->num_columns == 0) {
        printf("build_bound_update_values: Invalid table schema.\n");
        return false;
    }

    uint32_t num_assignments = set->num_assignments;

    if (num_assignments > schema->num_columns ||
        (size_t)num_assignments > SIZE_MAX / sizeof(BoundUpdateValue)) {
        printf("build_bound_update_values: Invalid assignment count.\n");
        return false;
    }

    BoundUpdateValue *bound_values = (BoundUpdateValue *) calloc(num_assignments, sizeof(BoundUpdateValue));

    if (!bound_values) {
        printf("build_bound_update_values: Allocation failed.\n");
        return false;
    }

    for (uint32_t i = 0; i < num_assignments; i++) {
        ExpressionNode *column_expr = set->assignments[i].column_ref;
        ExpressionNode *value_expr = set->assignments[i].value;

        if (!column_expr || column_expr->type != EXPR_COLUMN_REF || !value_expr) {
            printf("build_bound_update_values: Invalid assignment.\n");
            free(bound_values);
            return false;
        }

        ColumnRef *column_ref = &column_expr->expression_data.column_value;

        if (column_ref->relation_index != 0 || column_ref->column_index < 0 ||
            (uint32_t)column_ref->column_index >= schema->num_columns) {
            printf("build_bound_update_values: Invalid bound column.\n");
            free(bound_values);
            return false;
        }

        uint32_t column_index = (uint32_t)column_ref->column_index;

        if (!schema->columns[column_index]) {
            printf("build_bound_update_values: Invalid schema column.\n");
            free(bound_values);
            return false;
        }

        // Defensive duplicate check.
        for (uint32_t j = 0; j < i; j++) {
            if (bound_values[j].column_index == column_index) {
                printf("build_bound_update_values: Duplicate destination column.\n");
                free(bound_values);
                return false;
            }
        }

        // Assign existing expression and data type for the corresponding column
        bound_values[i].column_index = column_index;
        bound_values[i].expression = value_expr;
        bound_values[i].result_type = value_types[i];
    }

    // Publish only after successful construction
    out_values->table = table;
    out_values->values = bound_values;
    out_values->num_values = num_assignments;

    return true;
}


/* ---------- DELETE Output Construction ---------- */

// Build delete output
bool build_bound_delete_values(WhereNode *where, BindingContext *context, 
    BoundDeleteValues *out_values) {
    
    if (!context || !context->db || 
        !context->relations || 
        context->num_relations != 1 ||
        !context->relations[0].table) {
        printf("build_bound_delete_values: Invalid binding context.\n");
        return false;
    }

    if (!out_values) {
        printf("build_bound_delete_values: Invalid output or value types.\n");
        return false;
    }

    // Caller must provide an empty output
    if (out_values->table || out_values->where) {
        printf("build_bound_delete_values: Output must be empty.\n");
        return false;
    }

    Table *table = context->relations[0].table;
    
    out_values->table = table;
    out_values->where = where;

    return true;
}


/* ---------- CREATE TABLE Column Resolution and Constraint Validation ---------- */

// Resolves column index for a column name in the new table schema
bool resolve_create_table_column(const CreateTableNode *create_table, const char *column_name,
    uint32_t *out_index) {
    if (!create_table || !create_table->columns || !create_table->columns->column_defs ||
        !column_name || column_name[0] == '\0' || !out_index) {
        printf("resolve_create_table_column: Invalid arguments.\n");
        return false;
    }

    ColumnsNode *columns = create_table->columns;

    for (uint32_t i = 0; i < columns->num_column_defs; i++) {
        ColumnDefNode *column = columns->column_defs[i];

        if (!column || column->column_name[0] == '\0') {
            printf("resolve_create_table_column: Invalid column definition.\n");
            return false;
        }

        if (!strcmp(column->column_name, column_name)) {
            *out_index = i;
            return true;
        }
    }

    return false;
}

// Validates a constraint's column references
bool validate_constraint_columns(ExpressionNode **column_refs, uint32_t count,
    const CreateTableNode *create_table) {
    if (!column_refs || count == 0 || !create_table) {
        printf("validate_constraint_columns: Invalid input arguments.\n");
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (!column_refs[i] || column_refs[i]->type != EXPR_COLUMN_REF) {
            printf("validate_constraint_columns: Invalid column reference expression.\n");
            return false;
        }

        ColumnRef *column_ref = &column_refs[i]->expression_data.column_value;

        if (column_ref->column_name[0] == '\0' || column_ref->column_index != -1 ||
            column_ref->relation_index != -1) {
            printf("validate_constraint_columns: Invalid column reference.\n");
            return false;
        }

        // A proposed-table column must be unqualified or 
        // explicitly qualified by the proposed table name.
        if (column_ref->table_name[0] != '\0' &&
            strcmp(column_ref->table_name, create_table->table_name)) {
            printf("validate_constraint_columns: Invalid table qualifier.\n");
            return false;
        }

        uint32_t index;

        if (!resolve_create_table_column(create_table, column_ref->column_name, &index)) {
            printf("validate_constraint_columns: column resolution failed.\n");
            return false;
        }

        if (index > INT32_MAX) {
            printf("validate_constraint_columns: Column index exceeds INT32_MAX.\n");
            return false;
        }

        for (uint32_t j = 0; j < i; j++) {
            ColumnRef *previous = &column_refs[j]->expression_data.column_value;

            if (!strcmp(column_ref->column_name, previous->column_name)) {
                printf("validate_constraint_columns: Duplicate column references.\n");
                return false;
            }
        }
    }

    return true;
}

// Validates one PRIMARY KEY constraint among all column-level and table-level constraints
bool validate_primary_key_count(const CreateTableNode *create_table) {
    if (!create_table || !create_table->columns || !create_table->columns->column_defs) {
        printf("validate_primary_key_count: Invalid input arguments.\n");
        return false;
    }

    uint32_t primary_key_count = 0;

    // Count primary key constraints in the column-level constraints
    for (uint32_t i = 0; i < create_table->columns->num_column_defs; i++) {
        ColumnDefNode *column = create_table->columns->column_defs[i];

        if (!column || (column->num_constraints > 0 && !column->constraints)) {
            printf("validate_primary_key_count: Invalid Column Definition node.\n");
            return false;
        }

        for (uint32_t j = 0; j < column->num_constraints; j++) {
            if (!column->constraints || !column->constraints[j]) {
                printf("validate_primary_key_count: Invalid Column Definition constraint.\n");
                return false;
            }

            if (column->constraints[j]->type == AST_CONSTRAINT_PRIMARY_KEY) {
                primary_key_count++;

                if (primary_key_count > 1) {
                    return false;
                }
            }
        }
    }

    if (create_table->num_constraints > 0 && !create_table->constraints) {
        printf("validate_primary_key_count: Invalid table-level constraints.\n");
        return false;
    }

    // Count primary key constraints in the table-level constraint
    for (uint32_t i = 0; i < create_table->num_constraints; i++) {
        if (!create_table->constraints || !create_table->constraints[i]) {
            printf("validate_primary_key_count: Invalid table-level constraint.\n");
            return false;
        }

        if (create_table->constraints[i]->type == AST_CONSTRAINT_PRIMARY_KEY) {
            primary_key_count++;

            if (primary_key_count > 1) {
                return false;
            }
        }
    }

    return true;
}

// Retrieves a particular constraint as if column-level and table-level constraints were a single array
const ConstraintNode *get_create_table_constraint(const CreateTableNode *create_table,
    uint32_t position) {
    
    // Column-level constraint
    if (position < create_table->num_constraints) {
        return create_table->constraints[position];
    }

    position -= create_table->num_constraints;

    // Table-level constraint
    for (uint32_t i = 0; i < create_table->columns->num_column_defs; i++) {
        const ColumnDefNode *column = create_table->columns->column_defs[i];

        if (!column || (column->num_constraints > 0 && !column->constraints)) {
            printf("get_create_table_constraint: Invalid column definition.\n");
            return NULL;
        }

        if (position < column->num_constraints) {
            return column->constraints[position];
        }

        position -= column->num_constraints;
    }

    return NULL;
}

// Checks the uniqueness of constraint names
bool validate_create_table_constraint_names(const CreateTableNode *create_table) {
    if (!create_table || !create_table->columns ||
        !create_table->columns->column_defs ||
        (create_table->num_constraints > 0 && !create_table->constraints)) {
        printf("validate_create_table_constraint_names: Invalid Create Table node.\n");
        return false;
    }

    uint32_t total = create_table->num_constraints;

    for (uint32_t i = 0; i < create_table->columns->num_column_defs; i++) {
        const ColumnDefNode *column = create_table->columns->column_defs[i];

        if (!column || (column->num_constraints > 0 && !column->constraints) ||
            UINT32_MAX - total < column->num_constraints) {
            printf("validate_create_table_constraint_names: Invalid column definition.\n");
            return false;
        }

        total += column->num_constraints;
    }

    for (uint32_t i = 0; i < total; i++) {
        const ConstraintNode *current = get_create_table_constraint(create_table, i);

        if (!current) {
            printf("validate_create_table_constraint_names: Invalid constraint.\n");
            return false;
        }

        if (current->constraint_name[0] == '\0') {
            continue;
        }

        for (uint32_t j = 0; j < i; j++) {
            const ConstraintNode *previous = get_create_table_constraint(create_table, j);

            if (!previous) {
                printf("validate_create_table_constraint_names: Invalid constraint.\n");
                return false;
            }

            if (previous->constraint_name[0] != '\0' &&
                !strcmp(current->constraint_name, previous->constraint_name)) {
                printf("validate_create_table_constraint_names: Duplicate constraint name.\n");
                return false;
            }
        }
    }

    return true;
}


/* ---------- CREATE TABLE Foreign Key Validation ---------- */

// Resolves FOREIGN KEY constraint's referenced columns
bool resolve_fk_referenced_column(const ForeignKeyConstraintNode *fk,
    const CreateTableNode *create_table, const Table *referenced_table,
    const ColumnRef *ref, uint32_t *out_index, DataType *out_type) {

    if (!fk || !create_table || !ref || !out_index || !out_type) {
        printf("resolve_fk_referenced_column: Invalid input arguments.\n");
        return false;
    }

    if (ref->column_name[0] == '\0' || ref->column_index != -1 || ref->relation_index != -1) {
        printf("resolve_fk_referenced_column: Invalid initialized column reference.\n");
        return false;
    }

    if (ref->table_name[0] != '\0' &&
        strcmp(ref->table_name, fk->referenced_table_name)) {
        printf("resolve_fk_referenced_column: Invalid qualified column reference.\n");
        return false;
    }

    bool self_reference = strcmp(fk->referenced_table_name, create_table->table_name) == 0;

    if (self_reference) {
        if (!resolve_create_table_column(create_table, ref->column_name, out_index)) {
            printf("resolve_fk_referenced_column: Referenced column not found.\n");
            return false;
        }

        if (*out_index > INT32_MAX) {
            printf("resolve_fk_referenced_column: Referenced column index exceeds INT32_MAX.\n");
            return false;
        }

        const ColumnDefNode *column = create_table->columns->column_defs[*out_index];

        if (!column) {
            printf("resolve_fk_referenced_column: Invalid referenced column.\n");
            return false;
        }

        *out_type = column->type;
        return true;
    }

    // Only required for a non-self-reference
    if (!referenced_table ||
        !referenced_table->table_schema ||
        !referenced_table->table_schema->columns) {
        printf("resolve_fk_referenced_column: Invalid referenced table.\n");
        return false;
    }

    for (uint32_t i = 0; i < referenced_table->table_schema->num_columns; i++) {
        const Column *column = referenced_table->table_schema->columns[i];

        if (!column) {
            printf("resolve_fk_referenced_column: Invalid referenced column.\n");
            return false;
        }

        if (!strcmp(column->name, ref->column_name)) {
            if (i > INT32_MAX) {
                printf("resolve_fk_referenced_column: Referenced column index exceeds max limit.\n");
                return false;
            }

            *out_index = i;
            *out_type = column->type;
            return true;
        }
    }

    return false;
}

// Check whether local column and referenced column have the same data type
// and type parameters accordingly
bool fk_column_types_compatible(const ColumnDefNode *local_column,
    const ColumnDefNode *self_referenced_column, const Column *external_referenced_column) {
    
    if (!local_column ||
        (self_referenced_column == NULL) == (external_referenced_column == NULL)) {
        return false;
    }

    DataType referenced_type;
    const DataTypeArgs *referenced_type_args;


    if (self_referenced_column) {
        referenced_type = self_referenced_column->type;
        referenced_type_args = &self_referenced_column->type_args;
    }
    else {
        referenced_type = external_referenced_column->type;
        referenced_type_args = &external_referenced_column->type_args;
    }

    // Compare data types
    if (local_column->type != referenced_type) {
        return false;
    }

    // Compare type parameters for CHAR/VARCHAR/NUMERIC
    switch (local_column->type) {
        case CHAR:
        case VARCHAR:
            return local_column->type_args.length == referenced_type_args->length;

        case NUMERIC:
            return (
                local_column->type_args.precision == referenced_type_args->precision &&
                local_column->type_args.scale == referenced_type_args->scale
            );

        default:
            return true;
    }
}

// Verify whether the referenced key completely matches a PRIMARY KEY or a UNIQUE key
bool fk_ast_key_matches(ExpressionNode **key_refs, uint32_t key_count,
    ExpressionNode **referenced_refs, uint32_t referenced_count) {
    if (!key_refs || !referenced_refs || key_count != referenced_count) {
        printf("fk_ast_key_matches: Invalid input arguments.\n");
        return false;
    }

    for (uint32_t i = 0; i < key_count; i++) {
        if (!key_refs[i] || 
            !referenced_refs[i] || 
            key_refs[i]->type != EXPR_COLUMN_REF ||
            referenced_refs[i]->type != EXPR_COLUMN_REF) {
            printf("fk_ast_key_matches: Invalid local/referenced column expressions.\n");
            return false;
        }

        const char *key_name = key_refs[i]->expression_data.column_value.column_name;
        const char *referenced_name = referenced_refs[i]->expression_data.column_value.column_name;

        if (strcmp(key_name, referenced_name)) {
            return false;
        }
    }

    return true;
}

// Verify that the referenced key matches either a PRIMARY KEY or UNIQUE constraint 
bool fk_matches_create_table_key(const CreateTableNode *create_table, ExpressionNode **referenced_refs,
    uint32_t referenced_count) {

    if (!create_table ||
        !create_table->columns ||
        !create_table->columns->column_defs ||
        !referenced_refs ||
        referenced_count == 0) {
        printf("fk_matches_create_table_key: Invalid input arguments.\n");
        return false;
    }

    uint32_t total_constraints = create_table->num_constraints;

    for (uint32_t i = 0; i < create_table->columns->num_column_defs; i++) {
        const ColumnDefNode *column = create_table->columns->column_defs[i];

        if (!column ||
            (column->num_constraints > 0 && !column->constraints) ||
            UINT32_MAX - total_constraints < column->num_constraints) {
            printf("fk_matches_create_table_key: Invalid column definition.\n");
            return false;
        }

        total_constraints += column->num_constraints;
    }

    for (uint32_t i = 0; i < total_constraints; i++) {
        const ConstraintNode *candidate = get_create_table_constraint(create_table, i);

        if (!candidate) {
            printf("fk_matches_create_table_key: Invalid constraint.\n");
            return false;
        }

        switch (candidate->type) {
            case AST_CONSTRAINT_PRIMARY_KEY: {
                const PrimaryKeyConstraintNode *pk = &candidate->constraint_data.primary_key;

                if (fk_ast_key_matches(
                        pk->column_refs, 
                        pk->num_columns, 
                        referenced_refs, 
                        referenced_count)) {
                    return true;
                }

                break;
            }

            case AST_CONSTRAINT_UNIQUE: {
                const UniqueConstraintNode *unique =
                    &candidate->constraint_data.unique;

                if (fk_ast_key_matches(unique->column_refs, unique->num_columns, referenced_refs, referenced_count)) {
                    return true;
                }

                break;
            }

            default:
                break;
        }
    }

    printf("fk_matches_create_table_key: Referenced key doesn't match a PRIMARY KEY or UNIQUE constraint key.\n");
    return false;
}

// Validates that the FOREIGN KEY matches a PRIMARY KEY or UNIQUE constraint from the existing table
bool fk_matches_schema_key(const Schema *schema, const uint32_t *referenced_indexes,
    uint32_t num_referenced_columns) {
    if (!schema || !referenced_indexes || num_referenced_columns == 0) {
        printf("fk_matches_schema_key: Invalid arguments.\n");
        return false;
    }

    if (!schema->columns || schema->num_columns == 0 ||
        !schema->constraints || schema->num_constraints == 0) {
        printf("fk_matches_schema_key: Invalid referenced schema.\n");
        return false;
    }

    for (uint32_t i = 0; i < schema->num_constraints; i++) {
        const Constraint *constraint = schema->constraints[i];

        if (!constraint) {
            printf("fk_matches_schema_key: Invalid constraint.\n");
            return false;
        }

        const uint32_t *key_indexes = NULL;
        uint32_t key_count = 0;

        if (constraint->type == PRIMARY_KEY) {
            const PrimaryKeyConstraint *pk = &constraint->constraint_data.primary_key;

            key_indexes = pk->primary_key_columns;
            key_count = pk->amount_columns;
        }
        else if (constraint->type == UNIQUE) {
            const UniqueConstraint *unique = &constraint->constraint_data.unique_cols;

            key_indexes = unique->column_refs;
            key_count = unique->amount_columns;
        }
        else {
            continue;
        }

        if (!key_indexes || key_count == 0) {
            printf("fk_matches_schema_key: Invalid key metadata.\n");
            return false;
        }

        if (key_count != num_referenced_columns) {
            continue;
        }

        bool matches = true;

        for (uint32_t j = 0; j < key_count; j++) {
            if (key_indexes[j] != referenced_indexes[j]) {
                matches = false;
                break;
            }
        }

        if (matches) {
            return true;
        }
    }

    printf("fk_matches_schema_key: No matching PRIMARY KEY or UNIQUE key.\n");
    return false;
}


/* ---------- CREATE TABLE Default and Nullability Validation ---------- */

// Helper that checks whether an expression is a NULL literal
bool expression_is_explicit_null_literal(const ExpressionNode *expr) {
    if (!expr) {
        printf("expression_is_explicit_null_literal: NULL expression.\n");
        return false;
    }

    // An expression that is not a literal cannot be an explicit NULL literal.
    if (expr->type != EXPR_LITERAL) {
        return false;
    }

    const Literal *literal = &expr->expression_data.literal_value;

    return literal->is_untyped_null || (literal->literal != NULL && literal->literal->null_val);
}

// Helper that checks whether a constraint's key contains a particular column
bool constraint_key_contains_column(ExpressionNode *const *column_refs, uint32_t count,
    const char *column_name) {
    if (!column_refs || !column_name) {
        printf("constraint_key_contains_column: Invalid input arguments.\n");
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        const ExpressionNode *expr = column_refs[i];

        if (!expr || expr->type != EXPR_COLUMN_REF) {
            printf("constraint_key_contains_column: Invalid column reference expression.\n");
            return false;
        }

        if (!strcmp(expr->expression_data.column_value.column_name, column_name)) {
            return true;
        }
    }

    return false;
}

// Validate conflicting DEFAULT constraints among column-level and table-level constraints 
bool validate_create_table_defaults(const CreateTableNode *create_table) {
    if (!create_table || !create_table->columns || !create_table->columns->column_defs ||
        (create_table->num_constraints > 0 && !create_table->constraints)) {
        printf("validate_create_table_defaults: Invalid CREATE TABLE node.\n");
        return false;
    }

    const ColumnsNode *columns = create_table->columns;

    // Traverse all Column definitions
    for (uint32_t i = 0; i < columns->num_column_defs; i++) {
        const ColumnDefNode *column = columns->column_defs[i];

        if (!column || (column->num_constraints > 0 && !column->constraints)) {
            printf("validate_create_table_defaults: Invalid Column Definition node.\n");
            return false;
        }

        bool requires_non_null = false;
        bool has_default = false;
        bool has_null_default = false;

        // Inspect constraints declared on this Column definition
        for (uint32_t j = 0; j < column->num_constraints; j++) {
            const ConstraintNode *constraint = column->constraints[j];

            if (!constraint) {
                printf("validate_create_table_defaults: Invalid Column Definition node.\n");
                return false;
            }

            switch (constraint->type) {
                case AST_CONSTRAINT_PRIMARY_KEY: {
                    const PrimaryKeyConstraintNode *primary_key_con = &constraint->constraint_data.primary_key;

                    if (constraint_key_contains_column(
                            primary_key_con->column_refs, 
                            primary_key_con->num_columns, 
                            column->column_name)) {
                        requires_non_null = true;
                    }
                    break;
                }

                case AST_CONSTRAINT_NOT_NULL:
                    requires_non_null = true;
                    break;

                case AST_CONSTRAINT_DEFAULT: {
                    const DefaultConstraintNode *default_con = &constraint->constraint_data.default_value;

                    if (strcmp(default_con->column_name, column->column_name) != 0 || 
                        !default_con->default_expr || 
                        has_default) {
                        printf("validate_create_table_defaults: Invalid DEFAULT constraint.\n");
                        return false;
                    }

                    has_default = true;
                    has_null_default = expression_is_explicit_null_literal(default_con->default_expr);
                    break;
                }

                default:
                    break;
            }
        }

        // Inspect table-level constraints affecting this column
        for (uint32_t j = 0; j < create_table->num_constraints; j++) {
            const ConstraintNode *constraint = create_table->constraints[j];

            if (!constraint) {
                printf("validate_create_table_defaults: Invalid Column Definition node.\n");
                return false;
            }

            if (constraint->type == AST_CONSTRAINT_PRIMARY_KEY) {
                const PrimaryKeyConstraintNode *primary_key = &constraint->constraint_data.primary_key;

                if (constraint_key_contains_column(
                        primary_key->column_refs,
                        primary_key->num_columns,
                        column->column_name)) {
                    requires_non_null = true;
                }
            }
        }

        if (requires_non_null && has_null_default) {
            printf("validate_create_table_defaults: NULL DEFAULT on required column '%s'.\n", column->column_name);
            return false;
        }
    }

    return true;
}


/* ---------- CREATE TABLE Pending Reference Collection and Commit ---------- */

// Add a validated relation and column index resolutions without modifying the AST
bool pending_binding_add(PendingColumnBindings *pending, ColumnRef *column_ref, uint32_t column_index) {

    if (!pending || !column_ref ||
        column_ref->column_name[0] == '\0' ||
        column_ref->relation_index != -1 ||
        column_ref->column_index != -1 ||
        column_index > INT32_MAX) {
        printf("pending_binding_add: Invalid input arguments.\n");
        return false;
    }

    // Reject the same AST reference being collected twice
    for (size_t i = 0; i < pending->count; i++) {
        if (pending->items[i].ref == column_ref) {
            printf("pending_binding_add: Duplicate column reference addition failed.\n");
            return false;
        }
    }

    if (pending->count == pending->capacity) {
        size_t new_capacity = pending->capacity == 0 ? 8 : pending->capacity * 2;

        if (new_capacity < pending->capacity ||
            new_capacity > SIZE_MAX / sizeof(PendingColumnBinding)) {
            printf("pending_binding_add: Invalid updated capacity of pending column bindings.\n");
            return false;
        }

        PendingColumnBinding *new_items = realloc(
            pending->items,
            new_capacity * sizeof(PendingColumnBinding)
        );

        if (!new_items) {
            printf("pending_binding_add: Reallocation of pending column binding array failed.\n");
            return false;
        }

        pending->items = new_items;
        pending->capacity = new_capacity;
    }

    // Append new pending column biding
    pending->items[pending->count++] = (PendingColumnBinding) {
        .ref = column_ref,
        .relation_index = 0,
        .column_index = (int32_t)column_index
    };

    return true;
}


// Collect a reference to a column in the proposed table
bool collect_local_column_ref(ExpressionNode *expr, const CreateTableNode *create_table,
    PendingColumnBindings *pending) {
    if (!expr || 
        expr->type != EXPR_COLUMN_REF ||
        !create_table || !pending) {
        printf("collect_local_column_ref: Invalid input arguments.\n");
        return false;
    }

    ColumnRef *column_ref = &expr->expression_data.column_value;

    if (column_ref->table_name[0] != '\0' &&
        strcmp(column_ref->table_name, create_table->table_name) != 0) {
        printf("collect_local_column_ref: Invalid name-qualified column reference.\n");
        return false;
    }

    uint32_t index = 0;

    if (!resolve_create_table_column(create_table, column_ref->column_name, &index)) {
        printf("collect_local_column_ref: Column reference index resolution failed.\n");
        return false;
    }

    return pending_binding_add(pending, column_ref, index);
}


// Collect a reference to an FOREIGN KEY's referenced table
bool collect_fk_referenced_column_ref(ExpressionNode *expr, const ForeignKeyConstraintNode *fk,
    const CreateTableNode *create_table, Database *db, PendingColumnBindings *pending) {
    if (!expr || 
        expr->type != EXPR_COLUMN_REF ||
        !fk || !create_table || !db || !pending) {
        printf("collect_fk_referenced_column_ref: Invalid input arguments.\n");
        return false;
    }

    bool self_reference = strcmp(fk->referenced_table_name, create_table->table_name) == 0;

    Table *referenced_table = NULL;

    if (!self_reference) {
        referenced_table = database_find_table(db, fk->referenced_table_name);

        if (!referenced_table) {
            printf("collect_fk_referenced_column_ref: Referenced table doesn't exist.\n");
            return false;
        }
    }

    ColumnRef *column_ref = &expr->expression_data.column_value;

    uint32_t index = 0;
    DataType referenced_type;

    if (!resolve_fk_referenced_column(
            fk,
            create_table,
            referenced_table,
            column_ref,
            &index,
            &referenced_type)) {
        printf("collect_fk_referenced_column_ref: Referenced column index resolution failed.\n");
        return false;
    }

    // Indexes on referenced FK columns are relative
    // to the referenced table, not the newly created table
    return pending_binding_add(pending, column_ref, index);
}


// Recursively collect column references from CHECK expressions
bool collect_create_table_expression_refs(ExpressionNode *expr, const CreateTableNode *create_table, 
    PendingColumnBindings *pending) {
    if (!expr || !create_table || !pending) {
        printf("collect_create_table_expression_refs: Invalid input arguments.\n");
        return false;
    }

    switch (expr->type) {
        case EXPR_LITERAL:
            return true;

        case EXPR_COLUMN_REF:
            return collect_local_column_ref(expr, create_table, pending);

        case EXPR_UNARY:
            return collect_create_table_expression_refs(
                expr->expression_data.unary_expr.operand,
                create_table, pending
            );

        case EXPR_BINARY:
            return (
                collect_create_table_expression_refs(
                    expr->expression_data.binary_expr.left_operand,
                    create_table, pending
                ) &&
                collect_create_table_expression_refs(
                    expr->expression_data.binary_expr.right_operand,
                    create_table, pending
                )
            );

        case EXPR_IS_NULL:
            return collect_create_table_expression_refs(
                expr->expression_data.is_null_expr.operand,
                create_table, pending
            );

        case EXPR_IS_NOT_NULL:
            return collect_create_table_expression_refs(
                expr->expression_data.is_not_null_expr.operand,
                create_table, pending
            );

        case EXPR_IN: {
            In *in = &expr->expression_data.in_expr;

            if (!in->operand || !in->set_options ||
                in->option_count == 0) {
                return false;
            }

            if (!collect_create_table_expression_refs(
                    in->operand, create_table, pending)) {
                return false;
            }

            for (uint32_t i = 0; i < in->option_count; i++) {
                if (!collect_create_table_expression_refs(
                        in->set_options[i],
                        create_table,
                        pending)) {
                    return false;
                }
            }

            return true;
        }

        case EXPR_BETWEEN: {
            Between *between = &expr->expression_data.between_expr;

            return (
                collect_create_table_expression_refs(between->operand, create_table, pending) &&
                collect_create_table_expression_refs(between->lower, create_table, pending) &&
                collect_create_table_expression_refs(between->upper, create_table, pending)
            );
        }

        case EXPR_FUNCTIONS:
        case EXPR_TABLE_REF:
        default:
            return false;
    }
}


// Collect references belonging to a single constraint
bool collect_constraint_references(ConstraintNode *constraint, const CreateTableNode *create_table, 
    Database *db, PendingColumnBindings *pending) {
    if (!constraint || !create_table || !db || !pending) {
        printf("collect_constraint_references: Invalid input arguments.\n");
        return false;
    }

    switch (constraint->type) {
        case AST_CONSTRAINT_PRIMARY_KEY: {
            PrimaryKeyConstraintNode *pk = &constraint->constraint_data.primary_key;

            if (!pk->column_refs || pk->num_columns == 0) {
                printf("collect_constraint_references: Invalid PRIMARY KEY constraint.\n");
                return false;
            }

            for (uint32_t i = 0; i < pk->num_columns; i++) {
                if (!collect_local_column_ref(pk->column_refs[i], create_table, pending)) {
                    printf("collect_constraint_references: Local column reference's indexes could not be collected.\n");
                    return false;
                }
            }

            return true;
        }

        case AST_CONSTRAINT_UNIQUE: {
            UniqueConstraintNode *unique = &constraint->constraint_data.unique;

            if (!unique->column_refs || unique->num_columns == 0) {
                printf("collect_constraint_references: Invalid UNIQUE constraint.\n");
                return false;
            }

            for (uint32_t i = 0; i < unique->num_columns; i++) {
                if (!collect_local_column_ref(unique->column_refs[i], create_table, pending)) {
                    printf("collect_constraint_references: Local column reference's indexes could not be collected.\n");
                    return false;
                }
            }

            return true;
        }

        case AST_CONSTRAINT_FOREIGN_KEY: {
            ForeignKeyConstraintNode *fk = &constraint->constraint_data.foreign_key;

            if (!fk->local_column_refs ||
                !fk->referenced_column_refs ||
                fk->num_local_columns == 0 ||
                fk->num_local_columns != fk->num_referenced_columns) {
                printf("collect_constraint_references: Invalid FOREIGN KEY constraint.\n");
                return false;
            }

            for (uint32_t i = 0; i < fk->num_local_columns; i++) {
                if (!collect_local_column_ref(fk->local_column_refs[i], create_table, pending)) {
                    printf("collect_constraint_references: Local column reference's indexes could not be collected.\n");
                    return false;
                }
            }

            for (uint32_t i = 0; i < fk->num_referenced_columns; i++) {
                if (!collect_fk_referenced_column_ref(
                        fk->referenced_column_refs[i],
                        fk,
                        create_table,
                        db,
                        pending)) {
                    printf("collect_constraint_references: Referenced column reference's indexes could not be collected.\n");    
                    return false;
                }
            }

            return true;
        }

        case AST_CONSTRAINT_CHECK:
            return collect_create_table_expression_refs(
                constraint->constraint_data.check.check_expr,
                create_table,
                pending
            );

        case AST_CONSTRAINT_NOT_NULL:
            return collect_local_column_ref(
                constraint->constraint_data.not_null.column_ref,
                create_table,
                pending
            );

        // DEFAULT has a column_name field, not an ExpressionNode-based destination ColumnRef.
        // DEFAULT expressions cannot reference columns under the current binder rules.
        case AST_CONSTRAINT_DEFAULT:
            return true;

        default:
            return false;
    }
}


// Collect every reference first. Publish only after success.
// A failure leaves every target ColumnRef unchanged.
bool commit_create_table_references(CreateTableNode *create_table, Database *db) {
    if (!create_table || !create_table->columns || !create_table->columns->column_defs ||
        create_table->columns->num_column_defs == 0 ||
        (create_table->num_constraints > 0 && !create_table->constraints) || !db) {
        printf("commit_create_table_references: Invalid input arguments.\n");
        return false;
    }

    PendingColumnBindings pending = {0};

    // Collect column-level references
    for (uint32_t i = 0; i < create_table->columns->num_column_defs; i++) {
        ColumnDefNode *column = create_table->columns->column_defs[i];

        if (!column || (column->num_constraints > 0 && !column->constraints)) {
            printf("commit_create_table_references: Invalid Column definition.\n");
            free(pending.items);
            return false;
        }

        for (uint32_t j = 0; j < column->num_constraints; j++) {
            if (!collect_constraint_references(column->constraints[j], create_table, db, &pending)) {
                printf("commit_create_table_references: Column-level Constraint references index collection failed.\n");
                free(pending.items);
                return false;
            }
        }
    }

    // Collect table-level references
    for (uint32_t i = 0; i < create_table->num_constraints; i++) {

        if (!collect_constraint_references(create_table->constraints[i], create_table, db, &pending)) {
            printf("commit_create_table_references: Table-level Constraint references index collection failed.\n");
            free(pending.items);
            return false;
        }
    }

    // All resolutions succeeded. No allocation, lookup, or fallible operation occurs after this point.
    for (size_t i = 0; i < pending.count; i++) {
        PendingColumnBinding *binding = &pending.items[i];

        binding->ref->relation_index = binding->relation_index;
        binding->ref->column_index = binding->column_index;
    }

    free(pending.items);
    return true;
}


/* ---------- ALTER TABLE Validation and Proposed Schema Construction ---------- */

// Resolve a column in the proposed ALTER TABLE schema
bool resolve_alter_candidate_column(const Schema *working_schema, const ColumnDefNode *new_column,
    const char *column_name, uint32_t *out_index, DataType *out_type) {
    
   if (!working_schema || !new_column || !column_name || column_name[0] == '\0' ||
       !out_index || !out_type ||
       (working_schema->num_columns > 0 && !working_schema->columns)) {
        printf("resolve_alter_candidate_column: Invalid input arguments.\n");
        return false;
    }

    for (uint32_t i = 0; i < working_schema->num_columns; i++) {
        const Column *existing_column = working_schema->columns[i];

        if (!existing_column) {
            printf("resolve_alter_candidate_column: Invalid Column in the working schema.\n");
            return false;
        }

        // Existing column resolution
        if (!strcmp(existing_column->name, column_name)) {
            *out_index = i;
            *out_type = existing_column->type;
            return true;
        }
    }

    // New column resolution
    if (!strcmp(new_column->column_name, column_name)) {        
        if (working_schema->num_columns > INT32_MAX) {
            printf("resolve_alter_candidate_column: Maximum amount of columns exceeded.\n");
            return false;
        }

        *out_index = working_schema->num_columns;
        *out_type = new_column->type;
        return true;
    }

    return false;
}

// Verify whether the new column declares a candidate key
bool alter_new_column_has_key(const ColumnDefNode *column, const char *table_name) {
    if (!column || !table_name || 
        (column->num_constraints > 0 && !column->constraints)) {
        printf("alter_new_column_has_key: Invalid inputs.\n");
        return false;
    }

    bool has_key = false;

    for (uint32_t i = 0; i < column->num_constraints; i++) {
        ConstraintNode *constraint = column->constraints[i];

        if (!constraint) {
            printf("alter_new_column_has_key: Invalid constraint.\n");
            return false;
        }

        ExpressionNode **column_refs = NULL;
        uint32_t count = 0;

        switch (constraint->type) {
            case AST_CONSTRAINT_PRIMARY_KEY:
                column_refs = constraint->constraint_data.primary_key.column_refs;
                count = constraint->constraint_data.primary_key.num_columns;
                break;

            case AST_CONSTRAINT_UNIQUE:
                column_refs = constraint->constraint_data.unique.column_refs;
                count = constraint->constraint_data.unique.num_columns;
                break;

            default:
                // Other constraints don't establish eligible candidate keys
                continue;
        }

        if (!column_refs || count != 1 || !column_refs[0] || column_refs[0]->type != EXPR_COLUMN_REF) {
            printf("alter_new_column_has_key: Invalid candidate key structure.\n");
            return false;
        }

        ColumnRef *column_ref = &column_refs[0]->expression_data.column_value;

        if (strcmp(column_ref->column_name, column->column_name)) {
            printf("alter_new_column_has_key: Column-level keys must reference their owner column.\n");
            return false;
        }

        if (column_ref->table_name[0] != '\0' && 
            strcmp(column_ref->table_name, table_name) != 0) {
            printf("alter_new_column_has_key: Qualified references must belong to the altered table.");
            return false;
        }

        // A valid candidate key has been found
        has_key = true;
    }

    return has_key;
}

// Validate FOREIGN KEY constraint in an ALTER TABLE action
bool validate_alter_column_fk(const ForeignKeyConstraintNode *fk, const ColumnDefNode *column,
    const Table *table, Schema *working_schema, const BindingContext *context) {
    
    if (!fk || !column || !table || !working_schema || !context || !context->db) {
        printf("validate_alter_column_fk: Invalid inputs.\n");
        return false;
    }

    if (!context->relations || context->num_relations != 1) {
        return false;
    }

    const char *current_table_name = binding_relation_current_name(&context->relations[0]);

    if (!current_table_name) {
        return false;
    }

    if (fk->referenced_table_name[0] == '\0' || 
        !fk->local_column_refs || !fk->referenced_column_refs || 
        fk->num_local_columns != 1 || fk->num_referenced_columns != 1) {
        printf("validate_alter_column_fk: Invalid FOREIGN KEY constraint.\n");
        return false;
    }

    ExpressionNode *local_expr = fk->local_column_refs[0];
    ExpressionNode *remote_expr = fk->referenced_column_refs[0];

    if (!local_expr || !remote_expr ||
        local_expr->type != EXPR_COLUMN_REF || remote_expr->type != EXPR_COLUMN_REF) {
        printf("validate_alter_column_fk: Invalid local and referenced column expressions.\n");
        return false;
    }

    ColumnRef *local = &local_expr->expression_data.column_value;
    ColumnRef *remote = &remote_expr->expression_data.column_value;

    if (strcmp(local->column_name, column->column_name) ||
        (local->table_name[0] != '\0' && strcmp(local->table_name, current_table_name))) {
        printf("validate_alter_column_fk: Invalid local column reference.\n");
        return false;
    }

    if (remote->column_name[0] == '\0' ||
        (remote->table_name[0] != '\0' && strcmp(remote->table_name, fk->referenced_table_name))) {
        printf("validate_alter_column_fk: Invalid referenced column reference.\n");
        return false;
    }

    if (local->column_index != -1 || local->relation_index != -1 ||
        remote->column_index != -1 || remote->relation_index != -1) {
        printf("validate_alter_column_fk: Invalid initialized relation/column indexes.\n");
        return false;
    }

    // Check if the FOREIGN KEY is a self referenced
    bool self_reference = strcmp(fk->referenced_table_name, current_table_name) == 0;

    // Special case: FOREIGN KEY references the column being added now
    bool references_new_column = 
        self_reference && strcmp(remote->column_name, column->column_name) == 0;

    if (references_new_column) {
        return alter_new_column_has_key(column, current_table_name);
    }

    // Deduce the referenced schema based on FOREIGN KEY's self-reference status
    Schema *referenced_schema = NULL;

    if (self_reference) {
        referenced_schema = working_schema;
    }
    else {
        Table *referenced_table = database_find_table(context->db, fk->referenced_table_name);

        if (!referenced_table || !referenced_table->table_schema) {
            printf("validate_alter_column_fk: Invalid referenced table.\n");
            return false;
        }

        referenced_schema = referenced_table->table_schema;
    }

    if (!referenced_schema || !referenced_schema->columns) {
        printf("validate_alter_column_fk: Invalid referenced schema.\n");
        return false;
    }

    // Resolve the FOREIGN KEY's referenced column
    uint32_t referenced_index = 0;
    Column *referenced_column = NULL;

    for (uint32_t i = 0; i < referenced_schema->num_columns; i++) {
        Column *candidate = referenced_schema->columns[i];

        if (!candidate) {
            printf("validate_alter_column_fk: Invalid candidate referenced schema Column.\n");
            return false;
        }

        if (!strcmp(candidate->name, remote->column_name)) {
            referenced_column = candidate;
            referenced_index = i;
            break;
        }
    }

    if (!referenced_column || referenced_index > INT32_MAX) {
        printf("validate_alter_column_fk: Invalid resolved referenced schema Column.\n");
        return false;
    }

    // Validate data type compatibility between local and referenced columns
    if (!fk_column_types_compatible(column, NULL, referenced_column)) {
        printf("validate_alter_column_fk: Incompatible column data types.\n");
        return false;
    }

    // Validate that the FOREIGN KEY matches a candidate key from the referenced schema
    return fk_matches_schema_key(referenced_schema, &referenced_index, 1);
}

// Validate CHECK constraint in ALTER TABLE
bool validate_alter_column_check(ConstraintNode *constraint, ColumnDefNode *column,
    Table *table, Schema *working_schema, const BindingContext *context) {

    if (!constraint || constraint->type != AST_CONSTRAINT_CHECK ||
        !column || !table || !working_schema || !context || !context->db) {
        printf("validate_alter_column_check: Invalid inputs.\n");
        return false;
    }

    CheckConstraintNode *check = &constraint->constraint_data.check;

    if (!check->check_expr) {
        printf("validate_alter_column_check: Invalid CHECK constraint expression.\n");
        return false;
    }

    BindingRelation relation = {0};

    relation.table = table;
    relation.proposed_schema = working_schema;
    relation.proposed_column = column;

    const char *current_table_name = binding_relation_current_name(&context->relations[0]);

    if (!current_table_name ||
        strlen(current_table_name) >= sizeof(relation.proposed_table_name)) {
        return false;
    }

    strcpy(relation.proposed_table_name, current_table_name);

    BindingContext check_context = {0};

    check_context.db = context->db;
    check_context.relations = &relation;
    check_context.num_relations = 1;
    check_context.current_clause = BIND_CLAUSE_CHECK;

    BoundExpressionType result_type = {0};

    if (!bind_scalar_expression(check->check_expr, &check_context, &result_type)) {
        printf("validate_alter_column_check: CHECK expression binding failed.\n");
        return false;
    }

    return !result_type.is_untyped_null && result_type.type == BOOL;
}

// Construct a runtime constraint for the working schema from the column definition's constraint nodes
Constraint *build_runtime_alter_constraint(const ConstraintNode *constraint, 
    const ColumnDefNode *column, const Schema *working_schema, const BindingContext *context,
    const Table *table, char *runtime_name) {
    
    if (!constraint || !column || !working_schema || !context || !table) {
        printf("build_runtime_alter_constraint: Invalid inputs.\n");
        return NULL;
    }

    const char *current_table_name = binding_relation_current_name(&context->relations[0]);

    if (!current_table_name) {
        return NULL;
    }

    if (working_schema->num_columns > INT32_MAX) {
        printf("build_runtime_alter_constraint: Maximum number of columns in working schema exceeded.\n");
        return NULL;
    }

    if (!runtime_name || runtime_name[0] == '\0') {
        printf("build_runtime_alter_constraint: Invalid runtime constraint name.\n");
        return NULL;
    }

    uint32_t new_index = working_schema->num_columns;
    char *name = runtime_name;

    switch (constraint->type) {
        case AST_CONSTRAINT_PRIMARY_KEY:
            return constraint_create_primary_key(name, &new_index, 1);

        case AST_CONSTRAINT_UNIQUE:
            return constraint_create_unique(name, &new_index, 1);

        case AST_CONSTRAINT_NOT_NULL:
            return constraint_create_not_null(name, new_index);

        case AST_CONSTRAINT_DEFAULT:
            return constraint_create_default(name, new_index, constraint->constraint_data.default_value.default_expr);

        case AST_CONSTRAINT_FOREIGN_KEY: {
            // Perform FOREIGN KEY referenced table and column resolution first
            const ForeignKeyConstraintNode *fk = &constraint->constraint_data.foreign_key;

            if (!fk->referenced_column_refs || fk->num_referenced_columns != 1 ||
                !fk->referenced_column_refs[0]) {
                printf("build_runtime_alter_constraint: Invalid FOREIGN KEY constraint.\n");
                return NULL;
            }

            const ColumnRef *remote = &fk->referenced_column_refs[0]->expression_data.column_value;

            bool self_reference = strcmp(fk->referenced_table_name, current_table_name) == 0;

            const Schema *ref_schema = NULL;

            // Resolve referenced table and schema
            if (self_reference) {
                ref_schema = working_schema;
            }
            else {
                Table *referenced = database_find_table(context->db, fk->referenced_table_name);

                if (!referenced) {
                    printf("build_runtime_alter_constraint: Invalid referenced table.\n");
                    return NULL;
                }

                ref_schema = referenced->table_schema;
            }

            if (!ref_schema) {
                printf("build_runtime_alter_constraint: Invalid referenced table schema.\n");
                return NULL;
            }

            uint32_t referenced_index = 0;

            bool references_new_column = 
                self_reference && strcmp(remote->column_name, column->column_name) == 0;

            // Resolve referenced column in the referenced schema
            if (references_new_column) {
                referenced_index = new_index;
            }
            else {
                bool found = false;

                for (uint32_t i = 0; i < ref_schema->num_columns; i++) {
                    const Column *candidate = ref_schema->columns[i];

                    if (!candidate) {
                        printf("build_runtime_alter_constraint: Invalid candidate referenced schema Column.\n");
                        return NULL;
                    }

                    if (strcmp(candidate->name, remote->column_name) == 0) {
                        referenced_index = i;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    printf("build_runtime_alter_constraint: Candidate referenced schema Column resolution failed.\n");
                    return NULL;
                }
            }

            return constraint_create_foreign_keys(
                name,
                &new_index,
                1,
                (char *) fk->referenced_table_name,
                &referenced_index,
                1
            );
        }

        case AST_CONSTRAINT_CHECK: {
            const CheckConstraintNode *check = &constraint->constraint_data.check;

            if (!check->check_expr ||
                working_schema->num_columns >= INT32_MAX) {
                return NULL;
            }

            uint32_t total_columns = working_schema->num_columns + 1;

            uint32_t *indices = (uint32_t *) calloc(total_columns, sizeof(uint32_t));

            if (!indices) {
                printf("build_runtime_alter_constraint: Indices allocation failed.\n");
                return NULL;
            }

            uint32_t count = 0;

            if (!collect_bound_check_refs(check->check_expr, indices, total_columns, &count)) {
                printf("build_runtime_alter_constraint: Collection of bound column references failed.\n");
                free(indices);
                return NULL;
            }

            // Zero referenced columns is allowed
            Constraint *runtime_check = constraint_create_check(
                name,
                check->check_expr,
                count > 0 ? indices : NULL,
                count
            );

            free(indices);
            return runtime_check;
        }
        
        default:
            return NULL;
    }
}

// Add new column to the working schema as a result of an ADD COLUMN action
bool working_schema_add_column(Schema *schema, const ColumnDefNode *column,
    const BindingContext *context, const Table *table) {
    
    if (!schema || !column || !context || !table ||
        (schema->num_columns > 0 && !schema->columns) ||
        (schema->num_constraints > 0 && !schema->constraints) ||
        (column->num_constraints > 0 && !column->constraints)) {
        printf("working_schema_add_column: Invalid inputs.\n");
        return false;
    }

    uint32_t old_columns = schema->num_columns;
    uint32_t old_constraints = schema->num_constraints;
    uint32_t add_constraints = column->num_constraints;

    if (old_columns >= INT32_MAX || 
        add_constraints > UINT32_MAX - old_constraints ||
        (size_t)old_columns + 1 > SIZE_MAX/sizeof(Column *) ||
        (size_t)old_constraints + add_constraints > SIZE_MAX/sizeof(Constraint *)) {
        printf("working_schema_add_column: Invalid column and constraint totals.\n");
        return false;
    }

    // Prepare the new runtime Column and its Constraints
    Column *new_column = (Column *) calloc(1, sizeof(Column));

    if (!new_column) {
        printf("working_schema_add_column: New column allocation failed.\n");
        return false;
    }

    memcpy(new_column->name, column->column_name, sizeof(column->column_name));
    new_column->type = column->type;
    new_column->type_args = column->type_args;

    Constraint **prepared = NULL;

    if (add_constraints > 0) {
        prepared = (Constraint **) calloc(add_constraints, sizeof(Constraint *));

        if (!prepared) {
            printf("working_schema_add_column: New constraints array allocation failed.\n");
            free(new_column);
            return false;
        }
    }

    for (uint32_t i = 0; i < add_constraints; i++) {
        const ConstraintNode *ast_constraint = column->constraints[i];

        if (!ast_constraint) {
            goto failed;
        }

        char runtime_name[64] = {0};

        if (ast_constraint->constraint_name[0] != '\0') {
            // Preserve an explicitly provided SQL constraint name.
            memcpy(runtime_name, ast_constraint->constraint_name, sizeof(runtime_name));
            runtime_name[63] = '\0';
        }
        else {
            // Generate a unique name for an unnamed constraint.
            if (!generate_alter_constraint_name(ast_constraint, column, table, schema,
                    prepared, i, runtime_name)) {
                goto failed;
            }
        }

        prepared[i] = build_runtime_alter_constraint(
            ast_constraint, column, schema, context, table, runtime_name
        );

        if (!prepared[i]) {
            goto failed;
        }
    }

    // Prepare replacement pointer arrays for Columns and Constraints
    Column **new_columns = malloc(((size_t)old_columns + 1) * sizeof(Column *));

    if (!new_columns) {
        goto failed;
    }

    Constraint **new_constraints = NULL;

    if (old_constraints + add_constraints > 0) {
        new_constraints = malloc(((size_t)old_constraints + add_constraints) * sizeof(Constraint *));

        if (!new_constraints) {
            free(new_columns);
            goto failed;
        }
    }

    // Keep pointers to existing runtime objects
    if (old_columns > 0) {
        memcpy(new_columns, schema->columns, old_columns * sizeof(Column *));
    }

    if (old_constraints > 0) {
        memcpy(new_constraints, schema->constraints, old_constraints * sizeof(Constraint *));
    }

    // And append new column and constraints
    new_columns[old_columns] = new_column;

    for (uint32_t i = 0; i < add_constraints; i++) {
        new_constraints[old_constraints + i] = prepared[i];
    }

    // Commit: everything needed has been allocated
    free(schema->columns);
    free(schema->constraints);

    schema->columns = new_columns;
    schema->constraints = new_constraints;

    schema->num_columns = old_columns + 1;
    schema->num_constraints = old_constraints + add_constraints;

    // Runtime objects now belong to the working schema
    free(prepared);

    return true;

failed:
    for (uint32_t i = 0; i < add_constraints; i++) {
        constraint_free(prepared[i]);
    }

    free(prepared);
    free(new_column);

    return false;
}

// Collect all column references from a bound CHECK expression 
// in order to construct the corresponding runtime CHECK constraint
bool collect_bound_check_refs(const ExpressionNode *expr, uint32_t *indices, uint32_t capacity,
    uint32_t *count) {

    if (!expr || !indices || !count) {
        printf("collect_bound_check_refs: Invalid inputs.\n");
        return false;
    }

    switch (expr->type) {
        case EXPR_LITERAL:
            return true;

        case EXPR_COLUMN_REF: {
            const ColumnRef *ref = &expr->expression_data.column_value;

            // References must already be bound
            if (ref->relation_index != 0 ||
                ref->column_index < 0 ||
                (uint32_t)ref->column_index >= capacity) {
                printf("collect_bound_check_refs: Column references are not bound.\n");
                return false;
            }

            uint32_t index = (uint32_t)ref->column_index;

            // Avoid duplicate indices
            for (uint32_t i = 0; i < *count; i++) {
                if (indices[i] == index) {
                    return true;
                }
            }

            if (*count >= capacity) {
                return false;
            }

            indices[(*count)++] = index;
            return true;
        }

        case EXPR_UNARY:
            return collect_bound_check_refs(expr->expression_data.unary_expr.operand, indices, capacity, count);

        case EXPR_BINARY:
            return 
                collect_bound_check_refs(expr->expression_data.binary_expr.left_operand, indices, capacity, count) &&
                collect_bound_check_refs(expr->expression_data.binary_expr.right_operand, indices, capacity, count);

        case EXPR_IS_NULL:
            return collect_bound_check_refs(expr->expression_data.is_null_expr.operand, indices, capacity, count);

        case EXPR_IS_NOT_NULL:
            return collect_bound_check_refs(expr->expression_data.is_not_null_expr.operand, indices, capacity, count);

        case EXPR_IN: {
            const In *in = &expr->expression_data.in_expr;

            if (!in->operand || !in->set_options || in->option_count == 0) {
                printf("collect_bound_check_refs: Invalid IN expression.\n");
                return false;
            }

            if (!collect_bound_check_refs(in->operand, indices, capacity, count)) {
                printf("collect_bound_check_refs: Collection of bound column reference indexes failed.\n");
                return false;
            }

            for (uint32_t i = 0; i < in->option_count; i++) {
                if (!collect_bound_check_refs(in->set_options[i], indices, capacity, count)) {
                    printf("collect_bound_check_refs: Collection of bound column reference indexes failed.\n");
                    return false;
                }
            }

            return true;
        }

        case EXPR_BETWEEN: {
            const Between *between = &expr->expression_data.between_expr;

            return collect_bound_check_refs(between->operand, indices, capacity, count) &&
            collect_bound_check_refs(between->lower, indices, capacity, count) &&
            collect_bound_check_refs(between->upper, indices, capacity, count);
        }

        case EXPR_FUNCTIONS:
        case EXPR_TABLE_REF:
        default:
            return false;
    }
}

// Generate a constraint name for an unnamed constraint in ALTER TABLE
bool generate_alter_constraint_name(const ConstraintNode *constraint,
    const ColumnDefNode *column, const Table *table, const Schema *working_schema,
    Constraint *const *prepared, uint32_t prepared_count, char *out_name) {
    
    if (!constraint || !column || !working_schema || !out_name || 
        !table || table->name[0] == '\0' ||
        (working_schema->num_constraints > 0 && !working_schema->constraints) ||
        (column->num_constraints > 0 && !column->constraints) ||
        (prepared_count > 0 && !prepared)) {
        printf("generate_alter_constraint_name: Invalid inputs.\n");
        return false;
    }

    // ASTConstraintType to ConstraintType conversion
    ConstraintType type;
    
    switch(constraint->type) {
        case AST_CONSTRAINT_PRIMARY_KEY: type = PRIMARY_KEY; break;
        case AST_CONSTRAINT_FOREIGN_KEY: type = FOREIGN_KEY; break;
        case AST_CONSTRAINT_UNIQUE: type = UNIQUE; break;
        case AST_CONSTRAINT_DEFAULT: type = DEFAULT; break;
        case AST_CONSTRAINT_NOT_NULL: type = NOT_NULL; break;
        case AST_CONSTRAINT_CHECK: type = CHECK; break;
        default: 
            printf("generate_alter_constraint_name: Invalid AST constraint type.\n");
            return false;
    }

    // Determine the prefix of the constraint name
    char *prefix = constraint_type_prefix(type);

    if (!prefix) {
        printf("generate_alter_constraint_name: Invalid constraint name prefix.\n");
        return false;
    }

    // Determine the suffix of the constraint name
    for (uint32_t suffix = 1; suffix != 0; suffix++) {
        char ending[32];

        int ending_len = snprintf(ending, sizeof(ending), "_%s_%u", prefix, suffix);

        if (ending_len < 0 || (size_t)ending_len >= sizeof(ending) || ending_len >= 63) {
            printf("generate_alter_constraint_name: Invalid generated constraint name suffix.\n");
            return false;
        }

        size_t table_len = strlen(table->name);
        size_t max_table_len = 63 - (size_t)ending_len;

        if (table_len > max_table_len) {
            table_len = max_table_len;
        }

        int chars_written = snprintf(out_name, 64, "%.*s%s", (int)table_len, table->name, ending);

        if (chars_written < 0 || chars_written >= 64) {
            printf("generate_alter_constraint_name: Invalid generated constraint name.\n");
            return false;
        }

        bool collision = false;

        // Detect duplicate constraint name conflicts in the working schema
        for (uint32_t i = 0; i < working_schema->num_constraints; i++) {
            if (!working_schema->constraints[i]) {
                printf("generate_alter_constraint_name: Invalid schema constraint.\n");
                return false;
            }

            if (!strcmp(working_schema->constraints[i]->constraint_name, out_name)) {
                collision = true;
                break;
            }
        }

        // Detect prepared duplicate constraint name conflicts in the currect ADD COLUMN action
        for (uint32_t i = 0; !collision && i < prepared_count; i++) {
            if (!prepared[i]) {
                printf("generate_alter_constraint_name: Invalid action constraint.\n");
                return false;
            }

            if (!strcmp(prepared[i]->constraint_name, out_name)) {
                collision = true;
                break;
            }
        }


        // Detect explicit duplicate constraint name conflicts in the currect ADD COLUMN action
        for (uint32_t i = 0; !collision && i < column->num_constraints; i++) {
            const ConstraintNode *other = column->constraints[i];

            if (!other) {
                printf("generate_alter_constraint_name: Invalid named constraint.\n");
                return false;
            }

            if (other->constraint_name[0] != '\0' && !strcmp(other->constraint_name, out_name)) {
                collision = true;
            }
        }

        if (!collision) {
            return true;
        }
    }

    return false;
}


/* ---------- ALTER TABLE MODIFY COLUMN Validation Helpers ---------- */

// Validate the compatibity of 2 Column's data types, used by FOREIGN KEY validation
bool fk_runtime_types_compatible(const Column *left, const Column *right) {
    if (!left || !right) {
        printf("fk_runtime_types_compatible: Invalid input columns.\n");
        return false;
    }

    if (left->type != right->type) {
        return false;
    }

    switch (left->type) {
        case CHAR:
        case VARCHAR:
            return left->type_args.length == right->type_args.length;

        case NUMERIC:
            return left->type_args.precision == right->type_args.precision &&
                   left->type_args.scale == right->type_args.scale;

        default:
            return true;
    }
}

// Validate a modified FOREIGN KEY constraint in ALTER TABLE MODIFY COLUMN
bool validate_modified_foreign_key(const Constraint *constraint, const Table *table,
    const Schema *working_schema, const Database *db, const char *current_table_name) {
    if (!constraint || constraint->type != FOREIGN_KEY || !table || !working_schema || !db) {
        printf("validate_modified_foreign_key: Invalid inputs.\n");
        return false;
    }

    const ForeignKeyConstraint *fk = &constraint->constraint_data.foreign_key;

    if (!fk->foreign_key_columns || !fk->referenced_columns || fk->amount_columns == 0 ||
        fk->amount_columns != fk->amount_referenced_columns) {
        printf("validate_modified_foreign_key: Invalid FOREIGN KEY.\n");
        return false;
    }

    const Schema *referenced_schema = NULL;

    if (!strcmp(fk->referenced_table_name, current_table_name)) {
        // Self-referencing FK: use proposed schema.
        referenced_schema = working_schema;
    }
    else {
        Table *referenced_table = database_find_table(db, fk->referenced_table_name);

        if (!referenced_table) {
            printf("validate_modified_foreign_key: Invalid referenced table.\n");
            return false;
        }

        referenced_schema = referenced_table->table_schema;
    }

    if (!referenced_schema || !referenced_schema->columns) {
        printf("validate_modified_foreign_key: Invalid referenced table schema.\n");
        return false;
    }

    for (uint32_t i = 0; i < fk->amount_columns; i++) {
        uint32_t local_index = fk->foreign_key_columns[i];
        uint32_t remote_index = fk->referenced_columns[i];

        if (local_index >= working_schema->num_columns ||
            remote_index >= referenced_schema->num_columns) {
            printf("validate_modified_foreign_key: Invalid FOREIGN KEY column indexes.\n");
            return false;
        }

        const Column *local = working_schema->columns[local_index];
        const Column *remote = referenced_schema->columns[remote_index];

        if (!fk_runtime_types_compatible(local, remote)) {
            printf("validate_modified_foreign_key: Incompatible data types.\n");
            return false;
        }
    }

    return true;
}

// Reset Column reference indexes (-1) before binding the proposed modified CHECK constraint
bool reset_check_column_refs(ExpressionNode *expr) {
    if (!expr) {
        return false;
    }

    switch (expr->type) {
        case EXPR_LITERAL:
            return true;

        case EXPR_COLUMN_REF:
            expr->expression_data.column_value.column_index = -1;
            expr->expression_data.column_value.relation_index = -1;
            return true;

        case EXPR_UNARY:
            return reset_check_column_refs(expr->expression_data.unary_expr.operand);

        case EXPR_BINARY:
            return reset_check_column_refs(expr->expression_data.binary_expr.left_operand) &&
                   reset_check_column_refs(expr->expression_data.binary_expr.right_operand);

        case EXPR_IS_NULL:
            return reset_check_column_refs(expr->expression_data.is_null_expr.operand);

        case EXPR_IS_NOT_NULL:
            return reset_check_column_refs(expr->expression_data.is_not_null_expr.operand);

        case EXPR_IN: {
            In *in = &expr->expression_data.in_expr;

            if (!reset_check_column_refs(in->operand) ||
                (in->option_count > 0 && !in->set_options)) {
                return false;
            }

            for (uint32_t i = 0; i < in->option_count; i++) {
                if (!reset_check_column_refs(in->set_options[i])) {
                    return false;
                }
            }

            return true;
        }

        case EXPR_BETWEEN: {
            Between *between = &expr->expression_data.between_expr;

            return reset_check_column_refs(between->operand) &&
                   reset_check_column_refs(between->lower) &&
                   reset_check_column_refs(between->upper);
        }

        default:
            return false;
    }
}

// Validate a modified CHECK constraint in ALTER TABLE MODIFY COLUMN
bool validate_modified_check_expression(const Constraint *constraint, Table *table,
    Schema *working_schema, Database *db, const char *current_table_name) {
    if (!constraint || constraint->type != CHECK || !table || !working_schema || !db) {
        printf("validate_modified_check_expression: Invalid inputs.\n");
        return false;
    }

    const CheckConstraint *check = &constraint->constraint_data.check;

    if (!check->constraint_expr) {
        printf("validate_modified_check_expression: Invalid CHECK constraint expression.\n");
        return false;
    }

    ExpressionNode *copy = expression_node_copy(check->constraint_expr);

    if (!copy) {
        printf("validate_modified_check_expression: Invalid expression Node copy.\n");
        return false;
    }

    if (!reset_check_column_refs(copy)) {
        expression_node_free(copy);
        return false;
    }

    ColumnDefNode proposed_column = {0};

    BindingRelation relation = {0};
    relation.table = table;
    relation.proposed_schema = working_schema;
    relation.proposed_column = &proposed_column;
    strcpy(relation.proposed_table_name, current_table_name);

    BindingContext check_context = {0};
    check_context.db = db;
    check_context.relations = &relation;
    check_context.num_relations = 1;
    check_context.current_clause = BIND_CLAUSE_CHECK;

    BoundExpressionType result_type = {0};

    if (!bind_scalar_expression(copy, &check_context, &result_type)) {
        printf("validate_modified_check_expression: CHECK expression binding failed.\n");
        expression_node_free(copy);
        return false;
    }

    if (result_type.is_untyped_null || result_type.type != BOOL) {
        printf("validate_modified_check_expression: Invalid CHECK expression type.\n");
        expression_node_free(copy);
        return false;
    }

    expression_node_free(copy);
    return true;
}

// Validate a modified DEFAULT constraint in ALTER TABLE MODIFY COLUMN
bool validate_modified_default(const Constraint *constraint, DataType new_type,
    DataTypeArgs new_args, BindingContext *context) {
    if (!constraint || constraint->type != DEFAULT || !context || !context->db) {
        printf("validate_modified_default: Invalid inputs.\n");
        return false;
    }

    ExpressionNode *expr = constraint->constraint_data.default_value.default_expr;

    if (!expr) {
        printf("validate_modified_default: Invalid DEFAULT expression node.\n");
        return false;
    }

    BoundExpressionType result_type = {0};

    BindingClause previous = context->current_clause;
    context->current_clause = BIND_CLAUSE_DEFAULT;

    bool bound = bind_scalar_expression(expr, context, &result_type);

    context->current_clause = previous;

    if (!bound) {
        printf("validate_modified_default: Binding of DEFAULT expression failed.\n");
        return false;
    }

    // SQL NULL is compatible at the datatype level.
    // NOT NULL / PRIMARY KEY rules are checked separately.
    if (result_type.is_untyped_null || expression_is_explicit_null_literal(expr)) {
        return true;
    }

    if (expr->type == EXPR_LITERAL) {
        Value *value = expr->expression_data.literal_value.literal;

        if (!value || !value_can_assign(new_type, value)) {
            return false;
        }

        if (value->type == new_type && !value_fits_type_args(value, new_type, new_args)) {
            return false;
        }

        return true;
    }

    return value_types_assignable(new_type, result_type.type);
}

// Validate incoming FOREIGN KEYS from other database tables
bool validate_modify_incoming_foreign_keys(const Table *modified_table,
    const Schema *proposed_schema, const Database *db, const AlterColumnMap *map) {

    if (!modified_table || !proposed_schema || !db || !map ||
        map->num_columns != proposed_schema->num_columns ||
        !map->working_to_original || (db->table_count > 0 && !db->tables)) {
        printf("validate_modify_incoming_foreign_keys: Invalid inputs.\n");
        return false;
    }

    for (uint32_t i = 0; i < db->table_count; i++) {
        const Table *other = db->tables[i];

        if (!other || !other->table_schema) {
            printf("validate_modify_incoming_foreign_keys: Invalid database table.\n");
            return false;
        }

        // Self-references are already handled against the proposed schema by the local FK validator
        if (other == modified_table) {
            continue;
        }

        const Schema *other_schema = other->table_schema;

        // Traverse the other table's schema constraints
        for (uint32_t j = 0; j < other_schema->num_constraints; j++) {
            const Constraint *constraint = other_schema->constraints[j];

            if (!constraint) {
                printf("validate_modify_incoming_foreign_keys: Invalid constraint.\n");
                return false;
            }

            if (constraint->type != FOREIGN_KEY) {
                continue;
            }

            const ForeignKeyConstraint *fk = &constraint->constraint_data.foreign_key;

            if (strcmp(fk->referenced_table_name, modified_table->name)) {
                continue;
            }

            if (!fk->foreign_key_columns || !fk->referenced_columns ||
                fk->amount_columns == 0 ||
                fk->amount_columns != fk->amount_referenced_columns) {
                printf("validate_modify_incoming_foreign_keys: Invalid FOREIGN KEY constraint.\n");
                return false;
            }

            for (uint32_t j = 0; j < fk->amount_columns; j++) {
                uint32_t local_index = fk->foreign_key_columns[j];

                // External FK stores the ORIGINAL referenced column index
                uint32_t remote_original = fk->referenced_columns[j];

                uint32_t remote_working = 0;

                if (local_index >= other_schema->num_columns ||
                    remote_original >= modified_table->table_schema->num_columns) {
                    return false;
                }

                if (!alter_map_find_working(map, remote_original, &remote_working)) {
                    printf("validate_modify_incoming_foreign_keys: Referenced column missing in proposed schema.\n");
                    return false;
                }

                if (!fk_runtime_types_compatible(
                        other_schema->columns[local_index],
                        proposed_schema->columns[remote_working]
                    )) {
                    printf("validate_modify_incoming_foreign_keys: FOREIGN KEY datatype mismatch.\n");
                    return false;
                }
            }
        }
    }

    return true;
}

// Check that the DEFAULT value validates new data type arguments 
bool value_fits_type_args(const Value *value, DataType target_type, DataTypeArgs target_args) {
    if (!value || 
        target_type < INTEGER || target_type > JSONB ||
        value->type != target_type) {
        printf("value_fits_type_args: Invalid inputs.\n");
        return false;
    }

    if (value->null_val) {
        return true;
    }

    switch (target_type) {
        case CHAR: {
            if (target_args.length == 0 || !value->value.char_val.string) {
                printf("value_fits_type_args: Invalid CHAR value/length.\n");
                return false;
            }

            size_t actual_length = strlen(value->value.char_val.string);

            return actual_length <= target_args.length;
        }

        case VARCHAR: {
            if (!value->value.varchar_val.string || target_args.length == 0) {
                printf("value_fits_type_args: Invalid VARCHAR value/length.\n");
                return false;
            }

            size_t actual_length = strlen(value->value.varchar_val.string);

            return actual_length <= target_args.length;
        }
            
        case NUMERIC: {
            if (target_args.precision == 0 || target_args.scale > target_args.precision) {
                printf("value_fits_type_args: Invalid NUMERIC parameters.\n");
                return false;
            }

            // Safely determine the magnitude, including INT64_MIN
            uint64_t magnitude = value->value.numeric_val.val < 0
                ? (uint64_t)(-(value->value.numeric_val.val + 1)) + 1
                : (uint64_t)value->value.numeric_val.val;

            // Zero fits every valid NUMERIC declaration
            if (magnitude == 0) {
                return true;
            }

            // Count significant digits in the integer representation
            uint32_t digits = 0;
            uint64_t temp = magnitude;

            while (temp > 0) {
                digits++;
                temp /= 10;
            }

            // Determine the number of integer digits.
            uint32_t integer_digits =
                digits > value->value.numeric_val.scale
                    ? digits - value->value.numeric_val.scale
                    : 0;

            uint32_t allowed_integer_digits = target_args.precision - target_args.scale;

            if (integer_digits > allowed_integer_digits) {
                return false;
            }

            // Check that reducing the scale would not lose nonzero fractional digits
            if (value->value.numeric_val.scale > target_args.scale) {
                uint32_t excess = value->value.numeric_val.scale - target_args.scale;

                // An int64_t magnitude has at most 19 digits
                if (excess > 19) {
                    return false;
                }

                for (uint32_t i = 0; i < excess; i++) {
                    if (magnitude % 10 != 0) {
                        return false;
                    }

                    magnitude /= 10;
                }
            }
        }

        default:
            break;

    }

    return true;
}

// Validate one existing index against the proposed table schema
bool validate_modified_index(const Index *index, Schema *working_schema,
    uint32_t column_index, const AlterColumnMap *map) {
    if (!index || !index->key || !index->key->column_index_array || index->key->num_columns == 0 ||
        !working_schema || !working_schema->columns ||
        !map || !map->working_to_original ||
        map->num_columns != working_schema->num_columns ||
        column_index >= working_schema->num_columns) {
        return false;
    }

    uint32_t count = index->key->num_columns;

    if ((size_t)count > SIZE_MAX / sizeof(uint32_t)) {
        return false;
    }

    // Create a temporary key using WORKING indexes
    uint32_t *working_indexes = malloc((size_t)count * sizeof(*working_indexes));

    if (!working_indexes) {
        return false;
    }

    bool valid = false;
    bool affected = false;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t original_index = index->key->column_index_array[i];

        if (!alter_map_find_working(map, original_index, &working_indexes[i])) {
            goto cleanup;
        }

        if (working_indexes[i] >= working_schema->num_columns) {
            goto cleanup;
        }

        if (working_indexes[i] == column_index) {
            affected = true;
        }
    }

    // Stack-only copies of the metadata
    // No ownership of the live index or key is transferred
    IndexKey remapped_key = *index->key;
    remapped_key.column_index_array = working_indexes;

    Index remapped_index = *index;
    remapped_index.key = &remapped_key;

    // Check the physical definition against the correct columns
    if (!btree_index_definition_fits(&remapped_index, working_schema)) {
        goto cleanup;
    }

    if (affected) {
        for (uint32_t i = 0; i < count; i++) {
            Column *column = working_schema->columns[working_indexes[i]];

            if (!column) {
                goto cleanup;
            }

            DataType type = column->type;

            if (!supports_ordering(type) || !are_equality_compatible(type, type)) {
                goto cleanup;
            }
        }
    }

    valid = true;

cleanup:
    free(working_indexes);
    return valid;
}

// Orchestrate validation of all indexes that are affected by the modified column
bool validate_modified_column_indexes(Table *table, Schema *working_schema,
    uint32_t column_index, const AlterColumnMap *map) {
    if (!table || !working_schema ||
        !working_schema->columns ||
        working_schema->num_columns == 0 ||
        column_index >= working_schema->num_columns ||
        (table->total_secondary_indexes > 0 && !table->secondary_indexes)) {
        printf("validate_modified_column_indexes: Invalid inputs.\n");
        return false;
    }

    if (!map || !map->working_to_original || map->num_columns != working_schema->num_columns) {
        return false;
    }

    // Validate the canonical B+ Tree index
    if (!table->primary_index ||
        !validate_modified_index(table->primary_index, working_schema, column_index,map)) {
        printf("validate_modified_column_indexes: Primary index incompatibility.\n");
        return false;
    }

    // Validate all secondary indexes.
    for (uint32_t i = 0; i < table->total_secondary_indexes; i++) {
        Index *index = table->secondary_indexes[i];

        if (!validate_modified_index(index, working_schema, column_index, map)) {
            printf("validate_modified_column_indexes: Secondary index incompatibility.\n");
            return false;
        }
    }

    return true;
}


/* ---------- ALTER TABLE ADD CONSTRAINT Validation Helpers ---------- */

// Resolve constraint column references against a runtime schema
bool resolve_constraint_column_refs(ExpressionNode *const *refs, uint32_t count,
    const Schema *schema, const char *owner_name, uint32_t *out_indexes) {
    
    if (!refs || count == 0 || !schema || !schema->columns ||
        !owner_name || !out_indexes || count > schema->num_columns ||
        schema->num_columns > INT32_MAX) {
        printf("resolve_constraint_column_refs: Invalid inputs.\n");
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        const ExpressionNode *expr = refs[i];

        if (!expr || expr->type != EXPR_COLUMN_REF) {
            printf("resolve_constraint_column_refs: Invalid input expression nodes.\n");
            return false;
        }

        const ColumnRef *ref = &expr->expression_data.column_value;

        if (ref->column_name[0] == '\0' ||
            ref->column_index != -1 || ref->relation_index != -1 ||
            (ref->table_name[0] != '\0' && strcmp(ref->table_name, owner_name))) {
            printf("resolve_constraint_column_refs: Invalid column reference.\n");
            return false;
        }

        // Find the column in the proposed schema
        bool found = false;
        uint32_t index = 0;

        for (uint32_t j = 0; j < schema->num_columns; j++) {
            const Column *column = schema->columns[j];

            if (!column || column->name[0] == '\0') {
                printf("resolve_constraint_column_refs: Invalid schema column.\n");
                return false;
            }

            if (!strcmp(ref->column_name, column->name)) {
                index = j;
                found = true;
                break;
            }
        }

        if (!found) {
            printf("resolve_constraint_column_refs: Column doesn't exist in the proposed schema.\n");
            return false;
        }

        // Reject duplicate references within the same key
        for (uint32_t j = 0; j < i; j++) {
            if (out_indexes[j] == index) {
                printf("resolve_constraint_column_refs: Duplicate references within the same key.\n");
                return false;
            }
        }

        out_indexes[i] = index;
    }

    return true;
}

// Compare two ordered arrays of resolved column indexes
bool constraint_indexes_equal(const uint32_t *left, const uint32_t *right, uint32_t count) {
    if (!left || !right || count == 0) {
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (left[i] != right[i]) {
            return false;
        }
    }

    return true;
}

// Check if schema has an equivalent constraint

bool schema_has_equivalent_constraint(const Schema *schema, const Constraint *candidate) {
    if (!schema || !candidate ||
        (schema->num_constraints > 0 && !schema->constraints)) {
        return false;
    }

    // No existing constraints means no duplicate
    if (schema->num_constraints == 0) {
        return false;
    }

    for (uint32_t i = 0; i < schema->num_constraints; i++) {
        const Constraint *existing = schema->constraints[i];

        if (!existing || existing->type != candidate->type) {
            continue;
        }

        switch (candidate->type) {
            case PRIMARY_KEY: {
                const PrimaryKeyConstraint *a = &existing->constraint_data.primary_key;
                const PrimaryKeyConstraint *b = &candidate->constraint_data.primary_key;

                if (a->amount_columns == b->amount_columns &&
                    constraint_indexes_equal(
                        a->primary_key_columns,
                        b->primary_key_columns,
                        a->amount_columns)) {
                    return true;
                }

                break;
            }

            case UNIQUE: {
                const UniqueConstraint *a = &existing->constraint_data.unique_cols;
                const UniqueConstraint *b = &candidate->constraint_data.unique_cols;

                if (a->amount_columns == b->amount_columns &&
                    constraint_indexes_equal(
                        a->column_refs,
                        b->column_refs,
                        a->amount_columns)) {
                    return true;
                }

                break;
            }

            case FOREIGN_KEY: {
                const ForeignKeyConstraint *a = &existing->constraint_data.foreign_key;
                const ForeignKeyConstraint *b = &candidate->constraint_data.foreign_key;

                // Reject comparison if metadata differs.
                if (strcmp(a->referenced_table_name, b->referenced_table_name) ||
                    a->amount_columns != b->amount_columns ||
                    a->amount_referenced_columns != b->amount_referenced_columns) {
                    break;
                }

                if (constraint_indexes_equal(
                        a->foreign_key_columns,
                        b->foreign_key_columns,
                        a->amount_columns) &&
                    constraint_indexes_equal(
                        a->referenced_columns,
                        b->referenced_columns,
                        a->amount_referenced_columns)) {
                    return true;
                }

                break;
            }

            default:
                break;
        }
    }

    return false;
}

// Validate that another table's FOREIGN KEY references the target column
bool has_incoming_fk_dependency(const Database *db, const Table *target_table, uint32_t column_index) {
    // Fail closed on invalid metadata
    if (!db || !target_table || (db->table_count > 0 && !db->tables)) {
        return true; 
    }

    for (uint32_t i = 0; i < db->table_count; i++) {
        const Table *other = db->tables[i];

        if (!other || !other->table_schema) {
            return true;
        }

        // Self-references checked locally
        if (other == target_table) {
            continue; 
        }

        const Schema *schema = other->table_schema;

        // Fail closed on invalid metadata
        if (schema->num_constraints > 0 && !schema->constraints) {
            return true;
        }

        for (uint32_t j = 0; j < schema->num_constraints; j++) {
            const Constraint *constraint = schema->constraints[j];

            if (!constraint) {
                return true;
            }

            // Another table's FOREIGN KEY matches if it references
            // the target table and column
            if (constraint->type == FOREIGN_KEY &&
                !strcmp(
                    constraint->constraint_data.foreign_key.referenced_table_name,
                    target_table->name
                ) &&
                foreign_key_references_column(constraint, column_index)) {
                return true;
            }
        }
    }

    return false;
}

// Remove column from the working schema as a result of a DROP COLUMN action
// FAILURE CONTRACT:
// - On failure, the working schema may be partially modified.
// - The caller must discard the entire working schema using schema_free().
// - Do not reuse schema after a failed call.
bool working_schema_drop_column(Schema *schema, uint32_t column_index, const char *table_name) {
    if (!schema || !table_name || table_name[0] == '\0' ||
        !schema->columns || schema->num_columns <= 1 ||
        column_index >= schema->num_columns ||
        (schema->num_constraints > 0 && !schema->constraints)) {
        printf("working_schema_drop_column: Invalid inputs.\n");
        return false;
    }

    for (uint32_t i = 0; i < schema->num_columns; i++) {
        if (!schema->columns[i] || schema->columns[i]->name[0] == '\0') {
            printf("working_schema_drop_column: Invalid schema column.\n");
            return false;
        } 
    }

    const uint32_t old_count = schema->num_columns;

    // Temporary traversal stack for CHECK expression trees
    ExpressionNode **stack = NULL;
    size_t stack_capacity = 0;

    // Pass 0: validate references and prepare traversal capacity
    // Pass 1: update surviving references
    for (int pass = 0; pass < 2; pass++) {
        for (uint32_t i = 0; i < schema->num_constraints; i++) {
            Constraint *constraint = schema->constraints[i];

            if (!constraint) {
                goto failed;
            }

            uint32_t *refs[2] = {NULL, NULL};
            uint32_t counts[2] = {0, 0};
            uint32_t groups = 0;
            ExpressionNode *check_expr = NULL;

            switch (constraint->type) {
                case PRIMARY_KEY:
                    refs[0] = constraint->constraint_data.primary_key.primary_key_columns;
                    counts[0] = constraint->constraint_data.primary_key.amount_columns;
                    
                    groups = 1;
                    break;

                case UNIQUE:
                    refs[0] = constraint->constraint_data.unique_cols.column_refs;
                    counts[0] = constraint->constraint_data.unique_cols.amount_columns;
                    
                    groups = 1;
                    break;

                case FOREIGN_KEY: {
                    ForeignKeyConstraint *fk = &constraint->constraint_data.foreign_key;

                    refs[0] = fk->foreign_key_columns;
                    counts[0] = fk->amount_columns;
                    groups = 1;

                    // Referenced indexes are local only for self-FKs.
                    if (!strcmp(fk->referenced_table_name, table_name)) {
                        refs[1] = fk->referenced_columns;
                        counts[1] = fk->amount_referenced_columns;
                        groups = 2;
                    }

                    break;
                }

                case CHECK: {
                    CheckConstraint *check = &constraint->constraint_data.check;

                    refs[0] = check->column_refs;
                    counts[0] = check->amount_columns;
                    groups = 1;

                    check_expr = check->constraint_expr;

                    if (!check_expr) {
                        goto failed;
                    }

                    break;
                }

                case DEFAULT:
                case NOT_NULL: {
                    uint32_t owner = constraint->type == DEFAULT
                        ? constraint->constraint_data.default_value.column_ref
                        : constraint->constraint_data.not_null.column_ref;

                    if (owner >= old_count) {
                        goto failed;
                    }

                    // This constraint will disappear with the column
                    if (owner == column_index) {
                        continue;
                    }

                    if (pass == 1 && owner > column_index) {
                        if (constraint->type == DEFAULT) {
                            constraint->constraint_data.default_value.column_ref--;
                        } else {
                            constraint->constraint_data.not_null.column_ref--;
                        }
                    }

                    continue;
                }

                default:
                    goto failed;
            }

            // Validate/remap the constraint's local index arrays
            for (uint32_t g = 0; g < groups; g++) {
                if (counts[g] == 0 || !refs[g]) {

                    // CHECK without column references is valid
                    if (constraint->type == CHECK && counts[g] == 0 && !refs[g]) {
                        continue;
                    }

                    goto failed;
                }

                for (uint32_t j = 0; j < counts[g]; j++) {
                    uint32_t index = refs[g][j];

                    if (index >= old_count || index == column_index) {
                        goto failed;
                    }

                    if (pass == 1 && index > column_index) {
                        refs[g][j] = index - 1;
                    }
                }
            }

            if (!check_expr) {
                continue;
            }

            // Traverse the complete CHECK expression tree.
            // Pass 0 also grows the stack to its required capacity.
            size_t top = 0;

            if (stack_capacity == 0) {
                stack = malloc(16 * sizeof(*stack));

                if (!stack) {
                    goto failed;
                }

                stack_capacity = 16;
            }

            stack[top++] = check_expr;

            while (top > 0) {
                ExpressionNode *node = stack[--top];

                if (!node) {
                    goto failed;
                }

                uint32_t children = 0;

                switch (node->type) {
                    case EXPR_LITERAL:
                        break;

                    case EXPR_COLUMN_REF: {
                        ColumnRef *ref = &node->expression_data.column_value;

                        if (ref->relation_index != 0 || ref->column_index < 0 ||
                            (uint32_t)ref->column_index >= old_count ||
                            (uint32_t)ref->column_index == column_index) {
                            goto failed;
                        }

                        if (pass == 1 && (uint32_t)ref->column_index > column_index) {
                            ref->column_index--;
                        }

                        break;
                    }

                    case EXPR_UNARY:
                    case EXPR_IS_NULL:
                    case EXPR_IS_NOT_NULL:
                        children = 1;
                        break;

                    case EXPR_BINARY:
                        children = 2;
                        break;

                    case EXPR_BETWEEN:
                        children = 3;
                        break;

                    case EXPR_IN: {
                        In *in = &node->expression_data.in_expr;

                        if (!in->operand || in->option_count == UINT32_MAX ||
                            (in->option_count > 0 && !in->set_options)) {
                            goto failed;
                        }

                        children = in->option_count + 1;
                        break;
                    }

                    default:
                        // Functions and unsupported expression types
                        // are not permitted in CHECK expressions
                        goto failed;
                }

                if ((size_t)children > SIZE_MAX - top) {
                    goto failed;
                }

                size_t required = top + (size_t)children;

                if (required > stack_capacity) {
                    size_t new_capacity = stack_capacity;

                    while (new_capacity < required) {
                        if (new_capacity > SIZE_MAX / 2) {
                            new_capacity = required;
                            break;
                        }

                        new_capacity *= 2;
                    }

                    if (new_capacity > SIZE_MAX / sizeof(*stack)) {
                        goto failed;
                    }

                    ExpressionNode **expanded = realloc(stack, new_capacity * sizeof(*stack));

                    if (!expanded) {
                        goto failed;
                    }

                    stack = expanded;
                    stack_capacity = new_capacity;
                }

                for (uint32_t j = 0; j < children; j++) {
                    ExpressionNode *child = NULL;

                    switch (node->type) {
                        case EXPR_UNARY:
                            child = node->expression_data.unary_expr.operand;
                            break;

                        case EXPR_IS_NULL:
                            child = node->expression_data.is_null_expr.operand;
                            break;

                        case EXPR_IS_NOT_NULL:
                            child = node->expression_data.is_not_null_expr.operand;
                            break;

                        case EXPR_BINARY:
                            child = j == 0
                                ? node->expression_data.binary_expr.left_operand
                                : node->expression_data.binary_expr.right_operand;
                            break;

                        case EXPR_BETWEEN: {
                            Between *between = &node->expression_data.between_expr;

                            child = j == 0 ? between->operand
                                  : j == 1 ? between->lower
                                           : between->upper;
                            break;
                        }

                        case EXPR_IN: {
                            In *in = &node->expression_data.in_expr;

                            child = j == 0
                                ? in->operand
                                : in->set_options[j - 1];
                            break;
                        }

                        default:
                            goto failed;
                    }

                    if (!child) {
                        goto failed;
                    }

                    stack[top++] = child;
                }
            }
        }

        if (pass == 0) {
            // Remove DEFAULT/NOT NULL constraints on the dropped column
            uint32_t write = 0;
            uint32_t old_constraint_count = schema->num_constraints;

            for (uint32_t read = 0; read < old_constraint_count; read++) {
                Constraint *constraint = schema->constraints[read];

                bool remove = false;

                if (constraint->type == DEFAULT) {
                    remove = constraint->constraint_data.default_value.column_ref == column_index;
                }
                else if (constraint->type == NOT_NULL) {
                    remove = constraint->constraint_data.not_null.column_ref == column_index;
                }

                if (remove) {
                    constraint_free(constraint);
                }
                else {
                    schema->constraints[write++] = constraint;
                }
            }

            for (uint32_t i = write; i < old_constraint_count; i++) {
                schema->constraints[i] = NULL;
            }

            schema->num_constraints = write;
        }
    }

    free(stack);
    stack = NULL;

    // Remove the target column from the copied schema
    Column *removed = schema->columns[column_index];

    for (uint32_t i = column_index; i + 1 < old_count; i++) {
        schema->columns[i] = schema->columns[i + 1];
    }

    schema->num_columns = old_count - 1;
    schema->columns[schema->num_columns] = NULL;

    free(removed);

    return true;

failed:
    free(stack);
    printf("working_schema_drop_column: Schema update failed.\n");
    return false;
}

// Select between a proposed table name from an ALTER TABLE RENAME and the actual table name
const char *binding_relation_current_name(const BindingRelation *relation) {
    if (!relation || !relation->table) {
        return NULL;
    }

    if (relation->proposed_table_name[0] != '\0') {
        return relation->proposed_table_name;
    }

    return relation->table->name;
}

// only CHECK constraints require column-name rewriting in their stored expression trees 
// for your current runtime representation
bool rewrite_check_identifiers(ExpressionNode *expr, int32_t target_column_index,
    const char *new_column_name, const char *old_table_name, const char *new_table_name,
    bool apply) {
    if (!expr) {
        return false;
    }

    switch (expr->type) {
        case EXPR_LITERAL:
            return true;

        case EXPR_COLUMN_REF: {
            ColumnRef *ref = &expr->expression_data.column_value;

            if (target_column_index >= 0 &&
                ref->column_index == target_column_index) {

                if (!new_column_name || strlen(new_column_name) >= sizeof(ref->column_name)) {
                    return false;
                }

                if (apply) {
                    strcpy(ref->column_name, new_column_name);
                }
            }

            if (old_table_name && new_table_name && ref->table_name[0] != '\0' &&
                !strcmp(ref->table_name, old_table_name)) {

                if (strlen(new_table_name) >= sizeof(ref->table_name)) {
                    return false;
                }

                if (apply) {
                    strcpy(ref->table_name, new_table_name);
                }
            }

            return true;
        }

        case EXPR_UNARY:
            return rewrite_check_identifiers(
                expr->expression_data.unary_expr.operand,
                target_column_index, new_column_name,
                old_table_name, new_table_name, apply
            );

        case EXPR_BINARY: {
            Binary *binary = &expr->expression_data.binary_expr;

            return
                rewrite_check_identifiers(
                    binary->left_operand,
                    target_column_index, new_column_name,
                    old_table_name, new_table_name, apply
                ) &&
                rewrite_check_identifiers(
                    binary->right_operand,
                    target_column_index, new_column_name,
                    old_table_name, new_table_name, apply
                );
        }

        case EXPR_IS_NULL:
            return rewrite_check_identifiers(
                expr->expression_data.is_null_expr.operand,
                target_column_index, new_column_name,
                old_table_name, new_table_name, apply
            );

        case EXPR_IS_NOT_NULL:
            return rewrite_check_identifiers(
                expr->expression_data.is_not_null_expr.operand,
                target_column_index, new_column_name,
                old_table_name, new_table_name, apply
            );

        case EXPR_IN: {
            In *in = &expr->expression_data.in_expr;

            if (!in->operand || (in->option_count > 0 && !in->set_options)) {
                return false;
            }

            if (!rewrite_check_identifiers(
                    in->operand,
                    target_column_index, new_column_name,
                    old_table_name, new_table_name, apply)) {
                return false;
            }

            for (uint32_t i = 0; i < in->option_count; i++) {
                if (!rewrite_check_identifiers(
                        in->set_options[i],
                        target_column_index, new_column_name,
                        old_table_name, new_table_name, apply)) {
                    return false;
                }
            }

            return true;
        }

        case EXPR_BETWEEN: {
            Between *between = &expr->expression_data.between_expr;

            return
                rewrite_check_identifiers(
                    between->operand,
                    target_column_index, new_column_name,
                    old_table_name, new_table_name, apply
                ) &&
                rewrite_check_identifiers(
                    between->lower,
                    target_column_index, new_column_name,
                    old_table_name, new_table_name, apply
                ) &&
                rewrite_check_identifiers(
                    between->upper,
                    target_column_index, new_column_name,
                    old_table_name, new_table_name, apply
                );
        }

        default:
            return false;
    }
}


/* ---------- DROP TABLE Helpers ---------- */

// Validate that the current table is referenced by another table's FOREIGN KEY constriant
bool has_external_incoming_fk(const Database *db, const Table *target_table) {
    // Fail closed on invalid metadata
    if (!db || !target_table ||target_table->name[0] == '\0' ||
        (db->table_count > 0 && !db->tables)) {
        printf("has_external_incoming_fk: Invalid inputs.\n");
        return true;
    }

    // Iterate through all registered tables
    for (uint32_t i = 0; i < db->table_count; i++) {
        const Table *other = db->tables[i];

        if (!other || !other->table_schema) {
            printf("has_external_incoming_fk: Invalid database table.\n");
            return true;
        }

        // Ignore self-referencing FOREIGN KEYs
        if (other == target_table) {
            continue;
        }

        const Schema *schema = other->table_schema;

        if (schema->num_constraints > 0 && !schema->constraints) {
            printf("has_external_incoming_fk: Invalid table schema.\n");
            return true;
        }

        // Inspect every constraint of the other table
        for (uint32_t j = 0; j < schema->num_constraints; j++) {
            const Constraint *constraint = schema->constraints[j];

            if (!constraint) {
                printf("has_external_incoming_fk: Invalid schema constraint.\n");
                return true;
            }

            if (constraint->type != FOREIGN_KEY) {
                continue;
            }

            const ForeignKeyConstraint *fk = &constraint->constraint_data.foreign_key;

            // Check whether the FK references the target table
            if (!strcmp(fk->referenced_table_name, target_table->name)) {
                return true;
            }
        }
    }

    return false;
}