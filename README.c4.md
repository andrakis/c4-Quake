# games/quake — Quake on C4IX, C4DOS and the bare machine

id Software's Quake (1996), as quakegeneric has it (GPL WinQuake, software
rendered, no assembly), compiled by c4lc and played on libjs's whole display,
with its sound effects on libjs's sound card. The shareware episode ships with
it. It is played from the keyboard and the mouse.

Unlike every game before it, Quake's arithmetic is floating point: it is what an
inline-math flag for c4lc would be for.

## Where it runs

| where | how | platform |
|---|---|---|
| C4IX, the whole display | `quake`; the desktop gives way until it ends | `qg-gui.c`, `qg-c4ix.c` |
| C4IX, a window | `quake -window` in a Terminal; also when a program that does not give way has the display | `qg-gui.c`, `qg-c4ix.c` |
| C4DOS | `QUAKE` at the prompt, in 128 MB | `qg-gui.c`, `qg-bare.c` |
| a grin window, under C4DOS | as Doom's | `qg-gui.c`, `qg-bare.c` |
| the bare machine | `quake.c4r` as the machine's image; the page's `?system=quake`, in 128 MB | `qg-gui.c`, `qg-bare.c` |

Keys are Quake's own. The mouse locks to the display at the first click, until
Esc; it looks (`+mlook`), as players now expect.

## The registered game: `quake -home`

`quake -home` plays the person's own Quake, all four episodes: the registered
game's `id1/pak1.pak`, with its `pak0.pak`. Add them with the page's **Uploads**
(c4's libjs/README.md), in the folder `/home/quake` (in `/home` itself they play
too), then start it: Go > Games > Quake (your files) on C4IX, or the page's
`?system=quake-full` on the bare machine. Each pak is taken from the first place
that has it (`COM_AddGameDirectory`, `common.c`; `qg_home_dirs`, `qg-sum.c`):

| | its folder | then |
|---|---|---|
| C4IX (it has a `/home`) | `/home/quake/` | `/home/` |
| the bare machine, C4DOS | `quake/` at drive 0's root (`A:\QUAKE`) | drive 0's root |

The registered `pak0.pak` (v1.06) is the shareware's own, byte for byte, so
where none was uploaded the game's own is used, and `pak1.pak` alone is enough.
Without `pak1.pak` (`gfx/pop.lmp`, which says the game is registered), `quake
-home` says what it wants and stops. With `-home` the hunk is 16 MB rather than
8, for the later episodes' bigger maps.

A path of the person's is opened as it is, not by its last part as the game's
own `./id1/...` are, and only where it can seek: on C4IX a name its filesystem
does not have is the disk's, as a stream that cannot (`qg_fopen`). On C4IX the
paks are files of the host's in `/home`, read a part at a time; on the bare
machine and under C4DOS each is read whole into memory when it opens (libc's
`bare.c`, doubling as it reads), 18 MB and 34 MB, so `quake-full` has 256 MB.
Config and saved games are where the game runs, the shareware's beside them.

## What is c4's here

- `qg-sum.c`: the main loop, the clock, one `rand` for every build, the flat disks
  (`fopen`), and a timedemo's frames checksummed (`-sum`).
- `qg-gui.c`, `qg-c4ix.c`, `qg-bare.c`, `qg-host.c`: the platforms. 320x200, as the
  original ran in VGA: `vid.aspect` says its pixels are not square, and the display
  shows it 4:3. Frames go out 8-bit, as `GUI_BLIT8`.
