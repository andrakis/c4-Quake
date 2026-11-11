// qg-snd.c -- Quake's sound on libjs's card (libjs/guest/snd.h), in place of
// snd_dma.c and its mixer: sixteen channels of samples, which the host mixes, so a
// sound costs the game a file to load and a few volumes, not a mix a frame.
//
//   0, 1   the level's ambience, water and wind, by the leaf the player is in
//   2-5    the four loudest of the looped sounds a map places (torches, lava,
//          machines), one of each
//   6-15   everything that starts: shots, footsteps, monsters, doors and lifts
//
// Ten for what starts, where Quake's own mixer has eight. With four, a fight cut one
// sound off for another a few times a second, and a lift's loop took one of the four
// for as long as it moved (E1M3, demo1). A card without SND_HAS_PCM16 has eight
// channels: then two ambience, two placed and four that start, as before.
//
// Which channel a sound that starts takes is snd_dma.c's SND_PickChannel: the same
// entity's same channel (any of them for channel -1), else the one closest to its end,
// but never the player's own sound for another entity's, and a loop last of all (it
// has no end of its own: snd_dma.c could take a lift's loop for a ricochet just as it
// went round, and the lift was then silent until it stopped). A silent sound (null.wav,
// which doors and lifts play to say nothing) only stops what it overrides, and a
// temporary entity's (a ricochet, an explosion) is placed where it is, as Quake
// places it, not played at full volume wherever it was.
//
// Placed in the stereo field by the formula snd_dma.c's SND_Spatialize has, again
// about twenty times a second, for what moves and for the player turning. The card
// has no loop: a looped sound is played again, from its cue point, in the frame that
// finds it has ended. -nosound leaves everything as snd_null.c has it, so that a
// timedemo's frames do not change.
#include "quakedef.h"
#include "snd.h"

enum { MAX_SFX = 512, MAX_STATIC = 128, MAX_CAND = 32, CH_AMBIENT = 0, CH_STATIC = 2,
	MAX_NSTATIC = 4, MAX_NDYN = 10 };

cvar_t bgmvolume = {"bgmvolume", "1", true};
cvar_t volume = {"volume", "0.7", true};
cvar_t ambient_level = {"ambient_level", "0.3"};
cvar_t ambient_fade = {"ambient_fade", "100"};

// what a sound is, in the cache: unsigned 8-bit mono
typedef struct {
	int len, rate;
	int loop;		// where a loop starts again, -1 for none
	int silent;		// nothing to hear in it
	unsigned char data[4];
} qsnd_t;

typedef struct {
	sfx_t *sfx;
	int entnum, entchannel;
	vec3_t origin;
	float dist_mult;
	int master;		// 0-255
	int left, right;	// as last told to the card
	int loop;		// qsnd_t's
	double end;		// when it will have finished, by the host's clock
} qchan_t;

typedef struct {
	sfx_t *sfx;
	vec3_t origin;
	float dist_mult;
	int master;
	int left, right;	// this update's
} qstatic_t;

static int on, ambient_on = 1;
static int nstat, ch_dyn, ndyn;	// how many placed, where those that start begin, how many
static sfx_t known[MAX_SFX];
static int nknown;
static qchan_t dyn[MAX_NDYN];
static qstatic_t statics[MAX_STATIC];
static int nstatic;
static sfx_t *static_now[MAX_NSTATIC];
static int static_vol[MAX_NSTATIC];
static sfx_t *ambient_sfx[2];
static int ambient_vol[2];
static double last_update, last_ambient;
static vec3_t l_origin, l_right;

// ---- the sounds ----

static int w_short (byte *p) { return p[0] | (p[1] << 8); }
static int w_long (byte *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); }

// the chunk NAME in a WAV, its bytes, 0 if it has none
static byte *w_chunk (byte *data, int size, char *name, int *len)
{
	byte *p, *end;
	int n;
	end = data + size;
	p = data + 12;
	while (p + 8 <= end)
	{
		n = w_long (p + 4);
		// (some of the pak's own WAVs have a length past their end)
		if (n < 0 || n > end - (p + 8))
		{
			if (p[0] != name[0] || p[1] != name[1] || p[2] != name[2] || p[3] != name[3])
				return 0;
			n = end - (p + 8);
		}
		if (p[0] == name[0] && p[1] == name[1] && p[2] == name[2] && p[3] == name[3])
		{
			*len = n;
			return p + 8;
		}
		p = p + 8 + ((n + 1) & ~1);
	}
	return 0;
}

