#include "config.h"

#include <exception>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "life.hh"
#include "gui_x.hh"
//---------------------------------------------------------------------------
int main (int argc, char **argv)
{
	enum { mode_test, mode_x, mode_help, mode_version };

	int mode = mode_x;

	try {
		if (argc > 1) {
			if ((strcmp (argv[1], "-bm") == 0) ||
			    (strcmp (argv[1], "-test") == 0))
				mode = mode_test;
			else if ((strcmp (argv[1], "-h") == 0) || 
				 (strcmp (argv[1], "--help") == 0))
				mode = mode_help;
			else if ((strcmp (argv[1], "-V") == 0) || 
				 (strcmp (argv[1], "--version") == 0))
				mode = mode_version;
		}

		if (mode <= mode_x)
			life::init ();

		switch (mode) {
		case mode_help: {
			printf ("This program is part of " PACKAGE_STRING ". Usage:\n"
				"\tlife <files...>\n"
				"\tlife -test <num> <file>\n"
				"\tlife {-h|--help}\n"
				"\tlife {-V|--version}\n"
				"See README for controls in GUI mode.\n");
		} break;
		case mode_version: {
			printf (PACKAGE_STRING "\n");
		} break;
		case mode_test: { // benchmark and test mode
			int need_print = false;
			int N = 0;
			
			if (argc > 3) {
				N = atoi (argv[2]);
				life::field f;
				if (f.load (argv[3])) {
					f.fast_evolve (N);
					if (need_print)
						f.print ();
					f.print_info ();
				}
			}
		} break;
		default: { // default gui_x mode
			life::field f;
			life::gui_x g;

			g.attach (&f);
			g.main_loop (argc - 1, argv + 1);
		} break;
		}
	} catch (std::exception &e) {
		fprintf (stderr, "[%s]\n", e.what ());
	}

	return 0;
}
//---------------------------------------------------------------------------
