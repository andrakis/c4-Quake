// qg-sum.c -- what every build of the port shares: the main loop, and a
// timedemo's frames checksummed (with -sum), so that c4lc's frames can be held
// to gcc's. Without a display yet: this is the baseline's platform.
#include "quakedef.h"
#include "quakegeneric.h"
#include <unistd.h>

// c4's disks are flat: id1/pak0.pak is pak0.pak beside the game. That name first:
// on C4IX it is a file of the filesystem, which can seek, while the disk also
// answers the full path, by its last part, with a stream that cannot.
#undef fopen
FILE *qg_fopen (char *path, char *mode)
{
	char *b, *q;
	FILE *f;
	b = path;
	for (q = path ; *q ; q++)
		if (*q == '/')
			b = q + 1;
	if ((f = fopen (b, mode)))
		return f;
	return fopen (path, mode);
}

static int qg_timing;
static unsigned rand_s = 1;
int qg_rand (void)
{
	rand_s = rand_s * 1103515245u + 12345u;
	return (rand_s >> 16) & 0x7fff;
}

unsigned qg_ms (void);
void qg_sleep (int ms);

// The clock: the machine's, but while frames are being checksummed, when it is one
// that counts calls, so that the two builds see the same times.
static int virtclock;
double qg_now (void)
{
	static double t;
	if (virtclock)
	{
		t += 0.1;
		return t;
	}
	return qg_ms () / 1000.0;
}

static int frames, dosum, stopat, dump;
static unsigned ms0;
static unsigned sum;

static unsigned char pal[768];
void qg_sum_palette (unsigned char *palette)
{
	int i;
	for (i = 0 ; i < 768 ; i++)
		pal[i] = palette[i];
}

void qg_sum_frame (void *pixels)
{
	unsigned char *p;
	int i;
	unsigned s;
	if (!frames)
		ms0 = qg_ms ();
	++frames;
	if (!dosum)
		return;
	p = pixels;
	s = sum;
	for (i = 0 ; i < QUAKEGENERIC_RES_X * QUAKEGENERIC_RES_Y ; i++)
		s = s * 31 + p[i];
	for (i = 0 ; i < 768 ; i++)
		s = s * 31 + pal[i];
	sum = s;
	// every so often, so that where two builds part can be found
	if (dosum > 1 && frames % dosum == 0)
		printf ("sum: frame %d %08x\n", frames, s);
	// the view, as bits, and one frame's every pixel: what part of it parts
	if (dump && frames >= dump && frames < dump + 3)
	{
		int x, y, *v;
		extern vec3_t r_origin, vpn, vright, vup;
		extern float xcenter, ycenter, xscale, yscale, xscaleinv, yscaleinv, xscaleshrink, yscaleshrink, scale_for_mip;
		for (i = 0 ; i < 3 ; i++)
			printf ("view: %d %08x %08x %08x %08x\n", i, *(int *)&r_origin[i], *(int *)&vpn[i], *(int *)&vright[i], *(int *)&vup[i]);
		printf ("view: scales %08x %08x %08x %08x %08x %08x\n", *(int *)&xcenter, *(int *)&ycenter, *(int *)&xscale, *(int *)&yscale, *(int *)&xscaleinv, *(int *)&scale_for_mip);
		for (y = 0 ; y < QUAKEGENERIC_RES_Y ; y++)
		{
			printf ("pix: %d %d ", frames, y);
			for (x = 0 ; x < QUAKEGENERIC_RES_X ; x++)
				printf ("%02x", p[y * QUAKEGENERIC_RES_X + x]);
			printf ("\n");
		}
	}
}

void qg_done (void)
{
	// by the machine's real clock from the first frame: in the page, the wall's.
	// (Quake's own line divides by the fixed steps a timedemo is fed.)
	{
		unsigned ms;
		ms = qg_ms () - ms0;
		if (!ms)
			ms = 1;
		printf ("speed: %d frames in %d.%03d s, %d.%d fps\n", frames, (int)(ms / 1000), (int)(ms % 1000),
			(int)(frames * 1000 / ms), (int)(frames * 10000 / ms % 10));
	}
	printf ("timedemo: %d frames, checksum %08x\n", frames, sum);
	exit (0);
}

int main(int argc, char **argv)
{
	int started, i, timing;
	double oldtime, newtime;
	started = 0;
	timing = 0;
	for (i = 1 ; i < argc ; i++)
	{
		if (!strcmp (argv[i], "-sum"))
		{
			dosum = i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9' ? atoi (argv[i + 1]) : 1;
			virtclock = 1;
		}
		if (!strcmp (argv[i], "-frames") && i + 1 < argc)
			stopat = atoi (argv[i + 1]);
		if (!strcmp (argv[i], "-dump") && i + 1 < argc)
			dump = atoi (argv[i + 1]);
		if (!strcmp (argv[i], "+timedemo"))
			timing = 1;
	}
	// On C4IX the game lives in /usr/games/quake, and starts there; on the bare
	// machine and under C4DOS there is only the one directory, and the chdir fails.
	chdir ("/usr/games/quake");
	QG_Create (argc, argv);
	if (timing)
	{
		// a fixed step, so that every build plays the same frames
		for (;;)
		{
			QG_Tick (0.05);
			if (cls.timedemo)
				started = 1;
			if ((started && !cls.timedemo) || (stopat && frames >= stopat))
				break;
		}
		qg_done ();
	}
	Cbuf_AddText ("+mlook\n");		// the mouse looks, as players now expect
	oldtime = qg_now () - 0.1;
	for (;;)
	{
		newtime = qg_now ();
		if (newtime - oldtime < 1.0 / 72.0)
		{
			qg_sleep (1);
			continue;
		}
		QG_Tick (newtime - oldtime);
		oldtime = newtime;
	}
	return 0;
}
