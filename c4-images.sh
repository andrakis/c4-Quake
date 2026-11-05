# c4-images.sh c4ix|bare -- Quake's part of c4's build-images.sh, which
# sources it from c4's root (games/README.md in c4)
GAMEDIR=games/quake
case "$1" in
c4ix)
    # linked against C4IX's libc, with the shareware pak beside it
    # (c4ix.vfs.txt: /usr/games/quake)
    if [ -f .quake/qg-sum.c4o ] && [ -f $PB/lib/libc.c4s ]; then
        say "linking Quake for C4IX"
        ./c4rlink32 .quake/*.c4o .quake/plat/qg-c4ix.c4o -l $PB/lib/libc.c4s -o $DISK/c4ix-quake.c4r > /dev/null
        if [ -f $GAMEDIR/data/pak0.pak ]; then cp $GAMEDIR/data/pak0.pak $DISK/pak0.pak; fi
    fi ;;
bare)
    if [ -f .quake/qg-sum.c4o ]; then
        item quake
        ./c4rlink32 .quake/*.c4o .quake/plat/qg-bare.c4o .libc-bare/libc-*.c4o -o $GAMESDISK/quake.c4r > /dev/null
        if [ -f $GAMEDIR/data/pak0.pak ]; then cp $GAMEDIR/data/pak0.pak $GAMESDISK/; fi
        GAMES_ON_DISK="$GAMES_ON_DISK QUAKE"
    fi ;;
esac
