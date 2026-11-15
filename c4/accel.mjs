// accel.mjs -- Quake's accelerators for c4's libjs (its libjs/accel.js,
// docs/accel.md): the renderer's innermost loops, run by the host when the
// machine has them and Video Options' "Browser accelerated" is on.
// source/c4accel.h is the game's side, with each operation's number (QA_*);
// d_scan.c, d_sprite.c, d_polyse.c and r_surf.c call them, and keep their own
// C for every other machine. Each does exactly what that C does: the same
// pixels in the same order, 32-bit arithmetic wrapping, and the floats as
// c4's FPU computes them: each single operation Math.fround of the double
// one, a double's in doubles, a conversion to int toward zero with what does
// not fit the most negative int.
export default function quake({ reader, writer, reader32, writer32, args, fval, toInt }) {
  const fr = Math.fround;

  // a span list's espan_t: u, v, count, pnext
  const SPAN_U = 0, SPAN_V = 4, SPAN_COUNT = 8, SPAN_NEXT = 12;

  // QA_SPANS8 (pspan, pbase, viewbuffer, screenwidth, cachewidth, sadjust,
  // tadjust, bbextents, bbextentt, gradients): D_DrawSpans8, the whole list.
  // gradients points at the nine floats d_sdivzstepu, d_tdivzstepu,
  // d_zistepu, d_sdivzstepv, d_tdivzstepv, d_zistepv, d_sdivzorigin,
  // d_tdivzorigin, d_ziorigin, copied there in that order (c4accel.h).
  // Perspective every eight pixels, the affine run between.
  function spans8(m) {
    let [pspan, pbase, vb, sw, cw, sadjust, tadjust, bbs, bbt, grad] = args(m, 10);
    const r32 = reader32(m), rt = reader(m), wr = writer(m);
    const g = k => fval(r32((grad + 4 * k) | 0));
    const su = g(0), tu = g(1), zu = g(2), sv = g(3), tv = g(4), zv = g(5), so = g(6), to = g(7), zo = g(8);
    const su8 = fr(su * 8), tu8 = fr(tu * 8), zu8 = fr(zu * 8);
    let sstep = 0, tstep = 0;
    do {
      const pu = r32(pspan + SPAN_U), pv = r32(pspan + SPAN_V);
      let dest = (vb + Math.imul(sw, pv) + pu) | 0;
      let count = r32(pspan + SPAN_COUNT);
      const du = fr(pu), dv = fr(pv);
      let sdivz = fr(fr(so + fr(dv * sv)) + fr(du * su));
      let tdivz = fr(fr(to + fr(dv * tv)) + fr(du * tu));
      let zi = fr(fr(zo + fr(dv * zv)) + fr(du * zu));
      let z = fr(65536 / zi);
      let s = (toInt(fr(sdivz * z)) + sadjust) | 0;
      if (s > bbs) s = bbs; else if (s < 0) s = 0;
      let t = (toInt(fr(tdivz * z)) + tadjust) | 0;
      if (t > bbt) t = bbt; else if (t < 0) t = 0;
      do {
        const n = count >= 8 ? 8 : count;
        count = (count - n) | 0;
        let snext, tnext;
        if (count) {
          sdivz = fr(sdivz + su8); tdivz = fr(tdivz + tu8); zi = fr(zi + zu8);
          z = fr(65536 / zi);
          snext = (toInt(fr(sdivz * z)) + sadjust) | 0;
          if (snext > bbs) snext = bbs; else if (snext < 8) snext = 8;
          tnext = (toInt(fr(tdivz * z)) + tadjust) | 0;
          if (tnext > bbt) tnext = bbt; else if (tnext < 8) tnext = 8;
          sstep = (snext - s) >> 3;
          tstep = (tnext - t) >> 3;
        } else {
          const m1 = fr(n - 1);
          sdivz = fr(sdivz + fr(su * m1)); tdivz = fr(tdivz + fr(tu * m1)); zi = fr(zi + fr(zu * m1));
          z = fr(65536 / zi);
          snext = (toInt(fr(sdivz * z)) + sadjust) | 0;
          if (snext > bbs) snext = bbs; else if (snext < 8) snext = 8;
          tnext = (toInt(fr(tdivz * z)) + tadjust) | 0;
          if (tnext > bbt) tnext = bbt; else if (tnext < 8) tnext = 8;
          if (n > 1) {
            sstep = ((snext - s) / (n - 1)) | 0;
            tstep = ((tnext - t) / (n - 1)) | 0;
          }
        }
        for (let k = n; k > 0; k--, dest = (dest + 1) | 0, s = (s + sstep) | 0, t = (t + tstep) | 0)
          wr(dest, rt((pbase + (s >> 16) + Math.imul(t >> 16, cw)) | 0));
        s = snext;
        t = tnext;
      } while (count > 0);
      pspan = r32(pspan + SPAN_NEXT);
    } while (pspan !== 0);
    return 0;
  }

  // QA_ZSPANS (pspan, zbuffer, zwidth, gradients): D_DrawZSpans, the whole
  // list, 1/z as shorts. zi is a float's sum held in a double, as the C has it.
  function zspans(m) {
    let [pspan, zbuf, zw, grad] = args(m, 4);
    const r32 = reader32(m), wr = writer(m);
    const g = k => fval(r32((grad + 4 * k) | 0));
    const zu = g(2), zv = g(5), zo = g(8);
    const izistep = toInt(fr(fr(zu * 32768) * 65536));
    do {
      const pu = r32(pspan + SPAN_U), pv = r32(pspan + SPAN_V);
      let dest = (zbuf + 2 * (Math.imul(zw, pv) + pu)) | 0;
      let n = r32(pspan + SPAN_COUNT);
      const zi = fr(fr(zo + fr(fr(pv) * zv)) + fr(fr(pu) * zu));
      let izi = toInt(zi * 32768 * 65536);
      for (; n > 0; n--, dest = (dest + 2) | 0, izi = (izi + izistep) | 0) {
        const h = izi >> 16;
        wr(dest, h & 255);
        wr((dest + 1) | 0, (h >> 8) & 255);
      }
      pspan = r32(pspan + SPAN_NEXT);
    } while (pspan !== 0);
    return 0;
  }

  // QA_TURB8 (pspan, pbase, viewbuffer, screenwidth, sadjust, tadjust,
  // bbextents, bbextentt, turb, gradients): Turbulent8 and
  // D_DrawTurbulent8Span, water and lava, the whole list: sixteen pixels
  // between perspective steps, each through the sine table turb
  function turb8(m) {
    let [pspan, pbase, vb, sw, sadjust, tadjust, bbs, bbt, turb, grad] = args(m, 10);
    const r32 = reader32(m), rt = reader(m), wr = writer(m), rturb = reader32(m);
    const g = k => fval(r32((grad + 4 * k) | 0));
    const su = g(0), tu = g(1), zu = g(2), sv = g(3), tv = g(4), zv = g(5), so = g(6), to = g(7), zo = g(8);
    const su16 = fr(su * 16), tu16 = fr(tu * 16), zu16 = fr(zu * 16);
    let sstep = 0, tstep = 0;
    do {
      const pu = r32(pspan + SPAN_U), pv = r32(pspan + SPAN_V);
      let dest = (vb + Math.imul(sw, pv) + pu) | 0;
      let count = r32(pspan + SPAN_COUNT);
      const du = fr(pu), dv = fr(pv);
      let sdivz = fr(fr(so + fr(dv * sv)) + fr(du * su));
      let tdivz = fr(fr(to + fr(dv * tv)) + fr(du * tu));
      let zi = fr(fr(zo + fr(dv * zv)) + fr(du * zu));
      let z = fr(65536 / zi);
      let s = (toInt(fr(sdivz * z)) + sadjust) | 0;
      if (s > bbs) s = bbs; else if (s < 0) s = 0;
      let t = (toInt(fr(tdivz * z)) + tadjust) | 0;
      if (t > bbt) t = bbt; else if (t < 0) t = 0;
      do {
        const n = count >= 16 ? 16 : count;
        count = (count - n) | 0;
        let snext, tnext;
        if (count) {
          sdivz = fr(sdivz + su16); tdivz = fr(tdivz + tu16); zi = fr(zi + zu16);
          z = fr(65536 / zi);
          snext = (toInt(fr(sdivz * z)) + sadjust) | 0;
          if (snext > bbs) snext = bbs; else if (snext < 16) snext = 16;
          tnext = (toInt(fr(tdivz * z)) + tadjust) | 0;
          if (tnext > bbt) tnext = bbt; else if (tnext < 16) tnext = 16;
          sstep = (snext - s) >> 4;
          tstep = (tnext - t) >> 4;
        } else {
          const m1 = fr(n - 1);
          sdivz = fr(sdivz + fr(su * m1)); tdivz = fr(tdivz + fr(tu * m1)); zi = fr(zi + fr(zu * m1));
          z = fr(65536 / zi);
          snext = (toInt(fr(sdivz * z)) + sadjust) | 0;
          if (snext > bbs) snext = bbs; else if (snext < 16) snext = 16;
          tnext = (toInt(fr(tdivz * z)) + tadjust) | 0;
          if (tnext > bbt) tnext = bbt; else if (tnext < 16) tnext = 16;
          if (n > 1) {
            sstep = ((snext - s) / (n - 1)) | 0;
            tstep = ((tnext - t) / (n - 1)) | 0;
          }
        }
        let rs = s & 0x7fffff, rtt = t & 0x7fffff;    // (CYCLE<<16)-1
        for (let k = n; k > 0; k--, dest = (dest + 1) | 0) {
          const st = (((rs + rturb((turb + 4 * ((rtt >> 16) & 127)) | 0)) | 0) >> 16) & 63;
          const tt = (((rtt + rturb((turb + 4 * ((rs >> 16) & 127)) | 0)) | 0) >> 16) & 63;
          wr(dest, rt((pbase + (tt << 6) + st) | 0));
          rs = (rs + sstep) | 0;
          rtt = (rtt + tstep) | 0;
        }
        s = snext;
        t = tnext;
      } while (count > 0);
      pspan = r32(pspan + SPAN_NEXT);
    } while (pspan !== 0);
    return 0;
  }

  // QA_LBLOCK (rows, sh, ll, lr, lls, lrs, src, srcstep, cm, dest, deststep):
  // R_DrawSurfaceBlock8_mip0..3's block, rows rows of rows pixels, each right
  // to left with the light from lr stepping (ll - lr) >> sh
  function lblock(m) {
    let [rows, sh, ll, lr, lls, lrs, src, sstep, cm, dest, dstep] = args(m, 11);
    sh &= 31;
    const rs = reader(m), rc = reader(m), wr = writer(m);
    for (let i = 0; i < rows; i++) {
      const step = (ll - lr) >> sh;
      let light = lr;
      for (let b = rows - 1; b >= 0; b--) {
        wr((dest + b) | 0, rc((cm + (light & 0xff00) + rs((src + b) | 0)) | 0));
        light = (light + step) | 0;
      }
      src = (src + sstep) | 0;
      lr = (lr + lrs) | 0;
      ll = (ll + lls) | 0;
      dest = (dest + dstep) | 0;
    }
    return 0;
  }

  // QA_SSPAN (count, s, t, sstep, tstep, pbase, cachewidth, dest, pz, izi,
  // izistep): a sprite's run against the z buffer, D_SpriteDrawSpans: a texel
  // not 255 drawn where *pz <= izi >> 16, which it sets; gives izi after.
  // Run again after a fault it finds its own shorts equal and writes the same.
  function sspan(m) {
    let [n, s, t, sstep, tstep, pbase, cw, dest, pz, izi, step] = args(m, 11);
    const rt = reader(m), rz = reader(m), wz = writer(m), wr = writer(m);
    for (; n > 0; n--) {
      const c = rt((pbase + (s >> 16) + Math.imul(t >> 16, cw)) | 0);
      if (c !== 255) {
        const z = ((rz(pz) | (rz((pz + 1) | 0) << 8)) << 16) >> 16, h = izi >> 16;
        if (z <= h) {
          wz(pz, h & 255);
          wz((pz + 1) | 0, (h >> 8) & 255);
          wr(dest, c);
        }
      }
      izi = (izi + step) | 0;
      dest = (dest + 1) | 0;
      pz = (pz + 2) | 0;
      s = (s + sstep) | 0;
      t = (t + tstep) | 0;
    }
    return izi;
  }

  // QA_POLYSPANS (pspanpackage, paspancount, perrorterm, erroradjustup,
  // erroradjustdown, countextrastep, ubasestep, acolormap, zistepx, lstepx,
  // ststepxwhole, sstepxfrac, tstepxfrac, skinwidth): D_PolysetDrawSpans8,
  // a model's triangle's spans against the z buffer, to the package whose
  // count is -999999; d_aspancount and errorterm are read and left as the C
  // leaves them. Its spanpackage_t: pdest, pz, count, ptex, sfrac, tfrac,
  // light, zi.
  function polyspans(m) {
    let [pk, pasp, perr, up, down, extra, ubase, cmap, zix, lx, whole, sfr, tfr, skinw] = args(m, 14);
    const r32 = reader32(m), w32 = writer32(m), rt = reader(m), rc = reader(m), rz = reader(m), wz = writer(m), wr = writer(m);
    let asp = r32(pasp), err = r32(perr);
    do {
      let lcount = (asp - r32(pk + 8)) | 0;
      err = (err + up) | 0;
      if (err >= 0) { asp = (asp + extra) | 0; err = (err - down) | 0; }
      else asp = (asp + ubase) | 0;
      if (lcount) {
        let dest = r32(pk), pz = r32(pk + 4), ptex = r32(pk + 12);
        let sfrac = r32(pk + 16), tfrac = r32(pk + 20), light = r32(pk + 24), zi = r32(pk + 28);
        do {
          const h = zi >> 16;
          if (h >= (((rz(pz) | (rz((pz + 1) | 0) << 8)) << 16) >> 16)) {
            wr(dest, rc((cmap + rt(ptex) + (light & 0xff00)) | 0));
            wz(pz, h & 255);
            wz((pz + 1) | 0, (h >> 8) & 255);
          }
          dest = (dest + 1) | 0;
          zi = (zi + zix) | 0;
          pz = (pz + 2) | 0;
          light = (light + lx) | 0;
          ptex = (ptex + whole) | 0;
          sfrac = (sfrac + sfr) | 0;
          ptex = (ptex + (sfrac >> 16)) | 0;
          sfrac &= 0xffff;
          tfrac = (tfrac + tfr) | 0;
          if (tfrac & 0x10000) { ptex = (ptex + skinw) | 0; tfrac &= 0xffff; }
          lcount = (lcount - 1) | 0;
        } while (lcount);
      }
      pk = (pk + 32) | 0;
    } while (r32(pk + 8) !== -999999);
    w32(pasp, asp);
    w32(perr, err);
    return 0;
  }

  return [spans8, zspans, turb8, lblock, sspan, polyspans];
}
