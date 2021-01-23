/*
 *	Copyright 2019 Andrey Terekhov, Victor Y. Fadeev
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

#ifdef _MSC_VER
	#pragma comment(linker, "/STACK:268435456")
#endif

#include "compiler.h"
#include "workspace.h"


const char *name = //"../tests/robots/network/blynk.c";
// "../tests/mips/0test.c";
 "../tests/executable/types/float/test_5.c";

int main(int argc, const char *argv[])
{
	//printf(""); // Not working without using printf

	workspace ws = ws_parse_args(argc, argv);

	if (argc < 2)
	{
		ws_add_file(&ws, name);
	}
	
	return compile_to_vm(&ws);
}
