// qg-gui.c -- the port's platform on libjs's display (libjs/guest/gui.h): C4IX,
// where the desktop gives the display way until the game ends (qg-c4ix.c), and
// the bare machine and C4DOS (qg-bare.c).
//
// Quake's 320x240 frames go out 8-bit, as GUI_BLIT8 with their palette, and the
// host scales them to 640x480. Keys come from the display's event ring as Quake's
// own key codes; the mouse is locked to the display from the first click until Esc
// (GUI_PTR_LOCK) and its motion is relative. A timedemo runs without the display:
// its frames are checksummed, not shown, unless -show is given too, so that the
// time it takes counts the drawing.
#include "quakedef.h"
#include "quakegeneric.h"
#include "quakekeys.h"
#include "gui.h"

int  qg_take (void);		// the display is ours to use (qg-c4ix.c, qg-bare.c)
void qg_give (void);
int  qg_win_open (void);		// or a window (C4IX's Terminal, grin's)
void qg_win_draw (unsigned char *pixels, int *pal);
int  qg_win_key (int *code, int *down);
void qg_win_close (void);
void qg_sum_frame (void *pixels);	// qg-sum.c
void qg_sum_palette (unsigned char *palette);

enum { QW = 640, QH = 480, QRING = 65536, QKQ = 128 };

static int on;			// the display is ours
static int win;			// or the game plays in a window
static int *ring;
static int pal[256];		// 0x00RRGGBB
static int kq[QKQ];		// keys waiting: (down << 9) | Quake's code
static int kh, kt;
static int mdx, mdy;		// the mouse's motion since Quake last asked

static void qg_close (void)
{
	if (!on)
		return;
	gui_cursor (GUI_PTR_ARROW);
	gui_detach ();
	qg_give ();
	on = 0;
}

static void qg_unwin (void) { qg_win_close (); }

static int qg_window (void)
{
	if (!(win = qg_win_open ()))
		return 0;
	atexit (qg_unwin);
	return 1;
}

void QG_Init (void)
{
	if (COM_CheckParm ("+timedemo") && !COM_CheckParm ("-show"))
		return;
	if ((COM_CheckParm ("-window") || COM_CheckParm ("--window")) && qg_window ())
		return;
	if (!gui_present ())
	{
		printf ("quake: this machine has no display\n");
		return;
	}
	if (!qg_take ())
	{
		if (qg_window ())
		{
			printf ("quake: a program has the display; playing in the window\n");
			return;
		}
		printf ("quake: a program has the display\n");
		return;
	}
	if (!(ring = (int *)malloc (QRING)) || !gui_attach (ring, QRING, QW, QH))
	{
		qg_give ();
		return;
	}
	gui_events (GUI_M_KEYS | GUI_M_MOVE | GUI_M_BUTTONS | GUI_M_WINDOW | GUI_M_WHEEL | GUI_M_REL);
	gui_cursor (GUI_PTR_LOCK);
	gui_clear (0x000000);
	gui_show ();
	on = 1;
	atexit (qg_close);
}

void QG_Quit (void)
{
	qg_close ();
}

void QG_SetPalette (unsigned char palette[768])
{
	int i;
	unsigned char *p;
	qg_sum_palette (palette);
	p = palette;
	for (i = 0 ; i < 256 ; i++, p += 3)
		pal[i] = (p[0] << 16) | (p[1] << 8) | p[2];
}

void QG_DrawFrame (void *pixels)
{
	qg_sum_frame (pixels);
	if (win)
	{
		qg_win_draw ((unsigned char *)pixels, pal);
		return;
	}
	if (!on)
		return;
	gui_blit8 (0, 0, QUAKEGENERIC_RES_X, QUAKEGENERIC_RES_Y, (char *)pixels, QUAKEGENERIC_RES_X, pal, QW, QH);
	gui_show ();
}

// a browser keyCode as Quake names the key, 0 for one it has no use for
static int qkey (int code)
{
	if (code >= 65 && code <= 90)
		return code + 32;	// letters, lower case
	if (code >= 48 && code <= 57)
		return code;		// digits
	if (code >= 112 && code <= 123)
		return K_F1 + (code - 112);
	switch (code)
	{
	case 37: return K_LEFTARROW;
	case 38: return K_UPARROW;
	case 39: return K_RIGHTARROW;
	case 40: return K_DOWNARROW;
	case 13: return K_ENTER;
	case 27: return K_ESCAPE;
	case 9: return K_TAB;
	case 8: return K_BACKSPACE;
	case 32: return K_SPACE;
	case 17: return K_CTRL;
	case 16: return K_SHIFT;
	case 18: return K_ALT;
	case 19: return K_PAUSE;
	case 36: return K_HOME;
	case 35: return K_END;
	case 33: return K_PGUP;
	case 34: return K_PGDN;
	case 45: return K_INS;
	case 46: return K_DEL;
	case 61: case 187: return '=';
	case 173: case 189: return '-';
	case 188: return ',';
	case 190: return '.';
	case 191: return '/';
	case 186: case 59: return ';';
	case 222: return '\'';
	case 219: return '[';
	case 221: return ']';
	case 220: return '\\';
	case 192: return '`';
	}
	return 0;
}

static void qg_queue (int k, int down)
{
	if (!k || (kt + 1) % QKQ == kh)
		return;
	kq[kt] = (down ? 512 : 0) | k;
	kt = (kt + 1) % QKQ;
}

static void qg_pump (void)
{
	int ev[4], b, k, code, down;
	// the window closed: the game ends, as it does when it is quit
	while (win && (k = qg_win_key (&code, &down)) != 0)
	{
		if (k < 0)
			exit (0);
		qg_queue (qkey (code), down);
	}
	while (on && gui_poll (ev))
	{
		if (ev[0] == GUI_EV_KEYDOWN || ev[0] == GUI_EV_KEYUP)
			qg_queue (qkey (ev[1]), ev[0] == GUI_EV_KEYDOWN);
		else if (ev[0] == GUI_EV_REL)
		{
			mdx = mdx + ev[1];
			mdy = mdy + ev[2];
		}
		else if (ev[0] == GUI_EV_DOWN || ev[0] == GUI_EV_UP)
		{
			b = ev[3] == 0 ? K_MOUSE1 : ev[3] == 1 ? K_MOUSE3 : K_MOUSE2;
			qg_queue (b, ev[0] == GUI_EV_DOWN);
		}
		else if (ev[0] == GUI_EV_WHEEL)
		{
			b = ev[3] < 0 ? K_MWHEELUP : K_MWHEELDOWN;
			qg_queue (b, 1);
			qg_queue (b, 0);
		}
	}
}

int QG_GetKey (int *down, int *key)
{
	qg_pump ();
	if (kh == kt)
		return 0;
	*down = kq[kh] >> 9;
	*key = kq[kh] & 511;
	kh = (kh + 1) % QKQ;
	return 1;
}

void QG_GetMouseMove (int *x, int *y)
{
	qg_pump ();
	*x = mdx;
	*y = mdy;
	mdx = 0;
	mdy = 0;
}

void QG_GetJoyAxes (float *axes)
{
	int i;
	for (i = 0 ; i < QUAKEGENERIC_JOY_MAX_AXES ; i++)
		axes[i] = 0;
}

unsigned qg_ms (void) { return __time (); }
void qg_sleep (int ms) { if (ms > 0) __c4_usleep (ms * 1000); }
