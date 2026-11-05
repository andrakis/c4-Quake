// qg-snd.c -- Quake's sound on libjs's card (libjs/guest/snd.h), in place of
// snd_dma.c and its mixer: eight channels of samples, which the host mixes, so a
// sound costs the game a file to load and a few volumes, not a mix a frame.
//
//   0, 1   the level's ambience, water and wind, by the leaf the player is in
//   2, 3   the two loudest of the sounds a map places (torches, lava, machines)
//   4-7    everything that starts: shots, footsteps, monsters
//
// Placed in the stereo field by the formula snd_dma.c's SND_Spatialize has, again
// about twenty times a second, for what moves and for the player turning. The card
// has no loop: a looped sound is played again when it has ended. -nosound leaves
// everything as snd_null.c has it, so that a timedemo's frames do not change.
#include "quakedef.h"
#include "snd.h"

enum { MAX_SFX = 512, MAX_STATIC = 128, CH_AMBIENT = 0, CH_STATIC = 2, NSTATIC = 2, CH_DYN = 4, NDYN = 4, NCH = 8 };

cvar_t bgmvolume = {"bgmvolume", "1", true};
cvar_t volume = {"volume", "0.7", true};
cvar_t ambient_level = {"ambient_level", "0.3"};
cvar_t ambient_fade = {"ambient_fade", "100"};

// what a sound is, in the cache: unsigned 8-bit mono
typedef struct {
	int len, rate, loop;
	unsigned char data[4];
} qsnd_t;

typedef struct {
	sfx_t *sfx;
	int entnum, entchannel;
	vec3_t origin;
	float dist_mult;
	int master;		// 0-255
	int left, right;	// as last told to the card
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
static sfx_t known[MAX_SFX];
static int nknown;
static qchan_t dyn[NDYN];
static qstatic_t statics[MAX_STATIC];
static int nstatic;
static sfx_t *static_now[NSTATIC];
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
	byte *d, *fmt, *pcm;
	int size, n, flen, plen, clen, bits, rate, count, i;
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
	q->loop = w_chunk (d, size, "cue ", &clen) != 0;
	if (bits == 16)
	{
		// the card's samples are 8-bit: the high byte, made unsigned
		for (i = 0 ; i < count ; i++)
			q->data[i] = pcm[i * 2 + 1] ^ 128;
	}
	else
		memcpy (q->data, pcm, count);
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

static void qs_dyn_place (qchan_t *c)
{
	if (c->entnum == cl.viewentity || c->entnum < 0)
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
	double now;
	int i, k;
	if (!on || !sfx)
		return;
	now = Sys_FloatTime ();
	// the same entity's same channel takes over; then a free one; then what ends soonest
	k = -1;
	if (entchannel)
		for (i = 0 ; i < NDYN ; i++)
			if (dyn[i].entnum == entnum && dyn[i].entchannel == entchannel && dyn[i].sfx)
				k = i;
	if (k < 0)
		for (i = 0 ; i < NDYN && k < 0 ; i++)
			if (!dyn[i].sfx || dyn[i].end <= now)
				k = i;
	if (k < 0)
	{
		k = 0;
		for (i = 1 ; i < NDYN ; i++)
			if (dyn[i].end < dyn[k].end)
				k = i;
	}
	if (!(q = qs_load (sfx)))
		return;
	c = &dyn[k];
	c->sfx = sfx;
	c->entnum = entnum;
	c->entchannel = entchannel;
	VectorCopy (origin, c->origin);
	c->dist_mult = attenuation / 1000.0;
	c->master = (int)(fvol * 255);
	c->end = now + (double)q->len / q->rate;
	qs_dyn_place (c);
	if (!c->left && !c->right)
	{
		c->sfx = 0;
		return;
	}
	snd_play (CH_DYN + k, (char *)q->data, q->len, q->rate, c->left | (c->right << 8));
}

void S_StopSound (int entnum, int entchannel)
{
	int i;
	if (!on)
		return;
	for (i = 0 ; i < NDYN ; i++)
		if (dyn[i].sfx && dyn[i].entnum == entnum && dyn[i].entchannel == entchannel)
		{
			snd_stop (CH_DYN + i);
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
	for (i = 0 ; i < NDYN ; i++)
		dyn[i].sfx = 0;
	for (i = 0 ; i < NSTATIC ; i++)
		static_now[i] = 0;
	ambient_vol[0] = 0;
	ambient_vol[1] = 0;
}

void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	qstatic_t *s;
	qsnd_t *q;
	if (!on || !sfx || nstatic == MAX_STATIC || !(q = qs_load (sfx)) || !q->loop)
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

// ---- about twenty times a second ----

static void qs_loop (int ch, sfx_t *sfx, int left, int right, int restart, int busy)
{
	qsnd_t *q;
	if (restart || !(busy & (1 << ch)))
	{
		if ((q = qs_load (sfx)))
			snd_play (ch, (char *)q->data, q->len, q->rate, left | (right << 8));
	}
	else
		snd_volume (ch, left | (right << 8));
}

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
		qs_loop (CH_AMBIENT + k, ambient_sfx[k], qs_clamp (ambient_vol[k]), qs_clamp (ambient_vol[k]), 0, busy);
	}
}

