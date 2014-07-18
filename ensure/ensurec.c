#include <Python.h>

static PyObject* ensure_error = NULL;

static PyObject* repr_to_bytes(PyObject* object)
{
	PyObject* object_repr = PyObject_Repr(object);
	if (object_repr == NULL) {
		return NULL;
	}

	PyObject* object_repr_bytes = PyUnicode_AsEncodedString(object_repr, "ASCII", "strict");
	Py_DECREF(object_repr);
	return object_repr_bytes;
}

static PyObject* ensurec_check_args(PyObject* self, PyObject* args)
{
	PyObject* posargs = NULL;
	PyObject* kwargs = NULL;
	PyObject* arg_properties = NULL;
	PyObject* target_function = NULL;
	if (!PyArg_ParseTuple(args, "OOOO", &posargs, &kwargs, &arg_properties, &target_function)) {
		PyErr_SetString(PyExc_TypeError, "Must take args, kwargs, arg_properties, f");
		return NULL;
	} else if (!PyTuple_Check(posargs)) {
		PyErr_SetString(PyExc_TypeError, "posargs is not a tuple");
		return NULL;
	} else if (!PyDict_Check(kwargs)) {
		PyErr_SetString(PyExc_TypeError, "kwargs is not a dict");
		return NULL;
	} else if (!PyList_Check(arg_properties)) {
		PyErr_SetString(PyExc_TypeError, "arg_properties is not a list");
		return NULL;
	}

	const Py_ssize_t posargs_length = PyTuple_GET_SIZE(posargs);

	Py_ssize_t i;
	const Py_ssize_t num_checked_args = PyList_GET_SIZE(arg_properties);
	for (i = 0; i < num_checked_args; i++) {
		PyObject* arg_property = PyList_GET_ITEM(arg_properties, i);
		if (PyTuple_Size(arg_property) != 3) {
			PyErr_SetString(PyExc_TypeError, "arg_property must be a tuple of length 3");
			return NULL;
		}
		PyObject* pos_object = PyTuple_GET_ITEM(arg_property, 2);

		PyObject* arg_name = NULL;
		PyObject* value = NULL;
		long pos = -1;
		if (pos_object != Py_None && posargs_length > (pos = PyLong_AsLong(pos_object))) {
			value = PyTuple_GET_ITEM(posargs, pos);
		} else {
			arg_name = PyTuple_GET_ITEM(arg_property, 0);
			value = PyDict_GetItem(kwargs, arg_name);
			if (value == NULL) {
				continue;
			}
		}

		PyObject* templ = PyTuple_GET_ITEM(arg_property, 1);
		if (!PyType_Check(templ)) {
			PyErr_SetString(PyExc_TypeError, "Arg property templ is not a type");
			return NULL;
		} else if (!PyObject_TypeCheck(value, (PyTypeObject*) templ)) {
			if (arg_name == NULL) {
				//This is set if it's a kwarg, not a pos arg.
				arg_name = PyTuple_GET_ITEM(arg_property, 0);
			}
			PyObject* arg_name_bytes = PyUnicode_AsEncodedString(arg_name, "ASCII", "strict");
			if (arg_name_bytes == NULL) {
				// error converting bytes
				return NULL;
			}
			const char* actual_arg_name = PyBytes_AS_STRING(arg_name_bytes);

			PyObject* target_function_repr_bytes = repr_to_bytes(target_function);
			if (target_function_repr_bytes == NULL) {
				Py_DECREF(arg_name_bytes);
				return NULL;
			}
			const char* actual_target_function_repr = PyBytes_AS_STRING(target_function_repr_bytes);

			PyObject* templ_repr_bytes = repr_to_bytes(templ);
			if (templ_repr_bytes == NULL) {
				Py_DECREF(target_function_repr_bytes);
				Py_DECREF(arg_name_bytes);
				return NULL;
			}
			const char* actual_templ_repr = PyBytes_AS_STRING(templ_repr_bytes);

			PyErr_Format(ensure_error, "Argument %s to %s does not match annotation type %s", actual_arg_name, actual_target_function_repr, actual_templ_repr);
			Py_DECREF(templ_repr_bytes);
			Py_DECREF(target_function_repr_bytes);
			Py_DECREF(arg_name_bytes);
			return NULL;
		}
	}

	Py_RETURN_NONE;
}

static PyMethodDef ensurec_methods[] = {
	{"check_args", ensurec_check_args, METH_VARARGS, "checks function parameters for the correct annotation"},

	{NULL, NULL, 0, NULL}
};

static void ensurec_module_free(void* object)
{
	if (ensure_error) {
		Py_DECREF(ensure_error);
		ensure_error = NULL;
	}
}

static struct PyModuleDef ensurec_module = {
	PyModuleDef_HEAD_INIT,
	"ensurec",
	"ensure c companion for faster performance",
	-1,
	ensurec_methods,
	NULL,
	NULL,
	NULL,
	ensurec_module_free
};

PyMODINIT_FUNC
PyInit_ensurec(void)
{
	PyObject* ensure = PyImport_ImportModule("ensure");
	if (ensure == NULL) {
		return NULL;
	}
	PyObject* ensure_dict = PyModule_GetDict(ensure);
	ensure_error = PyMapping_GetItemString(ensure_dict, "EnsureError");
	if (ensure_error == NULL) {
		Py_DECREF(ensure);
		return NULL;
	}
	Py_INCREF(ensure_error);
	Py_DECREF(ensure);

	return PyModule_Create(&ensurec_module);
}
