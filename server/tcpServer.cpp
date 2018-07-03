/* #include <stdlib.h> */
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#define PROTOPORT 33455    /* default ipv4 protocol port number      */
#define PROROPORT6 33446   /* default ipv6 protocol port number      */
#define QLEN 24            /* size of request queue                  */
#define MAX_FNAME_SIZE 255 /* max file name size                     */

int visits = 0;            /* counts client connections              */
/*------------------------------------------------------------------------
 * Program:   echoserver
 *
 * Purpose:   allocate a TCP and UDP socket and then repeatedly 
 *            executes the following:
 *      (1) wait for the next TCP connection or TCP packet or 
 *          UDP packet from a client
 *      (2) when accepting a TCP connection a child process is spawned
 *          to deal with the TCP data transfers on that new connection
 *      (2) when a TCP segment arrives it is served by the child process.
 *          The arriving segment is echoed back to the client
 *      (2) when a UDP packet arrives it is echoed back to the client. 
 *      (3) when the TCP connection is terminated the child then terminates
 *      (4) go back to step (1)
 *
 * Syntax:    server [[ port ] [buffer size]]
 *
 *       port  - protocol port number to use
 *       buffer size  - MSS of each packet sent
 *
 * Note:      The port argument is optional.  If no port is specified,
 *         the server uses the default given by PROTOPORT.
 *
 *------------------------------------------------------------------------
 */
