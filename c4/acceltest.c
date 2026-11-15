// acceltest.c -- c4's port: Quake's accelerators (c4/accel.mjs) against
// the loops they stand for, as r_surf.c and d_sprite.c have them (the
// surface blocks, the sprites' runs, the light maps), on made-up
// textures and steps. The span lists' (D_DrawSpans8, D_DrawZSpans,
// Turbulent8, D_PolysetDrawSpans8) are held to gcc's build frame by frame by
// test-quake itself. test-quake runs this on c4's libjs.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "c4accel.h"

int d_accel;

typedef union { unsigned int i; float f; } f2i;

unsigned seed = 12345;
int rnd(int n) { seed = seed * 1103515245 + 12345; return (int)((seed >> 8) % (unsigned)n); }

unsigned char *tex, *pal, *bufc, *bufa;
int fails, cases;
enum { TEXN = 65536, PALN = 65536 + 512, BUFN = 320 * 240 };

void fill(unsigned char *p, int n, int holes) {
    int i;
    for (i = 0; i < n; i++) { p[i] = rnd(256); if (holes && rnd(8) == 0) p[i] = 255; }
}
void check(char *what, int k, int same) {
    cases++;
    if (memcmp(bufc, bufa, BUFN) != 0 || !same) {
        fails++;
        if (fails < 10) printf("accel: %s, case %d, differs\n", what, k);
    }
}
void clear() { memset(bufc, 7, BUFN); memset(bufa, 7, BUFN); }

// Quake's D_DrawSpans8 run, its D_DrawZSpans and R_DrawSurfaceBlock8
void blockc(int rows, int sh, int ll, int lr, int lls, int lrs, unsigned char *src, int ss, unsigned char *cm, unsigned char *d, int ds) {
    int i, b, step, light;
    for (i = 0; i < rows; i++) {
        step = (ll - lr) >> sh; light = lr;
        for (b = rows - 1; b >= 0; b--) { d[b] = cm[(light & 0xFF00) + src[b]]; light += step; }
        src += ss; lr += lrs; ll += lls; d += ds;
    }
}
void test_lblock() {
    int k, mip, rows, ll, lr, lls, lrs, x, at;
    for (k = 0; k < 200; k++) {
        clear();
        mip = rnd(4); rows = 16 >> mip;
        ll = rnd(0xc000) + 0x1000; lr = rnd(0xc000) + 0x1000; lls = rnd(512) - 256; lrs = rnd(512) - 256;
        x = rnd(320 * 200); at = rnd(1024);
        blockc(rows, 4 - mip, ll, lr, lls, lrs, tex + at, 64, pal, bufc + x, 256);
        qa_lblock(rows, 4 - mip, ll, lr, lls, lrs, tex + at, 64, pal, bufa + x, 256);
        check("lblock", k, 1);
    }
}

void sspanc(int n, int s, int t, int ss, int ts, unsigned char *pb, int cw, unsigned char *d, short *pz, int *izi, int step) {
    int c;
    for (; n > 0; n--) {
        c = *(pb + (s >> 16) + (t >> 16) * cw);
        if (c != 255 && *pz <= (*izi >> 16)) { *pz = *izi >> 16; *d = c; }
        *izi += step; d++; pz++; s += ss; t += ts;
    }
}
void test_sspan() {
    int k, n, s, t, ss, ts, x, z, izi, izc, iza, i;
    for (k = 0; k < 200; k++) {
        clear();
        // a z buffer of shorts after the pixels, the same in both
        for (i = 0; i < 640; i++) { z = rnd(65536) - 32768; ((short *)(bufc + 64000))[i] = z; ((short *)(bufa + 64000))[i] = z; }
        n = 1 + rnd(16); s = (70 + rnd(115)) << 16 | rnd(65536); t = (70 + rnd(115)) << 16 | rnd(65536);
        ss = rnd(8 << 16) - (4 << 16); ts = rnd(8 << 16) - (4 << 16); x = rnd(32000);
        izi = rnd(1 << 30) * 4 + rnd(4); izc = izi;
        z = 2 * rnd(600);
        i = rnd(1 << 22) - (1 << 21);
        sspanc(n, s, t, ss, ts, tex, 256, bufc + x, (short *)(bufc + 64000 + z), &izc, i);
        iza = qa_sspan(n, s, t, ss, ts, tex, 256, bufa + x, bufa + 64000 + z, izi, i);
        check("sspan", k, iza == izc);
    }
}

