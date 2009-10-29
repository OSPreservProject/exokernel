/*  A simple ftpd program   */
/* #define EXO */
/* #define USE_TCP */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <dirent.h> 
/*#include <sys/dir.h>*/
#include <malloc.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#ifdef EXO
#include <tcp/fast_tcp.h>
#include <tcp/libtcp.h>
#include <exos/timex.h>
#else
#include <sys/time.h>
#endif


#define MAXLINE    512
#define CMD_LEN    15

#define MAX_CMD_LEN   20
#define MAX_MSG_LEN   512
#define MAX_ARG_LEN   256
#define TYPE_A        0
#define TYPE_I        1
#define T_DIR         0
#define T_REG         1
#define T_WILD        2
#define N_WRITESIZE   32768
#define F_WRITESIZE   8192
#define BLKSIZE       32768
#define A_BUF_SIZE    32768
#define LIST_BUF_SIZE 4096
#define MAX_DIR_ENT   256

#define DATA_SIZE 400000

#define SERVER	5
#define CLIENT	2

#ifdef USE_TCP
typedef int cont_h;
typedef void *data_h;
#else
typedef int cont_h;
typedef int data_h;
#endif

typedef struct mapper_t {
    char name[20];
    char ip_addr[4];
    char eth_addr[6];
} map_t;
map_t map_net[] = {
        /* 0 */ {"bach", {18, 26, 0, 92}, {8,0x0,0x2b,0x24,0x2d,0xa4}},
        /* 1 */ {"bebop", {18, 26, 0, 133}, {8,0x0,0x2b,0x24,0x32,0xe9}},
        /* 2 */ {"utrecht", {18, 26, 0, 84}, {8,0x0,0x20,0x73,0xb9,0x56}},
        /* 3 */ {"joplin", {18, 26, 0, 68}, {8,0x0,0x2b,0x14,0xb0,0x92}},
        /* 4 */ {"leiden", {18, 26, 0, 14}, {8,0x0,0x2b,0xd,0xc3,0x37}},
        /* 5 */ {"lion", {18, 26, 0, 26}, {8,0,0x2b,0x1e,0x9f,0xfe}},
        /* 6 */ {"maastricht", {18, 26, 0, 89}, {8,0,0x20,0x12,0x4c,0x22}},
        /* 7 */ {"amsterdam", {18, 26, 0, 9}, {8,0,0x20,0x73,0xb9,0x1c}},
        /* 8 */ {"was-orbit", {18, 26, 0, 151}, {8,0,0x2b,0x2a,0x21,0x13}},
        /* 9 */ {"thumbscrew", {18, 26, 0, 221}, {8, 0, 0x2b, 0x2b, 0xf7, 0xa1}},
        /* 10 */{"tune", {18, 26, 0, 136}, {8,0,0x2b,0x37,0x75,0xd0}},
        /* 11 */{"rotterdam", {18, 26, 0, 130}, {8, 0, 0x2b, 0x10, 0x74, 0xb8}},
        /* 12 */{"dungeon", {18, 26, 0, 220}, {8, 0, 0x2b, 0x15, 0x3f, 0x7c}},
        /* 13 */{"wig", {18, 26, 0, 20}, {8,0,0x2b,0x24,0x36,0xfa}},
        {"", {},{}}
};

/*  structure to store transmission  parameters */
struct trans_struct {
  char addr[16];
  int  port;
  char r_type;     /* representation type */
  char ls_type;    /* type of ls representation */
};



void process_command( cont_h cont );

int get_data_conn( data_h *, struct trans_struct * );
int get_port_info( char *arg, struct trans_struct *ptrans_struct );
int store_file( cont_h, char *name, struct trans_struct * );
int get_data( cont_h, data_h, int file_fd, struct trans_struct * );
void retrieve_file( cont_h, char *name, struct trans_struct * );
int snd_data( cont_h, data_h, int file_fd, struct trans_struct * ); 
void list_dir( cont_h, char *pathname, struct trans_struct * );
int ftpd_reply( cont_h, char * ); 
int writen( data_h, char *buf, int nbytes );
int readline( cont_h cont, char *buf, int maxlen );
int c_dir( cont_h, char *pathname );
void test_data( cont_h cont, char *arg, struct trans_struct *ptrans_struct );
int write_tmp (int fd, char *buf, int bytes_left);
void print_ls_entry( char *tmp_buffer, struct stat *stat_buf, char *name );
void mkd( cont_h, char * );
void rmd( cont_h, char * );
void dele( cont_h, char * );
void pwd( cont_h );
void help( cont_h, char args[4][256] );
void site( cont_h, char args[4][256] );

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, clilen, serv_port;
  struct sockaddr_in  cli_addr, serv_addr;
  int ret;

  serv_port = 21;

  if (argc == 2)
    sscanf( argv[1], "%d", &serv_port);
  printf("using port number: %d\n", serv_port);
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)  {
    printf( "ERROR: server could not open socket\n" );
    exit (-1);
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family      = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port        = htons(serv_port);

  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)  {
    printf( "ERROR: server could not bind local addresses.\n" );
    return(-1);
  }

  if ((ret = listen(sockfd, 5)) < 0) {
     printf ("ERROR: server could not listen on ftp port (%d): ret %d\n", serv_port, ret);
     exit(0);
  }

  for ( ; ; ) {
    clilen = sizeof(cli_addr);
    if ( (newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) < 0)
      printf( "accept error, errno: %d\n", errno );
    ftpd_reply( newsockfd, "220 Connection established." );
    process_command( newsockfd );

    
/*  Don't fork for now */
/*     if (newsockfd < 0) { */
/*       printf ("ERROR: server accept error.\n"); */
/*       exit(1); */
/*     }       */

/*     if ( (childpid = fork()) < 0) { */
/*       printf ("ERROR: server fork error.\n"); */
/*       return 1; */
/*       } */

/*     else if (childpid == 0) { */
/*       close(sockfd);       */
/*       ftpd_reply( newsockfd, "220 I guess we're connected" ); */

/*       process_command( newsockfd ); */
      
/*     }  */

    close(newsockfd);
  }
}

