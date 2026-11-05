// unlh5 IN OFFSET COMPRESSED ORIGINAL OUT: one LZH -lh5- member (scratch)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char *in; static long ip, in_end;
static unsigned bitbuf; static int bitcnt;
static unsigned getbits(int n) {
    unsigned v = 0;
    while (n--) {
        if (!bitcnt) { bitbuf = ip < in_end ? in[ip++] : 0; bitcnt = 8; }
        v = (v << 1) | ((bitbuf >> 7) & 1); bitbuf = (bitbuf << 1) & 255; --bitcnt;
    }
    return v;
}
// canonical Huffman, by length
typedef struct { int first[18], count[18], index[18]; short sym[512]; int single; } huff;
static void build(huff *h, unsigned char *len, int n) {
    int i, l, code = 0, k = 0;
    h->single = -1;
    for (l = 1; l <= 16; l++) { h->count[l] = 0; }
    for (i = 0; i < n; i++) if (len[i]) h->count[len[i]]++;
    for (l = 1; l <= 16; l++) {
        h->first[l] = code; h->index[l] = k;
        for (i = 0; i < n; i++) if (len[i] == l) h->sym[k++] = i;
        code = (code + h->count[l]) << 1;
    }
}
static int decode(huff *h) {
    int l, code = 0;
    if (h->single >= 0) return h->single;
    for (l = 1; l <= 16; l++) {
        code = (code << 1) | getbits(1);
        if (code - h->first[l] < h->count[l] && code >= h->first[l]) return h->sym[h->index[l] + code - h->first[l]];
    }
    fprintf(stderr, "bad code at %ld\n", ip); exit(1);
}
static void read_pt(huff *h, int nn, int nbit, int special) {
    unsigned char len[32]; int n, i, c;
    memset(len, 0, sizeof len);
    n = getbits(nbit);
    if (!n) { h->single = getbits(nbit); return; }
    i = 0;
    while (i < n) {
        c = getbits(3);
        if (c == 7) while (getbits(1)) c++;
        len[i++] = c;
        if (i == special) { c = getbits(2); while (c--) len[i++] = 0; }
    }
    build(h, len, nn);
}
static void read_c(huff *h, huff *pt) {
    unsigned char len[512]; int n, i, c, k;
    memset(len, 0, sizeof len);
    n = getbits(9);
    if (!n) { h->single = getbits(9); return; }
    i = 0;
    while (i < n) {
        c = decode(pt);
        if (c <= 2) { k = c == 0 ? 1 : c == 1 ? (int)getbits(4) + 3 : (int)getbits(9) + 20; while (k--) len[i++] = 0; }
        else len[i++] = c - 2;
    }
    build(h, len, 510);
}
int main(int argc, char **argv) {
    long off = atol(argv[2]), csize = atol(argv[3]), osize = atol(argv[4]), op = 0, fsz;
    FILE *f = fopen(argv[1], "rb");
    unsigned char *out = malloc(osize);
    static huff C, P, T;
    fseek(f, 0, SEEK_END); fsz = ftell(f); fseek(f, 0, SEEK_SET);
    in = malloc(fsz); fread(in, 1, fsz, f); fclose(f);
    ip = off; in_end = off + csize;
    while (op < osize) {
        int blk = getbits(16);
        memset(&C, 0, sizeof C); memset(&P, 0, sizeof P); memset(&T, 0, sizeof T);
        C.single = P.single = T.single = -1;
        read_pt(&T, 19, 5, 3);
        read_c(&C, &T);
        read_pt(&P, 14, 4, -1);
        while (blk-- && op < osize) {
            int c = decode(&C);
            if (c < 256) out[op++] = c;
            else {
                int n = c - 256 + 3, p = decode(&P), d;
                d = p ? (1 << (p - 1)) + (int)getbits(p - 1) : 0;
                while (n-- && op < osize) { out[op] = op - d - 1 >= 0 ? out[op - d - 1] : 0; op++; }
            }
        }
    }
    f = fopen(argv[5], "wb"); fwrite(out, 1, osize, f); fclose(f);
    printf("%ld bytes\n", op);
    return 0;
}
