#include "life.hh"
#include "view.hh"

namespace life {
//---------------------------------------------------------------------------
bool view::change_position (int delta_x, int delta_y)
{
	offset_x -= delta_x;
	offset_y -= delta_y;

	return delta_x || delta_y;
}
//---------------------------------------------------------------------------
bool view::change_size (int new_width, int new_height)
{
	bool result = (width != new_width) || (height != new_height);

	width  = new_width;
	height = new_height;

	return result;
}
//---------------------------------------------------------------------------
bool view::change_zoom (int new_zoom, int center_x, int center_y)
{
	new_zoom = min (max_zoom, max (new_zoom, min_zoom));

	if (zoom == new_zoom)
		return false;

	if (zoom > 0) {
		offset_x = div (offset_x + center_x - width  / 2, zoom);
		offset_y = div (offset_y + center_y - height / 2, zoom);
	} else {
		int inv_zoom = 2 << (-zoom);
		offset_x = (offset_x + center_x - width  / 2) * inv_zoom;
		offset_y = (offset_y + center_y - height / 2) * inv_zoom;
	}

	if (new_zoom > 0) {
		offset_x = offset_x * new_zoom - center_x + width  / 2;
		offset_y = offset_y * new_zoom - center_y + height / 2;
	} else {
		int inv_new_zoom = 2 << (-new_zoom);
		offset_x = div (offset_x, inv_new_zoom) - center_x + width  / 2;
		offset_y = div (offset_y, inv_new_zoom) - center_y + height / 2;
	}

	zoom = new_zoom;
	return true;
}
//---------------------------------------------------------------------------
bool view::zoom_to_fit_rect (rect &r)
{
	int size_x = r.x2 - r.x1;
	int size_y = r.y2 - r.y1;

	if (!(size_x && size_y && r.valid))
		return false;

	int zoom_x = 0, zoom_y = 0;

	if (size_x <= width)
		zoom_x = width / size_x;
	else
		do size_x /= 2; while (size_x > width  && (zoom_x--) > min_zoom);
	if (size_y <= height)
		zoom_y = height / size_y;
	else
		do size_y /= 2; while (size_y > height && (zoom_y--) > min_zoom);

	int new_zoom = min (zoom_x, zoom_y);

	bool a = change_zoom (new_zoom, width / 2, height / 2);
	bool b = change_position (width / 2  - screen_x ((r.x1 + r.x2) / 2),
				  height / 2 - screen_y ((r.y1 + r.y2) / 2));

	return a || b;
}
//---------------------------------------------------------------------------
} // (namespace life)
