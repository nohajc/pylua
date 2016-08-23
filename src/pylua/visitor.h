#ifndef visitor_h
#define visitor_h

#include <cstdio>
#include <cassert>
#include <map>
#include <string>
#include <functional>
#include <type_traits>
#include <memory>
#include <Python.h>
#include "../lua.h"
#include "../lobject.h"

using namespace std;

namespace pylua {
	typedef function<Proto*(PyObject*)> visitor_func;

	class PyAST_Visitor {
		// Table of visitors. Key: node type, Value: method pointer
		// Used for dynamic dispatch based on Python type string.
		map<string, visitor_func> v_fn_map;

	protected:
		void register_visitor_func(const string & type, visitor_func fn);

	public:
		virtual Proto * generic_visit(PyObject * node);
		virtual Proto * visit(PyObject * node);
	};

// Takes method with the given name and adds it to the v_fn_map visitor table.
// std::bind is used to bind the corresponding visitor instance pointer to the method as its first argument.
#define REG_VISITOR(name) \
	register_visitor_func(#name, bind(&remove_reference<decltype(*this)>::type::visit_ ## name, this, placeholders::_1))
}

#endif