void 
process_command(cont_h cont)
{ 
  int  n_args, n, line_ref, arg, arg_ref; 
  char args[4][256], buffer[MAX_MSG_LEN], line[MAX_MSG_LEN];
  struct trans_struct trans_params;
  trans_params.r_type = 'A';

  for ( ; ; ) { 
    n = readline(cont, line, MAX_MSG_LEN);     /* read in input */
    if (n == 0)
      return;
    else if (n < 0) {
      printf("ERROR: network read error.\n");
      exit(1);
    }
    args[0][0] = 0, args[1][0] = 0, args[2][0] = 0;
    line[strlen(line) - 2] = 0;        /* strip off CR-LF    */
    printf( "%s\n", line ); 
    arg_ref = 0, line_ref = 0, arg =  -1;
    while( line[line_ref] != '\0' && arg < 4 ) {
      while( line[line_ref] != ' ' && line[line_ref] != '\0'  ) {
	if (arg_ref == 0)
	  arg++;
	args[arg][arg_ref] = line[line_ref];
	arg_ref++, line_ref++;
      }
      args[arg][arg_ref] = '\0';
      if( line[line_ref] != '\0' )
	line_ref++;
      arg_ref = 0;
    }
    n_args = arg;
    
#if 0
    printf( "nargs: %d, %c %d, %c %d, %c %d\n", n_args, line[strlen(line)], line[strlen(line)], line[strlen(line) - 1], line[strlen(line) - 1], line[strlen(line) - 2], line[strlen(line) - 2] );
#endif
    
    /* start deciding which command needs to be run */
    if ( strcasecmp( args[0], "USER") == 0) {
      sprintf(buffer, "331 Password reqired for %s.", args[1]);
      ftpd_reply( cont, buffer );
    }
    else if ( strcasecmp( args[0], "PASS") == 0) {
      sprintf(buffer, "230 login successful");
      ftpd_reply( cont, buffer );
    }
    else if ( strcasecmp( args[0], "ACCT") == 0) {
      ftpd_reply( cont, "502 Command not implemented." );
    }
    else if ( strcasecmp( args[0], "TYPE" ) == 0) {
      trans_params.r_type = args[1][0];            /* set the representation type */
      sprintf(buffer, "200 Type set to %c", trans_params.r_type );
      ftpd_reply( cont, buffer );
    }
    else if ( strcasecmp( args[0], "MODE" ) == 0) {
      trans_params.r_type = args[1][0];            /* set the transfer mode */
      sprintf(buffer, "200 Mode set to %c", trans_params.r_type );
      ftpd_reply( cont, buffer );
    }
    else if ( strcasecmp( args[0], "STRU" ) == 0) {
      trans_params.r_type = args[1][0];            /* set the file structure */
      sprintf(buffer, "200 Structure set to %c", trans_params.r_type );
      ftpd_reply( cont, buffer );
    }
    else if ( strcasecmp( args[0], "PORT" ) == 0) {
      get_port_info( args[1], &trans_params );
      ftpd_reply( cont, "200 PORT command successful" );
    }
    else if ( strcasecmp( args[0], "NOOP" ) == 0)
      ftpd_reply( cont, "200 The server is OK." );
    else if ( strcasecmp( args[0], "STOR" ) == 0)
      store_file( cont, args[1], &trans_params ); /* arg is the file name */
    else if ( strcasecmp( args[0], "RETR" ) == 0)
      retrieve_file( cont, args[1], &trans_params );
    else if ( strcasecmp( args[0], "LIST" ) == 0) {
      trans_params.ls_type = 'L';
      list_dir( cont, args[1], &trans_params );
    }
    else if ( strcasecmp( args[0], "NLST" ) == 0) {
      if (strcmp( args[1], "-l" ) == 0) {
	trans_params.ls_type = 'L';
	list_dir( cont, ".", &trans_params );
      }
      else {
	trans_params.ls_type = 'S';
	list_dir( cont, args[1], &trans_params );
      }
    }
    else if ( strcasecmp( args[0], "CWD" ) == 0)
      c_dir( cont, args[1] ); 
    else if ( strcasecmp( args[0], "MKD" ) == 0)
      mkd( cont, args[1] );
    else if ( strcasecmp( args[0], "RMD" ) == 0)
      rmd( cont, args[1] );
    else if ( strcasecmp( args[0], "DELE" ) == 0)
      dele( cont, args[1] );
    else if ( strcasecmp( args[0], "CDUP" ) == 0)
      c_dir( cont, ".." );
    else if ( strcasecmp( args[0], "PWD" ) == 0)
      pwd( cont );
    else if ( strcasecmp( args[0], "SITE" ) == 0)
      site( cont, args );
    else if ( strcasecmp( args[0], "SYST" ) == 0)
      ftpd_reply( cont, "215 UNIX Type: L8" );
    else if ( strcasecmp( args[0], "HELP") == 0)
      help( cont, args );
    else if ( strcasecmp( args[0], "PASV" ) == 0)
      ftpd_reply( cont, "502 Command not implemented." );
    else if ( strcasecmp( args[0], "APPE" ) == 0)
      ftpd_reply( cont, "502 Command not implemented." );
    else if ( strcasecmp( args[0], "ALLO" ) == 0)
      ftpd_reply( cont, "502 Command not implemented." );
    else if ( strcasecmp( args[0], "RNFR" ) == 0)
      ftpd_reply( cont, "502 Command not implemented." );
    else if ( strcasecmp( args[0], "RNTO" ) == 0)
      ftpd_reply( cont, "502 Command not implemented." );
    else if ( strcasecmp( args[0], "ALLO" ) == 0)
      ftpd_reply( cont, "502 Command not implemented." );
    else if ( strcasecmp( args[0], "STAT" ) == 0)
      ftpd_reply( cont, "502 Command not implemented." );
    else if ( strcasecmp( args[0], "QUIT" ) == 0) {
      printf("\nquit processed\n");
      ftpd_reply( cont, "221 logging out" );
      close(cont);
/*      exit(0);  */  /*for now we aren't forking.  just return, not exit */
      return;
    }
    else if ( strcasecmp( args[0], "EXIT") == 0) {
      printf( "exiting\n" );
      exit(0);
    }
    else
      ftpd_reply( cont, "500 Command not recognized." );
  }  
}



