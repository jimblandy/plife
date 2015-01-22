#ifndef misc_hh
#define misc_hh
//---------------------------------------------------------------------------
namespace life {

	inline int div (int a, int b) { return (a >= 0 ? a / b : (a - b + 1) / b); }
	inline int mod (int a, int b) { return (a >= 0 ? a % b : (a + 1) % b + b - 1); }

	inline int min (int a, int b) { return (a < b) ? a : b; }
	inline int max (int a, int b) { return (a > b) ? a : b; }

}
//---------------------------------------------------------------------------
#endif
