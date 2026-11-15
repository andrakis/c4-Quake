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
// r_surf.c: surface-related refresh code

#include "quakedef.h"
#include "r_local.h"
#ifdef C4QUAKE
#include "c4accel.h"	/* c4: Quake's accelerators, d_accel (vid_null.c) */
#endif

drawsurf_t	r_drawsurf;

int				lightleft, sourcesstep, blocksize, sourcetstep;
int				lightdelta, lightdeltastep;
int				lightright, lightleftstep, lightrightstep, blockdivshift;
unsigned		blockdivmask;
void			*prowdestbase;
unsigned char	*pbasesource;
int				surfrowbytes;	// used by ASM files
unsigned		*r_lightptr;
int				r_stepback;
int				r_lightwidth;
int				r_numhblocks, r_numvblocks;
unsigned char	*r_source, *r_sourcemax;

void R_DrawSurfaceBlock8_mip0 (void);
void R_DrawSurfaceBlock8_mip1 (void);
void R_DrawSurfaceBlock8_mip2 (void);
void R_DrawSurfaceBlock8_mip3 (void);

static void	(*surfmiptable[4])(void) = {
	R_DrawSurfaceBlock8_mip0,
	R_DrawSurfaceBlock8_mip1,
	R_DrawSurfaceBlock8_mip2,
	R_DrawSurfaceBlock8_mip3
};



unsigned		blocklights[18*18];
#ifdef C4QUAKE
static float	c4_dl[4];	/* a light's local[0], local[1], rad and minlight, for qa_dlight */
#endif

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights (void)
{
	msurface_t *surf;
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;

	surf = r_drawsurf.surf;
	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		rad = cl_dlights[lnum].radius;
		dist = DotProduct (cl_dlights[lnum].origin, surf->plane->normal) -
				surf->plane->dist;
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = cl_dlights[lnum].origin[i] -
					surf->plane->normal[i]*dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

#ifdef C4QUAKE
		/* c4: the texels by the machine, when it lights them (d_accel) */
		if (d_accel)
		{
			c4_dl[0] = local[0]; c4_dl[1] = local[1]; c4_dl[2] = rad; c4_dl[3] = minlight;
			qa_dlight (blocklights, smax, tmax, c4_dl);
			continue;
		}
#endif
		
		for (t = 0 ; t<tmax ; t++)
		{
			td = local[1] - t*16;
			if (td < 0)
				td = -td;
			for (s=0 ; s<smax ; s++)
			{
				sd = local[0] - s*16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);
				if (dist < minlight)
#ifdef C4QUAKE
				{
					/* c4: the same sum, through int. An unsigned += a float is
					   the unsigned made a float, the sum, and back; c4's FPU
					   has float to and from int, and unsigned goes through the
					   library, some 300 instructions a texel under a rocket's
					   light. Here the float is above 0 (rad - dist > minlight's
					   own, which is not negative) and the light far below
					   2^31, where int converts exactly as unsigned does; past
					   2^31 it is done as it was. */
					unsigned	*bl = &blocklights[t*smax + s];
					float		sum;
					if (*bl < 0x80000000u)
					{
						sum = (float)(int)*bl + (rad - dist)*256;
						*bl = sum < 2147483648.0f ? (unsigned)(int)sum : (unsigned)sum;
					}
					else
						*bl += (rad - dist)*256;
				}
#else
					blocklights[t*smax + s] += (rad - dist)*256;
#endif
			}
		}
	}
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap (void)
{
	int			smax, tmax;
	int			t;
	int			i, size;
	byte		*lightmap;
	unsigned	scale;
	int			maps;
	msurface_t	*surf;

	surf = r_drawsurf.surf;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		for (i=0 ; i<size ; i++)
			blocklights[i] = 0;
		return;
	}

#ifdef C4QUAKE
	/* c4: the three loops by the machine, when it lights them (d_accel) */
	if (d_accel)
	{
		for (maps = 0 ; lightmap && maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ; maps++)
			;
		qa_lmfill (blocklights, size, r_refdef.ambientlight<<8, lightmap, maps, r_drawsurf.lightadj);
		if (surf->dlightframe == r_framecount)
			R_AddDynamicLights ();
		qa_lmfin (blocklights, size, 8 - VID_CBITS);
		return;
	}
#endif

// clear to ambient
	for (i=0 ; i<size ; i++)
		blocklights[i] = r_refdef.ambientlight<<8;


// add all the lightmaps
	if (lightmap)
		for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
			 maps++)
		{
			scale = r_drawsurf.lightadj[maps];	// 8.8 fraction		
			for (i=0 ; i<size ; i++)
				blocklights[i] += lightmap[i] * scale;
			lightmap += size;	// skip to next lightmap
		}

// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights ();

// bound, invert, and shift
	for (i=0 ; i<size ; i++)
	{
		t = (255*256 - (int)blocklights[i]) >> (8 - VID_CBITS);

		if (t < (1 << 6))
			t = (1 << 6);

		blocklights[i] = t;
	}
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation (texture_t *base)
{
	int		reletive;
	int		count;

	if (currententity->frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}
	
	if (!base->anim_total)
		return base;

	reletive = (int)(cl.time*10) % base->anim_total;

	count = 0;	
	while (base->anim_min > reletive || base->anim_max <= reletive)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}