int 
snd_data( cont_h cont, data_h data, int file_fd,
	     struct trans_struct *ptrans_params )
{
  int blksize, cnt, n_bytes, src_ref = 0, dest_ref = 0, b_time, n_write, n_written, total;
  char *buf, *dest_buf;
#ifndef EXO
  struct timeval s, e;
#endif
  blksize = BLKSIZE; 
  buf = 0;
  switch (ptrans_params->r_type) {

    /*  for sending binary formatted data */
  case 'I':
    if ((buf = (char *)malloc(BLKSIZE)) == NULL) 
      goto mem_error; 
#ifdef EXO
    B(); 
#else
    gettimeofday( &s, NULL );
#endif */
    total = 0;
    while ((cnt = read(file_fd, buf, BLKSIZE)) != 0) {
      n_bytes = 0;
      n_write = N_WRITESIZE;
      if ( (n_written = writen( data, buf + n_bytes, cnt)) != cnt  ) {
#ifdef USE_TCP
	tcp_close(data);
#else
	close(data);
#endif

#if 0
	printf( "write error in snd_data, cnt: %d, n_written: %d, errno: %d, total %d\n",
		cnt, n_written, errno, total );
#endif
	goto file_error;
      }
      total += n_written;
    }
#ifdef EXO
    E();
    b_time = timex_elapsed();
#else
    gettimeofday( &e, NULL );
    b_time = ( 1000000 * (e.tv_sec - s.tv_sec) ) + (e.tv_usec - s.tv_usec);
#endif
    printf( "binary transfer time: %d, bytes: %d, dtr: %d\n", b_time, total, (int) (total * 1000 / b_time));
    ftpd_reply( cont, "226 Transfer complete." );    
    free(buf);
    return (0);

  /* for sending ascii fromatted data */
  case 'A':
    if ( (buf = (char *) malloc(A_BUF_SIZE)) == NULL)	
      goto mem_error;
    if ( (dest_buf = (char *) malloc((int) (A_BUF_SIZE * 1.1))) == NULL )
      goto mem_error;
#ifdef EXO
    B();
#else
    gettimeofday( &s, NULL );
#endif
    total = 0;
    while( (n_bytes = read( file_fd, buf, A_BUF_SIZE )) != 0) {
      if (n_bytes < 0)
	goto file_error;
      while( src_ref < n_bytes ) {
	if (buf[src_ref] == '\n') {
	  dest_buf[dest_ref] = '\r';
	  dest_ref++;
	}
	dest_buf[dest_ref] = buf[src_ref];
	dest_ref++, src_ref++;
	if ((dest_ref + 1) >= ((int) (A_BUF_SIZE * 1.1)) || src_ref == n_bytes) {
	  if ( (cnt = writen( data, dest_buf, dest_ref)) != dest_ref) {
	    printf( "cnt: %d, dest_ref: %d, total: %d\n", cnt, dest_ref, total );
	    goto data_error;
	  }
	  total += cnt;
	  if ( src_ref < n_bytes )
	    dest_ref = 0;
	}
      }
      dest_ref = 0, src_ref = 0;
    }

#ifdef EXO
    E();
    b_time = timex_elapsed();
#else
    gettimeofday( &e, NULL );
    b_time = ( 1000000 * (e.tv_sec - s.tv_sec) ) + (e.tv_usec - s.tv_usec);
#endif
    printf( "ascii transfer time: %d, bytes: %d, dtr: %d\n",
	    b_time, total, (int) (total * 1000 / b_time));

    ftpd_reply( cont, "226 Transfer complete." );
    free(buf);
    free(dest_buf);
    return (0);
    
  default:
    ftpd_reply( cont, "550 Type not imlemented" );
  }

mem_error:
  ftpd_reply( cont, "451 Local resource failure: malloc");
  free(buf);
  return (-1);

file_error:
  ftpd_reply( cont, "551 Error on input file");
  free(buf);
  return (-1);

data_error:
  printf( "DATA ERROR\n" );
  ftpd_reply( cont, "426 Error on data connection");
  free(buf);
  return (-1);
}

  
/* function to get the port info from a port command */
int 
get_port_info( char *arg, struct trans_struct *ptrans_params )
{
  int p1, p2;
  int count_one = 0;
  int count_two = 0;
  while ( count_one < 4 ) {            /* get the address */
    if ( arg[count_two] == ',' ) {
      if (count_one != 3)              /* don't put a period at the end */
	ptrans_params->addr[count_two] = '.';
      count_one++;
    }
    else
      ptrans_params->addr[count_two] = arg[count_two];
    count_two++;
  }
  ptrans_params->addr[count_two - 1] = '\0';
  sscanf( arg + count_two, "%d,%d", &p1, &p2 );  /* get the port number */
  ptrans_params->port = 256 * p1 + p2;
#if 0
  printf("address: %s\nport:   %d\n", ptrans_params->addr, ptrans_params->port );
#endif
  return(0);
}



