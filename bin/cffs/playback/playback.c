
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

/*
 * Playback is a program to "playback" the diffs between file system snapshots.
 * This is done by following the directions for creating, deleting, and 
 * modifying files as directed by the output from ssdiff and sortmods.
 *
 * Usage:
 * 	playback [-v] [-f input] [-s seed] [-c command] -i ipg -n numgrps -d dir
 *
 * input is an optional input file.  If on input file is provided, playback
 * takes its input from stdin.  ipg is the number of inodes per cylinder
 * group on the file system.  numgrps is the number of cylinder groups on
 * the file system.  dir is the root directory of the file system where the
 * playback should be performed.  The command option is used to specify a 
 * command to be executed each time a "SCORE" directive is encountered in
 * the input file.  
 *
 * Note that playback assumes that its input is complete.  In other words, any
 * file or directory referred to by the input, should have been previously
 * created by the same input file.
 *
 * The program assumes that it is running on an FFS, and exploits this 
 * assumption to ensure that the distribution of files among cylinder
 * groups is the same as on the original file system.  If you create N-1
 * directories on an empty FFS, where N is the number of cylinder groups
 * on the file system, you will have a file system with one directory
 * in each cylinder group (the root directory is the Nth directory).  During
 * the simulation, the program uses the inode numbers in the input to
 * determine which cylinder group each file was in on the original file
 * system.  The files are then created in the corresponding directory on
 * the simulated file system.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <err.h>
#include "playback.h"
#include "ASSERT.h"

char **directory_table;
char verbose = FALSE;
int ipg;			/* Number of inodes per cylinder group */
int ncg;			/* Number of cylinder groups */
char *device; 			/* Device to be used for taking snapshots */
char *io_data;			/* Buffer of data for writing files */
char *command = NULL;		/* Command to run to score file system */
int last_cg = 0;		/* Last cyl. group where we created a file */

void usage();
void Create();
void DoCreate();
void Delete();
void DoDelete();
void Modify();
void Replace();
void Score();
void write_file();

int
main ( argc, argv )
int argc;
char *argv[];
{
    int ch;			/* For argument parsing */
    extern char *optarg;
    char *inputfile;		/* Input filename */
    FILE *input;		/* Input file */
    int seed;			/* Random number seed */
    char *rootdir;		/* Name of root of target file system */
    char buffer[MAX_LINE];	/* Buffer for reading input */
    int i;

    inputfile = NULL;
    rootdir = NULL;
    input = stdin;
    seed = getpid();
    while ((ch = getopt ( argc, argv, "vf:i:n:d:c:s:" ) ) != EOF ) {
	switch ( ch ) {
	    case 'v':		/* Verbose mode */
		verbose++;
		break;
	    case 'f':		/* Input file */
		inputfile = optarg;
		break;
	    case 'i':		/* Number of inodes per cylinder group */
		ipg = atoi ( optarg );
		break;
	    case 'n':		/* Number of cylinder groups */
		ncg = atoi ( optarg );
		break;
	    case 'd':		/* Root of target file system */
		rootdir = optarg;
		break;
	    case 'c':		/* Command for scoring */
		command = optarg;
		break;
	    case 's':		/* Random seed */
		seed = atoi ( optarg );
		break;
	    default:
		usage(argv[0]);
		exit(1);
	}
    }
    if ( ipg == 0  ||  ncg == 0  ||  rootdir == NULL ) {
	usage(argv[0]);
	exit(1);
    }
    if ( inputfile != NULL ) {
	input = fopen ( inputfile, "r" );
	if ( input == NULL ) {
	    err ( 1, "Could not open input file" );
	}
    }
    srand ( seed );

    /*
     * Initialize buffer of data for I/O.  Fill the buffer with data
     * because some file systems may perform optimizations when zero
     * data is written to a file.
     */

    io_data = malloc ( IO_SIZE );
    for ( i = 0;  i < IO_SIZE;  i++ ) {
	io_data[i] = (char) i;
    }

    /* Make directories.  One for each cylinder group. */

    directory_table = (char **) malloc ( ncg * sizeof ( char * ) );
    if ( directory_table == NULL ) {
	err ( 1, "malloc:  failed to allocated directory_table" );
    }
    directory_table[0] = rootdir;
    for ( i = 1;  i <= ncg;  i++ ) {
	sprintf ( buffer, "%s/%ddir", rootdir, i );
	directory_table[i] = malloc ( strlen ( buffer ) + 1 );
	if ( directory_table[i] == NULL ) {
	    err ( 1, "malloc:  failed to allocate directory_table entry" );
	}
	strcpy ( directory_table[i], buffer );
	if ( mkdir ( directory_table[i], 0777 ) == -1 ) {
	    err ( 1, "mkdir:  Couldn't create %s", directory_table[i] );
	}
    }

    /*
     * Here is where we do all the real work
     */
    while ( fgets ( buffer, MAX_LINE, input ) != NULL ) {
        switch ( buffer[0] ) {
	    case 'C':
		Create ( buffer );
		break;
	    case 'D':
		Delete ( buffer );
		break;
	    case 'M':
		Modify ( buffer );
		break;
	    case 'R':
		Replace ( buffer );
		break;
	    case 'S':
		Score ( );
		break;
	    default:
		fprintf ( stderr, "Unknown input format: %s\n", buffer );
		fprintf ( stderr, "Ignoring!\n" );
	}
    }

    return 0;
}

