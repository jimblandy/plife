#include "config.h"

#include <cstdlib>

#include "output_x.hh"
#include "def.hh"

namespace life {
//---------------------------------------------------------------------------
// template <> void draw_buffer <XRectangle>::flush () { XDrawRectangles (display, drawable, gc, buffer, n); n = 0; }
template <> void draw_buffer <XRectangle>::flush () { XFillRectangles (display, drawable, gc, buffer, n); n = 0; }
template <> void draw_buffer <XSegment>  ::flush () { XDrawSegments   (display, drawable, gc, buffer, n); n = 0; }
template <> void draw_buffer <XPoint>    ::flush () { XDrawPoints     (display, drawable, gc, buffer, n, CoordModeOrigin); n = 0; }
//---------------------------------------------------------------------------
static inline int confine (int k, int a, int b)
{
	return ((k < a) ? a : ((k > b) ? b : k));
}
//---------------------------------------------------------------------------
static inline bool on_border (XPoint P, XRectangle R)
{
	if ((P.x < R.x) || (P.x > R.x + R.width) || (P.y < R.y) || (P.y > R.y + R.height)) return false;
	if ((P.x > R.x) && (P.x < R.x + R.width) && (P.y > R.y) && (P.y < R.y + R.height)) return false;
	return true;
}
//---------------------------------------------------------------------------
output_x::output_x (Display *new_display, Window new_window) :
	view (),

	display (new_display),
	window (new_window),
	drawable (window),
	back_buffer (0),

	gc (XCreateGC (display, window, 0, NULL)),
	selection_gc (XCreateGC (display, window, 0, NULL)),

	have_double_buffer (false),
	have_grid (true),

	lines (display, drawable),
	lines_bold (display, drawable),
	rects_xor (display, drawable),

