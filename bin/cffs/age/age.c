
/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */

#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>

/* 
 * Age attempts to simulate the behavior of an aging file system.
 *
 * The file system is aged in generations, with a number of file operations
 * per generation.  There are two file operations: create and delete.
 * Depending on the fullness of the file system, one of them is chosen with
 * a certain probability.  If create is chosen, a new file is created with
 * a size according to a predefined size distribution.  If delete is
 * chosen, a random file is deleted.
 *
 * The initial file sytem is created by running a number of generations
 * until a certain fullness is reached.
 *
 * The number of directories used is fixed, and for each operation one is
 * randomly chosen.  In addition, to creating and deleting of files, we
 * probably should also create and delete directories.
 *
 * The program has as one of its defines the size of the file system, which
 * should correspond to the size of the underlying file sytem.  */

#define FS_SIZE     32768        /* in blocks */
#define BLOCKSIZE   4096	/* block size in bytes */
#define FULLNESS    50		/* initial fullness (percentage) */
#define NGEN        100		/* number of generations */
#define OPS_P_GEN   1000	/* operations per generation */

#define NDIRS       64		/* number of directories */
#define DIRNAME     "d"
#define FILENAME    "f"
#define MAXNAME     1024

#define MIN(a, b)   (a < b) ? a : b

/* File size distribution in blocks */
static int  file_sz_distribution[100] = { 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 3,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 6,
    7, 8, 10, 12, 14, 16, 32, 64, 128, 256
};


/* Probability of creating a file, indexed on fullness of fs, which
 * varies from 0% through 100%.
 */
static int  op_distribution[100+1] ={
#if 1
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
    70, 70, 70, 70, 70, 70, 70, 70, 70, 70,
    70, 70, 70, 70, 70, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
#elif 1
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
    70, 70, 70, 70, 70, 70, 70, 70, 70, 70,
    70, 70, 70, 70, 70, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 15, 15, 15, 15, 15,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
#else
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
#endif
    0
};

static char data[BLOCKSIZE];	/* the data written into files */
static char name[MAXNAME];
static int  size = 0;		/* fs size in blocks */
static int  id = 0;
static int  nfile = 0;		/* number of files in fs */


void
delete_file(void)
{
    int d;
    DIR *dir;
    struct dirent *f;
    struct stat statbuf;
    int s;
    int n;
    int r;
    int i;

    if (nfile == 0) return;

    /* Find randomly a directory with some files and count the number of files
     * in that directory.
     */
    do {
	d = rand() % NDIRS;
	sprintf(name, "%s%d", DIRNAME, d);

	printf("delete file in dir %s\n", name);

	if ((dir = opendir(name)) == 0) {
	    perror("opendir");
	    exit(-1);
	}

	/* skip . and .. */
	f = readdir(dir);
	f = readdir(dir);
	f = readdir(dir);
	n = 1;
	
	/* Count the number of entries in this directory, and break. */
        if (f != 0) {
	    while (readdir(dir) != 0) n++;

	    if (closedir(dir) < 0) {
		perror("closedir");
		exit(-1);
	    }

	    break;
	}

	if (closedir(dir) < 0) {
	    perror("closedir");
	    exit(-1);
	}

    } while (f == 0);

    /* Randomly select a file out of the selected directory. */

    r = rand() % n;

    printf("dir %d entries; delete dir entry %d\n", n, r);

    if ((dir = opendir(name)) == 0) {
	perror("opendir 2");
	exit(-1);
    }

    for (i = 0; i < r + 3; i++) f = readdir(dir);

    assert(f);

    /* Delete the file */

    sprintf(name, "%s%d/%s", DIRNAME, d, f->d_name);

    closedir(dir);
    
    printf("delete %s\n", name);

    if (stat(name, &statbuf) < 0) {
	perror("stat");
	exit(-1);
    }

    s = statbuf.st_size / BLOCKSIZE;

    printf("size %d\n", s);

    if (unlink(name) < 0) {
	perror("unlink");
	exit(-1);
    }

    size -= s;
    nfile--;

    assert(nfile >= 0);
    assert(size >= 0);
}


void
create_file(void)
{
    int d;
    int s;
    int fd;
    int i;

    d = rand() % NDIRS;
    s = file_sz_distribution[rand() % 100];

    sprintf(name, "%s%d/%s%d", DIRNAME, d, FILENAME, id);

    printf("create file %s size %d\n", name, s);

    if ((fd = open(name, O_RDWR|O_CREAT|O_EXCL, S_IWUSR|S_IRUSR|S_IXUSR)) < 0) {
	perror("open");
	exit(-1);
    }

    /* Make sure the file fits in the file system. */
    s = MIN(s, FS_SIZE - size);

    for (i = 0; i < s; i++) {
	if (write(fd, data, BLOCKSIZE) < 0) {
	    perror("write");
	    exit(-1);
	}
    }

    close(fd);

    id++;
    nfile++;
    size += s;

    assert(size <= FS_SIZE);
}


void
gen(void)
{
    int i;
    int j;
    int r;

    for (i = 0; i < OPS_P_GEN; i++) {
	j = ((size * 100) / FS_SIZE);
	r = rand() % 100;

	assert(j <= 100);
	assert(j >= 0);

	printf("size %d j %d r %d\n", size, j, r);
	if (r <= op_distribution[j]) create_file();
	else delete_file();
    }
}


void
age(int ngen)
{
    int i;

    for (i = 0; i < ngen; i++)
	gen();
}



void
mkdirs(int n)
{
    int i;

    for (i = 0; i < n; i++) {
	sprintf(name, "%s%d", DIRNAME, i);
	if (mkdir(name, S_IWUSR|S_IRUSR|S_IXUSR) < 0) {
	    perror("mkdirs");
	    exit(-1);
	}
        size++;
    }
}


void
init_fs(int fullness)
{
    int j;

    mkdirs(NDIRS);

    /* Create an initial file system. */
    do {
	gen();
	j = ((size * 100) / FS_SIZE);
    } while (j < fullness);

    printf("init_fs: we have an initial file system %d\n", j);
}



int
main(int argc, char *argv[])
{

    srand(getpid());

    init_fs(FULLNESS);
    age(NGEN);

{
   extern int alloced;
   extern void cffs_flush ();
   printf ("filling/aging complete: FULLNESS %d, NGEN %d, OPS_PER_GEN %d, alloced %d\n", FULLNESS, NGEN, OPS_P_GEN, alloced);
   cffs_flush ();
}
    return(0);
}

