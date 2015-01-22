#ifndef output_x_hh
#define output_x_hh

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdbe.h>

#include "life.hh"
#include "view.hh"
//---------------------------------------------------------------------------
namespace life {

	enum { update_mask = 1 };

	template <class T> class draw_buffer {
		static const int buffer_size = 1024;

		Display *display;
		Drawable &drawable;
		GC gc;

		int n;
		T buffer[buffer_size];
	public:
		void set_clip_mask (Pixmap pixmap) { XSetClipMask (display, gc, pixmap); }
		void set_clip_rectangles (XRectangle *R, int n) { XSetClipRectangles (display, gc, 0, 0, R, n, Unsorted); }
		void set_function (int function) { XSetFunction (display, gc, function); }

		void set_color (unsigned long pixel) { XSetForeground (display, gc, pixel); }
		inline void add (T &t) { buffer[n++] = t; if (n == buffer_size) flush (); }
		inline void flush ();

		draw_buffer (Display *new_display, Drawable &new_drawable) :
			display (new_display),
			drawable (new_drawable),
			gc (XCreateGC (display, drawable, 0, NULL)),
			n (0) { }
		~draw_buffer () { XFreeGC (display, gc); }
	};

	class output_x : public view {
		static const int num_greyscale_levels = 17; // don't change!

		Display *display;
		Window window;
		Drawable drawable;
		XdbeBackBuffer back_buffer;
		GC gc, selection_gc;

		bool have_double_buffer;
		bool have_grid;

		draw_buffer <XSegment> lines, lines_bold;
		draw_buffer <XRectangle> rects_xor;
		draw_buffer <XPoint> *points; // greyscale level from 0 to (num_greyscale_levels - 1)
		draw_buffer <XPoint> *points_sel;

		field *F;

		unsigned long pixel_from_rgb (int red, int green, int blue);

		void clear (XRectangle *R = NULL);
		void draw_grid (XRectangle *R, draw_buffer <XSegment> &grid, int step);
		void set_grid () { have_grid = (zoom >= 3); }
	public:
		void attach (field *new_F) { F = new_F; }

		void render (mask_t flags = 0, XRectangle *R = NULL);
		void toggle_cell (int cell_x, int cell_y);
		void toggle_selection (XRectangle *R = NULL);

		bool change_position (int delta_x, int delta_y, mask_t flags = 0);
		bool change_zoom (int new_zoom, int center_x, int center_y);
		bool zoom_to_fit_rect (rect &r);

		output_x (Display *new_display, Window new_window);
		~output_x ();
	};

}
//---------------------------------------------------------------------------
#endif
