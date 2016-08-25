#include <string>
#include <iostream>
#include <fstream>
#include "codegen.h"
#include "visitor.h"
#include "pyobjw.h"
#include "../lua.h"
#include "../ldo.h"
#include "../lgc.h"
#include "../lfunc.h"
#include "../llex.h"
#include "../lstring.h"
#include "../lparser.h"
#include "../lcode.h"

namespace pylua {
	using namespace std;

	class PyAST_Codegen: public PyAST_Visitor {
		lua_State * L;
		string chunkname;

		class LexEmul {
			lua_State * L;
		public:
			LexEmul(lua_State * ls) {
				L = ls;
			}
			struct LexState state; // TODO: We have to maintain LexState even though we don't parse.
					
			void setinput(TString * source) { // Equivalent of luaX_setinput
				state.decpoint = '.';
				state.L = L; // TODO: remove?
				//state.lookahead.token = TK_EOS;
				state.z = NULL;
				state.fs = NULL;
				state.linenumber = 1;
				state.lastline = 1;
				state.source = source;
			}
		};

		LexEmul lex;

	public:
		PyAST_Codegen(lua_State * ls, const char * chkname) : lex(ls) {
			REG_VISITOR(Module);
			REG_VISITOR(Expr);
			REG_VISITOR(If);

			L = ls;
			chunkname = chkname;
		}

		void * visit(PyObject * node, void * ud = NULL) {
			PyObjW nodew(node);
			PyObjW lineno = nodew["lineno"];
			if (lineno != PyObjW::None && PyLong_Check(lineno)) {
				lex.state.linenumber = PyLong_AsLong(lineno); // Set line number in the simulated lexer state
			}
			return PyAST_Visitor::visit(node, ud);
		}

		void * visit_Body(PyObject * node, void * ud = NULL) { // Called directly - no need to register, equivalent of chunk()
			PyObjW node_body(node);
			struct LexState * ls = &lex.state;
			assert(PyList_Check(node_body));

			enterlevel(ls);

			node_body.each([this, ls, ud](PyObject * node_sttmnt) {
				visit(node_sttmnt, ud);
				lua_assert(ls->fs->f->maxstacksize >= ls->fs->freereg &&
				ls->fs->freereg >= ls->fs->nactvar);
				ls->fs->freereg = ls->fs->nactvar;  // free registers
			});

			leavelevel(ls);

			return NULL;
		}

		void * visit_Block(PyObject * node, void * ud = NULL) { // Called directly - no need to register
			FuncState *fs = lex.state.fs;
			BlockCnt bl;
			enterblock(fs, &bl, 0);

			void * ret = visit_Body(node);

			lua_assert(bl.breaklist == NO_JUMP);
			leaveblock(fs);

			return ret;
		}

		void * visit_If(PyObject * node, void * ud) { // Equivalent of ifstat()
			PyObjW nodew(node);
			FuncState * fs = lex.state.fs;
			int flist;
			int escapelist = NO_JUMP;

			// test_then_block(ls) equivalent inlined
			PyObjW node_test = nodew["test"];
			assert(node_test != PyObjW::None);
			flist = cast(int, visit(node_test)); // cond(ls)

			PyObjW node_body = nodew["body"];
			assert(node_body != PyObjW::None);
			visit_Block(node_body); // block(ls);

			/*
			while (ls->t.token == TK_ELSEIF) {
				luaK_concat(fs, &escapelist, luaK_jump(fs));
				luaK_patchtohere(fs, flist);
				flist = test_then_block(ls);  // ELSEIF cond THEN block
			}
			if (ls->t.token == TK_ELSE) {
				luaK_concat(fs, &escapelist, luaK_jump(fs));
				luaK_patchtohere(fs, flist);
				luaX_next(ls);  // skip ELSE (after patch, for correct line info)
				block(ls);  // `else' part
			}
			else
				luaK_concat(fs, &escapelist, flist);
			luaK_patchtohere(fs, escapelist);
			check_match(ls, TK_END, TK_IF, line);
			*/

			PyObjW node_else = nodew["orelse"];
			assert(PyList_Check(node_else));

			while (node_else[0].type() == "If") { // Has nested if (elif)
				nodew = node_else[0];
				luaK_concat(fs, &escapelist, luaK_jump(fs));
				luaK_patchtohere(fs, flist);
				
				node_test = nodew["test"];
				assert(node_test != PyObjW::None);
				flist = cast(int, visit(node_test)); // cond(ls)

				node_body = nodew["body"];
				assert(node_body != PyObjW::None);
				visit_Block(node_body); // block(ls);

				node_else = nodew["orelse"];
				assert(PyList_Check(node_else));
			}

			if (PyList_Size(node_else) > 0) { // Has else part
				luaK_concat(fs, &escapelist, luaK_jump(fs));
				luaK_patchtohere(fs, flist);
				visit_Block(node_else); // block(ls);
			}
			else {
				luaK_concat(fs, &escapelist, flist);
			}
			luaK_patchtohere(fs, escapelist);

			return NULL;
		}