/*
 * Handle commands to create a new file.  This just parses arguments
 * and calls the internal file creation function (DoCreate).
 */
void
Create ( buffer )
char *buffer;
{
    char inodestr[16];
    int inode;
    int time;
    int size;
    char type[8];

    sscanf ( buffer, "C %s %d %d %s", inodestr, &time, &size, type );
    inode = atoi ( inodestr );
    if ( verbose ) {
	printf ( "Creating inode #%s, %s, %d bytes\n", inodestr, type, size );
    }

    DoCreate ( inodestr, inode, time, size, type );
}

void 
DoCreate ( inodestr, inode, time, size, type )
char *inodestr;
int inode;
int time;
int size;
char *type;
{
    int cgnum;
    char fileName[MAX_LINE];

    cgnum = inode / ipg;
    strcpy ( fileName, directory_table[cgnum] );
    strcat ( fileName, "/" );
    strcat ( fileName, inodestr );

    write_file ( fileName, size, TRUE, TRUE );
}

void
Delete ( buffer )
char *buffer;
{
    char inodestr[16];
    int inode;
    int time;
    int size;

    sscanf ( buffer, "D %s %d %d", inodestr, &time, &size );
    inode = atoi ( inodestr );
    if ( verbose ) {
	printf ( "Deleting inode #%s, %d bytes\n", inodestr, size );
    }

    DoDelete ( inodestr, inode, time, size );
}

void
DoDelete ( inodestr, inode, time, size )
char *inodestr;
int inode;
int time;
int size;
{
    int cgnum;
    char fileName[MAX_LINE];
    int rval;

    cgnum = inode / ipg;
    strcpy ( fileName, directory_table[cgnum] );
    strcat ( fileName, "/" );
    strcat ( fileName, inodestr );

    rval = unlink ( fileName );
    if ( rval == -1 ) {
	warn ( "Couldn't remove %s", fileName );
    }
}

/*
 * Modifications to a file typically mean that the file has been 
 * truncated to zero length and then rewritten. 
 *
 * If the file is a directory, we do not truncate it.
 */

