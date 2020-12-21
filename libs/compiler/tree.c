/*
 *	Copyright 2020 Andrey Terekhov, Victor Y. Fadeev, Dmitrii Davladov
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */

#include "tree.h"
#include "errors.h"
#include "logger.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


int is_operator(const int value)
{
	return value == TFuncdef		// Declarations
		|| value == TDeclid
		|| value == TDeclarr
		|| value == TStructbeg
		|| value == TStructend

		|| value == TBegin			// Operators
		|| value == TEnd
		|| value == TPrintid
		|| value == TPrintf
		|| value == TGetid
		|| value == TGoto
		|| value == TLabel
		|| value == TIf
		|| value == TFor
		|| value == TDo
		|| value == TWhile
		|| value == TSwitch
		|| value == TCase
		|| value == TDefault
		|| value == TReturnval
		|| value == TReturnvoid
		|| value == TBreak
		|| value == TContinue

		|| value == NOP				// Lexemes
		|| value == CREATEDIRECTC;
}

int is_expression(const int value)
{
	return value == TBeginit		// Declarations
		|| value == TStructinit

		|| value == TPrint			// Operator

		|| value == TCondexpr		// Expressions
		|| value == TSelect
		|| value == TAddrtoval
		|| value == TAddrtovald
		|| value == TIdenttoval
		|| value == TIdenttovald
		|| value == TIdenttoaddr
		|| value == TIdent
		|| value == TConst
		|| value == TConstd
		|| value == TString
		|| value == TStringd
		|| value == TSliceident
		|| value == TSlice
		|| value == TCall1
		|| value == TCall2
		|| value == TExprend;
}

int is_lexeme(const int value)
{
	return (value >= 9001 && value <= 9595
		&& value != CREATEDIRECTC)
		|| value == ABSIC;
}


size_t skip_expression(const tree *const tree, size_t i, int is_block)
{
	if (i == SIZE_MAX)
	{
		return SIZE_MAX;
	}

	if (tree[i] == NOP && !is_block)
	{
		return i + 1;
	}

	if (is_operator(tree[i]))
	{
		if (!is_block)
		{
			error(NULL, tree_expression_not_block, i, tree[i]);
			return SIZE_MAX;
		}
		return i;
	}

	if (is_block)
	{
		while (tree[i] != TExprend)
		{
			i = skip_expression(tree, i, 0);
		}

		return i == SIZE_MAX ? SIZE_MAX : i + 1;
	}

	switch (tree[i++])
	{
		case TBeginit:		// ArrayInit: n + 1 потомков (размерность инициализатора, n выражений-инициализаторов)
		{
			size_t n = tree[i++];
			for (size_t j = 0; j < n; j++)
			{
				i = skip_expression(tree, i, 1);
			}
			return i == SIZE_MAX ? SIZE_MAX : i;
		}
		case TStructinit:	// StructInit: n + 1 потомков (размерность инициализатора, n выражений-инициализаторов)
		{
			size_t n = tree[i++];
			for (size_t j = 0; j < n; j++)
			{
				i = skip_expression(tree, i, 1);
			}
			return i == SIZE_MAX ? SIZE_MAX : i;
		}

		case TPrint:		// Print: 2 потомка (тип значения, выражение)
			return i + 1;

		case TCondexpr:
			return i;
		case TSelect:
			return i + 1;

		case TAddrtoval:
			return i;
		case TAddrtovald:
			return i;

		case TIdenttoval:
			return i + 1;
		case TIdenttovald:
			return i + 1;

		case TIdenttoaddr:
			return i + 1;
		case TIdent:
			i = skip_expression(tree, i + 1, 1);	// Может быть общий TExprend
			return i == SIZE_MAX ? SIZE_MAX : i - 1;

		case TConst:
			return i + 1;
		case TConstd:		// d - double
			return i + 2;

		case TString:
		{
			int n = tree[i++];
			return i + n;
		}
		case TStringd:		// d - double
		{
			int n = tree[i++];
			return i + n * 2;
		}

		case TSliceident:
			i = skip_expression(tree, i + 2, 1);			// 2 ветви потомков
			i = skip_expression(tree, i, 1);
			return i == SIZE_MAX ? SIZE_MAX : i - 1;		// Может быть общий TExprend
		case TSlice:
			i = skip_expression(tree, i + 1, 1);			// 2 ветви потомков
			i = skip_expression(tree, i, 1);
			return i == SIZE_MAX ? SIZE_MAX : i - 1;		// Может быть общий TExprend

		case TCall1:
			return i + 1;
		case TCall2:
			i = skip_expression(tree, i + 1, 1);
			return i == SIZE_MAX ? SIZE_MAX : i - 1;		// Может быть общий TExprend

		case TExprend:
			if (is_block)
			{
				error(NULL, tree_expression_texprend, i - 1, tree[i - 1]);
				return SIZE_MAX;
			}
			return i - 1;
	}

	if (is_lexeme(tree[i - 1]))
	{
		while (!is_expression(tree[i]))
		{
			if (is_operator(tree[i]))
			{
				return i;
			}

			i++;
		}

		return i;
	}

	error(NULL, tree_expression_unknown, i - 1, tree[i - 1]);
	return SIZE_MAX;
}

