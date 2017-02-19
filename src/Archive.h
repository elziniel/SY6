#include <stdio.h>

typedef struct archive st_archive;
int ecrire(int archive, int type, st_archive *a);
st_archive *lire(int archive);
