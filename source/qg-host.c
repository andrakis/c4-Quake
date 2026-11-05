// qg-host.c -- the port's platform on a host, built with gcc: no window and no
// sound. The reference the c4lc build is held to: a timedemo's frames are
// checksummed (qg-sum.c).
#include "quakedef.h"
#include "quakegeneric.h"
#include <unistd.h>
#include <sys/time.h>

void qg_sum_frame (void *pixels);
void qg_sum_palette (unsigned char *palette);

void QG_Init (void) {}
void QG_Quit (void) {}
int QG_GetKey (int *down, int *key) { return 0; }
void QG_GetMouseMove (int *x, int *y) { *x = 0; *y = 0; }
void QG_GetJoyAxes (float *axes) { *axes = 0; }
void QG_SetPalette (unsigned char palette[768]) { qg_sum_palette (palette); }
void QG_DrawFrame (void *pixels) { qg_sum_frame (pixels); }
unsigned qg_ms (void)
{
	struct timeval tv;
	gettimeofday (&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
void qg_sleep (int ms) { usleep (ms * 1000); }
