#include "config.h"

#include <cstdlib>
#include <cctype>
#include <cstring>

#include "life.hh"
#include "def.hh"
#include "misc.hh"

// #define PROFILE
#define MAX_EMPTY_COUNT 4

namespace life {
//---------------------------------------------------------------------------
static mask_t birth_mask = 8;
static mask_t stay_mask = 12;

static uint8_t LT[0x10000];		// fast-lookup table
static uint8_t LT_stagger[0x10000];	// another one

#ifdef PROFILE
static long evolve_counter = 0;
static long insert_counter = 0;
static long recycle_counter = 0;
static long erase_counter = 0;
static long step_counter = 0;
static long skip_fresh_counter = 0;
#endif
//---------------------------------------------------------------------------
inline bool between (key_t x, key_t a, key_t b) { return (key_t) (x - a - 1) < (key_t) (b - a - 1); }
//---------------------------------------------------------------------------
inline void evolve_16 (uint8_t *p, uint8_t *q)
{
	uint32_t a = as_uint32 (p), b = as_uint32 (p + 6), c = as_uint32 (p + 12);

	register union {
		uint32_t ul;
		uint8_t  uc[4];
	} x;

	uint8_t lo, hi;

#ifndef WORDS_BIGENDIAN
	x.ul = (a & 0x00106080) | (c & 0x00010608);
	lo = x.uc[0] | x.uc[1] | x.uc[2];
	x.ul = b & 0x00116688;
	hi = x.uc[0] | x.uc[1] | x.uc[2];
	q[0] = LT[lo + (hi << 8)];

	x.ul = (a & 0x10608000) | (c & 0x01060800);
	lo = x.uc[1] | x.uc[2] | x.uc[3];
	x.ul = b & 0x11668800;
	hi = x.uc[1] | x.uc[2] | x.uc[3];
	q[1] = LT[lo + (hi << 8)];
#else
	x.ul = (a & 0x80601000) | (c & 0x08060100);
	lo = x.uc[0] | x.uc[1] | x.uc[2];
	x.ul = b & 0x88661100;
	hi = x.uc[0] | x.uc[1] | x.uc[2];
	q[0] = LT[lo + (hi << 8)];

	x.ul = (a & 0x00806010) | (c & 0x00080601);
	lo = x.uc[1] | x.uc[2] | x.uc[3];
	x.ul = b & 0x00886611;
	hi = x.uc[1] | x.uc[2] | x.uc[3];
	q[1] = LT[lo + (hi << 8)];
#endif
}
//---------------------------------------------------------------------------
// stagger-step evolving
inline void evolve_16_s (uint8_t *p, uint8_t *q)
{
	uint32_t x = (as_uint32 (p) & 0x33333333) | (as_uint32 (p + 6) & 0xcccccccc);

#ifndef WORDS_BIGENDIAN
	q[0] = LT_stagger[x & 0xffff];
	x >>= 8;
	q[1] = LT_stagger[x & 0xffff];
#else
        x >>= 8;
        q[1] = LT_stagger[x & 0xffff];
        x >>= 8;
        q[0] = LT_stagger[x & 0xffff];
#endif
}
//---------------------------------------------------------------------------
inline bool evolve (bool self, int num)
{
	return ((!self) && ((1 << num) & birth_mask)) || (self && ((1 << num) & stay_mask));
}

inline int count_bits (mask_t bits) {
	int num;
	for (num = 0; bits; bits >>= 1)
		num += (bits & 1);
	return num;
}

static void fill_lookup_table ()
{
	mask_t ct_mask[4] = { 0x0400, 0x0200, 0x4000, 0x2000 };	// mask for the cell in 16-bit table index
	mask_t nt_mask[4] = { 0xeae0, 0x7570, 0xae0e, 0x5707 };	// mask for its neighbours in 16-bit table index
	// the same for stagger-step:
#ifndef WORDS_BIGENDIAN
	mask_t cs_mask[4] = { 0x0020, 0x1000, 0x0008, 0x0400 };	// mask for the cell in 16-bit table index
	mask_t ns_mask[4] = { 0x151f, 0x2f2a, 0x54f4, 0xf8a8 };	// mask for its neighbours in 16-bit table index
#else
	mask_t cs_mask[4] = { 0x2000, 0x0010, 0x0800, 0x0004 };	// mask for the cell in 16-bit table index
	mask_t ns_mask[4] = { 0x1f15, 0x2a2f, 0xf454, 0xa8f8 };	// mask for its neighbours in 16-bit table index
#endif

	for (mask_t mask = 0; mask < 0x10000; mask++) {
		LT[mask] = 0;
		LT_stagger[mask] = 0;

		for (int k = 0; k < 4; k++) {
			if (evolve (mask & ct_mask[k], count_bits (mask & nt_mask[k]))) LT [mask] |= cq_mask[k];
			if (evolve (mask & cs_mask[k], count_bits (mask & ns_mask[k]))) LT_stagger [mask] |= cq_mask[k];
		}
	}
}
//---------------------------------------------------------------------------
void init ()
{
	fill_lookup_table ();
}
//---------------------------------------------------------------------------
void set_rule (mask_t new_stay_mask, mask_t new_birth_mask)
{
	new_stay_mask  &= 0x01ff;
	new_birth_mask &= 0x01fe;

	if ((stay_mask ^ new_stay_mask) || (birth_mask ^ new_birth_mask)) {
		stay_mask  = new_stay_mask;
		birth_mask = new_birth_mask;

		fill_lookup_table ();
	}
}
//---------------------------------------------------------------------------
void set_rule_from_string (char *s)
{
	if (s) {
		mask_t stay_mask = 0, birth_mask = 0;
		mask_t *p = &stay_mask;
		char c;

		while ((c = tolower (*s))) {
			if (isdigit (c))
				(*p) |= 1 << (c - '0');
			if (c == 's') p = &stay_mask;
			if (c == 'b') p = &birth_mask;
			if (c == '/') p = &birth_mask;
			s++;
		}
		set_rule (stay_mask, birth_mask);
	} else
		set_rule ();
}
//---------------------------------------------------------------------------
char *get_rule_string ()
{
	static char s[32];
	int pos = 0;

	s[pos++] = 'B';
	for (int k = 0; k <= 8; k++)
		if (birth_mask & (1 << k))
			s[pos++] = '0' + k;
	s[pos++] = '/';
	s[pos++] = 'S';
	for (int k = 0; k <= 8; k++)
		if (stay_mask & (1 << k))
			s[pos++] = '0' + k;
	s[pos++] = 0;
	return s;
}
//---------------------------------------------------------------------------
void print_rule ()
{
	printf ("[Rule: %s]\n", get_rule_string ());
}
//---------------------------------------------------------------------------
int box::check_empty ()
{
	if (as_uint32 (&qc[1][1])) return 0;
	if (as_uint32 (&qc[2][1])) return 0;
	if (as_uint32 (&qc[3][1])) return 0;
	if (as_uint32 (&qc[4][1])) return 0;
	return 1;
}
//---------------------------------------------------------------------------
void box::clear ()
{
	as_uint32 (&qc[1][1]) = as_uint32 (&qc[2][1]) =
		as_uint32 (&qc[3][1]) = as_uint32 (&qc[4][1]) = 0;
}
//---------------------------------------------------------------------------
void box::print (FILE *output)
{
	const char live = '*', dead = '.';

	for (int j = 1; j <= 4; j++) {
		for (int i = 1; i <= 4; i++) {
			if (qc[j][i] & NW_MASK) putc (live, output); else putc (dead, output);
			if (qc[j][i] & NE_MASK) putc (live, output); else putc (dead, output);
		}
		putc ('\n', output);
		for (int i = 1; i <= 4; i++) {
			if (qc[j][i] & SW_MASK) putc (live, output); else putc (dead, output);
			if (qc[j][i] & SE_MASK) putc (live, output); else putc (dead, output);
		}
		putc ('\n', output);
	}
}
//---------------------------------------------------------------------------
inline int box::count_cells ()
{
	int num_cells = 0;
	for (int i = 1; i <= 4; i++)
		for (int j = 1; j <= 4; j++)
			num_cells += qc_count_cells (qc[j][i]);
	return num_cells;
}
//---------------------------------------------------------------------------
inline node::node (key_t new_key) :
	key (new_key),
	valid (true),
	fresh (true),
	empty_count (0)
{
}
//---------------------------------------------------------------------------
inline void node::init (key_t new_key)
{
	key = new_key;
	valid = fresh = true;
	empty_count = 0;
}
//---------------------------------------------------------------------------
inline void node::clear ()
{
	next = prev = this;
	B[0].clear ();
	B[1].clear ();
}
//---------------------------------------------------------------------------
inline bool node::check_stable ()
{
	return (memcmp (B[0].qc, B[1].qc, sizeof (box)) == 0);
}
//---------------------------------------------------------------------------
inline void node::clear_env (node *new_env)
{
	for (int k = 0; k < 4; k++)
		env[k] = new_env;
}
//---------------------------------------------------------------------------
inline void node::print (int index)
{
	printf ("[box 0x%08x]\n", key);
	printf ("fresh = %d\n", fresh);

	printf ("env = { ");
	for (int k = 0; k < 4; k++)
		if (env[k]->valid) {
			key_t key = env[k]->key;
			printf ("%x ", key);
		} else
			printf ("(none) ");
	printf ("}\n");

	B[index].print ();
}
//---------------------------------------------------------------------------
inline void node::link (node *p, int dir)
{
	env[dir] = p;
	p->env[dir ^ 1] = this;
}
//---------------------------------------------------------------------------
inline void node::unlink (node *void_env)
{
	for (int dir = 0; dir < 4; dir++)
		env[dir]->env[dir ^ 1] = void_env;
}
//---------------------------------------------------------------------------
node *field::insert (key_t key, node *hint)
{
	if (hint->key == key) return hint;

	register node *a, *b = hint;

	do {
		a = b;
		b = b->next;
		if (b->key == key) return b;
#ifdef PROFILE
		step_counter++;
#endif
	} while (!between (key, a->key, b->key));

#ifdef PROFILE
	insert_counter++;
#endif

	node *c;

	if ((c = recycled)) {
#ifdef PROFILE
		recycle_counter++;
#endif
		recycled = c->next;
		c->init (key);
	} else
		c = new node (key);

	b->prev = a->next = c;
	c->clear_env (&empty_node);
	c->B[index].clear ();

	c->prev = a;
	c->next = b;
	return c;
}
//---------------------------------------------------------------------------
inline node *field::insert_back (key_t key, node *hint)
{
	if (hint->key == key) return hint;

	register node *a = hint, *b;

	do {
		b = a;
		a = a->prev;
		if (a->key == key) return a;
#ifdef PROFILE
		step_counter++;
#endif
	} while (!between (key, a->key, b->key));

#ifdef PROFILE
	insert_counter++;
#endif

	node *c;

	if ((c = recycled)) {
#ifdef PROFILE
		recycle_counter++;
#endif
		recycled = c->next;
		c->init (key);
	} else
		c = new node (key);

	b->prev = a->next = c;
	c->clear_env (&empty_node);
	c->B[index].clear ();

	c->prev = a;
	c->next = b;
	return c;
}
//---------------------------------------------------------------------------
// Given the key, find the node with that key (if one exists),
// or the node after which it should be inserted (otherwise).
node *field::quick_find (key_t key)
{
	if (int (key - quick_node->key) > 0) {
		node *a, *b = quick_node;
		do {
			a = b;
			b = b->next;
			if (a->key == key) return quick_node = a;
		} while (!between (key, a->key, b->key));
		return quick_node = a;
	} else {
		node *a = quick_node, *b;
		do {
			b = a;
			a = a->prev;
			if (b->key == key) return quick_node = b;
		} while (!between (key, a->key, b->key));
		return quick_node = a;
	}
}
//---------------------------------------------------------------------------
inline void field::erase (node *p)
{
//	if (!p->key) return;

#ifdef PROFILE
	erase_counter++;
#endif

	p->next->prev = p->prev;
	p->prev->next = p->next;

	p->unlink (&empty_node);

	p->next = recycled;
	recycled = p;
}
//---------------------------------------------------------------------------
node *field::quick_insert (key_t key)
{
	if (key > quick_node->key)
		return (quick_node = insert (key, quick_node));
	else
		return (quick_node = insert_back (key, quick_node));
}
//---------------------------------------------------------------------------
// (this erase is not too quick, it just takes care of the `quick_node')
void field::quick_erase (node *p)
{
	if (!p->key) return;

	if (quick_node == p)
		quick_node = p->next;
	erase (p);
}
//---------------------------------------------------------------------------
template <int index> inline void node::evolve ()
{
// evolve <0> forms B[1] and evolve <1> forms B[0]

	uint8_t (&src) [6][6] = B[index].qc;
	uint8_t (&dest)[6][6] = B[index ^ 1].qc;

	as_uint32 (&src[5][1]) = as_uint32 (&env[S]->B[index].qc[1][1]);
	as_uint32 (&src[0][1]) = as_uint32 (&env[N]->B[index].qc[4][1]);

	src[0][0] = env[W]->env[N]->B[index].qc[4][4];
	src[0][5] = env[E]->env[N]->B[index].qc[4][1];

	src[5][0] = env[S]->env[W]->B[index].qc[1][4];
	src[5][5] = env[S]->env[E]->B[index].qc[1][1];

//	for (int k = 1; k <= 4; k++) src[k][0] = env[W]->B[index].qc[k][4];
//	for (int k = 1; k <= 4; k++) src[k][5] = env[E]->B[index].qc[k][1];
// the same unrolled:
	src[1][0] = env[W]->B[index].qc[1][4];
	src[2][0] = env[W]->B[index].qc[2][4];
	src[3][0] = env[W]->B[index].qc[3][4];
	src[4][0] = env[W]->B[index].qc[4][4];

	src[1][5] = env[E]->B[index].qc[1][1];
	src[2][5] = env[E]->B[index].qc[2][1];
	src[3][5] = env[E]->B[index].qc[3][1];
	src[4][5] = env[E]->B[index].qc[4][1];
//------
	evolve_16 (&src[0][0], &dest[1][1]);
	evolve_16 (&src[0][2], &dest[1][3]);
	evolve_16 (&src[1][0], &dest[2][1]);
	evolve_16 (&src[1][2], &dest[2][3]);
	evolve_16 (&src[2][0], &dest[3][1]);
	evolve_16 (&src[2][2], &dest[3][3]);
	evolve_16 (&src[3][0], &dest[4][1]);
	evolve_16 (&src[3][2], &dest[4][3]);

#ifdef PROFILE
	evolve_counter++;
#endif

	fresh = false;

	empty_count++;
	empty_count *= B[index ^ 1].check_empty ();
}
//---------------------------------------------------------------------------
template <int index> void field::pre_evolve ()
{
	key_t mn;
	const int dmn[] = { -field_side, field_side, -1, 1,
			    -field_side - 1, field_side + 1,
			    -field_side + 1, field_side - 1 };

	node *p, *qN, *qS, *q;

	p = q = qN = qS = &head;

// For each node in the list we find the nodes it could affect:
// these nodes are added to the list, if needed
// (in which case they are marked 'fresh' and skipped during this pass),
// or just found among existing ones.
// In either case, a link between the nodes is made via the env[] array;
// diagonal links go through an intermediate node -- one of W, E, or S nighbours.
	do {
		mn = p->key;
		if (p->empty_count == 0) {
			if (!p->env[E]->valid)
				if ((p->B[index].qc[1][4] | p->B[index].qc[2][4] |
				     p->B[index].qc[3][4] | p->B[index].qc[4][4]) & E_MASK)
					p->link (insert (mn + dmn[E], p), E);

			if (!p->env[W]->valid)
				if ((p->B[index].qc[1][1] | p->B[index].qc[2][1] |
				     p->B[index].qc[3][1] | p->B[index].qc[4][1]) & W_MASK)
					p->link (insert (mn + dmn[W], q), W);

			if (as_uint32 (&p->B[index].qc[4][1]) & S_MASK_4) {
				if (p->env[S]->valid)
					qS = p->env[S];
				else
					p->link (qS = insert (mn + dmn[S], qS), S);
				if (!p->env[S]->env[W]->valid && (p->B[index].qc[4][1] & SW_MASK))
					p->env[S]->link (insert_back (mn + dmn[SW], qS), W);
				if (!p->env[S]->env[E]->valid && (p->B[index].qc[4][4] & SE_MASK))
					p->env[S]->link (qS = insert (mn + dmn[SE], qS), E);
			}
			if (as_uint32 (&p->B[index].qc[1][1]) & N_MASK_4) {
				if (p->env[N]->valid)
					qN = p->env[N];
				else
					p->link (qN = insert (mn + dmn[N], qN), N);
				if (!p->env[W]->env[N]->valid && (p->B[index].qc[1][1] & NW_MASK))
					p->env[W]->link (insert_back (mn + dmn[NW], qN), N);
				if (!p->env[E]->env[N]->valid && (p->B[index].qc[1][4] & NE_MASK))
					p->env[E]->link (qN = insert (mn + dmn[NE], qN), N);
			}
		}

		do {
			q = p;
			p = p->next;
		} while (p->key && p->fresh);
	} while (p->key);
}
//---------------------------------------------------------------------------
void field::evolve ()
{
	quick_node = &head;	// for using "erase (p)" safely
	node *p, *q;

	if (index == 0) {
		pre_evolve <0> ();

		p = q = &head;
		do {
			p->evolve <0> ();
			if (p->empty_count > MAX_EMPTY_COUNT) {
				p->next_empty = q;
				q = p;
			}
			p = p->prev;
		} while (p->key);

		p = q;		// the last empty node
		while (p->key) {
			q = p->next_empty;
			erase (p);
			p = q;
		}
	} else {
		pre_evolve <1> ();

		p = &head;
		do {
			p->evolve <1> ();
			p = p->prev;
		} while (p->key);
	}
	index ^= 1;
	gen_no++;
}
//---------------------------------------------------------------------------
// Here go things related to stagger-step evolving:
//---------------------------------------------------------------------------
inline void node::stagger_evolve_0 ()
{
// evolve B[1] from B[0]

	uint8_t (&src) [6][6] = B[0].qc;
	uint8_t (&dest)[6][6] = B[1].qc;

	as_uint32 (&src[0][1]) = as_uint32 (&env[N]->B[0].qc[4][1]);
	src[0][0] = env[W]->env[N]->B[0].qc[4][4];

	src[1][0] = env[W]->B[0].qc[1][4];
	src[2][0] = env[W]->B[0].qc[2][4];
	src[3][0] = env[W]->B[0].qc[3][4];
	src[4][0] = env[W]->B[0].qc[4][4];
//------
	evolve_16_s (&src[0][0], &dest[1][1]);
	evolve_16_s (&src[0][2], &dest[1][3]);
	evolve_16_s (&src[1][0], &dest[2][1]);
	evolve_16_s (&src[1][2], &dest[2][3]);
	evolve_16_s (&src[2][0], &dest[3][1]);
	evolve_16_s (&src[2][2], &dest[3][3]);
	evolve_16_s (&src[3][0], &dest[4][1]);
	evolve_16_s (&src[3][2], &dest[4][3]);

	fresh = false;

	empty_count++;
	empty_count *= B[1].check_empty ();
}
//---------------------------------------------------------------------------
void node::stagger_evolve_1 ()
{
// evolve B[0] from B[1]

	uint8_t (&src) [6][6] = B[1].qc;
	uint8_t (&dest)[6][6] = B[0].qc;

	as_uint32 (&src[5][1]) = as_uint32 (&env[S]->B[1].qc[1][1]);
	src[5][5] = env[S]->env[E]->B[1].qc[1][1];

	src[1][5] = env[E]->B[1].qc[1][1];
	src[2][5] = env[E]->B[1].qc[2][1];
	src[3][5] = env[E]->B[1].qc[3][1];
	src[4][5] = env[E]->B[1].qc[4][1];
//------
	evolve_16_s (&src[1][1], &dest[1][1]);
	evolve_16_s (&src[1][3], &dest[1][3]);
	evolve_16_s (&src[2][1], &dest[2][1]);
	evolve_16_s (&src[2][3], &dest[2][3]);
	evolve_16_s (&src[3][1], &dest[3][1]);
	evolve_16_s (&src[3][3], &dest[3][3]);
	evolve_16_s (&src[4][1], &dest[4][1]);
	evolve_16_s (&src[4][3], &dest[4][3]);

	fresh = false;

	empty_count++;
	empty_count *= B[0].check_empty ();
}
//---------------------------------------------------------------------------
void field::stagger_pre_evolve_0 ()
{
	key_t mn;

	node *p, *qS, *q;

	p = q = qS = &head;

// For each node in the list we find the nodes it could affect:
// these nodes are added to the list, if needed
// or just found among existing ones.
// In case a node is added, it is marked 'fresh' and skipped during this pass,
// but most of the time these nodes are left behind.
// In either case, a link between the nodes is made via the env[] array;
// diagonal links go through an intermediate node -- one of W, E, or S nighbours.
	do {
		mn = p->key;
		if (p->empty_count == 0) {
			if (!p->env[E]->valid)
				if ((p->B[0].qc[1][4] | p->B[0].qc[2][4] |
				     p->B[0].qc[3][4] | p->B[0].qc[4][4]))
					p->link (insert (mn + 1, p), E);

			if (as_uint32 (&p->B[0].qc[4][1])) {
				if (p->env[S]->valid)
					qS = p->env[S];
				else
					p->link (qS = insert (mn + field_side, qS), S);
				if (!p->env[S]->env[E]->valid && (p->B[0].qc[4][4]))
					p->env[S]->link (qS = insert (mn + field_side + 1, qS), E);
			} else
				if (p->env[S]->valid)
					qS = p->env[S];
		}

		do {
			q = p;
			p = p->next;
		} while (p->key && p->fresh);
	} while (p->key);
}
//---------------------------------------------------------------------------
void field::stagger_pre_evolve_1 ()
{
	key_t mn;

	node *p, *qN, *q;

	p = q = qN = &head;

	do {
		mn = p->key;
		if (p->empty_count == 0) {
			if (!p->env[W]->valid)
				if ((p->B[1].qc[1][1] | p->B[1].qc[2][1] |
				     p->B[1].qc[3][1] | p->B[1].qc[4][1]))
					p->link (insert (mn - 1, q), W);

			if (as_uint32 (&p->B[1].qc[1][1])) {
				if (p->env[N]->valid)
					qN = p->env[N];
				else
					p->link (qN = insert (mn - field_side, qN), N);
				if (!p->env[W]->env[N]->valid && (p->B[1].qc[1][1]))
					p->env[W]->link (insert_back (mn - field_side - 1, qN), N);
			} else
				if (p->env[N]->valid)
					qN = p->env[N];
		}

		do {
			q = p;
			p = p->next;
		} while (p->key && p->fresh);
	} while (p->key);
}
//---------------------------------------------------------------------------
void field::fast_evolve (int N)
{
	quick_node = &head;	// for using "erase (p)" safely
	node *p, *q;

	if (N & 1) {
		evolve ();
		N--;
	}

	for (int k = 0; k < N; k++) {
		if (index == 0) {
			stagger_pre_evolve_0 ();
			p = q = &head;
			do {
				p->stagger_evolve_0 ();
				q = p->prev;
				if (p->key && p->empty_count > MAX_EMPTY_COUNT)
					erase (p);
				p = q;
			} while (p->key);
		} else {
			stagger_pre_evolve_1 ();
			p = &head;
			do {
				p->stagger_evolve_1 ();
				p = p->prev;
			} while (p->key);
		}
		index ^= 1;
	}

	gen_no += N;
}
//---------------------------------------------------------------------------
// stagger-step finishes here
//---------------------------------------------------------------------------
bool field::get_cell (int x, int y)
{
	key_t key = key_from_x_y (x, y);
	uint8_t mask = mask_from_x_y (x, y);

	node *p = quick_find (key);
	if (p->key != key) return false; // no such node

	uint8_t c = p->B[index].qc[j_from_y (y)][i_from_x (x)];

	return bool (c & mask);
}
//---------------------------------------------------------------------------
bool field::set_cell (int x, int y, bool state)
{
	key_t key = key_from_x_y (x, y);
	uint8_t mask = mask_from_x_y (x, y);

	node *p = quick_insert (key);

	p->touch ();

	uint8_t c = p->B[index].qc[j_from_y (y)][i_from_x (x)];

	if (state) {
		p->B[index].qc[j_from_y (y)][i_from_x (x)] |= mask;
	} else {
		p->B[index].qc[j_from_y (y)][i_from_x (x)] &= ~mask;
		if (p->B[index].check_empty ())
			quick_erase (p);
	}
	return bool (c & mask) ^ state;
}
//---------------------------------------------------------------------------
bool field::toggle_cell (int x, int y)
{
	key_t key = key_from_x_y (x, y);
	uint8_t mask = mask_from_x_y (x, y);

	node *p = quick_insert (key);

	p->touch ();

	uint8_t c = (p->B[index].qc[j_from_y (y)][i_from_x (x)] ^= mask);

	if (p->B[index].check_empty ())
		quick_erase (p);

	return bool (c & mask);
}
//---------------------------------------------------------------------------
unsigned int field::count_cells ()
{
	unsigned int num_cells = 0;
	node *p = &head;
	do {
		num_cells += p->B[index].count_cells ();
		p = p->next;
	 } while (p->key);
	return num_cells;
}
//---------------------------------------------------------------------------
// the "lifecycle" of a field:
//---------------------------------------------------------------------------
field::field () :
	head (0),
	empty_node (0),
	recycled (NULL),
	selection ()
{
	head.clear ();
	empty_node.clear ();
	empty_node.clear_env (&empty_node);
	empty_node.fresh = false;
	empty_node.valid = false;

	clear ();
}
//---------------------------------------------------------------------------
void field::clear ()
{
	gen_no = base_gen_no = 0;
	index = 0;

	node *a, *b;
	a = head.next;
	while (a->key) {
		b = a->next;
		delete a;
		a = b;
	}

	head.clear ();
	head.clear_env (&empty_node);

	a = recycled;
	while (a) {
		b = a->next;
		delete a;
		a = b;
	}
	recycled = NULL;

	quick_node = &head;
}
//---------------------------------------------------------------------------
field::~field ()
{
	clear ();
}
//---------------------------------------------------------------------------
// misc printing:
//---------------------------------------------------------------------------
void field::print ()
{
	node *p = &head;

	printf ("----------------\n");
	printf ("[generation %u]\n", gen_no);

	do {
		p->print (index);
		p = p->next;
	} while (p->key);
}
//---------------------------------------------------------------------------
void field::print_info ()
{
	unsigned int num_cells = count_cells ();
	unsigned int num_cells_in_box = count_cells (selection);

	if (selection.valid)
		printf ("generation %u (+%u):  %u cell(s), %u inside %d x %d box at (%d, %d)\n",
			gen_no, gen_no - base_gen_no, num_cells, num_cells_in_box,
			selection.x2 - selection.x1, selection.y2 - selection.y1,
			selection.x1, selection.y1);
	else
		printf ("generation %u (+%u):  %u cell(s)\n",
			gen_no, gen_no - base_gen_no, num_cells);

	base_gen_no = gen_no;
}
//---------------------------------------------------------------------------
void field::print_misc_info ()
{
	unsigned int
		num_nodes = 0,
		num_empty_nodes = 0,
		num_stable_nodes = 0;

	node *p = &head;

	do {
		num_nodes++;
		if (p->empty_count) num_empty_nodes++;
		if (p->check_stable ()) num_stable_nodes++;
		p = p->next;
	 } while (p->key);

	printf ("generation %u (+%u):  %u/%u/%u node(s)\n",
		gen_no, gen_no - base_gen_no, num_empty_nodes, num_stable_nodes, num_nodes);

#ifdef PROFILE
	printf ("evolve_counter     = %ld\n", evolve_counter);
	printf ("insert_counter     = %ld\n", insert_counter);
	printf ("recycle_counter    = %ld\n", recycle_counter);
	printf ("erase_counter      = %ld\n", erase_counter);
	printf ("step_counter       = %ld\n", step_counter);
	printf ("skip_fresh_counter = %ld\n", skip_fresh_counter);
#endif
}
//---------------------------------------------------------------------------
} // (namespace life)
