#include <stdint.h>
#include "../../include/semantic_binder.h"
#include "semantic_binder_utils.h"
#include "../../include/ast.h"
#include "../../include/database.h"
#include "../../include/expressions.h"
#include "../../include/data_types.h"
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
        case EXPR_TABLE_REF:
            return bind_table_ref(expr, context);
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
bool bind_table_ref(ExpressionNode *expr, BindingContext *context) {
    if (!expr || expr->type != EXPR_TABLE_REF) {
        printf("bind_table_ref: Invalid input expression.\n");
        return false;
    }

    if (!context || !context->db || 
        !context->relations || context->num_relations == 0) {
        printf("bind_table_ref: Invalid input context or output.\n");
        return false;
    }

    TableRef *table_ref = &expr->expression_data.table_value;

    // Validate table name
    if (table_ref->table_name[0] == '\0') {
        printf("bind_table_ref: Empty column name.\n");
        return false;
    }

    bool can_be_bound = false;

    // Search for table names in existing table names
    // We want at least one table name appearance (multiple same-table instances can appear)
    for (uint32_t i = 0; i < context->num_relations; i++) {
        Table *table = context->relations[i].table;

        if (!table || table->name[0] == '\0') {
            printf("bind_column_ref: Invalid binding context structure.\n");
            return false;
        }

        if (!strcmp(table_ref->table_name, table->name)) {
            can_be_bound = true;
            break;
        }
    }

    return can_be_bound;
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

    if (!bind_scalar_expression(function->expression, context, &argument_type)) {
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

bool bind_select(ASTNode *root, BindingContext *context);

bool bind_insert(ASTNode *root, BindingContext *context);

bool bind_update(ASTNode *root, BindingContext *context);

bool bind_delete(ASTNode *root, BindingContext *context);

bool bind_create_table(ASTNode *root, BindingContext *context);

bool bind_drop_table(ASTNode *root, BindingContext *context);

bool bind_alter_table(ASTNode *root, BindingContext *context);

bool bind_truncate_table(ASTNode *root, BindingContext *context);

bool bind_create_index(ASTNode *root, BindingContext *context);

bool bind_drop_index(ASTNode *root, BindingContext *context);