	F (NULL)
{
	XWindowAttributes attr;
	XGetWindowAttributes (display, window, &attr);

	width  = attr.width;
	height = attr.height;

	int screen = DefaultScreen (display);
	unsigned long white_pixel = WhitePixel (display, screen);
//	unsigned long black_pixel = BlackPixel (display, screen);
	unsigned long selection_pixel = pixel_from_rgb (0, 0, 0xffff);

	XSetBackground (display, gc, white_pixel);
	XSetForeground (display, gc, white_pixel);
	XSetWindowBackground (display, window, white_pixel);

	XSetForeground (display, selection_gc, selection_pixel ^ white_pixel);
	XSetFunction (display, selection_gc, GXxor);

	rects_xor.set_color (white_pixel);
	rects_xor.set_function (GXxor);

	lines.set_color (pixel_from_rgb (0xe4e4, 0xe4e4, 0xe4e4));
	lines_bold.set_color (pixel_from_rgb (0xc0c0, 0xc0c0, 0xc0c0));

	points     = new draw_buffer <XPoint> [num_greyscale_levels] (display, drawable);
	points_sel = new draw_buffer <XPoint> [num_greyscale_levels] (display, drawable);

	points    [0].set_color (white_pixel);
	points_sel[0].set_color (selection_pixel);

	for (int k = 1; k < num_greyscale_levels; k++) {
		int level = ((num_greyscale_levels - 1 - k) * 0xc000) / (num_greyscale_levels - 1);
		unsigned long pixel = pixel_from_rgb (level, level, level);

		points    [k].set_color (pixel);
		points_sel[k].set_color (white_pixel ^ pixel ^ selection_pixel);
	}

	int major, minor;
	have_double_buffer = XdbeQueryExtension (display, &major, &minor);
	if (!have_double_buffer)
		fprintf (stderr, "output_x: double-buffering extension is not available\n");
	else {
//		fprintf (stderr, "output_x: using double-buffering extension version %d.%d\n", major, minor);
		back_buffer = XdbeAllocateBackBufferName (display, window, XdbeUndefined);
	}
}
//---------------------------------------------------------------------------
output_x::~output_x ()
{
	if (back_buffer)
		XdbeDeallocateBackBufferName (display, back_buffer);

	delete [] points_sel;
	delete [] points;

	XFreeGC (display, selection_gc);
	XFreeGC (display, gc);
}
//---------------------------------------------------------------------------
unsigned long output_x::pixel_from_rgb (int red, int green, int blue)
{
	XColor xcolor = { 0, red, green, blue };
	XAllocColor (display, DefaultColormap (display, DefaultScreen (display)), &xcolor);
	return xcolor.pixel;
}
//---------------------------------------------------------------------------
void output_x::clear (XRectangle *R)
{
	XRectangle T = { 0, 0, width, height };
	if (!R) R = &T;

	if (R->width && R->height) {
		XFillRectangle (display, drawable, gc, R->x, R->y, R->width, R->height);

		draw_grid (R, lines, zoom);
		draw_grid (R, lines_bold, 8 * zoom);
	}
}
//---------------------------------------------------------------------------
void output_x::draw_grid (XRectangle *R, draw_buffer <XSegment> &grid, int step)
{
	if (!have_grid)
		return;

	const int corner_x = R->x + offset_x - width  / 2;
	const int corner_y = R->y + offset_y - height / 2;

	const int base_x = R->x + step - 1 - mod (corner_x, step);
	const int base_y = R->y + step - 1 - mod (corner_y, step);

	const int x_max = R->x + R->width;
	const int y_max = R->y + R->height;

	for (int x = base_x; x < x_max; x += step) {
		XSegment s = { x, R->y, x, y_max - 1};
		grid.add (s);
	}

	for (int y = base_y; y < y_max; y += step) {
		XSegment s = { R->x, y, x_max - 1, y };
		grid.add (s);
	}

	grid.flush ();
}
//---------------------------------------------------------------------------
void output_x::toggle_cell (int x, int y)
{
	int side = zoom - int (have_grid);
	XRectangle r = { screen_x (x), screen_y (y), side, side };

	rects_xor.add (r);
	rects_xor.flush ();
}
//---------------------------------------------------------------------------
void output_x::toggle_selection (XRectangle *R)
{
	rect &s = F->selection;

	if (!s.valid)
		return;

	int delta = int (have_grid);
	int x1 = screen_x (s.x1) - int (zoom > 0), x2 = screen_x (s.x2);
	int y1 = screen_y (s.y1) - int (zoom > 0), y2 = screen_y (s.y2);

	x1 = confine (x1, -3, width + 1); y1 = confine (y1, -3, height + 1);
	x2 = confine (x2, -2, width + 2); y2 = confine (y2, -2, height + 2);

	XGCValues values;
	values.line_width = 1 + 2 * delta;
	XChangeGC (display, selection_gc, GCLineWidth, &values);

	if (R)
		XSetClipRectangles (display, selection_gc, 0, 0, R, 1, Unsorted);
	else
		XSetClipMask (display, selection_gc, None);

	XDrawRectangle (display, drawable, selection_gc, x1, y1, x2 - x1 - delta, y2 - y1 - delta);
}
//---------------------------------------------------------------------------
void output_x::render (mask_t flags, XRectangle *R)
{
	if (!F) return;

	const bool update = (flags & update_mask);
	const bool use_back_buffer = !update && !R && have_double_buffer;

	XRectangle T = { 0, 0, width, height };
	if (!R) R = &T;

//	printf ("[render: %d %d %d %d]\n", R->x, R->y, R->width, R->height);

	if (use_back_buffer)
		drawable = back_buffer;

	if (!update)
		clear (R);

	const int x0 = R->x, x1 = x0 + R->width;
	const int y0 = R->y, y1 = y0 + R->height;

	int box_screen_x, box_screen_y;

	int m, n;		// box coords
	int x, y;		// window coords
	int i, j;

	if (!update)
		toggle_selection (R);

	if (zoom <= 0) {
		const int qcpp = 1 << (-zoom); // the width of a pixel measured in quadrucells, "quadrucells per pixel"
		const int box_side = 4 / qcpp;

		const int base_x = (field_side * box_side - width  + 1) / 2 + offset_x;
		const int base_y = (field_side * box_side - height + 1) / 2 + offset_y;

		const int m0 = max ((base_x + x0) / box_side, 0);
		const int n0 = max ((base_y + y0) / box_side, 0);

		const int m1 = min ((base_x + x1 - 1) / box_side, field_side - 1);
		const int n1 = min ((base_y + y1 - 1) / box_side, field_side - 1);

		const key_t key0 = key_from_m_n (m0, n0);

		rect &s = F->selection;

		XRectangle r = { screen_x (s.x1),  screen_y (s.y1),
				 screen_x (s.x2) - screen_x (s.x1),
				 screen_y (s.y2) - screen_y (s.y1) };
//		XDrawRectangles (display, drawable, selection_gc, &r, 1);

		node *p = F->quick_find (key0);
		node *pS = NULL; // a hint for where the next (scan)line of nodes starts
		int index = F->get_index ();

		do {
			m = m_from_key (p->key);
			n = n_from_key (p->key);

			// this can be useful later, when we finish the (scan)line of nodes:
			// grab any path to the south if we see one and
			// if it is closer to the left side of the visible region
			if (p->env[S]->valid && (p->env[S]->key > p->key) && // don't want to wrap around
			    ((m <= m0) || !pS))
				pS = p->env[S];

			box_screen_x = m * box_side - base_x;
			box_screen_y = n * box_side - base_y;

			if ((m >= m0) && (m <= m1) && (n >= n0) && (n <= n1) && (p->empty_count <= 1)) {
				box_screen_x = m * box_side - base_x;
				box_screen_y = n * box_side - base_y;

				if (p->empty_count >= 2)
					continue;

				switch (qcpp) {
				case 1:
					for (i = 0; i < 4; i++)
						for (j = 0; j < 4; j++) {
							XPoint P = { box_screen_x + i, box_screen_y + j };
							(s.valid ? on_border (P, r) ? points_sel : points : points)
								[4 * qc_count_cells (p->B[index].qc[j + 1][i + 1])].add (P);
						}
					break;
				case 2:
					for (i = 0; i < 2; i++)
						for (j = 0; j < 2; j++) {
							int i0 = 2 * i + 1, i1 = i0 + 1;
							int j0 = 2 * j + 1, j1 = j0 + 1;
							int num_cells =
								qc_count_cells (p->B[index].qc[j0][i0]) +
								qc_count_cells (p->B[index].qc[j0][i1]) +
								qc_count_cells (p->B[index].qc[j1][i0]) +
								qc_count_cells (p->B[index].qc[j1][i1]);

							XPoint P = { box_screen_x + i, box_screen_y + j };
							(s.valid ? on_border (P, r) ? points_sel : points : points)
								[num_cells].add (P);
						}
					break;
				case 4:
					int num_cells = 0;
					for (i = 1; i <= 4; i++)
						for (j = 1; j <= 4; j++)
							num_cells += qc_count_cells (p->B[index].qc[j][i]);
					XPoint P = { box_screen_x, box_screen_y };
					(s.valid ? on_border (P, r) ? points_sel : points : points)
						[(num_cells + 3) / 4].add (P);
				}
			}
			if (n > n1) break;

			p = p->next;

			// we might want to use the "pS" hint if we have finished the (scan)line of nodes:
			if (((n_from_key (p->key) > n) || (m_from_key (p->key) > m1)) && pS) {
				key_t key1 = key_from_m_n (m0, n + 1);
				while (pS->prev->key >= key1) pS = pS->prev;
				p = pS;
			}

			if (n_from_key (p->key) > n)
				pS = NULL;
		} while (p->key);

		for (int k = 0; k < num_greyscale_levels; k++) {
			points[k].flush ();
			points_sel[k].flush ();
		}
	} else {
		const int box_side = 8 * zoom;	// visible box size
		const int side = zoom - int (have_grid);

		const int base_x = (field_side * box_side - width  + 1) / 2 + offset_x;
		const int base_y = (field_side * box_side - height + 1) / 2 + offset_y;

		const int m0 = max ((base_x + x0) / box_side, 0);
		const int n0 = max ((base_y + y0) / box_side, 0);

		const int m1 = min ((base_x + x1 - 1) / box_side, field_side - 1);
		const int n1 = min ((base_y + y1 - 1) / box_side, field_side - 1);

		const key_t key0 = key_from_m_n (m0, n0);

		uint32_t line;

		node *p = F->quick_find (key0);
		node *pS = NULL;
		int index = F->get_index ();

		if (!update)
			rects_xor.set_clip_rectangles (R, 1);

		do {
			m = m_from_key (p->key);
			n = n_from_key (p->key);

			if (p->env[S]->valid && (p->env[S]->key > p->key) && ((m <= m0) || !pS))
				pS = p->env[S];

			if ((m >= m0) && (m <= m1) && (n >= n0) && (n <= n1) && (p->empty_count <= 1)) {
				box_screen_x = m * box_side - base_x;
				box_screen_y = n * box_side - base_y;

				if (p->empty_count >= 2)
					continue;

				for (j = 0; j < 4; j++) {
					y = box_screen_y + 2 * zoom * j;

					line =
						as_uint32 (&p->B[index].qc[j + 1][1]) ^
						(update ? as_uint32 (&p->B[1 - index].qc[j + 1][1]) : 0);

					for (i = 0; i < 4; i++, line >>= 8) {
#ifndef WORDS_BIGENDIAN
						x = box_screen_x + 2 * zoom * i;
#else
						x = box_screen_x + 2 * zoom * (3 - i);
#endif

						XRectangle r[4] = { { x, y, side, side },
								    { x + zoom, y, side, side },
								    { x, y + zoom, side, side },
								    { x + zoom, y + zoom, side, side } };

						if (line & NW_MASK) rects_xor.add (r[0]);
						if (line & NE_MASK) rects_xor.add (r[1]);
						if (line & SW_MASK) rects_xor.add (r[2]);
						if (line & SE_MASK) rects_xor.add (r[3]);
					}
				}
			}

			if (n > n1) break;

			p = p->next;

			if (((n_from_key (p->key) > n) || (m_from_key (p->key) > m1)) && pS) {
				key_t key1 = key_from_m_n (m0, n + 1);
				while (pS->prev->key >= key1) pS = pS->prev;
				p = pS;
			}

			if (n_from_key (p->key) > n)
				pS = NULL;
		} while (p->key);

		rects_xor.flush ();
		rects_xor.set_clip_mask (None);
	}

	if (use_back_buffer) {
		XdbeSwapInfo swap_info = { window, XdbeUndefined };
		XdbeSwapBuffers (display, &swap_info, 1);

		drawable = window;
	}
}
//---------------------------------------------------------------------------
bool output_x::change_position (int delta_x, int delta_y, mask_t flags)
{
	if (!view::change_position (delta_x, delta_y))
		return false;

	if ((abs (delta_x) >= width) || (abs (delta_y) >= height) || !(flags & update_mask)) {
		render ();
		return true;
	}

	int src_x  = 0, src_y  = 0;
	int dest_x = 0, dest_y = 0;

	int area_width  = width  - abs (delta_x);
	int area_height = height - abs (delta_y);

	if (delta_x > 0) dest_x = delta_x; else src_x = -delta_x;
	if (delta_y > 0) dest_y = delta_y; else src_y = -delta_y;

	XCopyArea (display, window, window, gc, src_x, src_y, area_width, area_height, dest_x, dest_y);

	XRectangle Rx = { 0, 0, abs (delta_x), height }; // vertical region
	XRectangle Ry = { 0, 0, width, abs (delta_y) };  // horizontal region

	if (delta_x < 0) Rx.x = width  + delta_x;
	if (delta_y < 0) Ry.y = height + delta_y; else Rx.y = delta_y;

	render (0, &Rx);
	render (0, &Ry);

	return true;
}
//---------------------------------------------------------------------------
bool output_x::change_zoom (int new_zoom, int center_x, int center_y)
{
	bool result;
	if ((result = view::change_zoom (new_zoom, center_x, center_y))) {
		set_grid ();
		render ();
	}
	return result;
}
//---------------------------------------------------------------------------
bool output_x::zoom_to_fit_rect (rect &r)
{
	bool result;
	if ((result = view::zoom_to_fit_rect (r))) {
		set_grid ();
		render ();
	}
	return result;
}
//---------------------------------------------------------------------------
} // (namespace life)
