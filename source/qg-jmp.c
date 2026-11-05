// qg-jmp.c -- setjmp and longjmp on the c4 VM, which has no opcode to set bp.
//
// A frame is bp[0] the caller's bp and bp[1] the return pc; one parameter sits
// at bp+2, and of two the first at bp+3. LEV is sp = bp, bp = *sp++, pc = *sp++,
// and leaves A alone, so two of them in a row can put every register back:
//
//   setjmp keeps its own frame's address and what is in it: where it returns to,
//   and the bp to return with.
//   longjmp writes those back into that frame, then points its own frame at it,
//   with an LEV for a return address. Its LEV loads bp with setjmp's frame and goes
//   to the other LEV, which is setjmp's own return: to the same pc with the same
//   bp and sp, and A the value given. The frames between are simply left behind.
//
// The frame that called setjmp must still be live, as C requires. Locals it has
// changed since are as they were last stored: the VM keeps them all in memory.
//
// Only as C allows it: the whole condition of an if or a switch, or a comparison
// with a constant. In `r = setjmp (b)` the stack machine has r's address on its
// stack across the call, where what ran since has written over it.
#include <setjmp.h>

int qg_setjmp (int *b)
{
	int *f;
	f = (int *)&b - 2;
	b[0] = f[0];
	b[1] = f[1];
	b[2] = (int)f;
	return 0;
}

// nothing but ENT and an LEV: its LEV is the one longjmp returns through
static void qg_lev (void)
{
}

// its frame is below longjmp's, so writing setjmp's frame cannot overwrite it
static int qg_jump (int *b, int v)
{
	int *me, *f, *p;
	me = (int *)&b - 3;
	p = (int *)&qg_lev;
	while (*p != 8)		// LEV
		++p;
	f = (int *)b[2];
	f[0] = b[0];
	f[1] = b[1];
	me[0] = (int)f;
	me[1] = (int)p;
	return v;
}

void qg_longjmp (int *b, int v)
{
	qg_jump (b, v ? v : 1);
}
