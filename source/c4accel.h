/* c4accel.h -- c4's port: Quake's accelerators, c4/accel.mjs, from the
   game's side (c4's libjs/guest/accel.h, docs/accel.md). On a machine that
   has them, with Video Options' "Browser accelerated" on, the host runs
   these in the game's own JavaScript, one instruction each; elsewhere, and
   in gcc's build, the C runs as before. d_accel (vid_null.c) is the first
   of Quake's numbers while drawing with them, else 0. The numbers are the
   module's operations in its order. */
#ifndef C4ACCEL_H
#define C4ACCEL_H

#include "accel.h"

enum { QA_SPANS8, QA_ZSPANS, QA_TURB8, QA_LBLOCK, QA_SSPAN, QA_POLYSPANS };

extern int d_accel;

#ifdef __c4_inttypes__
#define QA_OP(op) (d_accel + (op))
/* the whole span list; grad the nine gradients' floats (d_scan.c's c4_grad) */
#define qa_spans8(pspan, pbase, vb, sw, cw, sadj, tadj, bbs, bbt, grad) \
	__c4_opcode((pspan), (pbase), (vb), (sw), (cw), (sadj), (tadj), (bbs), (bbt), (grad), QA_OP(QA_SPANS8))
#define qa_zspans(pspan, zbuf, zw, grad) __c4_opcode((pspan), (zbuf), (zw), (grad), QA_OP(QA_ZSPANS))
#define qa_turb8(pspan, pbase, vb, sw, sadj, tadj, bbs, bbt, turb, grad) \
	__c4_opcode((pspan), (pbase), (vb), (sw), (sadj), (tadj), (bbs), (bbt), (turb), (grad), QA_OP(QA_TURB8))
#define qa_lblock(rows, sh, ll, lr, lls, lrs, src, srcstep, cm, dest, deststep) \
	__c4_opcode((rows), (sh), (ll), (lr), (lls), (lrs), (src), (srcstep), (cm), (dest), (deststep), QA_OP(QA_LBLOCK))
#define qa_sspan(n, s, t, sstep, tstep, pbase, width, dest, pz, izi, izistep) \
	__c4_opcode((n), (s), (t), (sstep), (tstep), (pbase), (width), (dest), (pz), (izi), (izistep), QA_OP(QA_SSPAN))
#define qa_polyspans(pk, pasp, perr, up, down, extra, ubase, cmap, zix, lx, whole, sfr, tfr, skinw) \
	__c4_opcode((pk), (pasp), (perr), (up), (down), (extra), (ubase), (cmap), (zix), (lx), (whole), (sfr), (tfr), (skinw), \
		QA_OP(QA_POLYSPANS))
#else
#define qa_spans8(pspan, pbase, vb, sw, cw, sadj, tadj, bbs, bbt, grad) 0
#define qa_zspans(pspan, zbuf, zw, grad) 0
#define qa_turb8(pspan, pbase, vb, sw, sadj, tadj, bbs, bbt, turb, grad) 0
#define qa_lblock(rows, sh, ll, lr, lls, lrs, src, srcstep, cm, dest, deststep) 0
#define qa_sspan(n, s, t, sstep, tstep, pbase, width, dest, pz, izi, izistep) 0
#define qa_polyspans(pk, pasp, perr, up, down, extra, ubase, cmap, zix, lx, whole, sfr, tfr, skinw) 0
#endif

#endif
