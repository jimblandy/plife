#ifndef gui_x_hh
#define gui_x_hh

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "output_x.hh"
//---------------------------------------------------------------------------
namespace life {

	class gui_x {
		enum { mode_idle, mode_drag, mode_pencil, mode_draw, mode_select, mode_adjust };

		static const int default_step = 4;

		static const int num_modes = 6;
		static const int cursor_shape[];

		static const int wm_atom_count = 2;
		static char *wm_atom_names[];

		Display *display;
		Window window;

		Atom wm_atoms[wm_atom_count];

		int win_width, win_height;

		int mouse_x, mouse_y;
		int cell_x, cell_y;
		int start_cell_x, start_cell_y;

		int mode;
		int prefix;
		int step;

		int delta_x, delta_y;

		bool pause, quit;
		bool cell_state;

		field *F;
		output_x *V;

		int file_no;
		int num_files;
		char **file_array;

		char *clipboard;
		int (*old_error_handler) (Display *display, XErrorEvent *xerror);

		void on_focus_in (XEvent &event);
		void on_configure (XEvent &event);
		void on_client_message (XEvent &event);
		void on_expose (XEvent &event);
		void on_key_press (XEvent &event);
		void on_key_release (XEvent &event);
		void on_button_press (XEvent &event);
		void on_motion (XEvent &event);
		void on_button_release (XEvent &event);
		void on_selection_request (XEvent &event);

		bool update_cell_x_y ();

		void set_title ();
		void set_cursor (unsigned int shape);
		void set_mode (int new_mode);

		void change_position (int delta_x, int delta_y, mask_t flags = 0);
		void change_zoom (int new_zoom, bool centered);

		void goto_file (int new_file_no);
	public:
		void attach (field *new_F) { F = new_F; V->attach (F); V->render (); }

		void evolve (int N = 1, bool verbose = false);
		void main_loop (int new_num_files = 0, char **new_file_array = NULL);

		gui_x ();
		~gui_x ();
	};

}
//---------------------------------------------------------------------------
#endif
