# c4.mk -- this game's part of c4's Makefile. c4 includes it from
# games/quake/c4.mk, its submodule, and runs it from c4's root, with c4's
# tools and variables (games/README.md in c4). Without it, c4 builds
# everything else, and test-quake says it was skipped.

# Quake (games/quake/README.c4.md): quakegeneric's units with c4lc, the same
# objects for C4IX, the bare machine and C4DOS, but for the two that say
# whether the display has to be asked for (qg-c4ix.c, qg-bare.c, in
# .quake/plat). c4-images.sh links them: against C4IX's libc, and whole with
# the bare libc of libc-bare-objs.
QUAKE       := games/quake/source
QUAKE_UNITS := cd_null chase cl_demo cl_input cl_main cl_parse cl_tent cmd common console \
               crc cvar d_edge d_fill d_init d_modech d_part d_polyse d_scan d_sky d_sprite \
               d_surf d_vars d_zpoint draw host_cmd host in_null keys mathlib menu model \
               net_loop net_main net_none net_vcr nonintel pr_cmds pr_edict pr_exec r_aclip \
               r_alias r_bsp r_draw r_edge r_efrag r_light r_main r_misc r_part r_sky r_sprite \
               r_surf r_vars sbar screen sv_main sv_move sv_phys sv_user sys_null vid_null \
               view wad world zone quakegeneric qg-sum