/* fuction to store a file on the server */
int 
store_file( cont_h cont, char *name,
	       struct trans_struct *ptrans_params ) 
{ 
  int file_fd;
  data_h data;

  /* open the file to put data into */
  printf("%s\n %d\n%d\n", name, (int) strlen(name), name[strlen(name) - 1] );
  if ( (file_fd = creat( name, 0666 )) < 0) {
    printf("ERROR opening  file for writing,  errno: %d\n", errno);
    ftpd_reply(cont, "450 Could not open remote file for writing.");
    return(-1);
  }

  if ( get_data_conn( &data, ptrans_params ) < 0 ) {
    ftpd_reply( cont, "425 Can't open data connection." );
    return(-1);
  }

  ftpd_reply( cont, "150 File status okay; about to open data connection." );

  get_data( cont, data, file_fd, ptrans_params);
  printf("store file: %s \n", name);
#ifdef USE_TCP
  tcp_close(data);
  tcp_exit(data);
#else
  close(data);
#endif
  close(file_fd);
  return(0);
}


int 
get_data( cont_h cont, data_h data, int file_fd,
	     struct trans_struct *ptrans_params)
{
  char *buf = NULL, *dest_buf = NULL;
  /*cr_end is to check if the read ended with a cr*/
  int byte_count = 0, bare_lfs = 0, cnt, src_ref = 0, dest_ref = 0, n_bytes, n, cr_end = 0; 

