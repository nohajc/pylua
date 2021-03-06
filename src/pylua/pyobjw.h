#ifndef pyobjw_h
#define pyobjw_h

#include <Python.h>
#include <string>

namespace pylua {
	class PyObjW {
		PyObject * obj;
		bool has_ref;
	public:
		static PyObjW None;

		PyObjW(PyObject * o = NULL, bool nref = false) {
			obj = o;
			has_ref = nref;
		}

		static PyObjW NewRef(PyObject * o) {
			return PyObjW(o, true);
		}

		~PyObjW(){
			if (has_ref) {
				Py_DECREF(obj);
			}
		}

		bool operator ==(PyObjW & other) {
			return obj == other.obj;
		}

		bool operator !=(PyObjW & other) {
			return !(*this == other);
		}

		operator PyObject*() {
			return obj;
		}

		std::string type() {
			PyObjW cls = (*this)["__class__"];
			assert(cls != None);
			PyObjW nam = cls["__name__"];
			assert(nam != None);
			return PyUnicode_AsUTF8(nam);
		}

		PyObjW operator[](const char * attrname) {
			PyObject * attr = PyObject_GetAttrString(obj, attrname);
			if (!attr) return None;

			return NewRef(attr);
		}

		PyObjW operator[](PyObject * attrname) {
			PyObject * attr = PyObject_GetAttr(obj, attrname);
			if (!attr) return None;

			return NewRef(attr);
		}

		PyObjW operator[](int n) {
			PyObject * item = PyList_GetItem(obj, n);
			if (!item) return None;

			return item;
		}

		template<typename F>
		void each(F func) {
			PyObject * iter = PyObject_GetIter(obj);
			assert(iter != NULL);
			PyObject * elem;

			while (elem = PyIter_Next(iter)) {
				func(elem);
				Py_DECREF(elem);
			}

			Py_DECREF(iter);
		}
	};
}

#endif