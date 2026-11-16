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
export default function quake({ reader, writer, reader32, writer32, probe, args, fval, fbits, toInt }) {
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


  // QA_PARTICLES (first, prm): R_DrawParticles once its dead are freed (the
  // C does that): each particle drawn as D_DrawParticle draws it, and moved
  // as the loop moves it. prm is c4_pp (r_part.c): the view's vectors and
  // centre, the frame's times, the screen's numbers, the colour ramps. The
  // drawing is the z buffer's (run again, it finds its own shorts and writes
  // the same); the moves are written only once every particle's page is
  // known to be there, so a fault before then leaves them as they were.
  // particle_t: org 0, color 12, next 16, vel 20, ramp 32, die 36, type 40.
  function particles(m) {
    const [first, prm] = args(m, 2);
    const r32 = reader32(m), w32 = writer32(m), rz = reader(m), wz = writer(m), wr = writer(m);
    const f = k => fval(r32((prm + 4 * k) | 0)), n = k => r32((prm + 4 * k) | 0);
    const ox = f(0), oy = f(1), oz = f(2), rx = f(3), ry = f(4), rzz = f(5);
    const ux = f(6), uy = f(7), uz = f(8), px = f(9), py = f(10), pzz = f(11);
    const xc = f(12), yc = f(13), ft = f(14), t1 = f(15), t2 = f(16), t3 = f(17), grav = f(18), dvel = f(19);
    const vbottom = n(20), vright = n(21), vy = n(22), vx = n(23), zbuf = n(24), zw = n(25), vbuf = n(26), scan = n(27);
    const pshift = n(28), pmin = n(29), pmax = n(30), yshift = n(31), sw = n(32), ramp1 = n(33), ramp2 = n(34), ramp3 = n(35);
    const moves = [];
    for (let p = first; p !== 0; p = r32((p + 16) | 0)) {
      probe(m, p, 44, true);
      const o0 = fval(r32(p)), o1 = fval(r32((p + 4) | 0)), o2 = fval(r32((p + 8) | 0));
      const color = fval(r32((p + 12) | 0));
      // D_DrawParticle
      const l0 = fr(o0 - ox), l1 = fr(o1 - oy), l2 = fr(o2 - oz);
      const tx = fr(fr(fr(l0 * rx) + fr(l1 * ry)) + fr(l2 * rzz));
      const ty = fr(fr(fr(l0 * ux) + fr(l1 * uy)) + fr(l2 * uz));
      const tz = fr(fr(fr(l0 * px) + fr(l1 * py)) + fr(l2 * pzz));
      draw: if (!(tz < 8)) {
        const zi = fr(1 / tz);
        const u = toInt(fr(xc + fr(zi * tx)) + 0.5), v = toInt(fr(yc - fr(zi * ty)) + 0.5);
        if (v > vbottom || u > vright || v < vy || u < vx) break draw;
        let pz = (zbuf + 2 * (Math.imul(zw, v) + u)) | 0;
        let pd = (vbuf + r32((scan + 4 * v) | 0) + u) | 0;
        const izi = toInt(fr(zi * 32768));
        let pix = izi >> pshift;
        if (pix < pmin) pix = pmin; else if (pix > pmax) pix = pmax;
        const c = toInt(color) & 255;
        for (let count = pix << yshift; count; count = (count - 1) | 0, pz = (pz + 2 * zw) | 0, pd = (pd + sw) | 0) {
          for (let i = 0; i < pix; i++) {
            const a = (pz + 2 * i) | 0;
            if ((((rz(a) | (rz((a + 1) | 0) << 8)) << 16) >> 16) <= izi) {
              wz(a, izi & 255);
              wz((a + 1) | 0, (izi >> 8) & 255);
              wr((pd + i) | 0, c);
            }
          }
        }
      }
      // the move, written below
      let v0 = fval(r32((p + 20) | 0)), v1 = fval(r32((p + 24) | 0)), v2 = fval(r32((p + 28) | 0));
      let ramp = fval(r32((p + 32) | 0)), die = r32((p + 36) | 0), col = r32((p + 12) | 0);
      const type = r32((p + 40) | 0);
      const n0 = fr(o0 + fr(v0 * ft)), n1 = fr(o1 + fr(v1 * ft)), n2 = fr(o2 + fr(v2 * ft));
      const rampTo = (step, top, table) => {
        ramp = fr(ramp + step);
        if (ramp >= top) die = fbits(-1);
        else col = fbits(fr(r32((table + 4 * toInt(ramp)) | 0)));
      };
      switch (type) {
        case 0: break;                                              // pt_static
        case 3: rampTo(t1, 6, ramp3); v2 = fr(v2 + grav); break;    // pt_fire
        case 4: rampTo(t2, 8, ramp1);                               // pt_explode
          v0 = fr(v0 + fr(v0 * dvel)); v1 = fr(v1 + fr(v1 * dvel)); v2 = fr(v2 + fr(v2 * dvel));
          v2 = fr(v2 - grav); break;
        case 5: rampTo(t3, 8, ramp2);                               // pt_explode2
          v0 = fr(v0 - fr(v0 * ft)); v1 = fr(v1 - fr(v1 * ft)); v2 = fr(v2 - fr(v2 * ft));
          v2 = fr(v2 - grav); break;
        case 6:                                                     // pt_blob
          v0 = fr(v0 + fr(v0 * dvel)); v1 = fr(v1 + fr(v1 * dvel)); v2 = fr(v2 + fr(v2 * dvel));
          v2 = fr(v2 - grav); break;
        case 7:                                                     // pt_blob2
          v0 = fr(v0 - fr(v0 * dvel)); v1 = fr(v1 - fr(v1 * dvel));
          v2 = fr(v2 - grav); break;
        case 1: case 2: v2 = fr(v2 - grav); break;                  // pt_grav, pt_slowgrav
      }
      moves.push(p, fbits(n0), fbits(n1), fbits(n2), col, fbits(v0), fbits(v1), fbits(v2), fbits(ramp), die);
    }
    for (let k = 0; k < moves.length; k += 10) {
      const p = moves[k];
      w32(p, moves[k + 1]); w32((p + 4) | 0, moves[k + 2]); w32((p + 8) | 0, moves[k + 3]);
      w32((p + 12) | 0, moves[k + 4]);
      w32((p + 20) | 0, moves[k + 5]); w32((p + 24) | 0, moves[k + 6]); w32((p + 28) | 0, moves[k + 7]);
      w32((p + 32) | 0, moves[k + 8]); w32((p + 36) | 0, moves[k + 9]);
    }
    return 0;
  }

  // QA_LMFILL (bl, size, ambient, lightmap, nmaps, adj): R_BuildLightMap's
  // first two loops, blocklights to the ambient light, then each of nmaps
  // lightmaps (size bytes each) times its scale adj[i], as unsigned. Run
  // again it fills again first.
  function lmfill(m) {
    const [bl, size, ambient, lm, nmaps, adj] = args(m, 6);
    const r32 = reader32(m), w32 = writer32(m), rl = reader(m);
    for (let i = 0; i < size; i++) w32((bl + 4 * i) | 0, ambient);
    for (let k = 0, at = lm; k < nmaps; k++, at = (at + size) | 0) {
      const scale = r32((adj + 4 * k) | 0);
      for (let i = 0; i < size; i++) {
        const a = (bl + 4 * i) | 0;
        w32(a, (r32(a) + Math.imul(rl((at + i) | 0), scale)) | 0);
      }
    }
    return 0;
  }

  // QA_DLIGHT (bl, smax, tmax, prm): R_AddDynamicLights' texels for one
  // light, prm its four floats local[0], local[1], rad and minlight (the C
  // works those out). blocklights[t*smax + s] += (rad - dist)*256, an
  // unsigned and a float's sum, as C makes it. Its texels are known to be
  // there before the first is written.
  function dlight(m) {
    const [bl, smax, tmax, prm] = args(m, 4);
    const r32 = reader32(m), w32 = writer32(m);
    const l0 = fval(r32(prm)), l1 = fval(r32((prm + 4) | 0)), rad = fval(r32((prm + 8) | 0)), minl = fval(r32((prm + 12) | 0));
    probe(m, bl, 4 * smax * tmax, true);
    for (let t = 0; t < tmax; t++) {
      let td = toInt(fr(l1 - fr(t * 16)));
      if (td < 0) td = -td;
      for (let s = 0; s < smax; s++) {
        let sd = toInt(fr(l0 - fr(s * 16)));
        if (sd < 0) sd = -sd;
        const dist = fr(sd > td ? sd + (td >> 1) : td + (sd >> 1));
        if (dist < minl) {
          const a = (bl + 4 * (t * smax + s)) | 0;
          const sum = fr(fr(r32(a) >>> 0) + fr(fr(rad - dist) * 256));
          w32(a, sum < 2147483648 ? toInt(sum) : Math.trunc(sum) | 0);
        }
      }
    }
    return 0;
  }

  // QA_LMFIN (bl, size, shift): R_BuildLightMap's last loop, bound, invert
  // and shift; its words known to be there before the first is written
  function lmfin(m) {
    const [bl, size, shift] = args(m, 3);
    const r32 = reader32(m), w32 = writer32(m);
    probe(m, bl, 4 * size, true);
    for (let i = 0; i < size; i++) {
      const a = (bl + 4 * i) | 0;
      let t = (65280 - r32(a)) >> shift;
      if (t < 64) t = 64;
      w32(a, t);
    }
    return 0;
  }


  // QA_ALIASVERTS (fv, st, pv, n, prm): R_AliasTransformAndProjectFinalVerts,
  // a model's n vertices transformed, projected and lit into finalvert_t (v[6]
  // u, v, s, t, light, 1/z; flags), from trivertx_t (v[3], lightnormalindex)
  // and stvert_t (onseam, s, t). prm is r_alias.c's c4_av: aliastransform's
  // twelve floats, aliasxcenter, aliasycenter, r_plightvec, r_shadelight,
  // r_ambientlight and r_avertexnormals. It writes only fv.
  function aliasverts(m) {
    let [fv, st, pv, n, prm] = args(m, 5);
    const r32 = reader32(m), w32 = writer32(m), rb = reader(m);
    const f = k => fval(r32((prm + 4 * k) | 0));
    const t00 = f(0), t01 = f(1), t02 = f(2), t03 = f(3), t10 = f(4), t11 = f(5), t12 = f(6), t13 = f(7);
    const t20 = f(8), t21 = f(9), t22 = f(10), t23 = f(11), xc = f(12), yc = f(13);
    const l0 = f(14), l1 = f(15), l2 = f(16), shade = f(17), ambient = r32((prm + 72) | 0), normals = r32((prm + 76) | 0);
    for (let i = 0; i < n; i++, fv = (fv + 32) | 0, pv = (pv + 4) | 0, st = (st + 12) | 0) {
      const b0 = rb(pv), b1 = rb((pv + 1) | 0), b2 = rb((pv + 2) | 0), li = rb((pv + 3) | 0);
      const zi = fr(1 / fr(fr(fr(fr(b0 * t20) + fr(b1 * t21)) + fr(b2 * t22)) + t23));
      w32((fv + 20) | 0, toInt(zi));
      w32(fv, toInt(fr(fr(fr(fr(fr(fr(b0 * t00) + fr(b1 * t01)) + fr(b2 * t02)) + t03) * zi) + xc)));
      w32((fv + 4) | 0, toInt(fr(fr(fr(fr(fr(fr(b0 * t10) + fr(b1 * t11)) + fr(b2 * t12)) + t13) * zi) + yc)));
      w32((fv + 8) | 0, r32((st + 4) | 0));
      w32((fv + 12) | 0, r32((st + 8) | 0));
      w32((fv + 24) | 0, r32(st));
      const nv = (normals + 12 * li) | 0;
      const lightcos = fr(fr(fr(fval(r32(nv)) * l0) + fr(fval(r32((nv + 4) | 0)) * l1)) + fr(fval(r32((nv + 8) | 0)) * l2));
      let temp = ambient;
      if (lightcos < 0) {
        temp = (temp + toInt(fr(shade * lightcos))) | 0;
        if (temp < 0) temp = 0;
      }
      w32((fv + 16) | 0, temp);
    }
    return 0;
  }

  // QA_POLYTRI (p1, p2, p3, prm): D_PolysetRecursiveTriangle, a small
  // triangle split down to its pixels, each drawn against the z buffer; p1
  // to p3 are finalvert_t v[6]s (only 0-3 and 5 are read). prm is
  // d_polyse.c's c4_tri: zspantable, skintable, d_pcolormap, d_scantable,
  // d_viewbuffer. Run again it finds its own shorts and writes the same.
  function polytri(m) {
    const [a1, a2, a3, prm] = args(m, 4);
    const r32 = reader32(m), rz = reader(m), rs = reader(m), rc = reader(m), wz = writer(m), wr = writer(m);
    const zspan = r32(prm), skin = r32((prm + 4) | 0), cmap = r32((prm + 8) | 0), scan = r32((prm + 12) | 0), vb = r32((prm + 16) | 0);
    const vert = a => [r32(a), r32((a + 4) | 0), r32((a + 8) | 0), r32((a + 12) | 0), 0, r32((a + 20) | 0)];
    const far = d => d < -1 || d > 1;
    const tri = (p1, p2, p3) => {
      for (;;) {
        if (far(p2[0] - p1[0]) || far(p2[1] - p1[1])) break;
        if (far(p3[0] - p2[0]) || far(p3[1] - p2[1])) { const t = p1; p1 = p2; p2 = p3; p3 = t; break; }
        if (far(p1[0] - p3[0]) || far(p1[1] - p3[1])) { const t = p1; p1 = p3; p3 = p2; p2 = t; break; }
        return;                       // the whole triangle is drawn
      }
      const nw = [(p1[0] + p2[0]) >> 1, (p1[1] + p2[1]) >> 1, (p1[2] + p2[2]) >> 1, (p1[3] + p2[3]) >> 1, 0,
                  (p1[5] + p2[5]) >> 1];
      // the point, if this is a leading edge
      if (!(p2[1] > p1[1]) && !(p2[1] === p1[1] && p2[0] < p1[0])) {
        const z = nw[5] >> 16;
        const zb = (r32((zspan + 4 * nw[1]) | 0) + 2 * nw[0]) | 0;
        if (z >= (((rz(zb) | (rz((zb + 1) | 0) << 8)) << 16) >> 16)) {
          wz(zb, z & 255);
          wz((zb + 1) | 0, (z >> 8) & 255);
          const pix = rc((cmap + rs((r32((skin + 4 * (nw[3] >> 16)) | 0) + (nw[2] >> 16)) | 0)) | 0);
          wr((vb + r32((scan + 4 * nw[1]) | 0) + nw[0]) | 0, pix);
        }
      }
      tri(p3, p1, nw);
      tri(p3, nw, p2);
    };
    tri(vert(a1), vert(a2), vert(a3));
    return 0;
  }

  // QA_SINCOS (pa, ps, pc): the sine and cosine of the float at pa, as the
  // floats at ps and pc: (float)sin(a) and (float)cos(a), AngleVectors'
  // three pairs, with the browser's Math.sin and Math.cos rather than
  // libc's. Those can differ from libc's in a double's last bit, which a
  // float hardly ever keeps; the user chose the speed (2026-11-16). What it
  // writes is the same if it runs again, so it needs no probe.
  function sincos(m) {
    const [pa, ps, pc] = args(m, 3);
    const r32 = reader32(m), w32 = writer32(m);
    const a = fval(r32(pa));
    w32(ps, fbits(fr(Math.sin(a))));
    w32(pc, fbits(fr(Math.cos(a))));
    return 0;
  }

  return [spans8, zspans, turb8, lblock, sspan, polyspans, particles, lmfill, dlight, lmfin, aliasverts, polytri,
    sincos];
}