  switch ( ptrans_params->r_type ) {
    /* for getting ascii data */
  case 'A':
    if ( (buf = (char *) malloc(A_BUF_SIZE)) == NULL)
      goto mem_error;
    if ( (dest_buf = (char *) malloc(A_BUF_SIZE)) == NULL)       
      goto mem_error;
#ifdef USE_TCP
    while( (n_bytes = tcp_read( data, buf, A_BUF_SIZE)) !=  0)
#else
    while( (n_bytes = read( data, buf, A_BUF_SIZE)) !=  0)
#endif
      { 
      if (n_bytes < 0) 
	goto data_error;
      while( src_ref < n_bytes ) {
	if (cr_end > 0) {
	  if (buf[0] != '\n') {
	    dest_buf[0] = '\r';
	    dest_ref++;
	  }
	  cr_end = 0;
	}
	if (buf[src_ref] == '\n' && cr_end < 1)
	  bare_lfs++;
	while ( buf[src_ref] == '\r' ) {
	  src_ref++;
	  if (src_ref == n_bytes) 
	    cr_end = 1;
	  else if (buf[src_ref] != '\n') {
	    dest_buf[dest_ref] = '\r';
	    dest_ref++;
	  }
	}
	if (cr_end < 1) {
	  dest_buf[dest_ref] = buf[src_ref];
	  dest_ref++, src_ref++;
	}
      }
      write_tmp( file_fd, dest_buf, dest_ref ); 
      /*      write( file_fd, dest_buf, dest_ref ); */
      /* dest ref is # of bytes to write */ 
      dest_ref = 0, src_ref = 0;
    }
    ftpd_reply( cont, "226 Transfer complete");
    free(buf);
    free(dest_buf);
    return (0);

    /* for getting binary data */
  case 'I':
    if ( (buf = (char *) malloc(BLKSIZE)) == NULL)
      goto mem_error;
#ifdef USE_TCP
    while ((cnt = tcp_read( data, buf, BLKSIZE )) > 0) 
#else
    while ((cnt = read( data, buf, BLKSIZE )) > 0) 
#endif
      {
      if ( (n = write_tmp( file_fd, buf, cnt) != cnt)) {
	printf( "file error, errno: %d  n: %d\n", errno, n );
	goto file_error;
      }
      byte_count += cnt;
      if (cnt < 0)
	goto data_error;
    }
    ftpd_reply( cont, "226 Transfer complete");
    free(buf);
    return (0);
      
  default:
    ftpd_reply( cont, "553 Not implemented yet" );
  }

mem_error:
  ftpd_reply( cont, "451 Local resource failure: malloc");
  free(buf);
  free(dest_buf);
  return (-1);

file_error:
  ftpd_reply( cont, "551 File Error");
  free(buf);
  free(dest_buf);
  return (-1);

data_error:
  ftpd_reply( cont, "426 Error on data connection");
  free(buf);
  free(dest_buf);
  return (-1);
}

/* fuction to retrieve a file from the server */
void
retrieve_file( cont_h cont, char *name,
	       struct trans_struct *ptrans_params ) 
{ 
  int file_fd;
  data_h data;

  /* open the file to read data from */
  printf("%s\n %d\n%d\n", name, (int) strlen(name), name[strlen(name) - 1] );
  if ( (file_fd = open( name, O_RDONLY, 0 )) < 0) {
    printf("ERROR opening  file for reading,  errno: %d\n", errno);
    if (errno == ENOENT)
      ftpd_reply(cont, "550 The file does not exist");
    else
      ftpd_reply(cont, "450 The file could not be accessed");
    return;  }

  if ( get_data_conn( &data, ptrans_params ) < 0) {
    printf( "could not get data connection\n" );
    ftpd_reply( cont, "425 Can't open data connection." );
    return;
  }

  ftpd_reply( cont, "150 File status okay" );

  snd_data( cont, data, file_fd, ptrans_params);
  printf("retrieve file: %s \n", name);
#ifdef USE_TCP
  tcp_close(data);
  tcp_exit(data);
#else
  close(data);
#endif
  close(file_fd);
}

/* function to send directory listings */
void 
list_dir( cont_h cont, char *pathname,
	     struct trans_struct *ptrans_params )
{

  int           buf_ref = 0, dir_end, file_ref = 0, type;
  data_h        data;
  DIR           *dp = NULL; 
  struct dirent *dirp;
  struct stat stat_buf;
  char buffer[LIST_BUF_SIZE], tmp_buffer[MAX_DIR_ENT], file[256];

  if( pathname[0] == '\0' )
    pathname[0] = '.', pathname[1] = '\0';

  printf( "%s\n", pathname );
  

  if (pathname[strlen(pathname) - 1] == '*')
    type = T_WILD;
  else {
    /* determine the type of the file */
    if( lstat( pathname, &stat_buf ) < 0) {
      ftpd_reply( cont, "450 could not find file" );
      return;
    }
    if (S_ISREG(stat_buf.st_mode))
      type = T_REG;
    else if (S_ISDIR(stat_buf.st_mode))
      type = T_DIR;
    else 
      type = T_REG;
  }


  if( get_data_conn( &data, ptrans_params ) < 0) {
    ftpd_reply( cont, "425 Could not open data connection" );
    return;
  }

  ftpd_reply( cont, "150 ready to transmit list data"); 

