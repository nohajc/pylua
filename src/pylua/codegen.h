#ifndef codegen_h
#define codegen_h

#include <Python.h>
#include "../lua.h"

#ifdef __cplusplus
extern "C" {
#endif

	PyObject * gen_AST_from_file(const char * fname);
	int protected_codegen_run(lua_State * L, PyObject * ast, const char * chunkname);
	int codegen_run(lua_State * L, void * ud);

#ifdef __cplusplus
}
#endif

#endif
