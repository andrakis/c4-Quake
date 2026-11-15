/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// vid_null.c -- null video driver to aid porting efforts

#include "quakedef.h"
#include "d_local.h"

#include "quakegeneric.h"

#ifdef C4QUAKE
/* c4: a machine that has Quake's accelerators (c4/accel.mjs, c4accel.h)
   draws the span lists, z spans, water, surface blocks, sprites and models'
   spans itself, one instruction each (d_scan.c, d_sprite.c, d_polyse.c,
   r_surf.c): the same pixels. vid_accel, saved in config.cfg, is the Video
   Options menu's "Browser accelerated"; d_accel is the first of Quake's
   numbers while the frame draws that way, else 0. */
#include "c4accel.h"
cvar_t	vid_accel = {"vid_accel", "1", true};
int		d_accel;
static int	vid_accel_base = -1;

static int VID_AccelThere (void)
{
	if (vid_accel_base < 0)
		vid_accel_base = accel_find ("quake");
	return vid_accel_base;
}

static void VID_AccelUpdate (void)
{
	d_accel = vid_accel.value != 0 ? VID_AccelThere () : 0;
}

extern void (*vid_menudrawfn)(void);
extern void (*vid_menukeyfn)(int key);
void M_DrawCharacter (int cx, int line, int num);
void M_Print (int cx, int cy, char *str);
void M_DrawPic (int x, int y, qpic_t *pic);
void M_DrawCheckbox (int x, int y, int on);
void M_Menu_Options_f (void);

static void VID_MenuDraw (void)
{
	qpic_t	*p;

	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ((320-p->width)/2, 4, p);
	M_Print (16, 32, "   Browser accelerated");
	if (VID_AccelThere ())
	{
		M_DrawCheckbox (220, 32, vid_accel.value != 0);
		M_Print (16, 56, "   The browser draws the walls'");
		M_Print (16, 64, "   pixels: the same frames, faster");
	}
	else
	{
		M_Print (220, 32, "n/a");
		M_Print (16, 56, "   Not on this machine");
	}
	M_DrawCharacter (200, 32, 12+((int)(realtime*4)&1));
}

static void VID_MenuKey (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;
	case K_ENTER:
	case K_LEFTARROW:
	case K_RIGHTARROW:
		if (VID_AccelThere ())
		{
			S_LocalSound ("misc/menu3.wav");
			Cvar_SetValue ("vid_accel", vid_accel.value ? 0 : 1);
			VID_AccelUpdate ();
		}
		break;
	}
}
#endif

viddef_t	vid;				// global video state

#define	BASEWIDTH	QUAKEGENERIC_RES_X
#define	BASEHEIGHT	QUAKEGENERIC_RES_Y

byte	vid_buffer[BASEWIDTH*BASEHEIGHT];
short	zbuffer[BASEWIDTH*BASEHEIGHT];
byte	*surfcache;
size_t	surfcache_size;

void	VID_SetPalette (unsigned char *palette)
{
	// quake generic
	QG_SetPalette(palette);
}

void	VID_ShiftPalette (unsigned char *palette)
{
	// quake generic
	QG_SetPalette(palette);
}

void	VID_Init (unsigned char *palette)
{
	vid.maxwarpwidth = vid.width = vid.conwidth = BASEWIDTH;
	vid.maxwarpheight = vid.height = vid.conheight = BASEHEIGHT;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);	// non-square pixels at 320x200, shown 4:3
	vid.numpages = 1;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	vid.buffer = vid.conbuffer = vid_buffer;
	vid.rowbytes = vid.conrowbytes = BASEWIDTH;
	
	d_pzbuffer = zbuffer;

	surfcache_size = D_SurfaceCacheForRes(BASEWIDTH, BASEHEIGHT);
	surfcache = malloc(surfcache_size);
	D_InitCaches (surfcache, surfcache_size);
#ifdef C4QUAKE
	Cvar_RegisterVariable (&vid_accel);
	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;
	VID_AccelUpdate ();
#endif

	// quake generic
	QG_Init();
}

void	VID_Shutdown (void)
{
		free(surfcache);
}

void	VID_Update (vrect_t *rects)
{
	// quake generic
	QG_DrawFrame(vid.buffer);
#ifdef C4QUAKE
	VID_AccelUpdate ();	// vid_accel may have been set since (config.cfg, the console)
#endif
}

/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
}