// R_BuildLightMap's loops and R_AddDynamicLights' texels, as r_surf.c has them
unsigned blc[18 * 18], bla[18 * 18];
int adj[4];
float dlp[4];
void lightc(unsigned *bl, int size, int ambient, unsigned char *lm, int nmaps, int *adjs) {
    int i, k;
    for (i = 0; i < size; i++) bl[i] = ambient;
    for (k = 0; k < nmaps; k++, lm += size)
        for (i = 0; i < size; i++) bl[i] += lm[i] * (unsigned)adjs[k];
}
void dlightc(unsigned *bl, int smax, int tmax, float l0, float l1, float rad, float minlight) {
    int s, t, sd, td;
    float dist;
    for (t = 0; t < tmax; t++) {
        td = l1 - t * 16;
        if (td < 0) td = -td;
        for (s = 0; s < smax; s++) {
            sd = l0 - s * 16;
            if (sd < 0) sd = -sd;
            if (sd > td) dist = sd + (td >> 1);
            else dist = td + (sd >> 1);
            if (dist < minlight)
                bl[t * smax + s] += (rad - dist) * 256;
        }
    }
}
void finc(unsigned *bl, int size) {
    int i, t;
    for (i = 0; i < size; i++) {
        t = (255 * 256 - (int)bl[i]) >> 2;
        if (t < (1 << 6)) t = (1 << 6);
        bl[i] = t;
    }
}
int same_bl(int n) { int i; for (i = 0; i < n; i++) if (blc[i] != bla[i]) return 0; return 1; }
void test_light() {
    int k, smax, tmax, size, nmaps, amb, i, at;
    for (k = 0; k < 200; k++) {
        smax = 1 + rnd(18); tmax = 1 + rnd(18); size = smax * tmax;
        nmaps = rnd(5); amb = rnd(256) << 8; at = rnd(TEXN - 4 * 324);
        for (i = 0; i < 4; i++) adj[i] = rnd(1 << 9);
        lightc(blc, size, amb, tex + at, nmaps, adj);
        qa_lmfill(bla, size, amb, tex + at, nmaps, adj);
        cases++; if (!same_bl(size)) { fails++; if (fails < 10) printf("accel: lmfill, case %d, differs\n", k); }
        for (i = 0; i < 1 + rnd(3); i++) {
            dlp[0] = (float)(rnd(40000) - 10000) * 0.0137f; dlp[1] = (float)(rnd(40000) - 10000) * 0.0119f;
            dlp[2] = (float)rnd(30000) * 0.0173f; dlp[3] = dlp[2] - (float)rnd(3000) * 0.031f;
            dlightc(blc, smax, tmax, dlp[0], dlp[1], dlp[2], dlp[3]);
            qa_dlight(bla, smax, tmax, dlp);
        }
        cases++; if (!same_bl(size)) { fails++; if (fails < 10) printf("accel: dlight, case %d, differs\n", k); }
        finc(blc, size);
        qa_lmfin(bla, size, 2);
        cases++; if (!same_bl(size)) { fails++; if (fails < 10) printf("accel: lmfin, case %d, differs\n", k); }
    }
}

int main() {
    d_accel = accel_find("quake");
    if (!d_accel) { printf("accel: Quake's are not here
"); return 0; }
    tex = malloc(TEXN); pal = malloc(PALN); bufc = malloc(BUFN); bufa = malloc(BUFN);
    fill(tex, TEXN, 1); fill(pal, PALN, 0);
    test_lblock(); test_sspan(); test_light();
    if (fails) printf("accel: %d of %d cases differ
", fails, cases);
    else printf("accel: Quake's, %d cases, each as its loop has it
", cases);
    return fails != 0;
}