  switch( type ) {

  case T_REG:
    buf_ref = 0;
    printf( "about to print single file data\n" );
    if (ptrans_params->ls_type == 'S')
      sprintf( tmp_buffer, "%s\n", pathname );
    else {
      printf( "about to print in long format" );
      print_ls_entry( tmp_buffer, &stat_buf, pathname );
    }
    printf( "about to print to send buffer\n" );
    memcpy( buffer + buf_ref, tmp_buffer, strlen(tmp_buffer));
    buf_ref += strlen(tmp_buffer);
    break;

  case T_WILD:
  /* first section is to  seperate dir from wildcard file */
    buf_ref = strlen(pathname);
    while (pathname[buf_ref] != '/' && buf_ref != -1)
      buf_ref--;
    buf_ref++;
    dir_end = buf_ref;
    while (pathname[buf_ref] != '*') {
      file[file_ref] = pathname[buf_ref];
      file_ref++, buf_ref++;
    }    
    pathname[dir_end] = '\0';
    file[file_ref] = '\0';

  case T_DIR:
    
    buf_ref = 0;
    if ( (dp = (DIR *) opendir(pathname)) == NULL) {
      ftpd_reply( cont, "450 could not open directory." );
      return;
    }
    while ( (dirp = (struct dirent *) readdir(dp)) != NULL ) {

      if (type ==  T_DIR || strncmp( file, dirp->d_name, strlen(file)) == 0) {
	if (ptrans_params->ls_type == 'S') {
	  sprintf(tmp_buffer,"%s\r\n", dirp->d_name );
	}
	else if (ptrans_params->ls_type == 'L') {
	  lstat( dirp->d_name, &stat_buf );
	  print_ls_entry( tmp_buffer, &stat_buf, dirp->d_name );
	}

	if ( (strlen(tmp_buffer) + buf_ref) > LIST_BUF_SIZE) {
	  buffer[buf_ref] = '\0';
	  writen( data, buffer, strlen(buffer) );
	  buf_ref = 0;
	}

	memcpy( buffer + buf_ref, tmp_buffer, strlen(tmp_buffer));
	buf_ref += strlen(tmp_buffer);

      }
    }
  }
  buffer[buf_ref] = '\0';
  writen( data, buffer, strlen(buffer) );
#ifdef USE_TCP
  tcp_close( data );
#else
  close(data);
#endif
  ftpd_reply( cont, "226 list complete");
  if( type != T_REG)
  closedir(dp);
  return;
}


/* function to print an ls entry using info returned by lstat */
void
print_ls_entry( char *tmp_buffer, struct stat *stat_buf, char *name ) 
{
  char mode_buf[11];

  if (S_ISDIR(stat_buf->st_mode))
    mode_buf[0] = 'd';
  else if (S_ISLNK(stat_buf->st_mode))
    mode_buf[0] = 'l';
  else if (S_ISBLK(stat_buf->st_mode))
    mode_buf[0] = 'b';
  else if (S_ISCHR(stat_buf->st_mode))
    mode_buf[0] = 'c';
  else if (S_ISFIFO(stat_buf->st_mode))
    mode_buf[0] = 'p';
  else 
    mode_buf[0] = '-';
  mode_buf[1] = (stat_buf->st_mode & 0400) ? 'r' : '-';
  mode_buf[2] = (stat_buf->st_mode & 0200) ? 'w' : '-';
  mode_buf[3] = (stat_buf->st_mode & 0100) ? 'x' : '-';
  mode_buf[4] = (stat_buf->st_mode & 0040) ? 'r' : '-';
  mode_buf[5] = (stat_buf->st_mode & 0020) ? 'w' : '-';
  mode_buf[6] = (stat_buf->st_mode & 0010) ? 'x' : '-';
  mode_buf[7] = (stat_buf->st_mode & 0004) ? 'r' : '-';
  mode_buf[8] = (stat_buf->st_mode & 0002) ? 'w' : '-';
  mode_buf[9] = (stat_buf->st_mode & 0001) ? 'x' : '-';
  mode_buf[10] = 0;
  
  sprintf(tmp_buffer, "%s %2d %8d.%-8d  %9d  %s\r\n",
	  mode_buf,
	  (int) stat_buf->st_nlink,
	  (int) stat_buf->st_uid,
	  (int) stat_buf->st_gid,
	  (int) stat_buf->st_size, 
	  name );
}

/* function to change the working directory */
int 
c_dir( cont_h cont, char *pathname )
{
  if ( chdir( pathname ) < 0) {
    ftpd_reply( cont, "550 Could not change working directory");
    return (-1);
  }
  ftpd_reply( cont, "250 Working directory changed" );
  return (0);
}

void
mkd( cont_h cont, char *pathname )
{
  if( mkdir( pathname, 0755 ) < 0 ) {
    ftpd_reply( cont, "550 Directory not created." );
    return;
  }
  ftpd_reply( cont, "257 Directory created." );
}

void
rmd( cont_h cont, char *pathname )
{
  if( rmdir( pathname ) < 0) {
    ftpd_reply( cont, "550 Directory not removed." );
    return;
  }
  ftpd_reply( cont, "250 Directory removed." );
}

void
dele( cont_h cont, char *pathname )
{
  if( unlink( pathname ) < 0) {
    ftpd_reply( cont, "550 Could not delete file." );
    return;
  }
  ftpd_reply( cont, "250 File deleted." );
}

void
pwd( cont_h cont )
{
  char buf[MAX_MSG_LEN];
  char path[MAX_MSG_LEN];

  sprintf( buf, "257 \"%s\" is the current directory.", getcwd( path, MAX_MSG_LEN) );
  ftpd_reply( cont, buf );
}