static qsnd_t *qs_load (sfx_t *s)
{
	qsnd_t *q;
	char name[MAX_QPATH + 8];
	byte *d, *fmt, *pcm, *cue;
	int size, flen, plen, clen, bits, rate, count, i, v;
	if ((q = (qsnd_t *)Cache_Check (&s->cache)))
		return q;
	strcpy (name, "sound/");
	strcat (name, s->name);
	d = COM_LoadTempFile (name);
	if (!d)
	{
		Con_Printf ("Couldn't load %s\n", name);
		return 0;
	}
	size = com_filesize;
	if (size < 44 || d[0] != 'R' || d[8] != 'W' || !(fmt = w_chunk (d, size, "fmt ", &flen)) || !(pcm = w_chunk (d, size, "data", &plen)))
	{
		Con_Printf ("%s is not a WAV\n", s->name);
		return 0;
	}
	if (w_short (fmt) != 1 || w_short (fmt + 2) != 1)
	{
		Con_Printf ("%s is not mono PCM\n", s->name);
		return 0;
	}
	rate = w_long (fmt + 4);
	bits = w_short (fmt + 14);
	count = bits == 16 ? plen / 2 : plen;
	q = (qsnd_t *)Cache_Alloc (&s->cache, sizeof(qsnd_t) + count, s->name);
	if (!q)
		return 0;
	q->len = count;
	q->rate = rate;
	// a cue point loops it, from the first point's sample (GetWavinfo's loopstart)
	q->loop = -1;
	if ((cue = w_chunk (d, size, "cue ", &clen)))
	{
		q->loop = clen >= 28 ? w_long (cue + 24) : 0;
		if (q->loop < 0 || q->loop >= count)
			q->loop = 0;
	}
	if (bits == 16)
	{
		// the card's samples are 8-bit: the high byte, made unsigned
		for (i = 0 ; i < count ; i++)
			q->data[i] = pcm[i * 2 + 1] ^ 128;
	}
	else
		memcpy (q->data, pcm, count);
	q->silent = 1;
	for (i = 0 ; i < count && q->silent ; i++)
	{
		v = q->data[i] - 128;
		if (v > 2 || v < -2)
			q->silent = 0;
	}
	return q;
}

static sfx_t *qs_find (char *name)
{
	int i;
	if (!name || Q_strlen (name) >= MAX_QPATH)
		return 0;
	for (i = 0 ; i < nknown ; i++)
		if (!Q_strcmp (known[i].name, name))
			return &known[i];
	if (nknown == MAX_SFX)
		Sys_Error ("S_FindName: out of sfx_t");
	strcpy (known[nknown].name, name);
	return &known[nknown++];
}

sfx_t *S_PrecacheSound (char *sample)
{
	sfx_t *s;
	if (!on)
		return NULL;
	s = qs_find (sample);
	if (s)
		qs_load (s);
	return s;
}

void S_TouchSound (char *sample)
{
	sfx_t *s;
	if (on && (s = qs_find (sample)))
		Cache_Check (&s->cache);
}

void S_ClearPrecache (void) {}
void S_BeginPrecaching (void) {}
void S_EndPrecaching (void) {}
void S_ClearBuffer (void) {}
void S_ExtraUpdate (void) {}
void S_AmbientOff (void) { ambient_on = 0; }
void S_AmbientOn (void) { ambient_on = 1; }

// ---- where a sound is ----

static int qs_clamp (int v)
{
	v = (int)(v * volume.value);
	return v < 0 ? 0 : v > 255 ? 255 : v;
}

static void qs_place (vec3_t origin, float dist_mult, int master, int *left, int *right)
{
	vec3_t v;
	float dist, dot, scale;
	VectorSubtract (origin, l_origin, v);
	dist = VectorNormalize (v) * dist_mult;
	dot = DotProduct (l_right, v);
	scale = (1.0 - dist) * (1.0 + dot);
	*right = qs_clamp ((int)(master * scale));
	scale = (1.0 - dist) * (1.0 - dot);
	*left = qs_clamp ((int)(master * scale));
}

