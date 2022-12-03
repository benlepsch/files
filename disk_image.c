#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
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
  if (debug)
    printf("got new block\n");
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

int read_int(int pos)
{
  int *ptr = (int *)&rawdata[pos];
  return *ptr;
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

void read_block(int blockno, char *data, int nbytes) {
  int ri = blockno*BLOCK_SZ;
  int i;
  for (i = 0; i < nbytes; i++, ri++) {
    data[i] = (char)read_int(ri);
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

  //fseek(fpr, 0, SEEK_SET);

  if (debug)
    printf("input file open\n");

  // populate dblocks
  for (i = 0; i < N_DBLOCKS; i++) {
    int blockno = get_free_block(n_iblocks);
    ip->dblocks[i] = blockno;
    
    // read from file
    char buf[BLOCK_SZ];
    size_t nread = fread(buf, 1, BLOCK_SZ, fpr);

    if (debug) {
      printf("read from file: %s\n", buf);
      printf("bytes read: %lu\n", nread);
    }

    if (nread < BLOCK_SZ) {
      // write out the reest of the buf so it doesnt look like weird
      for (i = nread; i < BLOCK_SZ; i++) {
        buf[i] = '0';
      }
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
  fclose(fpr);

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

void extract_files(char* file, int uid, int gid) {
  FILE *fpr;
  fpr = fopen(file, "r");
  if (!fpr) {
    perror(file);
    exit(-1);
  }

  int b;
  // read entire file into rawdata
  for (b = 0; b < TOTAL_BLOCKS; b++) {
    int i;
    char buf[BLOCK_SZ];
    size_t nread = fread(buf, 1, BLOCK_SZ, fpr);

    if (nread < BLOCK_SZ) {
      for (i = nread; i < BLOCK_SZ; i++) {
        buf[i] = '0';
      }
    }

    for (i = 0; i < BLOCK_SZ; i++) {
      write_int(b*BLOCK_SZ + i, (int)buf[i]);
    }

    if (nread < BLOCK_SZ)
      break;
  }

  // now we traverse the entire file 
  int i;
  char filename = 'a';
  for (i = 0; i < TOTAL_BLOCKS; i++) {
    int j;
    for (j = 0; j < 10; j++) {
      if ((read_int(i*BLOCK_SZ + j*100 + 8) == uid) && (read_int(i*BLOCK_SZ + j*100 + 12) == gid)) {
        int size = read_int(i*BLOCK_SZ + j*100 + 16);
        printf("File found at block %i, file size %i\n", i, size);

        // commented out because incomplete and segfaulting :()
        // // reconstruct a file
        // // open a file to put data into
        // FILE *w;
        // w = fopen(filename, "w+");
        // filename += 1;

        // // read data blocks
        // // while size is > 0
        // int c;
        // int dbase = i*BLOCK_SZ + j*100 + 32;
        // for (c = 0; c < N_DBLOCKS; c++) {
        //   int blockno = read_int(dbase);
        //   char data[BLOCK_SZ];

        //   if (size > BLOCK_SZ) {
        //     read_block(blockno, data, BLOCK_SZ);
        //     fwrite(data, 1, BLOCK_SZ, w);
        //   } else {
        //     read_block(blockno, data, size);
        //     fwrite(data, 1, size, w);
        //   }

        //   size -= BLOCK_SZ;

        //   if (size < 0)
        //     break;
          
        //   dbase += 4;
        // }

        // // read indirect blocks
        // if (size > 0) {
        //   int ibase = i*BLOCK_SZ + j*100 + 32 + 4*N_DBLOCKS;
        //   for (c = 0; c < N_IBLOCKS; c++) {
        //     int y;
        //     for (y = 0; y < BLOCK_SZ / sizeof(int); y++) {
        //       int blockno = read_int(ibase + 4*y);
        //       char data[BLOCK_SZ];

        //       if (size > BLOCK_SZ) {
        //         read_block(blockno, data, BLOCK_SZ);
        //         fwrite(data, 1, BLOCK_SZ, w);
        //       } else {
        //         read_block(blockno, data, size);
        //         fwrite(data, 1, size, w);
        //       }

        //       size -= BLOCK_SZ;

        //       if (size < 0)
        //         break;
        //     }
        //   }
        // }

        // // read doubly indirect block
        // if (size > 0) {
        //   int dibase = i*BLOCK_SZ + j*100 + 32 + 4*N_DBLOCKS + 4*N_IBLOCKS;
        //   for (c = 0; c < BLOCK_SZ / sizeof(int); c++) {
        //     int iblockno = read_int(dibase + 4*c);
        //     int k;
        //     for (k = iblockno; k < iblockno + BLOCK_SZ; k += 4) {
        //       int blockno = read_int(k);
        //       char data[BLOCK_SZ];

        //       if (size > BLOCK_SZ) {
        //         read_block(blockno, data, BLOCK_SZ);
        //         fwrite(data, 1, BLOCK_SZ, w);
        //       } else {
        //         read_block(blockno, data, size);
        //         fwrite(data, 1, size, w);
        //       }

        //       size -= BLOCK_SZ;

        //       if (size < 0)
        //         break;
        //     }
        //   }
        // }



        // fclose(w);
      }
    }
  }
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
  // disk_image -insert -image IMAGE_FILE -nblocks N -iblocks M -inputfile FILE -u UID -g GID -block D -inodepos I

  if (argc < 2) {
    printf("missing argument: -create or -insert or -extract\n");
    exit(0);
  }

  if (!strcmp(argv[1], "-create")) {
    if (argc < 18) {
      printf("incorrect format, should be: \n");
      printf("disk_image -create/-insert -image IMAGE_FILE -nblocks N -iblocks M -inputfile FILE -u UID -g GID -block D -inodepos I\n");
      exit(0);
    }
    output_filename = argv[3];
    n_dblocks = atoi(argv[5]);
    n_iblocks = atoi(argv[7]);
    input_filename = argv[9];
    uid = atoi(argv[11]);
    gid = atoi(argv[13]);
    iblock = atoi(argv[15]);
    ipos = atoi(argv[17]);
  } 

  // disk_image -extract -image IMAGE_FILE -u UID -g GID -o PATH
  if (!strcmp(argv[1], "-extract")) {
    if (argc < 10) {
      printf("incorrect format, should be: \n");
      printf("disk_image -extract -image IMAGE_FILE -u UID -g GID -o PATH\n");
      exit(0);
    }
    input_filename = argv[3];
    uid = atoi(argv[5]);
    gid = atoi(argv[7]);
  }

  if(debug) {
    printf("iblcoks: %i\n", n_iblocks);
  }
  
  if (!strcmp(argv[1], "-create")) {
    outfile = fopen(output_filename, "wb");
    
    if (!outfile) {
      perror("datafile open\n");
      exit(-1);
    }

    if (debug)
      printf("about to place file\n");
    // fill in here to place file 
    place_file(input_filename, uid, gid, n_dblocks, n_iblocks, iblock, ipos);

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
  } else if (!strcmp(argv[1], "-extract")) {
    extract_files(input_filename, uid, gid);
  }

  printf("Done.\n");
  return;
}