		void * visit_Module(PyObject * node, void * ud) {
			PyObjW nodew(node);
			//struct LexState * ls = &lex.state;
			printf("| MODULE VISITOR |\n");

			//enterlevel(ls);

			PyObjW node_body = nodew["body"];
			assert(node_body != PyObjW::None);

			visit_Body(node_body);

			//leavelevel(&lex.state);

			return NULL;
		}

		void * visit_Expr(PyObject * node, void * ud) {
			printf("| EXPR VISITOR |\n");
			return generic_visit(node, ud);
		}

		virtual void * generic_visit(PyObject * node, void * ud = NULL) {
			//PyObject * node_vars = PyObject_GetAttrString(node, "__dict__");
			PyObjW node_vars = PyObjW(node)["__dict__"];
			if (node_vars != PyObjW::None) {
				printf("\t");
				PyObject_Print(node_vars, stdout, 0);
				printf("\n");
			}

			return PyAST_Visitor::generic_visit(node, ud);
		}

		Proto * start(PyObject * ast) { // Equivalent of luaY_parser()
			struct FuncState funcstate;
			lex.setinput(luaS_new(L, chunkname.c_str()));
			open_func(&lex.state, &funcstate);
			funcstate.f->is_vararg = VARARG_ISVARARG;  /* main func. is always vararg */

			visit(ast);

			close_func(&lex.state);
			lua_assert(funcstate.prev == NULL);
			lua_assert(funcstate.f->nups == 0);
			lua_assert(lexstate.fs == NULL);
			return funcstate.f;
		}
	};

	extern "C" {
		PyObject * gen_AST_from_file(const char * fname) {
			shared_ptr<istream> f;
			if (!fname) { // No filename given - read from stdin
				f.reset(&cin, [](...){}); // pass empty deleter to reset (cin must not be deleted)
			}
			else {
				f.reset(new ifstream(fname));
			}
		
			char * str;
			PyObject * ast;

			if (!f->good()) {
				return NULL;
			}

			f->seekg(0, ios::end);
			unsigned size = (unsigned)f->tellg();
			f->seekg(0);

			str = new char[size];
			f->read(str, size);

			PyCompilerFlags cf;
			cf.cf_flags = PyCF_ONLY_AST | PyCF_SOURCE_IS_UTF8;
			ast = Py_CompileStringFlags(str, fname, Py_file_input, &cf);
			assert(ast != NULL);

			delete [] str;
			return ast;
		}

		typedef struct {
			PyObject * ast;
			const char * chunkname;
		} data_t;

		void codegen_run(lua_State * L, void * ud) {
			data_t * data = cast(data_t *, ud);
			PyAST_Codegen cg(L, data->chunkname);
			
			// Taken from ldo.c, f_parser function
			int i;
			Proto *tf;
			Closure *cl;
			
			luaC_checkGC(L);

			//tf = ((c == LUA_SIGNATURE[0]) ? luaU_undump : luaY_parser)(L, p->z, &p->buff, p->name);
			tf = cg.start(data->ast);

			cl = luaF_newLclosure(L, tf->nups, hvalue(gt(L)));
			cl->l.p = tf;
			for (i = 0; i < tf->nups; i++)  /* initialize eventual upvalues */
			cl->l.upvals[i] = luaF_newupval(L);
			setclvalue(L, L->top, cl);
			incr_top(L);
		}

		int protected_codegen_run(lua_State * L, PyObject * ast, const char * chunkname) {
			data_t data;
			data.ast = ast;
			data.chunkname = chunkname;

			return luaD_pcall(L, codegen_run, &data, savestack(L, L->top), L->errfunc);
		}
	}

	class PyInitializer {
	public:
		PyInitializer() {
			Py_Initialize();
		}

		~PyInitializer() {
			Py_Finalize();
		}
	};

	static PyInitializer pi;
}
