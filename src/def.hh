#ifndef def_hh
#define def_hh

/*
  This header contains definitions needed to deal with internal
  universe representation, which is organised as follows:

  The universe is divided into boxes 8x8 cells in size and
  the field we consider is a square with 2^16 boxes on each side ("field_side").
  Each box is in turn divided into 4x4 "quadrucells" which look like:
  |A B|
  |C D|

  A quadrucell is redundantly stored in one byte as "DCDCBABA" (high bits first).
  We use a 6x6 array of quadrucells to store a box -
  the array's interior represents the cells themselves,
  and its boundary is used to store cells from neighbouring boxes.

  Notation:
  (x, y) are typically the cell coordinates, -2^18 <= x, y < 2^18;
  (i, j) are the coords of a quadrucell within a box, 1 <= i, j <= 4 for actual cells;
  (m, n) are the coords of a box, 0 <= m, n < 2^16;
  "index" is either 0 or 1, it specifies one of the 2 copies of the field.
*/
//---------------------------------------------------------------------------
#define N_MASK  0x0f
#define S_MASK  0xf0
#define W_MASK  0x55
#define E_MASK  0xaa

#define NW_MASK 0x05
#define NE_MASK 0x0a
#define SW_MASK 0x50
#define SE_MASK 0xa0

#define N_MASK_4 0x0f0f0f0f
#define S_MASK_4 0xf0f0f0f0
//---------------------------------------------------------------------------
namespace life {

	inline uint32_t &as_uint32 (void *p) { return *((uint32_t*) (p)); }

	static const uint8_t cq_mask[4] = { NW_MASK, NE_MASK, SW_MASK, SE_MASK };

	// directions to adjacent nodes:
	enum { N, S, W, E, NW, SE, NE, SW };

	enum { field_side = 0x10000 };

	inline uint8_t mask_from_x_y (int x, int y) { return cq_mask[2 * (y & 1) + (x & 1)]; }

	inline key_t key_from_m_n  (int m, int n) { return field_side * n + m; }

	inline key_t key_from_x_y  (int x, int y) {
		x += 4 * field_side;
		y += 4 * field_side;
		return field_side * (y >> 3) + (x >> 3);
	}

	inline int m_from_key (key_t key) { return key % field_side; }
	inline int n_from_key (key_t key) { return key / field_side; }

	inline int x0_from_key (key_t key) { return 8 * (key % field_side) - 4 * field_side; }
	inline int y0_from_key (key_t key) { return 8 * (key / field_side) - 4 * field_side; }

	inline int i_from_x (int x) { return 1 + ((x >> 1) & 3); }
	inline int j_from_y (int y) { return 1 + ((y >> 1) & 3); }

	inline int m_from_x (int x) { return (x >> 3) + field_side / 2; }
	inline int n_from_y (int y) { return (y >> 3) + field_side / 2; }

	inline int qc_count_cells (uint8_t qc) {
		uint8_t x = (qc >> 2) & 0xf;
		return x - (x >> 1) - (x >> 2) - (x >> 3);
	}

} // (namespace life)
//---------------------------------------------------------------------------
#endif