void
site( cont_h cont, char args[4][256] )
{
  int mode;

  if( strcasecmp( args[1], "CHMOD" ) == 0) {
    sscanf( args[2], "%o", &mode );
    if( chmod( args[3], mode ) < 0) {
      ftpd_reply( cont, "501 Chmod failed." );
      return;
    }
    ftpd_reply( cont, "200 CHMOD successful." );
  }
  else
    ftpd_reply( cont, "500 SITE command not recognized." );
}    

void
help( cont_h cont, char args[4][256] )
{
  if( strlen( args[1] ) == 0) {
    ftpd_reply( cont, "214-The following commands are recognized. (* means not implemented)" );
    ftpd_reply( cont, "    USER* PASS* ACCT* TYPE  MODE* STRU* PORT  NOOP  STOR" );
    ftpd_reply( cont, "    RETR  LIST  NLST  CWD   MKD   RMD   DELE  CDUP  PWD" );
    ftpd_reply( cont, "214 SYST  HELP  QUIT" );
  }
  else if( strcasecmp( args[1], "USER" ) == 0)
    ftpd_reply( cont, "214 Syntax: USER <sp> user-name; unimplemented" );
  else if( strcasecmp( args[1], "PASS" ) == 0)
    ftpd_reply( cont, "214 Syntax: PASS <sp> password; unimplemented" );
  else if( strcasecmp( args[1], "ACCT" ) == 0)
    ftpd_reply( cont, "214 Syntax: ACCT    (specify account); unimplemented" );
  else if( strcasecmp( args[1], "TYPE" ) == 0)
    ftpd_reply( cont, "214 Syntax: TYPE <sp> [A or I]" );
  else if( strcasecmp( args[1], "MODE" ) == 0)
    ftpd_reply( cont, "214 Syntax: MODE (specify transfer mode); unimplemented" );
  else if( strcasecmp( args[1], "STRU" ) == 0)
    ftpd_reply( cont, "214 Syntax: STRU (specify file structure); unimplemented" );
  else if( strcasecmp( args[1], "PORT" ) == 0)
    ftpd_reply( cont, "214 Syntax: PORT <sp> ip0,ip1,ip2,ip3,p1,p2" );
  else if( strcasecmp( args[1], "NOOP" ) == 0)
    ftpd_reply( cont, "214 Syntax: NOOP" );
  else if( strcasecmp( args[1], "STOR" ) == 0)
    ftpd_reply( cont, "214 Syntax: STOR <sp> file-name" );
  else if( strcasecmp( args[1], "RETR" ) == 0)
    ftpd_reply( cont, "214 Syntax: RETR <sp> file_name" );
  else if( strcasecmp( args[1], "LIST" ) == 0)
    ftpd_reply( cont, "214 Syntax: LIST [ <sp> path-name ]" );
  else if( strcasecmp( args[1], "NLST" ) == 0)
    ftpd_reply( cont, "214 Syntax: NLST [ <sp> path-name ] | [ -l ]" );
  else if( strcasecmp( args[1], "CWD" ) == 0)
    ftpd_reply( cont, "214 Syntax: CWD <sp> directory-name" );
  else if( strcasecmp( args[1], "MKD" ) == 0)
    ftpd_reply( cont, "214 Syntax: MKD <sp> directory-name" );
  else if( strcasecmp( args[1], "RMD" ) == 0)
    ftpd_reply( cont, "214 Syntax: RMD <sp> directory-name" );
  else if( strcasecmp( args[1], "DELE" ) == 0)
    ftpd_reply( cont, "214 Syntax: DELE <sp> file-name" );
  else if( strcasecmp( args[1], "CDUP" ) == 0)
    ftpd_reply( cont, "214 Syntax: CDUP (change to parent directory)" );
  else if( strcasecmp( args[1], "PWD" ) == 0)
    ftpd_reply( cont, "214 Syntax: PWD (return current working directory)" );
  else if( strcasecmp( args[1], "SYST" ) == 0)
    ftpd_reply( cont, "214 Syntax: SYST (get type of operating system" );
  else if( strcasecmp( args[1], "HELP" ) == 0)
    ftpd_reply( cont, "214 Syntax: HELP [ <sp> string ]" );
  else if( strcasecmp( args[1], "QUIT" ) == 0)
    ftpd_reply( cont, "214 Syntax: QUIT (terminate service)" );
  else if( strcasecmp( args[1], "SITE" ) == 0) {
    if( strcasecmp( args[2], "CHMOD" ) == 0)
      ftpd_reply( cont, "214 Syntax: CHMOD <sp> mode <sp> file-name" );
    else if( strlen( args[2] ) == 0) {
      ftpd_reply( cont, "214-The following SITE commands are recognized." );
      ftpd_reply( cont, "214 CHMOD" );
    }
    else
      ftpd_reply( cont, "214 No such command on this server" );
  }
  else
    ftpd_reply( cont, "214 No such command on this server" );
}

