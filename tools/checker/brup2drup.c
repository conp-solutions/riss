#include <stdio.h>
#include <stdlib.h>

void main (int argc, char** argv) {
  int tmp, lit;
  int *data, size = 0, max = 1000000, *buffer, bsize;
  int blast = max, lsize = 0;
  int fixed = 0;

  //if (argc <= 1) { printf("usage: ./brup2drup BRUP-file [DRUP-file]\n"); exit(0); }

  FILE *input  = stdin; //fopen(argv[1], "r");
  FILE *output = stdout;

  //if (argc >= 3) output = fopen(argv[2],           "w");
  //else           output = fopen("refutation.drup", "w");

  data   = (int*) malloc (sizeof(int) * max);
  buffer = (int*) malloc (sizeof(int) * max);

  bsize = 0;
  do {
start:
    if (bsize == fixed ) {
      tmp = fscanf (input, " b %i ", &lit);
      if (tmp == 1) { buffer[ fixed++ ] = -lit; bsize++; goto start; }
    }
    tmp = fscanf (input, " %i ", &lit);
    if (tmp > 0) {
      buffer[ bsize++ ] = lit;

      if (lit == 0) {
        int i, flag;

        for (i = 0; i < bsize; i++) fprintf(output, "%i ", buffer[ i ]); fprintf(output, "\n");
        if (bsize == 1) break;
delete:
        flag = (bsize < blast);
        if (flag) {
          for (i = 0; i < bsize - 1; i++)
            if (data[ lsize + i ] != buffer[ i ]) flag = 0;
        }
        if (flag) {
          fprintf(output, "d ");
          for (i = 0; i < blast; i++) fprintf(output, "%i ", data[ lsize + i ]);
          fprintf(output, "\n");
          size  = lsize;
          lsize--;
          blast = 1;
          while (data[ lsize - 1 ] != 0) {
            blast++;
            lsize--;
          }
          goto delete;
        }
        lsize = size;
        if (size + bsize >= max) {
          max = max * 2;
          data = (int *) realloc (data, sizeof(int) * max);
        }

        for (i = 0; i < bsize; i++) data[ size++ ] = buffer[ i ];
        blast = bsize;

        if (bsize == fixed + 1) fixed--;
        bsize = fixed;
      }
    }
    else break;

  }
  while (1);

  fclose (input);
  fclose (output);
}