// the player's own sounds are everywhere; the rest, temporary entities' (-1:
// ricochets, explosions) too, are where they are
static void qs_dyn_place (qchan_t *c)
{
	if (c->entnum == cl.viewentity)
	{
		c->left = qs_clamp (c->master);
		c->right = c->left;
		return;
	}
	qs_place (c->origin, c->dist_mult, c->master, &c->left, &c->right);
}

// ---- what starts ----

void S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	qchan_t *c;
	qsnd_t *q;
	double now, life, best;
	int i, k, m, master, left, right;
	float dist_mult;
	if (!on || !sfx || !(q = qs_load (sfx)))
		return;
	now = Sys_FloatTime ();
	// the same entity's same channel is overridden: by this sound, or by silence
	m = -1;
	if (entchannel)
		for (i = 0 ; i < ndyn && m < 0 ; i++)
			if (dyn[i].sfx && dyn[i].entnum == entnum && (dyn[i].entchannel == entchannel || entchannel == -1))
				m = i;
	dist_mult = attenuation / 1000.0;
	master = (int)(fvol * 255);
	if (entnum == cl.viewentity)
	{
		left = qs_clamp (master);
		right = left;
	}
	else
		qs_place (origin, dist_mult, master, &left, &right);
	if (q->silent || (!left && !right))
	{
		if (m >= 0)
		{
			snd_stop (ch_dyn + m);
			dyn[m].sfx = 0;
		}
		return;
	}
	// else a free channel, else the one nearest its end, but not the player's for another's
	k = m;
	if (k < 0)
	{
		best = 1e30;
		for (i = 0 ; i < ndyn ; i++)
		{
			if (!dyn[i].sfx)
				life = -1;
			else if (dyn[i].entnum == cl.viewentity && entnum != cl.viewentity)
				continue;
			else if (dyn[i].loop >= 0)
				life = 1e9;	// (a loop near its end is only going round again)
			else
				life = dyn[i].end - now;
			if (life < best)
			{
				best = life;
				k = i;
			}
		}
		if (k < 0)
			return;
	}
	c = &dyn[k];
	c->sfx = sfx;
	c->entnum = entnum;
	c->entchannel = entchannel;
	VectorCopy (origin, c->origin);
	c->dist_mult = dist_mult;
	c->master = master;
	c->left = left;
	c->right = right;
	c->loop = q->loop;
	c->end = now + (double)q->len / q->rate;
	snd_play (ch_dyn + k, (char *)q->data, q->len, q->rate, c->left | (c->right << 8));
}

void S_StopSound (int entnum, int entchannel)
{
	int i;
	if (!on)
		return;
	for (i = 0 ; i < ndyn ; i++)
		if (dyn[i].sfx && dyn[i].entnum == entnum && dyn[i].entchannel == entchannel)
		{
			snd_stop (ch_dyn + i);
			dyn[i].sfx = 0;
		}
}

void S_StopAllSounds (qboolean clear)
{
	int i;
	if (!on)
		return;
	snd_stop (0 - 1);
	nstatic = 0;
	for (i = 0 ; i < ndyn ; i++)
		dyn[i].sfx = 0;
	for (i = 0 ; i < nstat ; i++)
		static_now[i] = 0;
	ambient_vol[0] = 0;
	ambient_vol[1] = 0;
}

void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	qstatic_t *s;
	qsnd_t *q;
	if (!on || !sfx || nstatic == MAX_STATIC || !(q = qs_load (sfx)) || q->loop < 0)
		return;
	s = &statics[nstatic++];
	s->sfx = sfx;
	VectorCopy (origin, s->origin);
	s->master = (int)vol;
	s->dist_mult = (attenuation / 64) / 1000.0;
}

void S_LocalSound (char *sound)
{
	sfx_t *s;
	if (!on || !(s = S_PrecacheSound (sound)))
		return;
	S_StartSound (cl.viewentity, -1, s, vec3_origin, 1, 1);
}

