#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include "inode.h"

#define TOTAL_BLOCKS (10*1024)

#define toutput_filename "TEST_IMAGE"
#define tinput_filename "test_input"
#define tuid 20
#define tgid 40
#define tn_dblocks 10
#define tn_iblocks 1
#define tiblock 0
#define tipos 0

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

void write_block(int blockno, char data[]) {
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

void place_file(char *file, int uid, int gid, int n_dblocks, int n_iblocks, int iblock, int ipos)
{
  int i, nbytes = 0;
  int i2block_index, i3block_index;
  struct inode *ip;
  FILE *fpr;
  unsigned char buf[BLOCK_SZ];
  int finished_reading = 0;

  ip = malloc(sizeof(struct inode));

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

  // populate dblocks
  for (i = 0; i < N_DBLOCKS; i++) {
    int blockno = get_free_block(n_iblocks);
    ip->dblocks[i] = blockno;
    
    // read from file
    char buf[BLOCK_SZ];
    int nread = (int)fread(buf, BLOCK_SZ, 1, fpr);

    if (debug) {
      printf("read from file: %s\n", buf);
      printf("bytes read: %i\n", nread);
    }

    write_block(blockno, buf);

    nbytes += nread;

    if (nread < BLOCK_SZ) {
      finished_reading = 1;
      break;
    }
  }

  // fill in here if IBLOCKS needed
  // if so, you will first need to get an empty block to use for your IBLOCK
  // populate iblocks
  if (!finished_reading) {
    for (i = 0; i < N_IBLOCKS; i++) {
      // new iblock
      int blockno = get_free_block(n_iblocks);
      ip->iblocks[i] = blockno;

      // then fill iblock with pointers to data blocks?
      // 'pointers' in this case means the block number
      int j;
      for (j = 0; j < BLOCK_SZ / sizeof(int); j++) {
        int dblockno = get_free_block(n_iblocks);

        // add pointer
        write_int(blockno*BLOCK_SZ + j*sizeof(int), dblockno);

        // i'm beginning to think i could have written this better

        // fill data block
        // read from file
        char buf[BLOCK_SZ];
        int nread = (int)fread(buf, BLOCK_SZ, 1, fpr);

        if (debug) {
          printf("read from file: %s\n", buf);
          printf("bytes read: %i\n", nread);
        }

        // write data block, not the inode block
        write_block(dblockno, buf);

        nbytes += nread;

        if (nread < BLOCK_SZ) {
          finished_reading = 1;
          break;
        }
      }

      if (finished_reading)
        break;
    }
  }


  // populate i2block
  if (!finished_reading) {
    // allocate a doubly indirect block + store pointer
    int di_blockno = get_free_block(n_iblocks);
    ip->i2block = di_blockno;

    // fill with pointers (block numbers) of indirect blocks
    for (i = 0; i < BLOCK_SZ / sizeof(int); i++) {
      // get indirect block and update pointer
      int i_blockno = get_free_block(n_iblocks);
      write_int(BLOCK_SZ*di_blockno + i*sizeof(int), i_blockno);

      // fill indirect block with poitners to data blcokks
      int j;
      for (j = 0; j < BLOCK_SZ / sizeof(int); j++) {
        int dblockno = get_free_block(n_iblocks);
        write_int(BLOCK_SZ*i_blockno + j*sizeof(int), dblockno);

        // read from file
        char buf[BLOCK_SZ];
        int nread = (int)fread(buf, BLOCK_SZ, 1, fpr);

        if (debug) {
          printf("read from file: %s\n", buf);
          printf("bytes read: %i\n", nread);
        }

        write_block(dblockno, buf);

        nbytes += nread;

        if (nread < BLOCK_SZ) {
          finished_reading = 1;
          break;
        }
      }

      if (finished_reading)
        break;
    }
  }


  // populate i3block
  if (!finished_reading) {
    int ti_blockno = get_free_block(n_iblocks);
    ip->i3block = ti_blockno;

    for (i = 0; i < BLOCK_SZ / sizeof(int); i++) {
      // add another doubly indirect block, update pointer
      int di_blockno = get_free_block(n_iblocks);
      write_int(BLOCK_SZ*ti_blockno + i*sizeof(int), di_blockno);

      // fill with pointers (block numbers) of indirect blocks
      for (i = 0; i < BLOCK_SZ / sizeof(int); i++) {
        // get indirect block and update pointer
        int i_blockno = get_free_block(n_iblocks);
        write_int(BLOCK_SZ*di_blockno + i*sizeof(int), i_blockno);

        // fill indirect block with poitners to data blcokks
        int j;
        for (j = 0; j < BLOCK_SZ / sizeof(int); j++) {
          int dblockno = get_free_block(n_iblocks);
          write_int(BLOCK_SZ*i_blockno + j*sizeof(int), dblockno);

          // read from file
          char buf[BLOCK_SZ];
          int nread = (int)fread(buf, BLOCK_SZ, 1, fpr);

          if (debug) {
            printf("read from file: %s\n", buf);
            printf("bytes read: %i\n", nread);
          }

          write_block(dblockno, buf);

          nbytes += nread;

          if (nread < BLOCK_SZ) {
            finished_reading = 1;
            break;
          }
        }

        if (finished_reading)
          break;
      }
      if (finished_reading)
        break;
    }
  }

  ip->size = nbytes;  // total number of data bytes written for file
  printf("successfully wrote %d bytes of file %s\n", nbytes, file);

  // write inode to disk
  // i believe we literally just line up each field of the inode and write
  // to block iblock, position ipos
  // ipos being position in inodes, not bytes
  // inodes are 100 bytes in size so there will be some extra space at the end of each iblock
  // ah but here is a problem
  // if we get a block with get_free_block it will mark the whole block as taken for only one inode
  // well ill write it anyway and change it later i guess
  // or does it even matter? we specify block and position within the block
  int write_pos = iblock*BLOCK_SZ + 100*ipos;
  write_int(write_pos, ip->mode);
  write_int(write_pos + sizeof(int), ip->nlink);
  write_int(write_pos + 2*sizeof(int), ip->uid);
  write_int(write_pos + 3*sizeof(int), ip->gid);
  write_int(write_pos + 4*sizeof(int), ip->size);
  write_int(write_pos + 5*sizeof(int), ip->ctime);
  write_int(write_pos + 6*sizeof(int), ip->mtime);
  write_int(write_pos + 7*sizeof(int), ip->atime);
  // there's gotta be a better way to write this
  for (i = 0; i < N_DBLOCKS; i++) {
    write_int(write_pos + (8+i)*sizeof(int), ip->dblocks[i]);
  }
  for (i = 0; i < N_IBLOCKS; i++) {
    write_int(write_pos + (8+i+N_DBLOCKS)*sizeof(int), ip->iblocks[i]);
  }
  write_int(write_pos + (9+N_DBLOCKS+N_IBLOCKS)*sizeof(int), ip->i2block);
  write_int(write_pos + (10+N_DBLOCKS+N_IBLOCKS)*sizeof(int), ip->i3block);
}

void main(int argc, char* argv[]) // add argument handling
{
  int i;
  // zero out rawdata (i dont know if we have to do this)
  // we do actually it says zero out disk in assignment
  for (i = 0; i < TOTAL_BLOCKS*BLOCK_SZ; i++)
    write_int(i, (int)'0');

  FILE *outfile;
  char *output_filename;
  FILE *infile;
  char *input_filename;
  int n_dblocks;
  int n_iblocks;
  int uid;
  int gid;
  int iblock;
  int ipos;
  if (debug)
    printf("in program\n");
  // disk_image -create -image IMAGE_FILE -nblocks N -iblocks M -inputfile FILE -u UID -g GID -block D -inodepos I  
  // for (i = 1; i < argc - 1; i++) {
  //   if (strcmp(argv[i], "-image"))
  //     output_filename = argv[i+1];
  //   if (strcmp(argv[i], "-nblocks"))
  //     n_dblocks = atoi(argv[i+1]);
  //   if (strcmp(argv[i], "-iblocks"))
  //     n_iblocks = atoi(argv[i+1]);
  //   if (strcmp(argv[i], "-inputfile"))
  //     input_filename = argv[i+1];
  //   if (strcmp(argv[i], "-u"))
  //     uid = atoi(argv[i+1]);
  //   if (strcmp(argv[i], "-g"))
  //     gid = atoi(argv[i+1]);
  //   if (strcmp(argv[i], "-block"))
  //     iblock = atoi(argv[i+1]);
  //   if (strcmp(argv[i], "-inodepos"))
  //     ipos = atoi(argv[i+1]);
  //   printf(argv[i]);
  //   printf("\n");
  // }

  // if (debug) {
  //   printf("output filename: %s\n", output_filename);
  //   printf("input file: %s\n", input_filename);
  // }
  
  outfile = fopen(toutput_filename, "wb");
  
  if (!outfile) {
    perror("datafile open\n");
    exit(-1);
  }

  if (debug)
    printf("about to place file\n");
  // fill in here to place file 
  place_file(tinput_filename, tuid, tgid, tn_dblocks, tn_iblocks, tiblock, tipos);

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
