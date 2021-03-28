#include "compile.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

size_t global_counter_if = 0;
size_t global_counter_while = 0;

/*  Helper function to determine if a value is a power of 2.
 * To be used to optimize multiplication in opt1.
 * If it is a power of 2, it returns k, where 2^k = value, else 0.
 * @param1 is int64_t value.
*/
size_t power_of_2(int64_t value) {
    size_t power = 0;
    if (value != 0) {
        while (value != 1) {
            if (value % 2 != 0) {
                return 0;
            }
            value /= 2;
            power += 1;
        }
        return (power);
    }
    else {
        return 0;
    }
}

/* Helper function to determine if a node evaluates to a constant
 * @param1 is the node
 * return a boolean, 1 if yes, 0 if no.
*/
bool is_constants(node_t *node) {
    if (node->type == NUM) {
        return true;
    }
    else if (node->type == BINARY_OP) {
        binary_node_t *binary_node = (binary_node_t *) node;
        bool x = is_constants(binary_node->left);
        bool y = is_constants(binary_node->right);
        if (x && y) {
            return true;
        }
        else {
            return false;
        }
    }
    else {
        return false;
    }
}

/* Helper function to determine what constant a node evaluates to
 * @param1 is the node
 * To be used in conjunction with is_constants(node_t *node);
 * Returns the constant the node evaluates to.
*/
int64_t evaluate_constants(node_t *node) {
    // Returns the number if the node-type is NUM
    if (node->type == NUM) {
        num_node_t *num_node = (num_node_t *) node;
        return (num_node->value);
    }
    else {
        // Recursively calls the left and right if the node-type is Binary
        binary_node_t *binary_node = (binary_node_t *) node;
        char op = binary_node->op;
        int64_t val_right = evaluate_constants(binary_node->right);
        int64_t val_left = evaluate_constants(binary_node->left);
        // Performs the operations using C
        if (op == '+') {
            return (val_left + val_right);
        }
        else if (op == '-') {
            return (val_left - val_right);
        }
        else if (op == '*') {
            return (val_left * val_right);
        }
        else if (op == '/') {
            return (val_left / val_right);
        }
        // Returns 0 if all other conditions are not met.
        else {
            return 0;
        }
    }
}

