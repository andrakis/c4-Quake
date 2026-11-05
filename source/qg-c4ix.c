// qg-c4ix.c -- on C4IX the display is the desktop's, which gives it way to the
// game until the game ends (include/display.h): qg-gui.c asks first. With -window,
// or when a program that does not give way has the display, the game plays in the
// Terminal window it was started from instead (window.h): 8-bit frames the desktop
// scales. A game asks to be served sooner and longer than the rest, and runs on
// CPU 0 unless it was pinned (the kernel's sched_softpin).
//
// Two buffers: the desktop reads the one it was given when it next draws, and says
// so (WIN_FRAME); until then a newer frame waits in the other, and only the newest
// is sent. The game never waits on the window, and a frame is never shown half drawn.
#include <stdlib.h>
#include <string.h>
#include "display.h"
#include "window.h"
#include "quakegeneric.h"

int unice (int id, int nice);		// libc4ix (c4ix_user.h)
enum { QG_NICE = -5, QG_PIXELS = QUAKEGENERIC_RES_X * QUAKEGENERIC_RES_Y };

int qg_take (void) { unice (0, QG_NICE); return udisplay_take (0); }
void qg_give (void) { udisplay_give (); }

static char *fbuf[2];
static int *fpal[2];
static int sent, waiting, pending, closed;

int qg_win_open (void)
{
	int i;
	unice (0, QG_NICE);
	for (i = 0 ; i < 2 ; i++)
	{
		fbuf[i] = (char *)calloc (QG_PIXELS, 1);
		fpal[i] = (int *)calloc (256, sizeof(int));
		if (!fbuf[i] || !fpal[i])
			return 0;
	}
	return win_open ("Quake");
}

static void send_frame (int b)
{
	win_fb (fbuf[b], QUAKEGENERIC_RES_X, QUAKEGENERIC_RES_Y, QUAKEGENERIC_RES_X, fpal[b]);
	win_present ();
	sent = b;
	waiting = 1;
}

void qg_win_draw (unsigned char *pixels, int *pal)
{
	int b;
	if (closed)
		return;
	// the buffer the desktop is not reading
	b = waiting ? 1 - sent : sent;
	memcpy (fbuf[b], pixels, QG_PIXELS);
	memcpy (fpal[b], pal, 256 * sizeof(int));
	if (waiting)
		pending = 1;
	else
		send_frame (b);
}

// the window's next key, as a browser keyCode and whether it went down:
// 1, 0 for none waiting, -1 once the window has closed
int qg_win_key (int *code, int *down)
{
	int ev[3], k;
	if (closed)
		return 0 - 1;
	while (1)
	{
		k = win_poll (ev);
		if (k == WIN_NONE)
			return 0;
		if (k < 0)
		{
			closed = 1;
			return 0 - 1;
		}
		if (k == WIN_FRAME)
		{
			// the desktop has drawn what it had: the newer frame, if any
			waiting = 0;
			if (pending)
			{
				pending = 0;
				send_frame (1 - sent);
			}
			continue;
		}
		if (k == WIN_KEY || k == WIN_KEYUP)
		{
			*code = ev[0];
			*down = k == WIN_KEY;
			return 1;
		}
	}
}

void qg_win_close (void)
{
	if (!closed)
	{
		win_close ();
		closed = 1;
	}
}
