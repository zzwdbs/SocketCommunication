#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <regex.h>
#include <ctype.h>

#define PROTOPORT 33455 /* default protocol port number for ipv4 */
#define PROTOPORT6 33446 /* default protocol port number for ipv6 */
#define BUFSIZE 1440 /* default buffer size for ipv4 */
#define BUFSIZE6 1280 /* default buffer size for ipv6 */
#define MAX_MSG_LEN 50 /* maximun message lenghth */

extern int errno;
char LOCALHOST[] = "localhost"; /* default host name             */
char DEFAULTFILE[] = "fileToTransfer"; /* default file name */

/* function declarations */
void safeExit(FILE* outfile, char* sendbuf, char* recvbuf, int code);

/*------------------------------------------------------------------------
 * Program:   tcpClient
 *
 * Purpose:   Create a TCP connection to server and download a file that has
 *            the given file name.
 *
 * Principle: 1. Create a TCP socket. Type is given by command-line argument
 *           2. Connect a server using this socket. Send 
 *               first message that contains requested file name.
 *            3. Check server's reply, if requested file does not exist, 
 *                abandon the process and exit. Otherwise proceed to 4.
 *            4. Create a file with the given file name.
 *            5. Receive packet from server, extract the content and write
 *                to the file.
 *
 * Syntax:    tcpClient [[[[host [port]] [hostl]] [outfile]] [lenbuf]] 
 *       host        - name of a host on which server is executing
 *       port        - protocol port number server is using
 *       outfile      - name of the file to request from server
 *       hostl       - name of host on which the client is executing
 *                     (not used in this application)
 *       lenbuf      - MSS, size of read and write buffers
 *
 * Note:   All arguments are optional.  If no host name is specified,
 *         the client uses "localhost"; if no protocol port is
 *         specified, the client uses the default given by PROTOPORT.
 *------------------------------------------------------------------------
 */