/*
===============
R_DrawSurface
===============
*/
void R_DrawSurface (void)
{
	unsigned char	*basetptr;
	int				smax, tmax, twidth;
	int				u;
	int				soffset, basetoffset, texwidth;
	int				horzblockstep;
	unsigned char	*pcolumndest;
	void			(*pblockdrawer)(void);
	texture_t		*mt;

// calculate the lightings
	R_BuildLightMap ();
	
	surfrowbytes = r_drawsurf.rowbytes;

	mt = r_drawsurf.texture;
	
	r_source = (byte *)mt + mt->offsets[r_drawsurf.surfmip];
	
// the fractional light values should range from 0 to (VID_GRADES - 1) << 16
// from a source range of 0 - 255
	
	texwidth = mt->width >> r_drawsurf.surfmip;

	blocksize = 16 >> r_drawsurf.surfmip;
	blockdivshift = 4 - r_drawsurf.surfmip;
	blockdivmask = (1 << blockdivshift) - 1;
	
	r_lightwidth = (r_drawsurf.surf->extents[0]>>4)+1;

	r_numhblocks = r_drawsurf.surfwidth >> blockdivshift;
	r_numvblocks = r_drawsurf.surfheight >> blockdivshift;

//==============================

	pblockdrawer = surfmiptable[r_drawsurf.surfmip];
	// TODO: only needs to be set when there is a display settings change
	horzblockstep = blocksize;

	smax = mt->width >> r_drawsurf.surfmip;
	twidth = texwidth;
	tmax = mt->height >> r_drawsurf.surfmip;
	sourcetstep = texwidth;
	r_stepback = tmax * twidth;

	r_sourcemax = r_source + (tmax * smax);

	soffset = r_drawsurf.surf->texturemins[0];
	basetoffset = r_drawsurf.surf->texturemins[1];

// << 16 components are to guarantee positive values for %
	soffset = ((soffset >> r_drawsurf.surfmip) + (smax << 16)) % smax;
	basetptr = &r_source[((((basetoffset >> r_drawsurf.surfmip) 
		+ (tmax << 16)) % tmax) * twidth)];

	pcolumndest = r_drawsurf.surfdat;

	for (u=0 ; u<r_numhblocks; u++)
	{
		r_lightptr = blocklights + u;

		prowdestbase = pcolumndest;

		pbasesource = basetptr + soffset;

		(*pblockdrawer)();

		soffset = soffset + blocksize;
		if (soffset >= smax)
			soffset = 0;

		pcolumndest += horzblockstep;
	}
}

//=============================================================================

#ifdef C4QUAKE
/* c4: a row of a surface block written out, without the loop around it. On
   c4's stack machine the loop's counting was a quarter of each pixel's
   work and pix, kept in memory, another; the light steps the same way and
   the pixels are the same, in the same order. The last pixel's light step
   is left out: light starts again from lightright on the next row. */
#define C4_SURFPIX(b)	prowdest[b] = cm[(light & 0xFF00) + psource[b]]; light += lightstep
#define C4_SURFLAST	prowdest[0] = cm[(light & 0xFF00) + psource[0]]
#endif

/*
================
R_DrawSurfaceBlock8_mip0
================
*/
void R_DrawSurfaceBlock8_mip0 (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;
#ifdef C4QUAKE
	unsigned char	*cm = (unsigned char *)vid.colormap;
#endif

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 4;
		lightrightstep = (r_lightptr[1] - lightright) >> 4;

#ifdef C4QUAKE
		/* c4: the block by the machine, when it draws them (vid_null.c's
		   d_accel): the same pixels in the same order, the lights stepped
		   as the rows would have */
		if (d_accel)
		{
			qa_lblock(16, 4, lightleft, lightright, lightleftstep, lightrightstep,
			             psource, sourcetstep, cm, prowdest, surfrowbytes);
			psource += 16 * sourcetstep;
			prowdest += 16 * surfrowbytes;
			lightleft += 16 * lightleftstep;
			lightright += 16 * lightrightstep;
		}
		else
#endif
		for (i=0 ; i<16 ; i++)
		{
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 4;

			light = lightright;

#ifdef C4QUAKE
			C4_SURFPIX(15); C4_SURFPIX(14); C4_SURFPIX(13); C4_SURFPIX(12);
			C4_SURFPIX(11); C4_SURFPIX(10); C4_SURFPIX(9); C4_SURFPIX(8);
			C4_SURFPIX(7); C4_SURFPIX(6); C4_SURFPIX(5); C4_SURFPIX(4);
			C4_SURFPIX(3); C4_SURFPIX(2); C4_SURFPIX(1);
			C4_SURFLAST;
#else
			for (b=15; b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)vid.colormap)
						[(light & 0xFF00) + pix];
				light += lightstep;
			}
#endif
	
			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}


