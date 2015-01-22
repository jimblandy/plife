#define _ISOC99_SOURCE

#include "config.h"

#include <stdexcept>

#include "life.hh"
#include "def.hh"
#include "misc.hh"

namespace life {
//---------------------------------------------------------------------------
inline bool random_yes_no (int rate)
{
	if (rate <= 0 || rate >= 100)
		return true;

	return rand () < (RAND_MAX / 100) * rate;
}
//---------------------------------------------------------------------------
void rect::set (int new_x1, int new_y1, int new_x2, int new_y2)
{
	if (new_x2 > new_x1) { x1 = new_x1; x2 = new_x2; } else { x1 = new_x2; x2 = new_x1; }
	if (new_y2 > new_y1) { y1 = new_y1; y2 = new_y2; } else { y1 = new_y2; y2 = new_y1; }

	x1 = max (x1, -int (4 * field_side));
	y1 = max (y1, -int (4 * field_side));

	x2 = min (x2,  int (4 * field_side));
	y2 = min (y2,  int (4 * field_side));

	valid = (x2 > x1 && y2 > y1);
}
//---------------------------------------------------------------------------
unsigned int field::count_cells (rect &r)
{
	int x1 = r.x1, y1 = r.y1, x2 = r.x2, y2 = r.y2;

	if (x1 > x2 || y1 > y2 || !r.valid)
		return 0;

	node *p = &head;

	int num_cells = 0;

	do {
		int x0 = x0_from_key (p->key);
		int y0 = y0_from_key (p->key);

		if ((x0 + 8 > x1) || (x0 < x2) || (y0 + 8 > y1) || (y0 < y2)) {

			int x_min = max (x0, x1), x_max = min (x0 + 8, x2);
			int y_min = max (y0, y1), y_max = min (y0 + 8, y2);

			for (int x = x_min; x < x_max; x++)
				for (int y = y_min; y < y_max; y++)
					if (p->B[index].qc[j_from_y (y)][i_from_x (x)] & mask_from_x_y (x, y))
						num_cells++;
		}

		p = p->next;
	} while (p->key);

	return num_cells;
}
//---------------------------------------------------------------------------
rect field::bounding_rect ()
{
	int x1, y1, x2, y2;

	x1 = y1 =  4 * field_side;
	x2 = y2 = -4 * field_side;

	node *p = &head;

	do {
		int x0 = x0_from_key (p->key);
		int y0 = y0_from_key (p->key);

		if ((x0 < x1) || (y0 < y1) || (x0 + 8 > x2) || (y0 + 8 > y2))
			for (int i = 1; i <= 4; i++)
				for (int j = 1; j <= 4; j++) {
					uint8_t qc = p->B[index].qc[j][i];
					if (qc) {
						x1 = min (x1, x0 + 2 * i - 1 - bool (qc & W_MASK));
						x2 = max (x2, x0 + 2 * i - 1 + bool (qc & E_MASK));
						y1 = min (y1, y0 + 2 * j - 1 - bool (qc & N_MASK));
						y2 = max (y2, y0 + 2 * j - 1 + bool (qc & S_MASK));
					}
				}

		p = p->next;
	} while (p->key);

	rect r;
	if (x2 > x1 && y2 > y1)
		r.set (x1, y1, x2, y2);
	return r;
}
//---------------------------------------------------------------------------
rect field::shrink_rect (rect &r)
{
	int x1 = r.x1, y1 = r.y1, x2 = r.x2, y2 = r.y2;

	if (x1 > x2 || y1 > y2 || !r.valid)
		return r;

	node *p = &head;

	int new_x1 = x2, new_x2 = x1, new_y1 = y2, new_y2 = y1;

	do {
		int x0 = x0_from_key (p->key);
		int y0 = y0_from_key (p->key);

		if ((x0 + 8 > x1 && x0 < new_x1) || (x0 + 8 > new_x2 && x0 < x2) ||
		    (y0 + 8 > y1 && y0 < new_y1) || (y0 + 8 > new_y2 && y0 < y2)) {

			int x_min = max (x0, x1), x_max = min (x0 + 8, x2);
			int y_min = max (y0, y1), y_max = min (y0 + 8, y2);

			for (int x = x_min; x < x_max; x++)
				for (int y = y_min; y < y_max; y++)
					if (p->B[index].qc[j_from_y (y)][i_from_x (x)] & mask_from_x_y (x, y)) {
						new_x1 = min (x, new_x1); new_x2 = max (x + 1, new_x2);
						new_y1 = min (y, new_y1); new_y2 = max (y + 1, new_y2);
					}
		}

		p = p->next;
	} while (p->key);

	rect new_r;
	if (new_x2 > new_x1 && new_y2 > new_y1)
		new_r.set (new_x1, new_y1, new_x2, new_y2);
	return new_r;
}
//---------------------------------------------------------------------------
void field::clear_rect (rect &r)
{
	int x1 = r.x1, y1 = r.y1, x2 = r.x2, y2 = r.y2;

	if (x1 > x2 || y1 > y2 || !r.valid)
		return;

	int m1 = m_from_x (x1), m2 = m_from_x (x2 - 1);
	int n1 = n_from_y (y1), n2 = n_from_y (y2 - 1);

	node *p = &head, *q;

	do {
		int m = m_from_key (p->key);
		int n = n_from_key (p->key);

		q = p->next;

		if (m >= m1 && m <= m2 && n >= n1 && n <= n2) {
			p->touch ();

			if (m > m1 && m < m2 && n > n1 && n < n2)
				quick_erase (p);
			else {
				int x0 = x0_from_key (p->key);
				int y0 = y0_from_key (p->key);

				int x_min = max (x0, x1), x_max = min (x0 + 8, x2);
				int y_min = max (y0, y1), y_max = min (y0 + 8, y2);

				for (int x = x_min; x < x_max; x++)
					for (int y = y_min; y < y_max; y++)
						p->B[index].qc[j_from_y (y)][i_from_x (x)] &= ~mask_from_x_y (x, y);
			}
		}

		p = q;
	} while (p->key);
}
//---------------------------------------------------------------------------
void field::crop_to_rect (rect &r)
{
	int x1 = r.x1, y1 = r.y1, x2 = r.x2, y2 = r.y2;

	if (x1 > x2 || y1 > y2 || !r.valid)
		return;

	int m1 = m_from_x (x1 - 1), m2 = m_from_x (x2);
	int n1 = n_from_y (y1 - 1), n2 = n_from_y (y2);

	node *p = &head, *q;

	do {
		int m = m_from_key (p->key);
		int n = n_from_key (p->key);

		q = p->next;

		if (m <= m1 || m >= m2 || n <= n1 || n >= n2) {
			p->touch ();

			if (m < m1 || m > m2 || n < n1 || n > n2) {
				if (p->key == 0) p->B[index].clear ();
				quick_erase (p);
			} else {
				int x0 = x0_from_key (p->key);
				int y0 = y0_from_key (p->key);

				for (int x = x0; x < x0 + 8; x++)
					for (int y = y0; y < y0 + 8; y++)
						if (x < x1 || x >= x2 || y < y1 || y >= y2)
							p->B[index].qc[j_from_y (y)][i_from_x (x)] &= ~mask_from_x_y (x, y);
			}
		}

		p = q;
	} while (p->key);
}
//---------------------------------------------------------------------------
void field::fill_rect (rect &r, int rate)
{
	int x1 = r.x1, y1 = r.y1, x2 = r.x2, y2 = r.y2;

	if (x1 > x2 || y1 > y2 || !r.valid)
		return;

	int m1 = m_from_x (x1), m2 = m_from_x (x2 - 1);
	int n1 = n_from_y (y1), n2 = n_from_y (y2 - 1);

	if ((n2 - n1) * (n2 - n1) + (m2 - m1) * (m2 - m1) > 16000)
		return;

	node *p = &head;

	for (int n = n1; n <= n2; n++)
		for (int m = m1; m <= m2; m++) {
			key_t key = key_from_m_n (m, n);

			p = insert (key, p);
			p->touch ();

			int x0 = x0_from_key (p->key);
			int y0 = y0_from_key (p->key);

			int x_min = max (x0, x1), x_max = min (x0 + 8, x2);
			int y_min = max (y0, y1), y_max = min (y0 + 8, y2);

			for (int x = x_min; x < x_max; x++)
				for (int y = y_min; y < y_max; y++) {
					uint8_t &qc = p->B[index].qc[j_from_y (y)][i_from_x (x)];
					uint8_t mask = mask_from_x_y (x, y);
					qc &= ~mask;
					if (random_yes_no (rate))
						qc |= mask;
				}
		}
}
//---------------------------------------------------------------------------
class rle_maker {
	const int size;
	char *data;

