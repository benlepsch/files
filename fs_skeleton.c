#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include "inode.h"

#define TOTAL_BLOCKS (10*1024)

static unsigned char rawdata[TOTAL_BLOCKS*BLOCK_SZ];
static char bitmap[TOTAL_BLOCKS];

int get_free_block()
{
  // fill in here
  assert(blockno < TOTAL_BLOCKS);
  assert(bitmap[blockno]);
  return blockno;
}

void write_int(int pos, int val)
{
  int *ptr = (int *)&rawdata[pos];
  *ptr = val;
}

void place_file(char *file, int uid, int gid)
{
  int i, nbytes = 0;
  int i2block_index, i3block_index;
  struct inode *ip;
  FILE *fpr;
  unsigned char buf[BLOCK_SZ];

  ip->mode = 0;
  ip->nlink = 1;
  ip->uid = uid;
  ip->gid = gid;
  ip->ctime = random();
  ip->mtime = random();
  ip->atime = random();

  fpr = fopen(file, "rb");
  if (!fpr) {
    perror(file);
    exit(-1);
  }

  for (i = 0; i < N_DBLOCKS; i++) {
    int blockno = get_free_block();
    ip->dblocks[i] = blockno;
    // fill in here
  }

  // fill in here if IBLOCKS needed
  // if so, you will first need to get an empty block to use for your IBLOCK

  ip->size = nbytes;  // total number of data bytes written for file
  printf("successfully wrote %d bytes of file %s\n", nbytes, file);
}

void main() // add argument handling
{
  int i;
  FILE *outfile;

  outfile = fopen(output_filename, "wb");
  if (!outfile) {
    perror("datafile open");
    exit(-1);
  }

  // fill in here to place file 

  i = fwrite(rawdata, 1, TOTAL_BLOCKS*BLOCK_SZ, outfile);
  if (i != TOTAL_BLOCKS*BLOCK_SZ) {
    perror("fwrite");
    exit(-1);
  }

  i = fclose(outfile);
  if (i) {
    perror("datafile close");
    exit(-1);
  }

  printf("Done.\n");
  return;
}