void 
Modify ( buffer )
char *buffer;
{
    char inodestr[16];
    int inode;
    int time;
    int old_size, new_size;
    char type[8];
    int cgnum;
    char fileName[MAX_LINE];

    sscanf ( buffer, "M %s %d %d %d %s", inodestr, &time,
	     &old_size, &new_size, type );
    inode = atoi ( inodestr );
    if ( verbose ) {
	printf ( "Modifying inode #%d, %s, from %d bytes to %d bytes\n",
		 inode, type, old_size, new_size );
    }

    cgnum = inode / ipg;
    strcpy ( fileName, directory_table[cgnum] );
    strcat ( fileName, "/" );
    strcat ( fileName, inodestr );

    if ( strcmp ( type, "DIR" ) == 0 )
        write_file ( fileName, new_size, FALSE, FALSE );
    else
	write_file ( fileName, new_size, FALSE, TRUE );
}

void
Replace ( buffer )
char *buffer;
{
    char inodestr[16];
    int inode;
    int time;
    int old_size, new_size;
    char old_type[8], new_type[8];

    sscanf ( buffer, "R %s %d %d %d %s %s", inodestr, &time, &old_size,
	     &new_size, old_type, new_type );
    inode = atoi ( inodestr );
    if ( verbose ) {
	printf ( "Replaceing inode #%d, %s, %d bytes with %s, %d bytes\n",
		 inode, old_type, old_size, new_type, new_size );
    }

    /*
     * All we do is delete the old version of the file, and then create
     * a new version.
     */

    DoDelete ( inodestr, inode, time, old_size );
    DoCreate ( inodestr, inode, time, new_size, new_type );
}

/*
 * Score()
 * 
 * Compute the fragmentation score for the file system.
 */
void
Score()
{
    if ( command == NULL )
	return;
    else
	system ( command );
}

/*
 * Display proper usage for program.
 */

void
usage ( name )
char *name;
{
    fprintf ( stderr, "Usage:  %s [-v] [-f input] [-s seed] [-c command] -i ipg -n ncg -d dir\n", name );
}

/*
 * write_file()
 *
 * Write data to a file, possibly creating it.  We are given the file name
 * and the desired file size.  We just fill the file by doing sequential
 * writes, since that is what most file writes look like.  The create flag 
 * is true if the file should be created.  Otherwise we truncate the file
 * when we open it.
 */
void
write_file ( file, size, create, truncate )
char *file;
int size;
u_char create;
u_char truncate;
{
    int rval;
    int fd;
    int nbytes;

    ASSERT ( file != NULL );
    ASSERT ( size >= 0 );

    if ( create == TRUE ) {
	/*
	 * create the file.
	 */
	fd = open ( file, O_CREAT | O_TRUNC | O_WRONLY, 0666 );
	if ( fd < 0 ) {
	    fprintf ( stderr, "Could not create %s\n", file );
	    perror ( "open() failed" );
	}
    } else if ( truncate == TRUE ) {
	/*
	 * We're modifying a file.  Truncate it as we open it.
	 */
	/*
	 * Note:  For some reason, occasionally we attempt to modify files
	 * that don't really exist.  I suspect that this is a bug in the
	 * generation of the workload, but don't have time to track it down.
	 * For now, just create the file if it doesn't exist.  7/14/95
	 */
	fd = open ( file, O_CREAT | O_TRUNC | O_WRONLY, 0666 );
	if ( fd < 0 ) {
	    fprintf ( stderr, "Could not modify %s\n", file );
	    perror ( "open() failed" );
	}
    } else { 	/* create == FALSE  &&  truncate == FALSE */
	/* 
	 * We're modifying a directory.  Don't actually truncate it, as
	 * that's not what happens when a directory is changed.
	 */
	fd = open ( file, O_CREAT | O_WRONLY , 0666 );
	if ( fd < 0 ) {
	    fprintf ( stderr, "Could not modify %s\n", file );
	    perror ( "open() failed" );
	}
    }

    while ( size > 0 ) {
	if ( size > IO_SIZE )
	    nbytes = IO_SIZE;
	else
	    nbytes = size;
	rval = write ( fd, io_data, nbytes );
	if ( rval < 0 ) {
	    perror ( "Write failed" );
	}
	size -= rval;
    }

    close ( fd );
}