node node_operator(tree *const tree, size_t *i)
{
	node nd;
	if (*i == SIZE_MAX)
	{
		nd.tree = NULL;
		return nd;
	}

	nd.tree = tree;
	nd.type = *i;
	nd.argv = nd.type + 1;
	nd.argc = 0;
	nd.children = nd.type + 1;
	nd.amount = 0;

	*i += 1;
	switch (nd.tree[nd.type])
	{
		case TFuncdef:		// Funcdef: 2 потомка (ссылка на identab, тело функции)
			nd.argc = 2;
			nd.children = nd.type + nd.argc + 1;
			nd.amount = 1;

			*i += nd.argc;
			node_operator(tree, i);
			break;
		case TDeclid:		// IdentDecl: 6 потомков (ссылка на identab, тип элемента, размерность, all, usual, выражение-инициализатор (может не быть))
			nd.argc = 7;
			nd.children = nd.type + nd.argc + 1;
			nd.amount = 1;

			*i += nd.argc;
			*i = skip_expression(tree, *i, 1);
			break;
		case TDeclarr:		// ArrayDecl: n + 2 потомков (размерность массива, n выражений-размеров, инициализатор (может не быть))
		{
			nd.argc = 1;
			nd.children = nd.type + nd.argc + 1;
			nd.amount = node_get_arg(&nd, 0) + 1;

			*i += nd.argc;
			for (size_t j = 0; j < nd.amount; j++)
			{
				*i = skip_expression(tree, *i, 1);
			}

			node_operator(tree, i);
		}
		break;
		case TStructbeg:	// StructDecl: n + 2 потомков (размерность структуры, n объявлений полей, инициализатор (может не быть))
			nd.argc = 1;
			nd.children = nd.type + nd.argc + 1;
			
			*i += nd.argc;
			while (tree[*i] != TStructend)
			{
				node_operator(tree, i);
				nd.amount++;
			}

			if (*i != SIZE_MAX)
			{
				*i += 2;
			}
			break;

		case TBegin:
			nd.children = nd.type + nd.argc + 1;

			while (tree[*i] != TEnd)
			{
				node_operator(tree, i);
				nd.amount++;
			}
			
			if (*i != SIZE_MAX)
			{
				*i += 1;
			}
			break;

		case TPrintid:		// PrintID: 2 потомка (ссылка на reprtab, ссылка на identab)
			nd.argc = 1;
			nd.children = nd.type + nd.argc + 1;
			nd.amount = 1;

			*i += nd.argc;
			*i = skip_expression(tree, *i, 1);
			break;
		case TPrintf:		// Printf: n + 2 потомков (форматирующая строка, число параметров, n параметров-выражений)
		case TGetid:		// GetID: 1 потомок (ссылка на identab)
							// Scanf: n + 2 потомков (форматирующая строка, число параметров, n параметров-ссылок на identab)
		
		case TGoto:			// Goto: 1 потомок (ссылка на identab)
			nd.argc = 1;

			*i += nd.argc;
			break;
		case TLabel:		// LabeledStatement: 2 потомка (ссылка на identab, тело оператора)
			nd.argc = 1;
			nd.children = nd.type + nd.argc + 1;
			nd.amount = 1;

			*i += nd.argc;
			node_operator(tree, i);
			break;

		case TIf:			// If: 3 потомка (условие, тело-then, тело-else) - ветка else присутствует не всегда, здесь предлагается не добавлять лишних узлов-индикаторов, а просто проверять, указывает на 0 или нет
		{
			nd.argc = 1;
			nd.children = nd.type + nd.argc + 1;
			nd.amount = node_get_arg(&nd, 0) != 0 ? 3 : 2;

			*i += nd.argc;
			*i = skip_expression(tree, *i, 1);
			node_operator(tree, i);
			if (node_get_arg(&nd, 0) && *i != SIZE_MAX)
			{
				*i = node_get_arg(&nd, 0);
				node_operator(tree, i);
			}
		}
		break;

		case TFor:			// For: 4 потомка (выражение или объявление, условие окончания, выражение-инкремент, тело цикла); - первые 3 ветки присутствуют не всегда,  здесь также предлагается не добавлять лишних узлов-индикаторов, а просто проверять, указывает на 0 или нет
		{
			nd.argc = 4;
			nd.children = nd.type + nd.argc + 1;
			nd.amount = 1;

			size_t var = node_get_arg(&nd, 0);
			if (var != 0)
			{
				if (skip_expression(tree, var, 1) == SIZE_MAX)
				{
					nd.tree = NULL;
					return nd;
				}
			}

			size_t cond = node_get_arg(&nd, 1);
			if (cond != 0)
			{
				if (skip_expression(tree, cond, 1) == SIZE_MAX)
				{
					nd.tree = NULL;
					return nd;
				}
			}

			size_t inc = node_get_arg(&nd, 2);
			if (inc != 0)
			{
				if (skip_expression(tree, inc, 1) == SIZE_MAX)
				{
					nd.tree = NULL;
					return nd;
				}
			}

			*i = node_get_arg(&nd, 3);
			node_operator(tree, i);
		}
		break;
		case TDo:			// Do: 2 потомка (тело цикла, условие)
			nd.children = nd.type + nd.argc + 1;
			nd.amount = 2;

			node_operator(tree, i);
			*i = skip_expression(tree, *i, 1);
			break;
		case TWhile:		// While: 2 потомка (условие, тело цикла)

		case TSwitch:		// Switch: 2 потомка (условие, тело оператора)
		case TCase:			// Case: 2 потомка (условие, тело оператора)
			nd.children = nd.type + nd.argc + 1;
			nd.amount = 2;

			*i = skip_expression(tree, *i, 1);
			node_operator(tree, i);
			break;
		case TDefault:		// Default: 1 потомок (тело оператора)
			nd.children = nd.type + nd.argc + 1;
			nd.amount = 1;

			node_operator(tree, i);
			break;

		case TReturnval:	// ReturnValue: 2 потомка (тип значения, выражение)
			nd.argc = 1;
			nd.children = nd.type + nd.argc + 1;
			nd.amount = 1;

			*i += nd.argc;
			*i = skip_expression(tree, *i, 1);
			break;
		case TReturnvoid:	// ReturnVoid: нет потомков
		case TBreak:		// Break: нет потомков
		case TContinue:		// Continue: нет потомков

		case NOP:			// NoOperation: 0 потомков
			break;
		case CREATEDIRECTC:
			nd.children = nd.type + nd.argc + 1;

			while (tree[*i] != EXITC)
			{
				node_operator(tree, i);
				nd.amount++;
			}
			
			if (*i != SIZE_MAX)
			{
				*i += 1;
			}
			break;

		default:
			(*i)--;
			if (!is_expression(tree[*i]) && !is_lexeme(tree[*i]))
			{
				warning(NULL, tree_operator_unknown, *i, tree[*i]);
			}

			*i = skip_expression(tree, *i, 1);	// CompoundStatement: n + 1 потомков (число потомков, n узлов-операторов)
												// ExpressionStatement: 1 потомок (выражение)
			
			nd.children = nd.type + nd.argc + 1;
			nd.amount = 1;
			nd.type = *i - 1;
	}

	return nd;
}

