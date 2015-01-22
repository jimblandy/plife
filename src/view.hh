#ifndef view_hh
#define view_hh

#include "life.hh"
#include "misc.hh"
//---------------------------------------------------------------------------
namespace life {

	struct view {
		static const int min_zoom = -2;
		static const int max_zoom = 12;

		int width, height;
		int offset_x, offset_y;
		int zoom;

		int cell_x (int x) {
			x = x - width  / 2 + offset_x;
			return (zoom > 0) ? div (x, zoom) : (x << (1 - zoom));
		}
		int cell_y (int y) {
			y = y - height / 2 + offset_y;
			return (zoom > 0) ? div (y, zoom) : (y << (1 - zoom));
		}

		int screen_x (int x) { return (zoom > 0 ? x * zoom : div (x, 1 << (1 - zoom))) + width  / 2 - offset_x; }
		int screen_y (int y) { return (zoom > 0 ? y * zoom : div (y, 1 << (1 - zoom))) + height / 2 - offset_y; }

		bool change_position (int delta_x, int delta_y);
		bool change_size (int new_width, int new_height);
		bool change_zoom (int new_zoom, int center_x, int center_y);
		bool zoom_to_fit_rect (rect &r);

		view () : offset_x (0), offset_y (0), zoom (max_zoom / 3) { }
	};

}
//---------------------------------------------------------------------------
#endif