- `qg-snd.c`, in place of `snd_null.c`: sixteen channels of the card, which the host
  mixes, so a sound costs a file to load and a few volumes, not a mix a frame. Two
  are the level's ambience, four the loudest looped sounds the map places, ten
  whatever starts (Quake's own mixer has eight). A card without `SND_HAS_PCM16` has
  eight: two, two and four. A sound that starts takes its channel as `snd_dma.c`
  picks one: the same entity's channel, else the one nearest its end, never the
  player's own for another's, and a loop last of all. A looped sound (a lift, a
  door) goes round again, from its cue point, in the frame that finds it ended:
  the card has no loop. `null.wav`, which doors and lifts play to say nothing,
  takes no channel. Placed by `snd_dma.c`'s formula, about twenty times a second;
  a sound moves as its entity moves from where it started, since a lift's or a
  door's origin is its model's offset, near (0,0,0), not where it is.
  The pak's WAVs are 8-bit, 11025 Hz but for two; 20 have a `LIST` chunk whose
  length is past their end, after their data.
- Four voices for what starts were too few. In demo1 (E1M3) the lift that comes down
  on the player plays a loop for 17.6 seconds while a fight goes on: with four, 242
  sounds in the demo cut off another entity's sound still playing, and the lift's
  loop played once, 1.9 seconds of it. With ten, 34 do (mostly a nailgun's
  ricochets, a tenth of a second each, near their ends), the lift plays for 17.56
  of its 17.61 seconds, every one of the 75 sounds that start meanwhile plays with
  it, and up to 14 voices sound at once.
- Played is not heard. Every sound a lift, door or button made was turned down to
  nothing within a twentieth of a second of starting: the placement moved it to the
  entity's origin, which for a brush model is its offset from where the map built
  it. In demo1 the lift played for 17.5 seconds and could be heard for 0.09 of them,
  before the ten voices and after. Now it is heard for all of it, louder as it comes
  down. Most sounds cut off still are a sound cutting off its own entity's sound on
  the same channel -- the nailgun's every shot, a grenade's every bounce -- which is
  what Quake's own mixer does.
- `qg-jmp.c`: `setjmp` and `longjmp` on the VM, which has no way to set `bp` but
  `LEV`: two of them in a row put every register back. A server's error abandons the
  frame with it. Only as C allows it: the whole condition of an `if` or a `switch`, or
  a comparison with a constant. `r = setjmp (b)` has `r`'s address on the stack
  machine's stack across the call.
- `common.h`: little-endian everywhere it runs, so `LittleLong` and the rest are no
  calls at all (and c4lc has no pointer to a function returning a float).

## The baseline

What the compiler and interpreter work has gained since, feature by feature, is in c4's
[docs/quake-performance.md](https://github.com/andrakis/c4/blob/c4ix_and_toppings/docs/quake-performance.md).
By 2026-11-06, in the page: 10.4 fps with the MMU and 13.1 without.

Measured before any inline math (2026-11-05), demo1 at 320x240: 20.74 M VM
instructions a frame on the bare machine, 21.3 M under C4IX. Under node on helios:
115 s for its 979 frames bare, 145 s under C4IX, 234 s with the MMU. In the page, on
CDP 9224 at 320x200: 3.9 fps with the MMU, 5.2 without.

| by instructions | |
|---|---|
| game code | 64.9% |
| soft float helpers | 26.5%, `__c4fp_hw` alone 10.46% in 57 copies |
| `<ld16u>`, `<ld16s>`, `<st16>`, `<lsr>` | 3.5% |
| libc (`trunc`, `ceil` ...) | 3.2% |
| `long long` | 1.9% |

`D_DrawSpans8` 18.7%, `R_DrawSurfaceBlock8_mip0` and `mip1` 15.3%, `D_DrawZSpans` 7.1%.
By the wall, the MMU's user loop costs most (1.6x), and each FPU opcode leaves the
interpreter's hot loop: about 15-17% of the wall for 1.5% of the instructions.

## Testing

- `make quake-data` takes id's `quake106.zip` from `$QUAKE_ZIP` or `data/MIRRORS`, by
  its SHA-256, and takes `pak0.pak` out of it. It is never committed.
- `make test-quake` plays demo3 as a timedemo, built by gcc and by c4lc, on the bare
  machine, under C4DOS and inside C4IX: all must show the same frames, to the checksum
  of every pixel and the palette.
- `test-desktop-quake` in c4's `test-libjs`: Quake in a Terminal's window, in 64 MB.