static void qs_statics (int busy)
{
	qstatic_t *s, *best[NSTATIC];
	int i, k, loud, b;
	for (k = 0 ; k < NSTATIC ; k++)
		best[k] = 0;
	for (i = 0 ; i < nstatic ; i++)
	{
		s = &statics[i];
		qs_place (s->origin, s->dist_mult, s->master, &s->left, &s->right);
		loud = s->left > s->right ? s->left : s->right;
		if (!loud)
			continue;
		// (two of one sound would only be louder)
		for (k = 0 ; k < NSTATIC ; k++)
		{
			if (!best[k])
			{
				best[k] = s;
				break;
			}
			b = best[k]->left > best[k]->right ? best[k]->left : best[k]->right;
			if (loud > b)
			{
				if (k + 1 < NSTATIC && best[k]->sfx != s->sfx)
					best[k + 1] = best[k];
				best[k] = s;
				break;
			}
			if (best[k]->sfx == s->sfx)
				break;
		}
	}
	for (k = 0 ; k < NSTATIC ; k++)
	{
		if (!best[k])
		{
			if (static_now[k])
				snd_stop (CH_STATIC + k);
			static_now[k] = 0;
			continue;
		}
		qs_loop (CH_STATIC + k, best[k]->sfx, best[k]->left, best[k]->right, static_now[k] != best[k]->sfx, busy);
		static_now[k] = best[k]->sfx;
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
	if (now - last_update < 0.05)
		return;
	last_update = now;
	for (i = 0 ; i < NDYN ; i++)
	{
		c = &dyn[i];
		if (!c->sfx)
			continue;
		if (c->end <= now)
		{
			c->sfx = 0;
			continue;
		}
		if (c->entnum > 0 && c->entnum < cl.num_entities && c->entnum != cl.viewentity)
			VectorCopy (cl_entities[c->entnum].origin, c->origin);
		l = c->left;
		r = c->right;
		qs_dyn_place (c);
		if (l != c->left || r != c->right)
			snd_volume (CH_DYN + i, c->left | (c->right << 8));
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
	Cvar_RegisterVariable (&bgmvolume);
	Cvar_RegisterVariable (&volume);
	Cvar_RegisterVariable (&ambient_level);
	Cvar_RegisterVariable (&ambient_fade);
	Cmd_AddCommand ("play", qs_play_f);
	Cmd_AddCommand ("stopsound", qs_play_f);
	if (COM_CheckParm ("-nosound") || !snd_present () || !(snd_caps () & SND_HAS_PCM))
		return;
	on = 1;
	Con_Printf ("Sound: the machine's card, %d channels\n", (int)NCH);
	ambient_sfx[0] = S_PrecacheSound ("ambience/water1.wav");
	ambient_sfx[1] = S_PrecacheSound ("ambience/wind2.wav");
}

void S_Shutdown (void)
{
	if (on)
		snd_stop (0 - 1);
	on = 0;
}
