#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <exos/netinet/cksum.h>

/* default number of bytes we checksum at a time. 1460 is the
   ethernet max frame size - headers. We do 1/4 of this at a time
   in case we need to send a smaller amount beacuse of the window,
   retransmission, etc. */

#define DEF_PKT_SZ (1460/4)
int pktsz = DEF_PKT_SZ;

void main (int argc, char *argv[]) {
  int len;
  char *buffer;
  unsigned int csum;
  struct {
    char sig[22];
    unsigned int pktsz;
  } csum_header;
  char *header_str = "precomputed checksums\n";
  
  if (argc > 2) {
    fprintf (stderr, "netcsum usage: netcsum [pktsz]\n"
	     "where pktsz is the (optional) size block to checksum at a time\n");
    exit (1);
  }
  if (argc == 2) {
    pktsz = atoi (argv[1]);
    if (!pktsz || pktsz > 1460) {
      fprintf (stderr, "netcsum: invalid block size\n");
      exit (1);
    }
  }

  buffer = (char *)malloc (pktsz);
  if (!buffer) {
    fprintf (stderr, "netcsum: could not allocate buffer space\n");
    exit (1);
  }
  
  /* write a header consisting of a magic number follwed by the
     size of the checksums in this file */

  strcpy (csum_header.sig, header_str);
  csum_header.pktsz = pktsz;
  write (1, &csum_header, sizeof (csum_header));

  while ((len = read (0, buffer, pktsz)) != -1) {
    if (len == 0) {
      /* done with file */
      break;
    }
    if (len < pktsz) {
      /* make sure we're at the end of the file */
      assert (read (0, buffer, 1) == 0);
    }
    
    csum = inet_checksum ((unsigned short *)buffer, len, 0, 0);
    write (1, &csum, sizeof (csum));
  }
}



