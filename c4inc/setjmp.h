// setjmp.h -- c4 has none in its libc yet: qg-jmp.c is the VM's own
#ifndef C4QUAKE_SETJMP_H
#define C4QUAKE_SETJMP_H
typedef int jmp_buf[3];
int qg_setjmp (int *b);
void qg_longjmp (int *b, int v);
#define setjmp qg_setjmp
#define longjmp qg_longjmp
#endif
