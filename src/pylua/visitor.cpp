#include "visitor.h"
#include "pyobjw.h"

namespace pylua {
	void PyAST_Visitor::register_visitor_func(const string & type, visitor_func fn) {
		v_fn_map[type] = fn;
	}

	void * PyAST_Visitor::generic_visit(PyObject * node, void * ud) {
		assert(node != NULL);
		PyObjW nodew(node);

		PyObjW node_fields = nodew["_fields"];
		if (node_fields == PyObjW::None) {
			return NULL;
		}
		
		node_fields.each([this, &nodew, ud](PyObject * field_name) {
			PyObjW field_value = nodew[field_name];

			if (PyList_Check(field_value)) {
				field_value.each([this, ud](PyObject * lst_item) {
					visit(lst_item, ud);
				});
			}
			else {
				visit(field_value, ud);
			}
		});

		/*PyObject * node_fields_iter = PyObject_GetIter(node_fields);
		PyObject * field_name;
		
		while (field_name = PyIter_Next(node_fields_iter)) {
			PyObject * field_value = PyObject_GetAttr(node, field_name);

			if (PyList_Check(field_value)) {
				PyObject * lst_iter = PyObject_GetIter(field_value);
				PyObject * lst_item;

				while (lst_item = PyIter_Next(lst_iter)) {
					visit(lst_item);
					Py_DECREF(lst_item);
				}
				Py_DECREF(lst_iter);

				if (PyErr_Occurred()) {} // TODO
			}
			else {
				visit(field_value);
			}

			Py_DECREF(field_value);
			Py_DECREF(field_name);
		}

		Py_DECREF(node_fields_iter);
		Py_DECREF(node_fields);*/
		
		// Generic visitor does not know what to return from a subtree.
		// That's the job for the specific implementation provided by an inherited class.
		return NULL;
	}

	void * PyAST_Visitor::visit(PyObject * node, void * ud) {
		/*PyObject * node_class = PyObject_GetAttrString(node, "__class__");
		PyObject * node_class_name = PyObject_GetAttrString(node_class, "__name__");*/

		PyObjW node_class = PyObjW(node)["__class__"];
		PyObjW node_class_name = node_class["__name__"];
		string key = PyUnicode_AsUTF8(node_class_name);

		/*Py_DECREF(node_class_name);
		Py_DECREF(node_class);*/

		printf("Node type: %s\n", key.c_str());

		auto it = v_fn_map.find(key);

		if (it == v_fn_map.end()) {
			return generic_visit(node, ud);
		}
		return it->second(node, ud);
	}
}