size_t skip_operator(tree *const tree, size_t i)
{
	node nd = node_operator(tree, &i);
	return node_is_correct(&nd) ? i : SIZE_MAX;
}


void tree_print(syntax *const sx)
{
	node nd = node_get_root(sx);
	printf("-=root=-\nnum\t%zi\nargc\t%zi\n", nd.amount, nd.argc);
}


/*
 *	 __     __   __     ______   ______     ______     ______   ______     ______     ______
 *	/\ \   /\ "-.\ \   /\__  _\ /\  ___\   /\  == \   /\  ___\ /\  __ \   /\  ___\   /\  ___\
 *	\ \ \  \ \ \-.  \  \/_/\ \/ \ \  __\   \ \  __<   \ \  __\ \ \  __ \  \ \ \____  \ \  __\
 *	 \ \_\  \ \_\\"\_\    \ \_\  \ \_____\  \ \_\ \_\  \ \_\    \ \_\ \_\  \ \_____\  \ \_____\
 *	  \/_/   \/_/ \/_/     \/_/   \/_____/   \/_/ /_/   \/_/     \/_/\/_/   \/_____/   \/_____/
 */


node node_get_root(syntax *const sx)
{
	node nd;
	if (sx == NULL)
	{
		nd.tree = NULL;
		return nd;
	}

	nd.tree = sx->tree;
	nd.type = SIZE_MAX;
	nd.argv = 0;
	nd.argc = 0;
	nd.children = 0;
	nd.amount = 0;

	size_t i = 0;
	while (i != SIZE_MAX && nd.tree[i] != TEnd)
	{
		i = skip_operator(nd.tree, i);
		nd.amount++;
	}

	if (i == SIZE_MAX)
	{
		nd.tree = NULL;
	}

	return nd;
}

