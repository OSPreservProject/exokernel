/*
 *       WEBSYNC.C :
 *                   Synchronziation program for the webswamp utility.
 *
 *      
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>


#ifdef EXOPC
#include <exos/tick.h>
#include <exos/vm-layout.h>             /* for PAGESIZ */
#else
#define ae_gettick()	time(NULL)
#define ae_getrate()	1000000
#define PAGESIZ 4096
#endif
#define SERV_PORT 2001


/* structure for sending the results back to sync host */
struct swamp_results{
   unsigned int filesize;
   unsigned int usecs;
   unsigned int bpms;
   unsigned int webpages;
   unsigned int inpackets;
   unsigned int outpackets;
};
typedef struct swamp_results SWAMP_RESULTS;
typedef SWAMP_RESULTS *RESULTS_PTR;
RESULTS_PTR  results_data = NULL;

struct swamp_results_final{
     unsigned int filesize;
     unsigned int usecs; 
     unsigned int bpms;
     unsigned int webpages;
     unsigned int inpackets;
     unsigned int outpackets;
}; 
typedef struct swamp_results_final FINAL_RESULTS;
typedef FINAL_RESULTS *FINAL_RESULTS_PTR;
FINAL_RESULTS_PTR totals = NULL;


/* link list for holding names of clients for "-sync" option */

struct client_name{
  char *client;
  struct client_name *next;
};

typedef struct client_name CLIENT_NAME;
typedef CLIENT_NAME *CLIENTPTR;

/* stucture passed for syncing up machines */ 

struct swamp_info{
  char synchost[50];
  char clientname[50];
  char servername[50];
  char documentname[50];
  int  serverport;
  int  maxconns;
  int  iters;
};

typedef struct swamp_info SWAMP_INFO;
typedef SWAMP_INFO *SWAMP_INFOPTR;
SWAMP_INFOPTR  swamp_data = NULL;


/* creates linked list of hosts */
int 
insert(CLIENTPTR *start, char *client)
{
    CLIENTPTR newptr = NULL;
    newptr = malloc(sizeof(CLIENT_NAME));

    if(*start == NULL){   
      *start = malloc(sizeof(CLIENT_NAME)); 
      (*start)->client = client;            
      (*start)->next = NULL;                
      return (0);
    }
                                    
    newptr->client = client;               
    newptr->next = *start;
    *start = newptr;
    return(0);
}

void
printlist(CLIENTPTR start)
{ 
  CLIENTPTR newptr = NULL;
  newptr = start;
  printf("Printing\n");
 
 again:
       printf("Client: %s\n", newptr->client);
       if(newptr->next != NULL){
	 newptr = newptr->next;
	 goto again;
       }
}

/* sends UDP to specified host */
int
send_data(int sockfd, const struct sockaddr *servaddr, int servlen)
{
  sendto(sockfd, swamp_data, sizeof(*swamp_data), 0, servaddr, servlen);        
  return(0);
}

