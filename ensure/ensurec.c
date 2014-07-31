#include <Python.h>
#include "structmember.h"

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

static PyObject* ensurec_check_args_and_call4(PyObject* posargs, PyObject* kwargs, PyObject* arg_properties, PyObject* target_function) {
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
			if (kwargs != NULL) {
				value = PyDict_GetItem(kwargs, arg_name);
			}
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

	return PyObject_Call(target_function, posargs, kwargs);
}

static PyObject* ensurec_check_args_and_call(PyObject* self, PyObject* args)
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

	return ensurec_check_args_and_call4(posargs, kwargs, arg_properties, target_function);
}

typedef struct {
	PyObject_HEAD
	PyObject* arg_properties;
	PyObject* target_function;
} ensurec_WrappedFunctionObject;

static int WrappedFunction_init(ensurec_WrappedFunctionObject* self, PyObject* args, PyObject** kwargs)
{
	PyObject* arg_properties = PyTuple_GetItem(args, 0);
	if (arg_properties == NULL) {
		return -1;
	} else if (!PyList_Check(arg_properties)) {
		PyErr_SetString(PyExc_TypeError, "arg_properties is not a list");
		return -1;
	}

	PyObject* target_function = PyTuple_GetItem(args, 1);
	if (target_function == NULL) {
		return -1;
	} else if (!PyCallable_Check(target_function)) {
		PyErr_SetString(PyExc_TypeError, "target function isn not callable");
		return -1;
	}

	Py_INCREF(arg_properties);
	self->arg_properties = arg_properties;
	Py_INCREF(target_function);
	self->target_function = target_function;

	return 0;
}

static void WrappedFunction_dealloc(ensurec_WrappedFunctionObject* self)
{
	Py_XDECREF(self->target_function);
	Py_XDECREF(self->arg_properties);
}

static PyObject* WrappedFunction_call(ensurec_WrappedFunctionObject* self, PyObject* args, PyObject* kwargs)
{
	return ensurec_check_args_and_call4(args, kwargs, self->arg_properties, self->target_function);
}

static PyObject* WrappedFunction_getattr(ensurec_WrappedFunctionObject* self, const char* attr_name)
{
	return PyObject_GetAttrString(self->target_function, attr_name);
}

static int WrappedFunction_setattr(ensurec_WrappedFunctionObject* self, const char* attr_name, PyObject* attr_value)
{
	return PyObject_SetAttrString(self->target_function, attr_name, attr_value);
}

static PyObject* WrappedFunction_repr(ensurec_WrappedFunctionObject* self)
{
	return PyObject_Repr(self->target_function);
}

static PyObject* WrappedFunction_str(ensurec_WrappedFunctionObject* self)
{
	return PyObject_Str(self->target_function);
}

static PyObject* WrappedFunction_descr_get(PyObject* self, PyObject* obj, PyObject* objtype)
{
	return PyMethod_New(self /* func to call is us */, obj /* want to call with an instance of obj */);
}

static PyMemberDef WrappedFunction_members[] = {
	{"arg_properties", T_OBJECT_EX, offsetof(ensurec_WrappedFunctionObject, arg_properties), READONLY},
	{"f", T_OBJECT_EX, offsetof(ensurec_WrappedFunctionObject, target_function), READONLY},
	{NULL},
};

static PyTypeObject ensurec_WrappedFunctionType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "ensurec.WrappedFunction",
	.tp_basicsize = sizeof(ensurec_WrappedFunctionObject),
	.tp_dealloc = (destructor) WrappedFunction_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_doc = "Wraps a function to ensure that the arguments passed / return meet the annotation",
	.tp_new = PyType_GenericNew,
	.tp_getattr = (getattrfunc) WrappedFunction_getattr,
	.tp_setattr = (setattrfunc) WrappedFunction_setattr,
	.tp_repr = (reprfunc) WrappedFunction_repr,
	.tp_call = (ternaryfunc) WrappedFunction_call,
	.tp_str = (reprfunc) WrappedFunction_str,
	.tp_members = WrappedFunction_members,
	.tp_descr_get = WrappedFunction_descr_get,
	.tp_init = (initproc) WrappedFunction_init,
};

typedef struct {
	ensurec_WrappedFunctionObject base;
	PyObject* return_templ;
} ensurec_WrappedFunctionReturnObject;