	int pos, line_pos;

	int prev_char_0, prev_count_0; // "level 0" previous char and its count
	int prev_char_1, prev_count_1; // "level 1"

	void multi_put_0 (int c, int count) {
		int num = 0;
		bool need_wrap = (line_pos > margin);

		if (count <= 0) return;
#ifdef HAVE_SNPRINTF
		if (count == 1) num = snprintf (data + pos, size - pos, need_wrap ? "\n%c"   : "%c",   c);
		if (count > 1)  num = snprintf (data + pos, size - pos, need_wrap ? "\n%d%c" : "%d%c", count, c);
#else
		if (count == 1) num = sprintf (data + pos, need_wrap ? "\n%c"   : "%c",   c);
		if (count > 1)  num = sprintf (data + pos, need_wrap ? "\n%d%c" : "%d%c", count, c);
#endif
		if (num + 1 > size - pos)
			throw std::runtime_error ("rle_maker: buffer too small, this should not happen!");

		if (need_wrap) line_pos = 0;
		pos += num;
		line_pos += num;
	}

	void multi_put_1 (int c, int count) {
		if ((c == prev_char_0) && (prev_count_0 + count > 0))
			prev_count_0 += count;
		else {
			if ((prev_char_0 != '$') || (c != '!'))
				multi_put_0 (prev_char_0, prev_count_0);
			prev_char_0 = c;
			prev_count_0 = count;
		}
	}
public:
	static const int margin = 64;