int
get_results(int count)
{
  int sockfd, n, i, clilen;
  int max = 0;
  int min = 0;
  struct hostent *serverent;
  struct sockaddr_in servaddr, cliaddr;        printf("Getting Results\n\n\n");
       
  results_data = malloc(sizeof(SWAMP_RESULTS));  
  totals = malloc(sizeof(FINAL_RESULTS));
  bzero(totals, sizeof(*totals));  

  clilen = sizeof(cliaddr);
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  bzero( &servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(SERV_PORT);

   
  if(n =  bind(sockfd, (struct sockaddr *)&servaddr,  sizeof(servaddr) ) < 0)
    printf("Error on bind:%d\n", errno);

    for( i=1; i<=count; i++){
  
    n = recvfrom(sockfd, results_data, sizeof(*results_data), 0, (struct sockaddr *)&cliaddr, &clilen);

    serverent = gethostbyaddr(  (char *)&cliaddr.sin_addr, 4, AF_INET );

    printf("Results from: %s\n", (*serverent).h_name  );
    printf(" Bytes:%d\n usecs:%d\n bytes/sec:%d\n pages/sec:%f\n",  ntohl(results_data->filesize), 
             ntohl(results_data->usecs), ntohl(results_data->bpms), 
             (1000*((double)(ntohl(results_data->webpages))/ntohl(results_data->usecs))) ); 
    printf ("Done (webpages %d, inpackets %d, outpackets %d)\n\n", ntohl(results_data->webpages), 
             ntohl(results_data->inpackets), ntohl(results_data->outpackets));
  
    /* Tally final results */  
    totals->filesize   += ntohl(results_data->filesize); 
    totals->usecs      += ntohl(results_data->usecs);
    totals->webpages   += ntohl(results_data->webpages);
    totals->inpackets  += ntohl(results_data->inpackets);
    totals->outpackets += ntohl(results_data->outpackets);
    
    if( i == 1){
      min = ntohl(results_data->usecs);
      max = ntohl(results_data->usecs);
    }
    else{
      if(min > ntohl(results_data->usecs))
         min =  ntohl(results_data->usecs);
      else if(max < ntohl(results_data->usecs))
         max = ntohl(results_data->usecs);
    }
   }
 
    /* Print out combined totals */
    printf("----------------------------------\n TOTALS:\n");
    printf("Total Bytes: %d\n", totals->filesize);
    printf("Mininum usecs: %d\n", min);
    printf("Maximum usecs: %d\n", max);
    printf("Average usces: %d\n", totals->usecs/count);
    printf("Pages/sec (max secs): %f\n", (1000*((double)totals->webpages)/max) );
    printf("Bytes/sec (max secs): %f\n\n",(1000*((double)totals->filesize)/max) );
    printf("Webpages: %d\n", totals->webpages);
    printf("Inpackets: %d\n", totals->inpackets);
    printf("Outpackets: %d\n", totals->outpackets);
	  
 return(0);
}


int main (int argc, char **argv)
{
   int s, sockfd; 
   int count = 1;
   struct hostent *serverent;
   struct sockaddr_in servaddr;
   CLIENTPTR startptr = NULL;

   swamp_data = malloc(sizeof(SWAMP_INFO));  
   serverent = malloc(sizeof(struct hostent));   
   
   swamp_data->serverport = 80;
   swamp_data->maxconns   = 1;
   swamp_data->iters      = 100;
  

     if( argc < 6){
        printf ("Usage: %s <servername> <docname> [ <clientname> <serverport> <maxconns> <iters> ] 
                -h <hostnames> \n" , argv[0]);
        exit (0);
     } 
     strcpy(swamp_data->servername,  argv[1]);
     strcpy(swamp_data->documentname, argv[2]);
     strcpy(swamp_data->synchost, argv[3]);

     if( (argc >= 7)&&( strcmp(argv[4], "-h"))){                                printf("Set serverport\n");
        swamp_data->serverport = atoi (argv[4]);
        if ((swamp_data->serverport < 0) || (swamp_data->serverport > 65535)) {
           printf ("Invalid server port number specified: %d\n", swamp_data->serverport);
           exit (0);
         }
     }

    if( (argc >= 8) && strcmp(argv[5], "-h") )  {
       swamp_data->maxconns = atoi (argv[5]);
       if ((swamp_data->maxconns < 0) || (swamp_data->maxconns > 128)) {
         printf ("Invalid maximum concurrent connection count specified: %d\n", swamp_data->maxconns);
         exit (0);
      }
    }
    
    if( (argc >= 9) && strcmp(argv[6], "-h")){
       swamp_data->iters = atoi (argv[6]);
       if (swamp_data->iters < 0) {
          printf ("Invalid maximum iteration count specified: %d\n", swamp_data->iters);
          exit (0);
      }
    }
   
    for( s=4; s<=argc-1; s++) {             //  printf("count %d, string: %s\n", argc, argv[s]);
       if ( !strcmp(argv[s], "-h") ){     
          for(s=s+1; s<=argc-1; s++){             
            insert(&startptr, argv[s]);                 }
       break;
      }
    }
   
   bzero(&servaddr, sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_port = htons(SERV_PORT);
   sockfd = socket(AF_INET, SOCK_DGRAM, 0);   //  printf("opened socket\n");
   
 loop:
     serverent = gethostbyname( startptr->client );
     strcpy(swamp_data->clientname, startptr->client);
     //     printf("Client:%s\n", startptr->client); 
     bcopy( serverent->h_addr_list[0], (char *) &servaddr.sin_addr, serverent->h_length );
     send_data(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
      if( startptr->next != NULL){ 
        count++;
        startptr = startptr->next;
	goto loop;
      }
      if( !strcmp(swamp_data->documentname, "shutdown")){
	printf("Sending the shutdown signal...exiting!!\n");
	exit(0);
      }
	  
   close(sockfd);
   get_results(count);
   
  exit(0);
}
