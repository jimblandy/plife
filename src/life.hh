#ifndef life_hh
#define life_hh

#include <cstdio>
#include "types.hh"
//---------------------------------------------------------------------------
namespace life {

	void init ();

	void set_rule (mask_t stay_mask = 12, mask_t birth_mask = 8);
	void set_rule_from_string (char *s = NULL);

	char *get_rule_string ();	// returns a statically allocated string describing current Game rule
	void print_rule ();

//---------------------------------------------------------------------------
	struct rect {
		int x1, y1, x2, y2;
		bool valid;

		rect () : x1 (0), y1 (0), x2 (0), y2 (0), valid (false) {}

		void set (int new_x1 = 0, int new_y1 = 0, int new_x2 = 0, int new_y2 = 0);
	};
//---------------------------------------------------------------------------
	struct box {
		uint8_t qc[6][6];	// array of quadrucells (6x6 = 4x4 main + (4x1 + 4x4) boundary)

		int check_empty ();
		void clear ();

		void print (FILE *output = stdout);
		int count_cells ();
	};
//---------------------------------------------------------------------------
	struct node {
		box B[2];		// the bibox :)
		node *env[4];		// node's environs
		node *next, *prev, *next_empty;

		key_t key;

		bool valid, fresh;
		int empty_count;

		void init (key_t new_key);
		void clear ();
		void touch () {	fresh = false; empty_count = 0; }

		bool check_stable ();
		void clear_env (node *new_env);
		void print (int index);

		void link (node *p, int dir);
		void unlink (node *void_env);

		template <int index> void evolve ();

		void stagger_evolve_0 ();
		void stagger_evolve_1 ();

		node (key_t new_key);
	};
//---------------------------------------------------------------------------
	class field {
		int index;
		unsigned int gen_no, base_gen_no;

		node head, empty_node;
		node *recycled;

		node *quick_node;

		node *insert (key_t key, node *hint);
		node *insert_back (key_t key, node *hint);
		void erase (node *p);

		node *quick_insert (key_t key);
		void quick_erase (node *p);

		template <int index> void pre_evolve ();

		void stagger_pre_evolve_0 ();
		void stagger_pre_evolve_1 ();
	public:
		rect selection;

		node *quick_find (key_t key);
		node *get_phead () { return &head; }
		int get_index () { return index; }

		void clear ();

		rect bounding_rect ();
		rect shrink_rect (rect &r);

		unsigned int count_cells ();
		unsigned int count_cells (rect &r);

		void clear_rect (rect &r);
		void crop_to_rect (rect &r);
		void fill_rect (rect &r, int rate);
		char *new_rle_from_rect (rect &r);

		bool get_cell (int x, int y);
		bool set_cell (int x, int y, bool state = true);
		bool toggle_cell (int x, int y);

		void evolve ();
		void fast_evolve (int N = 1);

		void print ();
		void print_info ();
		void print_misc_info ();

		void load_life10X (FILE *input);
		void load_bell (FILE *input);
		void load_rle (FILE *input);
		void load_list (FILE *input, int x0 = 0, int y0 = 0);

		bool load (char *file_name);
		bool save (char *file_name);

		field ();
		~field ();
	};

} // (namespace life)
//---------------------------------------------------------------------------
#endif
