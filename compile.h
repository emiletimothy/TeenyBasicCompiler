#ifndef COMPILE_H
#define COMPILE_H

#include <stdbool.h>

#include "ast.h"

/**
 * Prints x86-64 assembly code that implements the given TeenyBASIC statement.
 * This function will be called on each statement in the TeenyBASIC program in order.
 *
 * @param node the statement to compile (either a PRINT, LET, label, GOTO, or IF)
 * @return true iff compilation succeeds
 */
bool compile_ast(node_t *node);

#endif /* COMPILE_H */