bool compile_ast(node_t *node) {
    // Obtains the type of the node
    node_type_t node_type = node->type;

    // Compiles the num node type.
    if (node_type == NUM) {
        num_node_t *num_node = (num_node_t *) node;
        // Assembly language to move NUM to the RDI Register
        printf("mov $%lu, %%rdi\n", num_node->value);
        return true;
    }
    // Compiles the print node type
    else if (node_type == PRINT) {
        print_node_t *print_node = (print_node_t *) node;
        compile_ast(print_node->expr);
        printf("call print_int\n");
        return true;
    }
    // Compiles the sequence node types
    else if (node_type == SEQUENCE) {
        sequence_node_t *sequence_node = (sequence_node_t *) node;
        for (size_t index = 0; index < sequence_node->statement_count; index++) {
            // Recursively compiles each statement in the sequence
            compile_ast(sequence_node->statements[index]);
        }
        return true;
    }
    // Compiles the binary node types
    else if (node_type == BINARY_OP) {
        binary_node_t *binary_node = (binary_node_t *) node;
        bool shift_flag;
        shift_flag = false;
        // Determines if the node evaluates to a constant.
        // It pushes that constant value to RDI if it does.
        if (is_constants(node)) {
            int64_t constant_value = evaluate_constants(node);
            printf("mov $%lu, %%rdi\n", constant_value);
        }
        else {
            if (binary_node->op == '*') {
                /* If the operation is 'multiply' and it evaluates to a constant that is a power of 2,
                 * a helper function is used to determine what that power of 2 is and left-shifts the number.
                 * Sets a shift_flag to true.
                */ 
                if (is_constants(binary_node->right)) {
                    int64_t value = evaluate_constants(binary_node->right);
                    int64_t shift = power_of_2(value);
                    if (shift != 0) {
                        compile_ast(binary_node->left);
                        printf("sal $%zu, %%rdi\n", shift);
                        shift_flag = true;
                    }
                }
            }
            if (shift_flag == false) {
                // Does the suboptimal version of everything now :(
                compile_ast(binary_node->right);
                printf("push %%rdi\n");

                compile_ast(binary_node->left);
                printf("pop %%r8\n");

                // Result of binary_node->left is in RDI
                // Result of binary_node->right is in R8
                char operation = binary_node->op;

                if (operation == '+') {
                    printf("add %%r8, %%rdi\n");
                }
                else if (operation == '-') {
                    // The sub result is moved from r8 to RDI
                    printf("sub %%r8, %%rdi\n");
                }
                else if (operation == '*') {
                    printf("imul %%r8, %%rdi\n");
                }
                else if (operation == '/') {
                    // The numerator is moved from r8 to rax
                    // The denominator is kept in rdi which is sign-extended to rax
                    printf("mov %%rdi, %%rax\n");
                    printf("cqto\n");
                    printf("idiv %%r8\n");
                    printf("mov %%rax, %%rdi\n");
                }
                else {
                    // Checks the conditional statements.
                    printf("cmp %%r8, %%rdi\n");
                }
            }
        }
        return true;
    }
    else if (node_type == VAR) {
        // Searches for the variable in the stack and returns its contents
        var_node_t *var_node = (var_node_t *) node;
        printf("mov -0x%02x(%%rbp), %%rdi\n", 8 * (var_node->name - 'A' + 1));
        return true;
    }
    else if (node_type == LET) {
        // Evaluates the expression of the value and assigns it to the stack variable
        let_node_t *let_node = (let_node_t *) node;
        compile_ast(let_node->value);
        printf("mov %%rdi, -0x%02x(%%rbp)\n", 8 * (let_node->var - 'A' + 1));
        return true;
    }
    else if (node_type == IF) {
        if_node_t *if_node = (if_node_t *) node;
        // Checks if the condition is true and jumps to the label if it is.
        ++global_counter_if;
        size_t local_counter_if = global_counter_if;
        compile_ast(if_node->condition);

        // Type casts to binary and checks operation.
        binary_node_t *binary_node = (binary_node_t *) if_node->condition;
        char operation = binary_node->op;
        if (operation == '=') {
            printf("jne IF_ELSE_label%zu\n", local_counter_if);
        }
        else if (operation == '>') {
            printf("jng IF_ELSE_label%zu\n", local_counter_if);
        }
        else if (operation == '<') {
            printf("jnl IF_ELSE_label%zu\n", local_counter_if);
        }
        // Defines IF_label(global_counter_if)
        printf("IF_label%zu:\n", local_counter_if);
        compile_ast(if_node->if_branch);
        printf("jmp ENDIF%zu\n", local_counter_if);
        // Defines IF_ELSE_label(global_counter_if)
        printf("IF_ELSE_label%zu:\n", local_counter_if);
        if (if_node->else_branch != NULL) {
            compile_ast(if_node->else_branch);
        }

        // Defines ENDIF_label
        printf("ENDIF%zu:\n", local_counter_if);

        return true;
    }
    else if (node_type == WHILE) {
        while_node_t *while_node = (while_node_t *) node;
        // Checks if the condition is true and jumps to label if it is.
        ++global_counter_while;
        size_t local_counter_while = global_counter_while;
        printf("WHILE_label%zu:\n", local_counter_while);
        compile_ast(while_node->condition);

        // Type casts to binary and checks operation.
        binary_node_t *binary_node = (binary_node_t *) while_node->condition;
        char operation = binary_node->op;

        if (operation == '=') {
            printf("jne ENDWHILE_label%zu\n", local_counter_while);
        }
        else if (operation == '>') {
            printf("jng ENDWHILE_label%zu\n", local_counter_while);
        }
        else if (operation == '<') {
            printf("jnl ENDWHILE_label%zu\n", local_counter_while);
        }
        compile_ast(while_node->body);

        // Loops back to the beginning of the WHILE loop.
        printf("jmp WHILE_label%zu\n", local_counter_while);

        // Breaks out of the WHILE loop.
        printf("ENDWHILE_label%zu:\n", local_counter_while);
        return true;
    }
    return false;
}