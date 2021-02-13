/*
 *	Copyright 2020 Andrey Terekhov, Maxim Menshikov, Dmitrii Davladov
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

#include "analyzer.h"
#include "codes.h"
#include "extdecl.h"
#include "keywords.h"
#include "lexer.h"
#include "tree.h"


const char *const DEFAULT_TREE = "tree.txt";
const char *const DEFAULT_NEW = "new.txt";


analyzer compiler_context_create(universal_io *const io, syntax *const sx, lexer *const lexer)
{
	analyzer context;
	context.io = io;
	context.sx = sx;
	context.lxr = lexer;

	context.sp = 0;
	context.sopnd = -1;
	context.blockflag = 1;
	context.leftansttype = -1;
	context.buf_flag = 0;
	context.error_flag = 0;
	context.line = 1;
	context.buf_cur = 0;
	context.temp_tc = 0;
	
	context.anstdispl = 0;

	return context;
}


/** Занесение ключевых слов в reprtab */
void read_keywords(analyzer *context)
{
	context->sx->keywords = 1;
	get_char(context->lxr);
	get_char(context->lxr);
	while (lex(context->lxr) != LEOF)
	{
		; // чтение ключевых слов
	}
	context->sx->keywords = 0;
}


size_t toreprtab(analyzer *context, char str[])
{
	int i;
	size_t oldrepr = REPRTAB_LEN;

	context->sx->hash = 0;

	REPRTAB_LEN += 2;
	for (i = 0; str[i] != 0; i++)
	{
		context->sx->hash += str[i];
		REPRTAB[REPRTAB_LEN++] = str[i];
	}
	context->sx->hash &= 255;

	REPRTAB[REPRTAB_LEN++] = 0;

	REPRTAB[oldrepr] = (int)context->sx->hashtab[context->sx->hash];
	REPRTAB[oldrepr + 1] = 1;
	return context->sx->hashtab[context->sx->hash] = oldrepr;
}

/** Инициализация modetab */
void init_modetab(analyzer *context)
{
	// занесение в modetab описателя struct {int numTh; int inf; }
	vector_add(&context->sx->modes, 0);
	vector_add(&context->sx->modes, MSTRUCT);
	vector_add(&context->sx->modes, 2);
	vector_add(&context->sx->modes, 4);
	vector_add(&context->sx->modes, LINT);
	vector_add(&context->sx->modes, (item_t)toreprtab(context, "numTh"));
	vector_add(&context->sx->modes, LINT);
	vector_add(&context->sx->modes, (item_t)toreprtab(context, "data"));

	// занесение в modetab описателя функции void t_msg_send(struct msg_info m)
	vector_add(&context->sx->modes, 1);
	vector_add(&context->sx->modes, MFUNCTION);
	vector_add(&context->sx->modes, LVOID);
	vector_add(&context->sx->modes, 1);
	vector_add(&context->sx->modes, 2);

	// занесение в modetab описателя функции void* interpreter(void* n)
	vector_add(&context->sx->modes, 9);
	vector_add(&context->sx->modes, MFUNCTION);
	vector_add(&context->sx->modes, LVOIDASTER);
	vector_add(&context->sx->modes, 1);
	vector_add(&context->sx->modes, LVOIDASTER);

	context->sx->start_mode = 14;
	context->line = 1;
}


/*
 *	 __     __   __     ______   ______     ______     ______   ______     ______     ______
 *	/\ \   /\ "-.\ \   /\__  _\ /\  ___\   /\  == \   /\  ___\ /\  __ \   /\  ___\   /\  ___\
 *	\ \ \  \ \ \-.  \  \/_/\ \/ \ \  __\   \ \  __<   \ \  __\ \ \  __ \  \ \ \____  \ \  __\
 *	 \ \_\  \ \_\\"\_\    \ \_\  \ \_____\  \ \_\ \_\  \ \_\    \ \_\ \_\  \ \_____\  \ \_____\
 *	  \/_/   \/_/ \/_/     \/_/   \/_____/   \/_/ /_/   \/_/     \/_/\/_/   \/_____/   \/_____/
 */


int analyze(universal_io *const io, syntax *const sx)
{
	if (!in_is_correct(io) || sx == NULL)
	{
		return -1;
	}

	universal_io temp = io_create();
	lexer lexer = create_lexer(&temp, sx);
	analyzer context = compiler_context_create(&temp, sx, &lexer);
	
	in_set_buffer(context.io, KEYWORDS);
	read_keywords(&context);
	in_clear(context.io);

	init_modetab(&context);

	io_erase(&temp);

	context.io = io;
	context.lxr->io = io;
	ext_decl(&context);

#ifndef GENERATE_TREE
	return context.error_flag || context.lxr->error_flag || !sx_is_correct(sx);
#else
	tables_and_tree(sx, DEFAULT_TREE);

	const int ret = context.error_flag || context.lxr->error_flag || !sx_is_correct(sx)
		|| tree_test(&sx->tree)
		|| tree_test_next(&sx->tree)
		|| tree_test_recursive(&sx->tree)
		|| tree_test_copy(&sx->tree);

	if (!ret)
	{
		tree_print(&sx->tree, DEFAULT_NEW);
	}
	return ret;
#endif
}
