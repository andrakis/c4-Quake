// qg-bare.c -- on the bare machine and under C4DOS the display is the game's to
// take: there is no one to ask (qg-gui.c). Except under grin, C4DOS's graphical
// shell, which keeps the display and publishes a table in C4DOS's slot 25 while it
// does (c4's docs/grin-plan.md 3.6): then the game plays in a window of grin's.
#define C4IX_SHARED 1
#include "gui.h"
#undef C4IX_SHARED
#include "quakegeneric.h"

// grin's table (c4's src/grin/gkit.h): its first eight words
struct grin_table {
	int magic, version, gen;
	void *env;
	void (*hello)(void);
	int  (*fwin_open)(char *title, int w, int h, int flags);
	void (*fwin_frame)(char *pixels, int w, int h, int pitch, int *pal);
	int  (*fwin_poll)(int *ev);
};
extern int *__c4dos_api;		// bare.c's: C4DOS's table, 0 on the bare machine

static struct grin_table *grin(void)
{
	struct grin_table *t;
	if (!__c4dos_api || __c4dos_api[0] != 4404292 || __c4dos_api[5] < 4)
		return 0;
	t = (struct grin_table *)__c4dos_api[25];
	return t && t->magic == 1196575054 && t->version >= 2 ? t : 0;
}

static struct grin_table *g;

int qg_take (void) { return grin () == 0; }
void qg_give (void) {}
int qg_win_open (void) { return (g = grin ()) != 0 && g->fwin_open ("Quake", QUAKEGENERIC_RES_X, QUAKEGENERIC_RES_Y, 0); }
void qg_win_draw (unsigned char *pixels, int *pal) { if (g) g->fwin_frame ((char *)pixels, QUAKEGENERIC_RES_X, QUAKEGENERIC_RES_Y, QUAKEGENERIC_RES_X, pal); }

// the window's next key, as a browser keyCode and whether it went down:
// 1, 0 for none waiting, -1 once the window has closed
int qg_win_key (int *code, int *down)
{
	int ev[4], k;
	if (!g)
		return 0 - 1;
	while ((k = g->fwin_poll (ev)) > 0)
	{
		if (ev[0] == GUI_EV_KEYDOWN || ev[0] == GUI_EV_KEYUP)
		{
			*code = ev[1];
			*down = ev[0] == GUI_EV_KEYDOWN;
			return 1;
		}
	}
	return k;
}

void qg_win_close (void) {}