/* function to open a data connection */
int 
get_data_conn( data_h *data, struct trans_struct *ptrans_params )
{

#ifdef USE_TCP  
  uint32 ip_src;
  uint32 ip_dst;
  printf( "ready to get tcp handle using tcp_connect\n" );
  memcpy (&ip_src, map_net[SERVER].ip_addr, 4);
  memcpy (&ip_dst, map_net[CLIENT].ip_addr, 4);
  if ((*data = (void *) tcp_connect (ptrans_params->port, ip_dst, map_net[CLIENT].eth_addr, 0, ip_src, map_net[SERVER].eth_addr)) == 0) {
    printf ("cannot establish connection to server\n");
    return (-1);
  }
  printf( "got handle using tcp_connect\n" ); 

#else

  struct sockaddr_in cli_addr, serv_addr;
  if ( (*data = socket(AF_INET, SOCK_STREAM, 0)) < 0)  {
    printf( "ERROR: server could not open socket\n" );
    return (-1);
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  cli_addr.sin_family      = AF_INET;
  cli_addr.sin_addr.s_addr = inet_addr(ptrans_params->addr);
  cli_addr.sin_port        = htons(ptrans_params->port);
printf ("get_data_conn: s_addr 0x%x, port %d\n", inet_addr(ptrans_params->addr), ptrans_params->port);
  if (connect(*data, (struct sockaddr *) &cli_addr, sizeof(cli_addr)) < 0) {
    printf("ERROR: client could not connect to server\n");
    printf("%d\n", errno);
    exit(1);
  }
#endif
  return(0);
}

/* Function to send a reply to the the ftp client */
int 
ftpd_reply( cont_h cont, char *mess )
{
  char buffer[MAX_MSG_LEN];
  int nleft, nwritten;
  /* add CR LF */
  sprintf(buffer, "%s\r\n", mess);

  nleft = strlen(buffer);

  while (nleft > 0) {
    nwritten = write( cont, buffer, nleft);
    if (nwritten <= 0) {
      printf( "error sending reply\n" );
      return( -1 );
    }
    nleft -= nwritten;
    mess   += nwritten;
  }
  return(0);
}

/* Function to write n number of bytes to the data connection  */
int 
writen( data_h data, char *buf, int nbytes)
{
  int    nleft, nwritten, bytes;

  nleft = nbytes;
  bytes = N_WRITESIZE;
  while (nleft > 0) {
    if (nleft <= N_WRITESIZE )
      bytes = nleft; 
#ifdef USE_TCP
    nwritten = tcp_write( data, buf, bytes);
#else
    nwritten = write( data, buf, bytes);
#endif
    if (nwritten <= 0) {
      /* Also might want to consider possibility that a 0 return */
      /* is not necessarily an error. */
      printf( "error in writen, nleft: %d\n", nleft );
      return(nwritten);
    }
    nleft -= nwritten;
    buf   += nwritten;
  }
#if 0
  printf( "nbytes: %d, nleft: %d\n", nbytes, nleft );
#endif
  return(nbytes - nleft);
}

/* Function to read in a command line from the ftp client */
int
readline( cont_h cont, char *buf, int maxlen )
{
  int buf_ref = 0;
  do {
    read( cont, buf + buf_ref, 1);
/*     printf( "read 1 byte, %c\n", buf[buf_ref] ); */
    buf_ref++; 
  } while( buf_ref < maxlen - 1 && (buf[buf_ref - 1] != '\n' || buf[buf_ref - 2] != '\r' ));
/*     if( buf[buf_ref - 1] != '\n' || buf[buf_ref - 2] != '\r' ) { */
/*     printf( "error: message too long, it will be truncated." ); */
/*     buf[buf_ref - 1] = '\n', buf[buf_ref - 2] = '\r'; */
/*   } */
  buf[buf_ref] = '\0';
  return( buf_ref );
}

/* This is used if there are problems with writing large chunks of data to a file */
int 
write_tmp (int fd, char *buf, int bytes_left)
{
  int n;
  char *buf_orig;

  buf_orig = buf;

  while( bytes_left > 0) {
    if (bytes_left <= F_WRITESIZE) {
/*      printf( "ready to write last piece of data\n" );*/
      if ( (n = write( fd, buf, bytes_left )) != bytes_left ) {
	printf( "ERROR did not  write all data\n" );
	return (-1);
      }
      bytes_left = 0;
/*      total += n; */
      buf += n;
    }
    else {
      /*       printf( "ready to  write %d bytes\n", F_WRITESIZE );  */
      if ( (n = write( fd, buf, F_WRITESIZE )) != F_WRITESIZE) {
	printf( "ERROR did  not write full %d bytes\n", F_WRITESIZE );
	return (-1);
      }
      bytes_left -= F_WRITESIZE;
/*      total += n; */
      buf += n;
    }
  }      
/*  return (total); */
  return (buf - buf_orig);
}
      