// ---- loops: every frame, so a loop waits at most a frame to go round ----

// sfx on channel ch: from the start (restart), or round its loop again if it has
// ended, or else at a new volume
static void qs_loop (int ch, sfx_t *sfx, int vol, int restart, int busy)
{
	qsnd_t *q;
	if (restart || !(busy & (1 << ch)))
	{
		if (!(q = qs_load (sfx)))
			return;
		if (restart || q->loop < 0)
			snd_play (ch, (char *)q->data, q->len, q->rate, vol);
		else
			snd_play (ch, (char *)q->data + q->loop, q->len - q->loop, q->rate, vol);
	}
	else
		snd_volume (ch, vol);
}

static void qs_rounds (int busy, double now)
{
	qchan_t *c;
	qsnd_t *q;
	int i;
	for (i = 0 ; i < ndyn ; i++)
	{
		c = &dyn[i];
		if (!c->sfx || c->end > now)
			continue;
		if (c->loop < 0 || !(q = qs_load (c->sfx)))
		{
			c->sfx = 0;
			continue;
		}
		if (!(busy & (1 << (ch_dyn + i))))
		{
			snd_play (ch_dyn + i, (char *)q->data + c->loop, q->len - c->loop, q->rate, c->left | (c->right << 8));
			c->end = now + (double)(q->len - c->loop) / q->rate;
		}
	}
	for (i = 0 ; i < nstat ; i++)
		if (static_now[i] && !(busy & (1 << (CH_STATIC + i))))
			qs_loop (CH_STATIC + i, static_now[i], static_vol[i], 0, busy);
	for (i = 0 ; i < 2 ; i++)
		if (ambient_vol[i] && ambient_sfx[i] && !(busy & (1 << (CH_AMBIENT + i))))
			qs_loop (CH_AMBIENT + i, ambient_sfx[i], qs_clamp (ambient_vol[i]) * 257, 0, busy);
}

// ---- about twenty times a second ----

static void qs_ambient (int busy, double now)
{
	mleaf_t *l;
	int k, want, step;
	if (!cl.worldmodel || cls.state != ca_connected || !cl.worldmodel->nodes)
		return;
	l = Mod_PointInLeaf (l_origin, cl.worldmodel);
	step = (int)((now - last_ambient) * ambient_fade.value);
	last_ambient = now;
	if (step < 1)
		step = 1;
	for (k = 0 ; k < 2 ; k++)
	{
		want = l && ambient_on && ambient_level.value ? (int)(ambient_level.value * l->ambient_sound_level[k]) : 0;
		if (want < 8)
			want = 0;
		if (ambient_vol[k] < want)
			ambient_vol[k] = ambient_vol[k] + step > want ? want : ambient_vol[k] + step;
		else if (ambient_vol[k] > want)
			ambient_vol[k] = ambient_vol[k] - step < want ? want : ambient_vol[k] - step;
		if (!ambient_vol[k])
		{
			if (busy & (1 << (CH_AMBIENT + k)))
				snd_stop (CH_AMBIENT + k);
			continue;
		}
		qs_loop (CH_AMBIENT + k, ambient_sfx[k], qs_clamp (ambient_vol[k]) * 257, 0, busy);
	}
}

