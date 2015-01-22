#define XK_MISCELLANY
#define XK_LATIN1
#define _ISOC99_SOURCE

#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include <X11/keysymdef.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>

#include "gui_x.hh"
#include "output_x.hh"
//---------------------------------------------------------------------------
namespace life {
//---------------------------------------------------------------------------
const int gui_x::cursor_shape [] = { XC_left_ptr, XC_fleur, XC_pencil, XC_pencil, XC_crosshair, XC_crosshair };
char *gui_x::wm_atom_names [] = { "WM_DELETE_WINDOW", "WM_TAKE_FOCUS" };
//---------------------------------------------------------------------------
static int error_handler (Display *display, XErrorEvent *xerror)
{
	char text[1024];
	XGetErrorText (display, xerror->error_code, text, 1024);
	fprintf (stderr, "[X error: %s, request = %d, minor = %d, id = %#lx]\n",
		 text, xerror->request_code, xerror->minor_code, xerror->resourceid);

	return 0;
}
//---------------------------------------------------------------------------
gui_x::gui_x () :
	win_width (512),
	win_height (384),
	mouse_x (0), mouse_y (0),
	cell_x (0), cell_y (0),
	mode (mode_idle),
	prefix (0),
	step (default_step),
	delta_x (0),
	delta_y (0),
	pause (true),
	quit (false),
	F (NULL),
	V (NULL),
	file_no (0),
	num_files (0),
	file_array (NULL),
	clipboard (NULL)
{
	if ((display = XOpenDisplay (NULL)) == NULL)
		throw std::runtime_error ("gui_x: cannot open display");

	old_error_handler = XSetErrorHandler (error_handler);

	int screen = DefaultScreen (display);
	Window root_window = RootWindow (display, screen);

	window = XCreateSimpleWindow (display, root_window, 0, 0, win_width, win_height, 0, 0, BlackPixel (display, screen));

	XInternAtoms (display, wm_atom_names, wm_atom_count, False, wm_atoms);
	XSetWMProtocols (display, window, wm_atoms, wm_atom_count);

	V = new output_x (display, window);

	XMapWindow (display, window);
}
//---------------------------------------------------------------------------
gui_x::~gui_x ()
{
	if (clipboard) delete clipboard;
	delete V;

	XUnmapWindow (display, window);
	XDestroyWindow (display, window);

	XSetErrorHandler (old_error_handler);

	XCloseDisplay (display);
}
//---------------------------------------------------------------------------
void gui_x::on_focus_in (XEvent &event)
{
	set_mode (mode_idle);
	delta_x = delta_y = 0;
}
//---------------------------------------------------------------------------
void gui_x::on_configure (XEvent &event)
{
	XConfigureEvent &configure = event.xconfigure;

	if (win_width == configure.width && win_height == configure.height)
		return;

	win_width = configure.width;
	win_height = configure.height;

//	printf ("[ConfigureNotify, win_width = %d, win_height = %d]\n", win_width, win_height);

	V->change_size (win_width, win_height);
}
//---------------------------------------------------------------------------
void gui_x::on_client_message (XEvent &event)
{
	if (event.xclient.data.l[0] == (long int) wm_atoms [0])
		quit = 1;
/*
	Atom type = event.xclient.message_type;
	char *atom_name = XGetAtomName (display, type);
	char *wm_message = XGetAtomName (display, event.xclient.data.l[0]);

	XFree (wm_message);
	XFree (atom_name);
*/
}
//---------------------------------------------------------------------------
void gui_x::on_expose (XEvent &event)
{
	XExposeEvent &expose = event.xexpose;

	XRectangle R = { expose.x, expose.y, expose.width, expose.height };

//	printf ("[Expose: %d %d %d %d]\n", expose.x, expose.y, expose.width, expose.height);

	V->render (0, &R);
}
//---------------------------------------------------------------------------
void gui_x::on_key_press (XEvent &event)
{
	KeySym key_sym = XLookupKeysym (&event.xkey, 0);
	int state = event.xkey.state;
	int delta = (state & ControlMask) ? 128 : 32;

	mouse_x = event.xkey.x;
	mouse_y = event.xkey.y;

	update_cell_x_y ();

	if (key_sym >= XK_0 && key_sym <= XK_9) {
		int digit = key_sym - XK_0;
		prefix = digit + prefix * 10;
		return;
	} 

	int real_prefix = (prefix ? prefix : 1);

	if (mode <= mode_drag) switch (key_sym) {
	case XK_Escape: quit = true; break;

//	case XK_End: change_position (32768, 32768); break;

	case XK_a:
		V->toggle_selection ();
		F->selection = F->bounding_rect ();
		V->toggle_selection ();
		break;

	case XK_o:
		V->toggle_selection ();
		F->selection.set ();
		break;

	case XK_Shift_L:
		if (mode == mode_idle) {
			set_mode (mode_pencil);
			pause = true;
		}
		break;

	case XK_Insert:
		if (state & ControlMask) {
			XSetSelectionOwner (display, XA_PRIMARY, None, CurrentTime);
			if (clipboard) delete clipboard;
			clipboard = F->new_rle_from_rect (F->selection);
			int size_x = F->selection.x2 - F->selection.x1;
			int size_y = F->selection.y2 - F->selection.y1;
			printf ("[copy region: %d x %d cells]\n", size_x, size_y);
			XSetSelectionOwner (display, XA_PRIMARY, window, CurrentTime);
		}
		break;


	case XK_Delete:
		if (state & ControlMask) {
			printf ("[clear field]\n");
			F->clear ();
			V->render ();
			pause = true;
		} else {
			F->clear_rect (F->selection);
			V->render ();
		}
		break;

	case XK_Page_Up:   goto_file ((state & ControlMask) ? 0 : file_no - 1); break;
	case XK_Page_Down: goto_file ((state & ControlMask) ? num_files - 1 : file_no + 1); break;
	case XK_BackSpace: goto_file (file_no); pause = true; break;

	case XK_c:
		F->crop_to_rect (F->selection);
		V->render ();
		break;
	case XK_f:
		F->fill_rect (F->selection, prefix);
		V->render ();
		break;

	case XK_g: if (prefix) goto_file (prefix - 1); break;
	case XK_i: F->print_info (); break;
	case XK_l:
		printf ("[File list]\n");
		for (int n = 0; n < num_files; n++)
			printf ("%5d: %s\n", n + 1, file_array[n]);
		break;
	case XK_m: F->print_misc_info (); break;
	case XK_n: set_rule (); print_rule (); break;
	case XK_r: V->render (); break;

	case XK_s:
		if (prefix) {
			printf ("[step = %d]\n", step = prefix);
			prefix = 0;
		}
		evolve (step);
		pause = true;
		break;

	case XK_h:
		V->toggle_selection ();
		F->selection = F->shrink_rect (F->selection);
		V->toggle_selection ();
		break;

	case XK_u: if (prefix) change_zoom (1 - prefix, true); break;
	case XK_x:
		if (true) {
			rect r = F->bounding_rect ();
			V->zoom_to_fit_rect (r);
			update_cell_x_y ();
		}
		break;
	case XK_z:
		if (prefix)
			change_zoom (prefix, true);
		else {
			V->zoom_to_fit_rect (F->selection);
			update_cell_x_y ();
		}
		break;

	case XK_minus:
	case XK_KP_Subtract:
		change_zoom (V->zoom - real_prefix, true); break;
	case XK_equal:
	case XK_KP_Add:
		change_zoom (V->zoom + real_prefix, true); break;

	case XK_Home:  change_position (V->offset_x, V->offset_y); break;

	case XK_Left:  delta_x =  delta; break;
	case XK_Right: delta_x = -delta; break;
	case XK_Up:    delta_y =  delta; break;
	case XK_Down:  delta_y = -delta; break;

	case XK_period:
		printf ("[step = %d (default), prefix = %d]\n", step = default_step, prefix = 0);
		break;

	case XK_space:
		evolve ();
		pause = true;
		break;

	case XK_Return:
		if (prefix) {
			evolve (prefix, true);
			prefix = 0;
		} else
			pause = !pause;
		break;
	}

	if (delta_x || delta_y)
		change_position (delta_x, delta_y);

	prefix = 0;

}
//---------------------------------------------------------------------------
void gui_x::on_key_release (XEvent &event)
{
	KeySym key_sym = XLookupKeysym (&event.xkey, 0);
//	int state = event.xkey.state;

	switch (key_sym) {
	case XK_Left: case XK_Right: delta_x = 0; break;
	case XK_Up:   case XK_Down:  delta_y = 0; break;

	case XK_Control_L:
	case XK_Shift_L:
		set_mode (mode_idle);
		set_title ();
		break;
	}
}
//---------------------------------------------------------------------------
void gui_x::on_button_press (XEvent &event)
{
	int button = event.xbutton.button;
	int state  = event.xbutton.state;

	mouse_x = event.xbutton.x;
	mouse_y = event.xbutton.y;

	switch (button) {
	case Button1:
		switch (mode) {
		case mode_pencil:
			if (V->zoom > 0) {
				cell_state = F->toggle_cell (cell_x, cell_y);
				V->toggle_cell (cell_x, cell_y);
				set_mode (mode_draw);
//				pause = true;
			} break;
		case mode_idle: set_mode (mode_drag); break;
		}
	}
	if (mode <= mode_pencil) switch (button) {
	case Button2:
		if (state & ControlMask)
			set_mode (mode_adjust);
		else {
			start_cell_x = cell_x;
			start_cell_y = cell_y;
			F->selection.set (cell_x, cell_y, cell_x, cell_y);
			V->render ();
			set_mode (mode_select);
		}
		break;
	case Button3:
		if (state & ControlMask) {
			pause = true;
			evolve ();
		} else
			pause = !pause;
		break;
	case Button4: change_zoom (V->zoom + 1, !(state & ControlMask)); break;
	case Button5: change_zoom (V->zoom - 1, !(state & ControlMask)); break;
	}
}
//---------------------------------------------------------------------------
void gui_x::on_motion (XEvent &event)
{
	int old_x = mouse_x;
	int old_y = mouse_y;

	mouse_x = event.xmotion.x;
	mouse_y = event.xmotion.y;

//	int state = event.xmotion.state;

	int old_cell_x = cell_x;
	int old_cell_y = cell_y;

	switch (mode) {
	case mode_idle:
		update_cell_x_y ();
		break;
	case mode_drag:
		change_position (mouse_x - old_x, mouse_y - old_y, update_mask);
		break;

	case mode_select:
	case mode_adjust:
	case mode_pencil:
	case mode_draw: if (update_cell_x_y ())
		switch (mode) {
		case mode_draw: if (V->zoom > 0)
			if (F->set_cell (cell_x, cell_y, cell_state))
				V->toggle_cell (cell_x, cell_y);
			break;
		case mode_select:
			V->toggle_selection ();
			F->selection.set (start_cell_x, start_cell_y, cell_x, cell_y);
			V->toggle_selection ();
			break;
		case mode_adjust:
			if (!F->selection.valid)
				break;

			int x1 = F->selection.x1, y1 = F->selection.y1;
			int x2 = F->selection.x2, y2 = F->selection.y2;

			// choose selection corner to adjust:
			((abs (x1 - cell_x) < abs (x2 - cell_x)) ? x1 : x2) += (cell_x - old_cell_x);
			((abs (y1 - cell_y) < abs (y2 - cell_y)) ? y1 : y2) += (cell_y - old_cell_y);

			V->toggle_selection ();
			F->selection.set (x1, y1, x2, y2);
			V->toggle_selection ();
			break;
		}
	}
}
//---------------------------------------------------------------------------
void gui_x::on_button_release (XEvent &event)
{
	int button = event.xbutton.button;
//	int state = event.xbutton.state;

	switch (button) {
	case Button1:
		switch (mode) {
		case mode_drag: set_mode (mode_idle); V->render (); break;
		case mode_draw: set_mode (mode_pencil); break;
		} break;
	case Button2:
		switch (mode) {
		case mode_adjust:
		case mode_select: set_mode (mode_idle); break;
		} break;
	}
}
//---------------------------------------------------------------------------
void gui_x::on_selection_request (XEvent &event)
{
	XSelectionRequestEvent request = event.xselectionrequest;
	XEvent answer;

	if (!clipboard) request.property = None; // refuse

	answer.xselection = (XSelectionEvent) {
		SelectionNotify,	// type
		0,			// serial
		True,			// send_event
		display,
		request.requestor,
		request.selection,
		request.target,
		request.property,
		request.time
	};

	XChangeProperty (display, request.requestor, request.property, request.target, 8,
			 PropModeReplace, (unsigned char *) clipboard, strlen (clipboard));

	XSendEvent (display, request.requestor, False, 0, &answer);
}
//---------------------------------------------------------------------------
bool gui_x::update_cell_x_y ()
{
	int new_cell_x = V->cell_x (mouse_x);
	int new_cell_y = V->cell_y (mouse_y);

	if (new_cell_x == cell_x && new_cell_y == cell_y)
		return false;

	cell_x = new_cell_x;
	cell_y = new_cell_y;

	set_title ();

	return true;
}
//---------------------------------------------------------------------------
void gui_x::set_title ()
{
	const char *title = "Game of Life";
	const char *file_name = (num_files ? file_array[file_no] : "scratch");
	const char *format = "%s [%s] (x=%d, y=%d)";

	const unsigned int buf_size = 1024;
	char full_title[buf_size];

#ifdef HAVE_SNPRINTF
	snprintf (full_title, buf_size, format, title, file_name, cell_x, cell_y);
#else
	if (strlen (format) + strlen (title) + strlen (file_name) + 6 * sizeof (int) < buf_size)
		sprintf (full_title, format, title, file_name, cell_x, cell_y);
	else
		strcpy (full_title, "(title too long!)");
#endif

	XSetStandardProperties (display, window, full_title, NULL, None, NULL, 0, 0);
}
//---------------------------------------------------------------------------
void gui_x::set_cursor (unsigned int shape)
{
	XSetWindowAttributes set_attr;
	set_attr.cursor = XCreateFontCursor (display, shape);
	XChangeWindowAttributes (display, window, CWCursor, &set_attr);
	XFreeCursor (display, set_attr.cursor);
}
//---------------------------------------------------------------------------
void gui_x::set_mode (int new_mode)
{
	if (new_mode < 0 || new_mode >= num_modes)
		return;
	mode = new_mode;
	set_cursor (cursor_shape[mode]);
}
//---------------------------------------------------------------------------
void gui_x::change_position (int delta_x, int delta_y, mask_t flags)
{
	V->change_position (delta_x, delta_y, flags);
	update_cell_x_y ();
}
//---------------------------------------------------------------------------
void gui_x::change_zoom (int new_zoom, bool centered)
{
	int x = (centered ? win_width  / 2 : mouse_x);
	int y = (centered ? win_height / 2 : mouse_y);

	if (V->change_zoom (new_zoom, x, y))
		update_cell_x_y ();
}
//---------------------------------------------------------------------------
void gui_x::evolve (int N, bool verbose)
{
	if (verbose)
		printf ("[skip %d generation(s)]\n", prefix);

	if (N == 1) {
		F->evolve ();
		V->render (update_mask);
	} else {
		F->fast_evolve (N);
		V->render ();
	}
}
//---------------------------------------------------------------------------
void gui_x::goto_file (int new_file_no)
{
	if (new_file_no < 0 || new_file_no >= num_files)
		return;

	file_no = new_file_no;

	if (num_files && file_array) {
		printf ("\033c"); // clear screen
		fflush (stdout);

		if (!F->load (file_array[file_no]))
			F->clear ();

		V->render ();
		set_title ();
	}
}
//---------------------------------------------------------------------------
void gui_x::main_loop (int new_num_files, char **new_file_array)
{
	if (!F)
		return;

	num_files = new_num_files;
	file_array = new_file_array;
	file_no = 0;

	XEvent event;
	long int event_mask =
		ButtonPressMask | ButtonReleaseMask |
		Button1MotionMask | PointerMotionMask |
		KeyPressMask | KeyReleaseMask |
		FocusChangeMask | StructureNotifyMask |
		ExposureMask;

	XSelectInput (display, window, event_mask);
//	XMapWindow (display, window);

	set_mode (mode_idle);
	set_title ();
	goto_file (0);

	quit = false;

	do {
		if (pause)
//			XWindowEvent (display, window, event_mask, &event);
			XNextEvent (display, &event);
		else
			while (!XCheckMaskEvent (display, event_mask, &event))
				evolve ();

		switch (event.type) {
		case ClientMessage: on_client_message (event); break;
		case KeyPress: on_key_press (event); break;
		case KeyRelease: on_key_release (event); break;
		case Expose: on_expose (event); break;
		case ButtonPress: on_button_press (event); break;
		case MotionNotify: on_motion (event); break;
		case ButtonRelease: on_button_release (event); break;
		case ConfigureNotify: on_configure (event); break;
		case FocusIn: on_focus_in (event); break;
		case SelectionRequest: on_selection_request (event); break;
		}
	} while (!quit);

//	XUnmapWindow (display, window);
}
//---------------------------------------------------------------------------
} // (namespace life)