	rle_maker (int new_size, char *new_data) :
		size (new_size), data (new_data),
		pos (0), line_pos (0),
		prev_char_0 (-1), prev_count_0 (0),
		prev_char_1 (-1), prev_count_1 (0) {}

	void put (int c, int count = 1) {
		if ((c == prev_char_1) && (prev_count_1 + count > 0))
			prev_count_1 += count;
		else {
			if ((prev_char_1 != 'b') || ((c != '$') && (c != '!')))
				multi_put_1 (prev_char_1, prev_count_1);
			prev_char_1 = c;
			prev_count_1 = count;
		}
	}
	void flush () { put (-1); put (0); }
};
//---------------------------------------------------------------------------
char *field::new_rle_from_rect (rect &r)
{
	int x1 = r.x1, y1 = r.y1, x2 = r.x2, y2 = r.y2;

	if (x1 > x2 || y1 > y2 || !r.valid)
		return NULL;

	int size_x = x2 - x1;
	int size_y = y2 - y1;

	// We need to estimate roughly the space needed to store the RLE;
	// first, we count the nodes that intersect with the selection:
	int size = 0;

	node *p = &head;
	int skip = 0;
	int old_x0 = x1, old_y0 = y1;
	do {
		int x0 = x0_from_key (p->key);
		int y0 = y0_from_key (p->key);
		if ((x0 + 8 > x1) && (x0 < x2) && (y0 + 8 > y1) && (y0 < y2)) {
	// each counted node can take up to 64 bytes;
	// also there can be additional digits arising from gaps between nodes:
			skip = x0 - ((y0 == old_y0) ? old_x0 : x1);
			size += 64 + 8 + skip;
			old_x0 = x0;
			old_y0 = y0;
		}
		p = p->next;
	} while (p->key);
	// finally, there will be pattern newlines ('$'), wraps across the margin and terminating stuff:
	size += size_y + size / rle_maker::margin + 10;

	const int header_len = 256;
	const int safety_pad = 16; // rle_maker won't know of these extra bytes allocated

	size += header_len;

	char *data = new char[size + safety_pad];
	int num = sprintf (data, "x = %d, y = %d, rule = %s\n", size_x, size_y, get_rule_string ());
	rle_maker M (size - num, data + num);

	p = &head;
	for (int y = y1; y < y2; ) {
		int y0_expected = y & (~7);
		int y0_actual;

		int x0_expected = x1 & (~7);
		int x0_actual;

		while ((y0_from_key (p->key) < y0_expected) && (p->next->key > p->key)) p = p->next;
		while ((x0_from_key (p->key) < x0_expected) && (p->next->key > p->key)) p = p->next;

		if ((y0_from_key (p->key) < y0_expected) || (x0_from_key (p->key) < x0_expected)) break;

		if ((y0_actual = y0_from_key (p->key)) > y0_expected) {
			M.put ('$', y0_actual - y);
			y = y0_actual;
			continue;
		}

		node *q = p;

		int j = j_from_y (y);
		for (int x = x1; x < x2; ) {
			x0_expected = x & (~7);

			if ((x0_actual = x0_from_key (q->key)) > x0_expected) {
				M.put ('b', x0_actual - x);
				x = x0_actual;
				continue;
			}

			M.put ((q->B[index].qc[j][i_from_x (x)] & mask_from_x_y (x, y)) ? 'o' : 'b');
			if ((x & 7) == 7) if (y0_from_key ((q = q->next)->key) != y0_expected) break;
			x++;
		}
		M.put ('$');
		y++;
	}

	M.put ('!');
	M.put ('\n');
	M.flush ();

	return data;
}
//---------------------------------------------------------------------------
} // (namespace life)