int main(int argc, char *argv[])
{
  struct protoent *tcpptrp6;/*pointer to tcp protocol table entry v6*/
  struct protoent *tcpptrp; /*pointer to tcp protocol table entry  	*/
  struct sockaddr_in6 sad6;  /* structure to hold server's address v6*/
  struct sockaddr_in sad;   /* structure to hold server's address   */
  struct sockaddr_in cad;   /* structure to hold client's address   */
  struct sockaddr_in6 cad6;  /* structure to hold cilent's address v6*/
  int tcpsd, tcpsd6;        /* server socket descriptors            */
  int connfd;               /* client socket descriptor             */
  int maxfdp1;              /* maximum descriptor plus 1            */
  int port, port6;          /* protocol port number                 */
  char *echobuf;            /* buffer for string the server sends   */
  size_t lenbuf, lenbuf6;   /* length of buffer                     */
  size_t maxlen;			
  size_t conlenbuf;
  int nread;                /* # of octets received in one read     */
  int nwrite;               /* # of octets sent in one write        */
  int retval;               /* function return flag for testing     */
  struct timeval tval;      /* max time to wait before next select  */
  socklen_t len, len6;      /* length of the socket address struct  */
  int flag;					/* whether the socket is ipv4 or ipv6   */
  pid_t pid;
  fd_set descset;
  int val;
  FILE *file;
  unsigned long filesize;
  struct stat statebuff;
  char filename[MAX_FNAME_SIZE];
  int send_size;
  /* Initialize variables                                               */
  file = NULL;
  memset((char *)&sad, 0, sizeof(sad));    /* clear sockaddr structure      */
  memset((char *)&cad, 0, sizeof(cad));    /* clear sockaddr structure      */
  memset((char *)&sad6, 0, sizeof(sad6));  /* clear sockaddr structure     */
  memset((char *)&cad6, 0, sizeof(cad6));  /* clear sockaddr sturcture*/
  sad.sin_family = AF_INET;                /* set family to Internet        */
  sad.sin_addr.s_addr = htonl(INADDR_ANY); /* set the local IP address      */
  sad6.sin6_family = AF_INET6;               /* set family to Internet        */
  sad6.sin6_addr = in6addr_any;   /* set the local IP address      */
  cad.sin_family = AF_INET;                /* set family to Internet         */
  cad6.sin6_family = AF_INET6;			 /* set family to Internet         */
  /* Check for command-line arguments                                  */
  /* If there are not arguments print an information message           */
  if (argc <= 1)
  {
    fprintf(stderr, "Command line arguments are required\n");
    fprintf(stderr, "In order the required arguments are:\n");
    fprintf(stderr, "port of ipv4 remote communication endpoint\n");
    fprintf(stderr, "          Default value 33455\n");
    fprintf(stderr, "port of ipv6 remote communication endpoint\n");
    fprintf(stderr, "		   Default value 33446\n");
    fprintf(stderr, "buffer size equals MSS for ipv4 packets\n");
    fprintf(stderr, " 	Default value 1440 \n");
    fprintf(stderr, "buffer size equals MSS for ipv6 packets\n");
    fprintf(stderr, "	Default value 1280 \n");
    fprintf(stderr, "To accept any particular default replace\n");
    fprintf(stderr, "the variable with a . in the argument list\n");
    exit(0);
  }

  /* Check command-line argument for buffer size  and extract          */
  /* Default buffer size is 1440                                       */
  /* ---to use default use . as argument or give no argument           */
  /* print error message and exit in case of error in reading          */
  lenbuf = 1440;
  if ((argc > 3) && strncmp(argv[3], ".", 1) != 0)
  {
    lenbuf = atoi(argv[3]);
  }
  else
  {
    lenbuf = 1440;
  }
  if ((argc > 4) && strncmp(argv[4], ".", 1) != 0)
  {
    lenbuf6 = atoi(argv[4]);
  }
  else
  {
    lenbuf6 = 1280;
  }
  maxlen = lenbuf > lenbuf6 ? lenbuf : lenbuf6;
  echobuf = (char *)malloc(maxlen * sizeof(int));
  if (echobuf == NULL)
  {
    fprintf(stderr, "echo buffer not created, size %s\n", argv[2]);
    exit(1);
  }

  /* Check command-line argument for the protocol port and extract      */
  /* port number if one is specified.  Otherwise, use the   default     */
  /* port value given by constant PROTOPORT                             */
  /* check the resulting port number to assure it is valid (>0)         */
  /* convert the valid port number to network byte order and insert it  */
  /* ---  into the socket address structure.                            */
  /* OR print an error message and exit if the port is invalid          */
  if (argc > 1 && strncmp(argv[1], ".", 1) != 0)
  {
    port = atoi(argv[1]);
  }
  else
  {
    port = PROTOPORT;
    printf("%d\n", port);
  }
  if (port > 0)
  {
    sad.sin_port = htons((u_short)port);
  }
  else
  {
    fprintf(stderr, "bad port number %s\n", argv[1]);
    free(echobuf);
    exit(1);
  }
  if (argc >2 && strncmp(argv[2], ".", 1)!=0)
  {
  	port6 = atoi(argv[2]);
  }
  else
  {
  	port6 = PROROPORT6;
  	printf("%d\n", port6);
  }
  if (port6 > 0)
  {
    sad6.sin6_port = htons((u_short)port6);
  }
  else
  {
    fprintf(stderr, "bad port number %s\n", argv[2]);
    free(echobuf);
    exit(1);
  }
  	

  /* Map TCP transport protocol name to protocol number                 */
  /* Create a tcp socket with a socket descriptor tcpsd                 */
  /* Bind a local address to the tcp socket                             */
  /* Put the TCP socket in passive listen state and specify the lengthi */
  /* --- of request queue                                               */
  /* If any of these four processes fail an explanatory error message   */
  /* --- will be printed to stderr and the server will terminate        */
  if (((long int)(tcpptrp = getprotobyname("tcp"))) == 0)
  {
    fprintf(stderr, "cannot map \"tcp\" to protocol number");
    free(echobuf);
    exit(1);
  }
  printf("%d\n", tcpptrp->p_proto);
  tcpsd = socket(AF_INET, SOCK_STREAM, tcpptrp->p_proto);
  tcpsd6 = socket(AF_INET6, SOCK_STREAM, tcpptrp->p_proto);
  if (tcpsd < 0)
  {
    fprintf(stderr, "tcp socket for ipv4 creation failed\n");
    free(echobuf);
    close(tcpsd6);
    exit(1);
  }
  if (tcpsd6 < 0)
  {
    fprintf(stderr, "tcp socket for ipv6 creation failed\n");
    free(echobuf);
    close(tcpsd6);
    close(tcpsd);
    exit(1);
  }
  if (bind(tcpsd, (struct sockaddr *)&sad, sizeof(sad)) < 0)
  {
    fprintf(stderr, "tcp bind failed\n");
    free(echobuf);
    close(tcpsd6);
    close(tcpsd);
    exit(1);
  }
  if (bind(tcpsd6, (struct sockaddr *)&sad6, sizeof(sad6)) < 0)
  {
    fprintf(stderr, "tcp bind failed\n");
    free(echobuf);
    close(tcpsd6);
    close(tcpsd);
    exit(1);
  }
  if (listen(tcpsd, QLEN) < 0)
  {
    fprintf(stderr, "listen failed\n");
    free(echobuf);
    close(tcpsd6);
    close(tcpsd);
    exit(1);
  }
   if (listen(tcpsd6, QLEN) < 0)
  {
    fprintf(stderr, "listen failed\n");
    free(echobuf);
    close(tcpsd6);
    close(tcpsd);
    exit(1);
  }
  /* add 12 bytes for extra options in default header */
  val = lenbuf + 12;
  if (setsockopt(tcpsd, IPPROTO_TCP, TCP_MAXSEG, &val, sizeof(val)) < 0)
  {
    printf("ERROR setting MAXSEG TCP-A\n");
  }
  val = IP_PMTUDISC_DONT;
  if (setsockopt(tcpsd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val)) < 0)
  {
    printf("ERROR setting MTU DISCOVER option for tcp A\n");
  }
  val = lenbuf6 + 12;
  if (setsockopt(tcpsd6, IPPROTO_TCP, TCP_MAXSEG, &val, sizeof(val)) < 0)
  {
    printf("ERROR setting MAXSEG TCP-A(ipv6)\n");
  }
  val = IP_PMTUDISC_DONT;
  if (setsockopt(tcpsd6, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val)) < 0)
  {
    printf("ERROR setting MTU DISCOVER option for tcp A(ipv6)\n");
  }
  /* Main server loop - accept and handle requests                     */

  /* Define the descriptor set for select, unset all descriptors       */
  /* determine the largest descriptor in use                           */
  FD_ZERO(&descset);
  maxfdp1 = (tcpsd > tcpsd6 ? tcpsd : tcpsd6) + 1;
  

  /* Repeatedly check each socket for arriving data                     */
  /* On each pass through the for loop do each of the following:        */
  /* --- set the descriptors for the TCP and UDP sockets in descset     */
  /* --- check each socket for TCP connection requests or UDP data      */
  /* --- process TCP connection requests or incoming UDP data           */
  val = IP_PMTUDISC_DONT;
  if (setsockopt(tcpsd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val)) < 0)
  {
    printf("ERROR setting MTU DISCOVER option B");
  }
  val = IP_PMTUDISC_DONT;
  if (setsockopt(tcpsd6, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val)) < 0)
  {
    printf("ERROR setting MTU DISCOVER option B(ipv6)");
  }
  printf("proceed to listening\n");
  for (;;)
  {
    FD_SET(tcpsd, &descset);
	FD_SET(tcpsd6, &descset);
    if ((retval = select(maxfdp1, &descset, NULL, NULL, &tval)) < 0)
    {
      if (errno == EINTR)
      {
        printf("EINTR; select was interrupted \n");
        continue;
      }
      else if (retval == -1)
      {
        fprintf(stderr, "select error\n");
      }
      /*  timed out, no descriptors ready */
      else if (retval == 0)
      {
        if ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
        {
          printf("child process %d terminated \n", pid);
        }
        printf("retval 0\n");
      }
    }

    if ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
      /*printf("child process %d terminated \n", pid);*/
    }
	if (FD_ISSET(tcpsd, &descset))
	{
		flag = 4;
	}else if (FD_ISSET(tcpsd6, &descset))
	{
		flag = 6;
	}
	printf("TCP connection request received. Version : %d\n", flag);
    if (1)
    { /* processing tcp data                                         */
      len = sizeof(sad);
	  len6 = sizeof(sad6);
	  conlenbuf = flag == 4 ? lenbuf : lenbuf6;
      /* !!!!!!!!!!!!!!!!!!start child process!!!!!!!!!!!!!!!!!!!!!!!*/
      if ((fork()) == 0)
      {
        /* accept incoming data from new TCP connection                */
        /* spawn new TCP connection server to service new connection   */
        if (flag == 4)
        {
        	val = IP_PMTUDISC_DONT;
        	if (setsockopt(tcpsd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val)) < 0)
        	{
          	printf("ERROR setting tcp MTU DISCOVER D\n");
        	}
        	connfd = accept(tcpsd, (struct sockaddr *)&cad, &len);
        }
        else if (flag == 6)
        {
        	val = IP_PMTUDISC_DONT;
        	if (setsockopt(tcpsd6, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val)) < 0)
        	{
          	printf("ERROR setting tcp MTU DISCOVER D\n");
        	}
        	connfd = accept(tcpsd6, (struct sockaddr *)&cad6, &len6);
        }
        /* close listening sockets                                  */
        printf("%d\n",flag);
        close(tcpsd);
        close(tcpsd6);
        tval.tv_sec = 5;
        tval.tv_usec = 0;
        if (setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, &tval,
                       sizeof(tval)) < 0)
        {
          printf("ERROR setting RCVTIMEOUT for tcp\n");
        }
        /* accept first message */
        if (nread = read(connfd, echobuf, conlenbuf) < 0)
        {
          fprintf(stderr, "error reading from TCP socket");
          free(echobuf);
          close(connfd);
          exit(1);
        }
        printf("Cient request file: %s\n", echobuf);
        /* process first message */
        strcpy(filename, echobuf);
        file = fopen(echobuf, "r");

        if (file == NULL)
        {
          write(connfd, "COULD NOT OPEN REQUESTED FILE", strlen("COULD NOT OPEN REQUESTED FILE"));
          free(echobuf);
          close(connfd);
          exit(1);
        }
        else
        {
          strcpy(echobuf, "");
          filesize = -1;

          if (stat(filename, &statebuff) >= 0)
          {
            filesize = statebuff.st_size;
          }
          if (filesize == -1)
          {
            fprintf(stderr, "fail to get file size");
            free(echobuf);
            close(connfd);
            exit(1);
          }
          sprintf(echobuf, "FILE SIZE IS %d bytes\n", filesize);
          printf("%s", echobuf);
          nwrite = write(connfd, echobuf, strlen(echobuf));
          sleep(1);
        }
        /*start file transmission    */
        send_size = 0;
        while (1)
        {
          val = fread(echobuf, sizeof(char), conlenbuf, file);
          send_size += val;
          nwrite = write(connfd, echobuf, val);
          if (nwrite < 0)
          {
            fprintf(stderr, "fail to send file");
            free(echobuf);
            close(connfd);
            exit(1);
          }
          if (send_size >= filesize)
          {
            break;
          }
          sleep(1);
        }
        /*closing connection*/
        printf("send %d bytes\n", send_size);
        printf("transmission complete\n");
        free(echobuf);
        close(connfd);
        exit(0);
      }
      /* !!!!!!!!!!!!!!!!!!!!end child process!!!!!!!!!!!!!!!!!!!!!!!*/

      /* close TCP connection in the parent                          */
      close(connfd);
    }
    /* !!!!!!!!!!!!!!!!!!!!end child process!!!!!!!!!!!!!!!!!!!!!!!*/
  }
}