node node_get_child(node *const nd, const size_t index)
{
	node child;
	if (!node_is_correct(nd) || index > nd->amount)
	{
		child.tree = NULL;
		return child;
	}

	size_t i = nd->children;
	for (size_t num = 0; num < index; num++)
	{
		if (nd->type == SIZE_MAX || node_get_type(nd))
		{
			i = skip_operator(nd->tree, i);
		}
		else
		{
			i = skip_expression(nd->tree, i, 1);
		}
	}

	child.tree = nd->tree;
	/*hild->type = i;
	return node_init(child);*/
	return child;
}


size_t node_get_amount(const node *const nd)
{
	return node_is_correct(nd) ? nd->amount : 0;
}

int node_get_type(const node *const nd)
{
	return node_is_correct(nd) ? nd->tree[nd->type] : INT_MAX;
}

int node_get_arg(const node *const nd, const size_t index)
{
	return node_is_correct(nd) && index < nd->argc ? nd->tree[nd->argv + index] : INT_MAX;
}


int node_is_correct(const node *const nd)
{
	return nd != NULL && nd->tree != NULL;
}


int tree_test(syntax *const sx)
{
	//tree_print(sx);
	// Тестирование функций
	size_t i = 0;
	while (i != SIZE_MAX && (int)i < sx->tc - 1)
	{
		i = skip_operator(sx->tree, i);
	}

	if (i == SIZE_MAX)
	{
		return -1;
	}

	if (sx->tree[i] == TEnd)
	{
		return 0;
	}

	error(NULL, tree_no_tend);
	return -1;
}