/*
================
R_DrawSurfaceBlock8_mip1
================
*/
void R_DrawSurfaceBlock8_mip1 (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;
#ifdef C4QUAKE
	unsigned char	*cm = (unsigned char *)vid.colormap;
#endif

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 3;
		lightrightstep = (r_lightptr[1] - lightright) >> 3;

#ifdef C4QUAKE
		/* c4: the block by the machine, when it draws them (vid_null.c's
		   d_accel): the same pixels in the same order, the lights stepped
		   as the rows would have */
		if (d_accel)
		{
			qa_lblock(8, 3, lightleft, lightright, lightleftstep, lightrightstep,
			             psource, sourcetstep, cm, prowdest, surfrowbytes);
			psource += 8 * sourcetstep;
			prowdest += 8 * surfrowbytes;
			lightleft += 8 * lightleftstep;
			lightright += 8 * lightrightstep;
		}
		else
#endif
		for (i=0 ; i<8 ; i++)
		{
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 3;

			light = lightright;

#ifdef C4QUAKE
			C4_SURFPIX(7); C4_SURFPIX(6); C4_SURFPIX(5); C4_SURFPIX(4);
			C4_SURFPIX(3); C4_SURFPIX(2); C4_SURFPIX(1);
			C4_SURFLAST;
#else
			for (b=7; b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)vid.colormap)
						[(light & 0xFF00) + pix];
				light += lightstep;
			}
#endif
	
			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}


/*
================
R_DrawSurfaceBlock8_mip2
================
*/
void R_DrawSurfaceBlock8_mip2 (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;
#ifdef C4QUAKE
	unsigned char	*cm = (unsigned char *)vid.colormap;
#endif

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 2;
		lightrightstep = (r_lightptr[1] - lightright) >> 2;

#ifdef C4QUAKE
		/* c4: the block by the machine, when it draws them (vid_null.c's
		   d_accel): the same pixels in the same order, the lights stepped
		   as the rows would have */
		if (d_accel)
		{
			qa_lblock(4, 2, lightleft, lightright, lightleftstep, lightrightstep,
			             psource, sourcetstep, cm, prowdest, surfrowbytes);
			psource += 4 * sourcetstep;
			prowdest += 4 * surfrowbytes;
			lightleft += 4 * lightleftstep;
			lightright += 4 * lightrightstep;
		}
		else
#endif
		for (i=0 ; i<4 ; i++)
		{
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 2;

			light = lightright;

#ifdef C4QUAKE
			C4_SURFPIX(3); C4_SURFPIX(2); C4_SURFPIX(1);
			C4_SURFLAST;
#else
			for (b=3; b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)vid.colormap)
						[(light & 0xFF00) + pix];
				light += lightstep;
			}
#endif
	
			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}


/*
================
R_DrawSurfaceBlock8_mip3
================
*/
void R_DrawSurfaceBlock8_mip3 (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;
#ifdef C4QUAKE
	unsigned char	*cm = (unsigned char *)vid.colormap;
#endif

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 1;
		lightrightstep = (r_lightptr[1] - lightright) >> 1;

#ifdef C4QUAKE
		/* c4: the block by the machine, when it draws them (vid_null.c's
		   d_accel): the same pixels in the same order, the lights stepped
		   as the rows would have */
		if (d_accel)
		{
			qa_lblock(2, 1, lightleft, lightright, lightleftstep, lightrightstep,
			             psource, sourcetstep, cm, prowdest, surfrowbytes);
			psource += 2 * sourcetstep;
			prowdest += 2 * surfrowbytes;
			lightleft += 2 * lightleftstep;
			lightright += 2 * lightrightstep;
		}
		else
#endif
		for (i=0 ; i<2 ; i++)
		{
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 1;

			light = lightright;

#ifdef C4QUAKE
			C4_SURFPIX(1);
			C4_SURFLAST;
#else
			for (b=1; b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)vid.colormap)
						[(light & 0xFF00) + pix];
				light += lightstep;
			}
#endif
	
			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

//============================================================================

/*
================
R_GenTurbTile
================
*/
void R_GenTurbTile (pixel_t *pbasetex, void *pdest)
{
	int		*turb;
	int		i, j, s, t;
	byte	*pd;
	
	turb = sintable + ((int)(cl.time*SPEED)&(CYCLE-1));
	pd = (byte *)pdest;

	for (i=0 ; i<TILE_SIZE ; i++)
	{
		for (j=0 ; j<TILE_SIZE ; j++)
		{	
			s = (((j << 16) + turb[i & (CYCLE-1)]) >> 16) & 63;
			t = (((i << 16) + turb[j & (CYCLE-1)]) >> 16) & 63;
			*pd++ = *(pbasetex + (t<<6) + s);
		}
	}
}


/*
================
R_GenTile
================
*/
void R_GenTile (msurface_t *psurf, void *pdest)
{
	if (psurf->flags & SURF_DRAWTURB)
	{
		R_GenTurbTile ((pixel_t *)
				((byte *)psurf->texinfo->texture + psurf->texinfo->texture->offsets[0]), pdest);
	}
	else if (psurf->flags & SURF_DRAWSKY)
	{
		R_GenSkyTile (pdest);
	}
	else
	{
		Sys_Error ("Unknown tile type");
	}
}

