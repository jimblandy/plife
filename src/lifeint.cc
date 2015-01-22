#include <Python.h>
#include <cstdio>
#include <cstring>

#include "life.hh"
#include "def.hh"

namespace life {
//---------------------------------------------------------------------------
static void add_cell (PyObject *list, long int x, long int y)
{
	PyList_Append (list, PyInt_FromLong (x));
	PyList_Append (list, PyInt_FromLong (y));
}	
//---------------------------------------------------------------------------
static PyObject *lifeint_set_rule (PyObject *self, PyObject *args)
{
	char *rule_string = NULL;

	if (PyArg_ParseTuple (args, "z", &rule_string))
		set_rule_from_string (rule_string);

	Py_INCREF (Py_None);
	return Py_None;
}
//---------------------------------------------------------------------------
static PyObject *lifeint_parse (PyObject *self, PyObject *args)
{
	char *s;

	long int x0, y0, axx, axy, ayx, ayy;

	if (!PyArg_ParseTuple (args, "zllllll", &s, &x0, &y0, &axx, &axy, &ayx, &ayy))
		return NULL;

	PyObject *list = PyList_New (0);

	long int x = 0, y = 0;

	if (strchr (s, '*')) {
		// parsing 'visual' format
		int c;
		while ((c = *s++))
			switch (c) {
			case '\n': if (x) { x = 0; y++; } break;
			case '.': x++; break;
			case '*':
				add_cell (list, x0 + x * axx + y * axy, y0 + x * ayx + y * ayy);
				x++;
				break;
			}
	} else {
		// parsing 'RLE' format
		int c;
		int prefix = 0;
		bool done = false;
		while ((c = *s++) && !done)
			if (isdigit (c))
				prefix = 10 * prefix + (c - '0');
			else {
				prefix += (prefix == 0);
				switch (c) {
				case '!': done = true; break;
				case '$': x = 0; y += prefix; break;
				case 'b': x += prefix; break;
				case 'o':
					for (int k = 0; k < prefix; k++, x++)
						add_cell (list, x0 + x * axx + y * axy, y0 + x * ayx + y * ayy);
					break;
				}
				prefix = 0;
			}
	}

	return list;
}
//---------------------------------------------------------------------------
static PyObject *lifeint_transform (PyObject *self, PyObject *args)
{
	long int x0, y0, axx, axy, ayx, ayy;

	PyObject *list;
	PyObject *new_list;

	if (!PyArg_ParseTuple (args, "O!llllll", &PyList_Type, &list, &x0, &y0, &axx, &axy, &ayx, &ayy))
		return NULL;

	new_list = PyList_New (0);

	int num_cells = PyList_Size (list) / 2;
	for (int n = 0; n < num_cells; n++) {
		long int x = PyInt_AsLong (PyList_GetItem (list, 2 * n));
		long int y = PyInt_AsLong (PyList_GetItem (list, 2 * n + 1));

		add_cell (new_list, x0 + x * axx + y * axy, y0 + x * ayx + y * ayy);
	}

	return new_list;
}
//---------------------------------------------------------------------------
static PyObject *lifeint_put (PyObject *self, PyObject *args)
{
	long int x0, y0, axx, axy, ayx, ayy;
	PyObject *list;

	if (!PyArg_ParseTuple (args, "O!llllll", &PyList_Type, &list, &x0, &y0, &axx, &axy, &ayx, &ayy))
		return NULL;

	int num_cells = PyList_Size (list) / 2;
	for (int n = 0; n < num_cells; n++) {
		long int x = PyInt_AsLong (PyList_GetItem (list, 2 * n));
		long int y = PyInt_AsLong (PyList_GetItem (list, 2 * n + 1));

		printf ("%ld %ld\n", x0 + x * axx + y * axy, y0 + x * ayx + y * ayy);
	}

	Py_INCREF (Py_None);
	return Py_None;
}
//---------------------------------------------------------------------------
static PyObject *lifeint_evolve (PyObject *self, PyObject *args)
{
	int N = 0;

	PyObject *given_list;
	PyObject *evolved_list;

	if (!PyArg_ParseTuple (args, "O!i", &PyList_Type, &given_list, &N))
		return NULL;

	field f;

	int num_cells = PyList_Size (given_list) / 2;
	for (int n = 0; n < num_cells; n++) {
		long int x = PyInt_AsLong (PyList_GetItem (given_list, 2 * n));
		long int y = PyInt_AsLong (PyList_GetItem (given_list, 2 * n + 1));

		f.set_cell (x, y);
	}

	f.fast_evolve (N);

	evolved_list = PyList_New (0);

	node *p = f.get_phead ();
	int index = f.get_index ();

	do {
		key_t mn = p->key;

		int x0 = x0_from_key (mn);
		int y0 = y0_from_key (mn);

		for (int x = x0; x < x0 + 8; x++)
			for (int y = y0; y < y0 + 8; y++)
				if (p->B[index].qc[j_from_y (y)][i_from_x (x)] & mask_from_x_y (x, y))
					add_cell (evolved_list, x, y);

		p = p->next;
	} while (p->key);

	return evolved_list;
}
//---------------------------------------------------------------------------
static PyObject *lifeint_load (PyObject *self, PyObject *args)
{
	PyObject *list;

	char *file_name;
	if (!PyArg_ParseTuple (args, "z", &file_name))
		return NULL;

	field f;

	fprintf (stderr, "loading: %s\n", file_name);
	if (!f.load (file_name))
		return NULL;

	list = PyList_New (0);

	node *p = f.get_phead ();
	int index = f.get_index ();

	do {
		key_t mn = p->key;

		int x0 = x0_from_key (mn);
		int y0 = y0_from_key (mn);

		for (int x = x0; x < x0 + 8; x++)
			for (int y = y0; y < y0 + 8; y++)
				if (p->B[index].qc[j_from_y (y)][i_from_x (x)] & mask_from_x_y (x, y))
					add_cell (list, x, y);

		p = p->next;
	} while (p->key);

	return list;
}
//---------------------------------------------------------------------------
static PyObject *lifeint_save (PyObject *self, PyObject *args)
{
	PyObject *list;
	char *file_name;
	char *s = NULL;		// the description string

	if (!PyArg_ParseTuple (args, "O!z|z", &PyList_Type, &list, &file_name, &s))
		return NULL;

	FILE *output;

	if ((output = fopen (file_name, "w"))) {
		fprintf (stderr, "saving: %s\n", file_name);

		if (s) {
			bool start_of_line = true;
			int c;
			while ((c = *s++)) {
				if (start_of_line)
					fprintf (output, "#C ");
				fputc (c, output);
				start_of_line = (c == '\n');
			}
			if (!start_of_line)
				fputc ('\n', output);
		}

		field f;

		int num_cells = PyList_Size (list) / 2;
		for (int n = 0; n < num_cells; n++) {
			long int x = PyInt_AsLong (PyList_GetItem (list, 2 * n));
			long int y = PyInt_AsLong (PyList_GetItem (list, 2 * n + 1));

			f.set_cell (x, y);
		}

		rect r = f.bounding_rect ();
		char *rle = f.new_rle_from_rect (r);
		if (rle)
			fwrite (rle, strlen (rle), 1, output);
		delete rle;

		fclose (output);
	} else {
		perror (file_name);
		return NULL;
	}

	Py_INCREF (Py_None);
	return Py_None;
}
//---------------------------------------------------------------------------
} // (namespace life)

static PyMethodDef lifeint_methods [] = {
	{ "set_rule",	life::lifeint_set_rule,		METH_VARARGS, "Set Game rule according to string" },
	{ "parse",	life::lifeint_parse,		METH_VARARGS, "Parse RLE or Life1.05 string and return a cell list" },
	{ "transform",	life::lifeint_transform,	METH_VARARGS, "Apply an affine transformation to cells in a list" },
	{ "put",	life::lifeint_put,		METH_VARARGS, "Print transformed pattern in Life 1.06 format" },
	{ "evolve",	life::lifeint_evolve,		METH_VARARGS, "Evolve Life pattern contained in a list" },
	{ "load",	life::lifeint_load,		METH_VARARGS, "Load a pattern from file and return cell list" },
	{ "save",	life::lifeint_save,		METH_VARARGS, "Save Life pattern to a file (in RLE)" },
	{ NULL, NULL, 0, NULL }
};

extern "C" void initlifeint ()
{
	Py_InitModule3 ("lifeint", lifeint_methods, "This internal module allows to evolve Life patterns given as cell lists");
	life::init ();
}
//---------------------------------------------------------------------------