# c4's alone: the display, the card and setjmp. On a host: nothing to draw on
# and no sound, the reference test-quake holds the c4lc build to.
QUAKE_C4    := qg-gui qg-snd qg-jmp
QUAKE_PLAT  := qg-c4ix qg-bare
QUAKE_HDRS  := $(wildcard $(QUAKE)/*.h games/quake/c4inc/*.h) \
               $(wildcard src/c4ix/libc/include/*.h src/c4ix/libc/include/sys/*.h) \
               src/c4ix/include/display.h src/c4ix/include/window.h src/c4ix/include/window.c \
               libjs/guest/gui.h libjs/guest/gui.c libjs/guest/snd.h libjs/guest/snd.c
QUAKE_CC    := ./c4sp32 -c 400000 src/c4sp/lisp/c4lc.lisp -O $(IMG_G) -mfuse -mfpu -fno-builtin -c -conforming \
               -D C4QUAKE -I games/quake/c4inc -I src/c4ix/libc/include -I src/c4ix/include \
               -I libjs/guest -I $(QUAKE)
quake-objs: $(QUAKE_UNITS:%=.quake/%.c4o) $(QUAKE_C4:%=.quake/%.c4o) $(QUAKE_PLAT:%=.quake/plat/%.c4o)
.quake/%.c4o: $(QUAKE)/%.c $(QUAKE_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .quake/plat
	$(QUAKE_CC) $< $@ > /dev/null
.quake/plat/%.c4o: $(QUAKE)/%.c $(QUAKE_HDRS) $(C4LC_LISP) .img_g | c4sp32
	@mkdir -p .quake/plat
	$(QUAKE_CC) $< $@ > /dev/null
PHONY += quake-objs

# ... and by gcc for the host, 32-bit as the c4lc build is. -mfpmath=sse: x87's
# excess precision is not what c4's FPU does, and the frames would part.
QUAKE_HOST_SRCS := $(QUAKE_UNITS:%=$(QUAKE)/%.c) $(QUAKE)/snd_null.c $(QUAKE)/qg-host.c
quake-host: $(QUAKE_HOST_SRCS) $(wildcard $(QUAKE)/*.h)
	gcc -m32 -std=gnu99 -O2 -msse2 -mfpmath=sse -ffp-contract=off -fno-strict-aliasing -w -DC4QUAKE -I$(QUAKE) \
	    -o $@ $(QUAKE_HOST_SRCS) -lm

# The data, which is not in the repository: id's shareware package as it was
# distributed, checked by its SHA-256, and pak0.pak out of it (resource.1 is an
# LH5 archive; its one member's place is fixed by that SHA). games/fetch.sh takes
# the package from $QUAKE_ZIP when that names a copy, or else from the mirrors
# in data/MIRRORS. Its licence is its own: it is not c4's to pass on.
QUAKE_DATA := games/quake/data
quake-data: $(QUAKE_DATA)/pak0.pak
$(QUAKE_DATA)/quake106.zip:
	@sh games/fetch.sh $@ ec6c9d34b1ae0252ac0066045b6611a7919c2a0d78a3a66d9387a8f597553239 QUAKE_ZIP $$(cat $(QUAKE_DATA)/MIRRORS 2>/dev/null)
$(QUAKE_DATA)/pak0.pak: $(QUAKE_DATA)/quake106.zip games/quake/tools/unlh5.c
	@gcc -O2 -w -o $(QUAKE_DATA)/unlh5 games/quake/tools/unlh5.c
	@python3 -I -c "import zipfile; zipfile.ZipFile('$<').extract('resource.1', '$(QUAKE_DATA)')"
	@$(QUAKE_DATA)/unlh5 $(QUAKE_DATA)/resource.1 29041 8612632 18689235 $@ > /dev/null
	@rm -f $(QUAKE_DATA)/resource.1 $(QUAKE_DATA)/unlh5
	@echo "35a9c55e5e5a284a159ad2a62e0e8def23d829561fe2f54eb402dbc0a9a946af  $@" | sha256sum -c --quiet
PHONY += quake-data

# test-quake: demo3 in a timedemo, which plays a tic a frame on a virtual clock
# (qg-sum.c, -sum), built by gcc and by c4lc -- on the bare machine, under C4DOS and
# inside C4IX -- must show the same frames, to the checksum of every pixel and the
# palette. A timedemo's frames do not depend on the display or on sound.
QUAKE_RUN := -nosound -nocdaudio -sum +timedemo demo3
test-quake: quake-data quake-host c4bb-images test-libc-bare
	@rm -rf .quake_t && mkdir -p .quake_t/id1
	@cp $(QUAKE_DATA)/pak0.pak .quake_t/id1/
	@cd .quake_t && ../quake-host $(QUAKE_RUN) 2>/dev/null | grep '^timedemo' > host.txt
	@cat .quake_t/host.txt
	@cd src/c4bb/images/games && { timeout 1500 node ../../../../libjs/cli.js -m 128 -d . quake.c4r $(QUAKE_RUN) \
	     > ../../../../.quake_t/bare.out 2>/dev/null & \
	   printf 'quake $(QUAKE_RUN)\nver\n' | timeout 1500 node ../../../../libjs/cli.js -m 128 -d . ../c4dos32.c4r \
	     > ../../../../.quake_t/dos.out 2>/dev/null; wait; }
	@printf 'quake $(QUAKE_RUN)\nexit\n' | timeout 1500 node libjs/cli.js -m 64 -d src/c4bb/images/disk src/c4bb/images/c4ix32.c4r \
	   2>/dev/null > .quake_t/c4ix.out
	@for w in bare dos c4ix; do grep '^timedemo' .quake_t/$$w.out | cmp -s .quake_t/host.txt - \
	   || { echo "test-quake: $$w's frames differ:"; tail -5 .quake_t/$$w.out; exit 1; }; done
	@rm -rf .quake_t
	@# built -mfpu: with no FPU it stops at once and says why, bare and under C4IX
	@cd src/c4bb/images/games && timeout 300 node ../../../../libjs/cli.js --no-fpu -m 128 -d . quake.c4r -nosound -nocdaudio 2>&1 | grep -q "needs an FPU" \
	   || { echo "test-quake: bare, with no FPU, it did not say why"; exit 1; }
	@printf 'quake -nosound -nocdaudio\nexit\n' | timeout 300 node libjs/cli.js --no-fpu -m 64 -d src/c4bb/images/disk src/c4bb/images/c4ix32.c4r 2>&1 | grep -q "needs an FPU" \
	   || { echo "test-quake: under C4IX, with no FPU, it did not say why"; exit 1; }
	@echo "test-quake: OK"
PHONY += test-quake

GAME_OBJS  += quake-objs
GAME_TESTS += test-quake