// the loudest of each looped sound the map places, the loudest few of those on the
// channels for them, each staying on the channel it has while it is chosen
static void qs_statics (int busy)
{
	qstatic_t *s, *cand[MAX_CAND], *pick[MAX_NSTATIC];
	int loud[MAX_CAND], i, j, k, n, np, l, used[MAX_NSTATIC];
	n = 0;
	for (i = 0 ; i < nstatic ; i++)
	{
		s = &statics[i];
		qs_place (s->origin, s->dist_mult, s->master, &s->left, &s->right);
		l = s->left > s->right ? s->left : s->right;
		if (!l)
			continue;
		// (two of one sound would only be louder)
		for (j = 0 ; j < n && cand[j]->sfx != s->sfx ; j++)
			;
		if (j == n)
		{
			if (n < MAX_CAND)
				n++;
			else
			{
				// as many sounds as that: the quietest gives way
				for (j = 0, k = 1 ; k < n ; k++)
					if (loud[k] < loud[j])
						j = k;
				if (loud[j] >= l)
					continue;
			}
		}
		else if (loud[j] >= l)
			continue;
		cand[j] = s;
		loud[j] = l;
	}
	// the loudest nstat of them
	for (np = 0 ; np < nstat && np < n ; np++)
	{
		for (j = np, k = np + 1 ; k < n ; k++)
			if (loud[k] > loud[j])
				j = k;
		s = cand[j]; cand[j] = cand[np]; cand[np] = s;
		l = loud[j]; loud[j] = loud[np]; loud[np] = l;
		pick[np] = cand[np];
	}
	// those already playing stay where they are; the rest take the channels left
	for (k = 0 ; k < nstat ; k++)
		used[k] = 0;
	for (i = 0 ; i < np ; i++)
		for (k = 0 ; k < nstat ; k++)
			if (!used[k] && static_now[k] == pick[i]->sfx)
			{
				used[k] = 1;
				static_vol[k] = pick[i]->left | (pick[i]->right << 8);
				qs_loop (CH_STATIC + k, pick[i]->sfx, static_vol[k], 0, busy);
				pick[i] = 0;
				break;
			}
	for (i = 0 ; i < np ; i++)
	{
		if (!pick[i])
			continue;
		for (k = 0 ; used[k] ; k++)
			;
		used[k] = 1;
		static_now[k] = pick[i]->sfx;
		static_vol[k] = pick[i]->left | (pick[i]->right << 8);
		qs_loop (CH_STATIC + k, pick[i]->sfx, static_vol[k], 1, busy);
	}
	for (k = 0 ; k < nstat ; k++)
		if (!used[k] && static_now[k])
		{
			snd_stop (CH_STATIC + k);
			static_now[k] = 0;
		}
}

void S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up)
{
	double now;
	qchan_t *c;
	int i, l, r, busy;
	if (!on)
		return;
	VectorCopy (origin, l_origin);
	VectorCopy (v_right, l_right);
	now = Sys_FloatTime ();
	busy = snd_busy ();
	qs_rounds (busy, now);
	if (now - last_update < 0.05)
		return;
	last_update = now;
	for (i = 0 ; i < ndyn ; i++)
	{
		c = &dyn[i];
		if (!c->sfx)
			continue;
		if (c->entnum > 0 && c->entnum < cl.num_entities && c->entnum != cl.viewentity)
			VectorCopy (cl_entities[c->entnum].origin, c->origin);
		l = c->left;
		r = c->right;
		qs_dyn_place (c);
		if (l != c->left || r != c->right)
			snd_volume (ch_dyn + i, c->left | (c->right << 8));
	}
	busy = snd_busy ();
	qs_statics (busy);
	qs_ambient (busy, now);
}

static void qs_play_f (void)
{
	if (Cmd_Argc () > 1)
		S_LocalSound (Cmd_Argv (1));
}

void S_Init (void)
{
	int caps;
	Cvar_RegisterVariable (&bgmvolume);
	Cvar_RegisterVariable (&volume);
	Cvar_RegisterVariable (&ambient_level);
	Cvar_RegisterVariable (&ambient_fade);
	Cmd_AddCommand ("play", qs_play_f);
	Cmd_AddCommand ("stopsound", qs_play_f);
	if (COM_CheckParm ("-nosound") || !snd_present () || !((caps = snd_caps ()) & SND_HAS_PCM))
		return;
	on = 1;
	// sixteen channels, or eight on a card from before it had sixteen
	nstat = caps & SND_HAS_PCM16 ? 4 : 2;
	ch_dyn = CH_STATIC + nstat;
	ndyn = (caps & SND_HAS_PCM16 ? 16 : 8) - ch_dyn;
	Con_Printf ("Sound: the machine's card, %d channels\n", ch_dyn + ndyn);
	ambient_sfx[0] = S_PrecacheSound ("ambience/water1.wav");
	ambient_sfx[1] = S_PrecacheSound ("ambience/wind2.wav");
}

void S_Shutdown (void)
{
	if (on)
		snd_stop (0 - 1);
	on = 0;
}
