#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include "inode.h"

#define TOTAL_BLOCKS (10*1024)

static unsigned char rawdata[TOTAL_BLOCKS*BLOCK_SZ];
static char bitmap[TOTAL_BLOCKS];
int debug = 1;

int get_free_block(int M)
{
  if (debug)
    printf("getting new block\n");
  
  int blockno;

  for (blockno = M; blockno < TOTAL_BLOCKS; blockno++) {
    if (!bitmap[blockno]) {
      bitmap[blockno] = 1;
      break;
    }
  }

  assert(blockno < TOTAL_BLOCKS);
  assert(bitmap[blockno]);
  return blockno;
}

int get_free_inode_block()
{
  return get_free_block(0);
}

void write_int(int pos, int val)
{
  int *ptr = (int *)&rawdata[pos];
  *ptr = val;
}

void write_block(int blockno, char* data) {
  // write data from char array into rawdata
  // assumes there's BLOCK_SZ bytes to write

  if (debug)
    printf("writing block\n");

  int ri = blockno*BLOCK_SZ; // start index into rawdata
  int i;
  for (i = 0; i < BLOCK_SZ; i++, ri++) {
    // rawdata[ri] = data[i];
    write_int(ri, (int)data[i]);
  }
}

void place_file(char *file, int uid, int gid, int n_dblocks, int n_iblocks)
{
  int i, nbytes = 0;
  int i2block_index, i3block_index;
  struct inode *ip;
  FILE *fpr;
  unsigned char buf[BLOCK_SZ];
  int finished_reading = 0;

  ip->mode = 0;
  ip->size = 0;
  ip->nlink = 1;
  ip->uid = uid;
  ip->gid = gid;
  ip->ctime = random();
  ip->mtime = random();
  ip->atime = random();

  if (debug)
    printf("opening file\n");

  fpr = fopen(file, "rb");
  if (!fpr) {
    perror(file);
    exit(-1);
  }

  if (debug)
    printf("input file open\n");

  for (i = 0; i < N_DBLOCKS; i++) {
    int blockno = get_free_block(n_iblocks);
    ip->dblocks[i] = blockno;
    
    // read from file
    char *buf = malloc(BLOCK_SZ);
    size_t nread = fread(buf, BLOCK_SZ, 1, fpr);
    write_block(blockno, buf);
    free(buf);

    ip->size += nread;

    if (nread < BLOCK_SZ) {
      finished_reading = 1;
      break;
    }
  }

  // fill in here if IBLOCKS needed
  // if so, you will first need to get an empty block to use for your IBLOCK

  for (i = 0; i < N_IBLOCKS; i++) {
    // new iblock
    int blockno = get_free_block(n_iblocks);
    ip->iblocks[i] = blockno;

  }

  ip->size = nbytes;  // total number of data bytes written for file
  printf("successfully wrote %d bytes of file %s\n", nbytes, file);
}

void main(int argc, char* argv[]) // add argument handling
{
  int i;
  FILE *outfile;
  char *output_filename;
  FILE *infile;
  char *input_filename;
  int n_dblocks;
  int n_iblocks;
  int uid;
  int gid;
  int block;
  int inodepos;
  printf("in program\n");
  // disk_image -create -image IMAGE_FILE -nblocks N -iblocks M -inputfile FILE -u UID -g GID -block D -inodepos I  
  for (i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "-image"))
      output_filename = argv[i+1];
    if (strcmp(argv[i], "-nblocks"))
      n_dblocks = atoi(argv[i+1]);
    if (strcmp(argv[i], "-iblocks"))
      n_iblocks = atoi(argv[i+1]);
    if (strcmp(argv[i], "-inputfile"))
      input_filename = argv[i+1];
    if (strcmp(argv[i], "-u"))
      uid = atoi(argv[i+1]);
    if (strcmp(argv[i], "-g"))
      gid = atoi(argv[i+1]);
    if (strcmp(argv[i], "-block"))
      block = atoi(argv[i+1]);
    if (strcmp(argv[i], "-inodepos"))
      inodepos = atoi(argv[i+1]);
    printf(argv[i]);
    printf("\n");
  }
  
  outfile = fopen(output_filename, "wb");
  
  if (!outfile) {
    perror("datafile open\n");
    exit(-1);
  }

  if (debug)
    printf("about to place file\n");
  // fill in here to place file 
  place_file(input_filename, uid, gid, n_dblocks, n_iblocks);

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