static int WrappedFunctionReturn_init(ensurec_WrappedFunctionReturnObject* self, PyObject* args, PyObject** kwargs)
{
	if (WrappedFunction_init(&self->base, args, kwargs) < 0) {
		return -1;
	}

	PyObject* return_templ = PyTuple_GetItem(args, 2);
	if (return_templ == NULL) {
		return -1;
	} else if (!PyType_Check(return_templ)) {
		PyErr_SetString(PyExc_TypeError, "return_templ is not a type");
		return -1;
	}

	Py_INCREF(return_templ);
	self->return_templ = return_templ;

	return 0;
}

static void WrappedFunctionReturn_dealloc(ensurec_WrappedFunctionReturnObject* self)
{
	Py_XDECREF(self->return_templ);
	WrappedFunction_dealloc(&self->base);
}

static PyObject* WrappedFunctionReturn_call(ensurec_WrappedFunctionReturnObject* self, PyObject* args, PyObject* kwargs)
{
	PyObject* value = WrappedFunction_call(&self->base, args, kwargs);
	if (value != NULL) {
		if (!PyObject_TypeCheck(value, (PyTypeObject*) self->return_templ)) {
			PyObject* target_function_repr_bytes = repr_to_bytes(self->base.target_function);
			if (target_function_repr_bytes == NULL) {
				return NULL;
			}
			const char* actual_target_function_repr = PyBytes_AS_STRING(target_function_repr_bytes);

			PyObject* templ_repr_bytes = repr_to_bytes(self->return_templ);
			if (templ_repr_bytes == NULL) {
				Py_DECREF(target_function_repr_bytes);
				return NULL;
			}
			const char* actual_templ_repr = PyBytes_AS_STRING(templ_repr_bytes);

			PyErr_Format(ensure_error, "Return value of %s does not match annotation type %s", actual_target_function_repr, actual_templ_repr);
			Py_DECREF(templ_repr_bytes);
			Py_DECREF(target_function_repr_bytes);
			return NULL;
		}
	}

	return value;
}

static PyTypeObject ensurec_WrappedFunctionReturnType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "ensurec.WrappedFunctionReturn",
	.tp_basicsize = sizeof(ensurec_WrappedFunctionReturnObject),
	.tp_dealloc = (destructor) WrappedFunctionReturn_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DEFAULT,
	.tp_doc = "Wraps a function to ensure that the arguments passed / return meet the annotation",
	.tp_new = PyType_GenericNew,
	.tp_call = (ternaryfunc) WrappedFunctionReturn_call,
	.tp_init = (initproc) WrappedFunctionReturn_init,
	.tp_base = &ensurec_WrappedFunctionType,
};

static PyMethodDef ensurec_methods[] = {
	{"check_args_and_call", ensurec_check_args_and_call, METH_VARARGS, "checks function parameters for the correct annotation and calls it"},

	{NULL, NULL, 0, NULL}
};

static void ensurec_module_free(void* object)
{
	Py_DECREF(&ensurec_WrappedFunctionReturnType);
	Py_DECREF(&ensurec_WrappedFunctionType);
	Py_DECREF(ensure_error);
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
	if (PyType_Ready(&ensurec_WrappedFunctionType) < 0) {
		PyErr_SetString(PyExc_RuntimeError, "Could not register WrappedFunction type");
		return NULL;
	}

	if (PyType_Ready(&ensurec_WrappedFunctionReturnType) < 0) {
		PyErr_SetString(PyExc_RuntimeError, "Could not register WrappedFunctionReturn type");
		return NULL;
	}

	PyObject* ensure = PyImport_ImportModule("ensure");
	if (ensure == NULL) {
		return NULL;
	}
	PyObject* ensure_dict = PyModule_GetDict(ensure);
	ensure_error = PyMapping_GetItemString(ensure_dict, "EnsureError");
	Py_DECREF(ensure);
	if (ensure_error == NULL) {
		return NULL;
	}

	PyObject* module = PyModule_Create(&ensurec_module);
	if (module == NULL) {
		return NULL;
	}

	Py_INCREF(ensure_error);
	Py_INCREF(&ensurec_WrappedFunctionReturnType);
	Py_INCREF(&ensurec_WrappedFunctionType);
	PyModule_AddObject(module, "WrappedFunction", (PyObject *)&ensurec_WrappedFunctionType);
	PyModule_AddObject(module, "WrappedFunctionReturn", (PyObject *)&ensurec_WrappedFunctionReturnType);

	return module;
}