int main(int argc, char *argv[])
{
  struct hostent *ptrh;   /* pointer to a host table entry         */
  struct protoent *ptrp;  /* pointer to a protocol table entry     */
  struct sockaddr_in sad; /* structure to hold an IP address       */
  struct sockaddr_in6 sad6; /* structure to hold an IP address       */
  int protocol;
  size_t lenbuf;          /* length of input and output buffers    */
  int maxfdp1;            /* maximum descriptor value,             */
  int sd;                 /* socket descriptor                     */
  int port;               /* protocol port number                  */
  char *host;             /* pointer to host name                  */
  char *hostl;            /* name of the client host               */
  int EOFFlag;            /* flag, set to 1 when input file at EOF */
  char *sendbuf;          /* buffer for data going to the server   */
  char *recvbuf;          /* buffer for data from the server       */
  char *fname;            /* name of requested file                */
  char message[MAX_MSG_LEN];   /*array for messages that may be used */
  FILE *outfile;          /* file descriptor for output file       */
  size_t file_size;        /* size of requested file                */
  size_t recv_size;        /* number of bytes recevied from server  */
 
  int val;
  char* p;
  regex_t regex;
  regmatch_t pmatch[1];

  /* Initialize variables */
  recv_size = 0;
  EOFFlag = 0;
  outfile = NULL;
  sendbuf = NULL;
  recvbuf = NULL;
  memset((char *)&sad, 0, sizeof(sad)); /* clear sockaddr structure      */
  memset((char *)&sad6, 0, sizeof(sad6)); /* clear sockaddr structure      */
  protocol = AF_INET;             /* set family to Ipv4        */
  
  

  /* Check for command-line arguments                                  */
  /* If there are not arguments print an information message           */
  if (argc <= 1)
  {
    fprintf(stderr, "Command line arguments are required\n");
    fprintf(stderr, "In order the required arguments are:\n");
    fprintf(stderr, "IP address of remote communication endpoint:\n");
    fprintf(stderr, "	Default value localhost\n");
    fprintf(stderr, "port of remote communication endpoint\n");
    fprintf(stderr, "	  Default value 20004\n");
    fprintf(stderr, "IP address of local communication endpoint\n");
    fprintf(stderr, "	  Default value localhost\n");
    fprintf(stderr, "Requested filename\n");
    fprintf(stderr, "	  Default value inputfile\n");
    fprintf(stderr, "output filename (containing echoed data\n");
    fprintf(stderr, "	  Default value fileToTransfer\n");
    fprintf(stderr, "buffer size equals MSS for each packet\n");
    fprintf(stderr, "          Default value 1440 for ipv4\n");
    fprintf(stderr, "To accept any particular default replace\n");
    fprintf(stderr, "the variable with a . in the argument list\n");
    exit(0);
  }
  
  /* Check host argument, protocal, and assign host name. */
  /* Do this first because we need to know the protocal is ipv4 or ipv6 */
  /* Default name is LOCALHOST, to use default use ? as argument   */
  /* Convert host name to equivalent IP address and copy to sad. */
  /* if host argument specified   */
  if ((argc > 1) && strncmp(argv[1], ".", 1) != 0)
  {
    host = argv[1];
  }
  else
  {
    host = LOCALHOST;
  }
  ptrh = gethostbyname(host);
  if (((char *)ptrh) == NULL)
  {
    ptrh = gethostbyname2(host, AF_INET6);
    if (((char *)ptrh) == NULL){
      fprintf(stderr, "invalid host: %s\n", host);
      safeExit(outfile, sendbuf, recvbuf, 1);
    }
    protocol = AF_INET6;
  }

  /* Check command-line argument for buffer size  and extract          */
  /* ---to use default use . as argument or give no argument           */
  /* print error message and exit in case of error in reading          */
  if ((argc > 5) && strncmp(argv[5], ".", 1) != 0)
  {
    lenbuf = atoi(argv[5]);
  }
  else
  {
    if (protocol == AF_INET)
    {
      lenbuf = BUFSIZE;
    }
    else
    {
      lenbuf = BUFSIZE6;
    }
  }
  sendbuf = (char*)malloc(lenbuf * sizeof(int));
  if (sendbuf == NULL)
  {
    fprintf(stderr, "send buffer not created, size %s\n", argv[5]);
    exit(1);
  }
  recvbuf = (char*)malloc(lenbuf * sizeof(int));
  if (recvbuf == NULL)
  {
    fprintf(stderr, "receive buffer not created size %s\n", argv[5]);
    free(sendbuf);
    exit(1);
  }

  /* Check command-line argument for requested filename and extract       */
  /* Default filename is fileToTransfer                                    */
  /* ---to use default use . as argument or give no argument           */
  /* put the fileName into the send buffer                            */
  if ((argc > 4) && strncmp(argv[4], ".", 1) != 0)
  {
    fname = argv[4];
  }
  else
  {
    fname = DEFAULTFILE;
  }

  /* Check command-line argument for client name                       */
  /* Default client name is localhost, to use default use . as argument   */
  /* ---to use default use . as argument or give no argument 3 or 4    */
  if ((argc > 3) && strncmp(argv[3], ".", 1) != 0)
  {
    hostl = argv[3];
  }
  else
  {
    hostl = LOCALHOST;
  }
  

  /* Check command-line argument for protocol port and extract         */
  /* port number if one is extracted.  Otherwise, use the default      */
  /* to use default given by constant PROTOPORT use . as argument      */
  /* Value will be converted to an integer and checked for validity    */
  /* An invalid port number will cause the application to terminate    */
  /* Map TCP transport protocol name to protocol number.               */
  if ((argc > 2) && strncmp(argv[2], ".", 1) != 0)
  {
    port = atoi(argv[2]);
  }
  else
  {
    if (protocol == AF_INET)
    {
      port = PROTOPORT;
    }
    else
    {
      port = PROTOPORT6;
    }
  }
  if (port<=0)
  {
    fprintf(stderr, "bad port number %s\n", argv[2]);
    safeExit(outfile, sendbuf, recvbuf, 1);
  }
  if (((long int)(ptrp = getprotobyname("tcp"))) == 0)
  {
    fprintf(stderr, "cannot map \"tcp\" to protocol number");
    safeExit(outfile, sendbuf, recvbuf, 1);
  }

  /* Create a TCP socket, and connect it the the specified server      */
  if(protocol == AF_INET)
  {
    printf("Create IPv4 socket\n");
    sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
  }
  else
  {
    printf("Create IPv6 socket\n");
    sd = socket(PF_INET6, SOCK_STREAM, ptrp->p_proto);
  }
  
  if (sd < 0)
  {
    fprintf(stderr, "socket creation failed\n");
    safeExit(outfile, sendbuf, recvbuf, 1);
  }
  val = IP_PMTUDISC_DONT;
  if (setsockopt(sd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val)) < 0)
  {
    printf("Error setting MTU discover A\n");
  }
  val = lenbuf + 12;
  if (setsockopt(sd, IPPROTO_TCP, TCP_MAXSEG, &val, sizeof(val)) < 0)
  {
    printf("Error setting MAXSEG ofption A\n");
  }
  
  if(protocol == AF_INET)
  {
    sad.sin_port = htons((u_short)port);
    sad.sin_family = protocol;
    memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);
    inet_ntop(protocol, &(sad.sin_addr), message, MAX_MSG_LEN);
    printf("IPv4 connecting to address: %s, encoded port: %d\n",message, sad.sin_port);
    val = connect(sd, (struct sockaddr *)&sad, sizeof(sad));
  }
  else
  {
    sad6.sin6_port = htons((u_short)port);
    sad6.sin6_family = protocol;
    memcpy(&sad6.sin6_addr, ptrh->h_addr, ptrh->h_length);
    inet_ntop(protocol, &(sad6.sin6_addr), message, MAX_MSG_LEN);
    printf("IPv6 connecting to address: %s, encoded port: %d\n", message, sad6.sin6_port);
    val = connect(sd, (struct sockaddr *)&sad6, sizeof(sad6));
  }
  
  if (val < 0)
  {
    fprintf(stderr, "connect failed\n");
    close(sd);
    safeExit(outfile, sendbuf, recvbuf, 1);
  }
  
  /* Send requested file name to the server */
  strcpy(sendbuf, fname);
  if (send(sd, sendbuf, strlen(fname), 0) < 0)
  {
    fprintf(stderr, "fail to send file name\n");
    close(sd);
    safeExit(outfile, sendbuf, recvbuf, 1);
  }
  
  /* Receive the first response message */
  if (recv(sd, recvbuf, lenbuf, 0) < 0){
    fprintf(stderr, "fail to receive response message\n");
    close(sd);
    safeExit(outfile, sendbuf, recvbuf, 1);
  }
  
  /* If the request file doesn't exit, exit program */
  /* If the file size is received, extract the integer */
  /* Otherwise report unknown message error */
  if (!strcmp(recvbuf, "COULD NOT OPEN REQUESTED FILE")){
    printf("Requested file %s doesn't exit on server\n", fname);
    close(sd);
    safeExit(outfile, sendbuf, recvbuf, 0);
  }
  else {
    if (regcomp(&regex, "FILE SIZE IS [0-9]+ bytes", REG_EXTENDED)){
      fprintf(stderr, "failed to compose regex\n");
      close(sd);
      safeExit(outfile, sendbuf, recvbuf, 1);
    }
    if (!regexec(&regex, recvbuf, 1, pmatch, 0)){
      for (p = recvbuf; !isdigit(*p); p++){};
      file_size = strtol(p, &p, 10);
      printf("Requested file size: %zu bytes\n", file_size);
      regfree(&regex);
    }
    else {
      printf("Invalid reply message: %s\n", recvbuf);
      regfree(&regex);
      close(sd);
      safeExit(outfile, sendbuf, recvbuf, 0);
    }
  }
  
  /* Open file to write                    */
  if ((outfile = fopen(fname, "wb+")) == NULL)
  {
    fprintf(stderr, "failed to create file: %s\n", fname);
    close(sd);
    safeExit(outfile, sendbuf, recvbuf, 1);
  } 
  
  /* Start file transmission                    */
  while ((val = recv(sd, recvbuf, lenbuf, 0)) > 0){
    recv_size += val;
    fwrite(recvbuf, sizeof(char), val, outfile);
    if (recv_size >= file_size){
      break;
    }
  }
  if (val < 0){
    fprintf(stderr, "failed to transimit file\n");
    close(sd);
    safeExit(outfile, sendbuf, recvbuf, 1);
  }
  printf("Received bytes: %zu/%zu\n", recv_size, file_size);
  printf("Transmission complete\n");
  close(sd);
  safeExit(outfile, sendbuf, recvbuf, 0);

}

/* release memory and exit with code 1 */
void safeExit(FILE* outfile, char* sendbuf, char* recvbuf, int code){
  if (outfile){
    fclose(outfile);
  }
  if (sendbuf){
    free(sendbuf);
  }
  if (recvbuf){
    free(recvbuf);
  }
  exit(code);
}
