#include "csapp.h"

int open_clientfd(char *hostname, int port) 
{
  int clientfd;
  struct hostent *hp;
  struct sockaddr_in serveraddr;

  if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return -1; /* check errno for cause of error */
  }
  /* Fill in the server's IP address and port */
  if ((hp = gethostbyname(hostname)) == NULL) {
    return -2; /* check h_errno for cause of error */
  }
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  bcopy((char *)hp->h_addr_list[0], 
	(char *)&serveraddr.sin_addr.s_addr, hp->h_length);
  serveraddr.sin_port = htons(port);

  /* Establish a connection with the server */
  if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0) {
    return -1;
  }
  return clientfd;
}

int Open_clientfd(char *hostname, int port) 
{
  int rc;

  if ((rc = open_clientfd(hostname, port)) < 0) {
    if (rc == -1) {
      unix_error("Open_clientfd Unix error");
    } else{        
      dns_error("Open_clientfd DNS error");
    }
  }
  return rc;
}

void app_error(char *msg) /* application error */
{
  fprintf(stderr, "%s\n", msg);
  exit(0);
}

char *Fgets(char *ptr, int n, FILE *stream) 
{
  char *rptr;

  if (((rptr = fgets(ptr, n, stream)) == NULL) && ferror(stream))
    app_error("Fgets error");

  return rptr;
}

void dns_error(char *msg) /* dns-style error */
{
  fprintf(stderr, "%s: DNS error %d\n", msg, h_errno);
  exit(0);
}

void unix_error(char *msg) /* unix-style error */
{
  fprintf(stderr, "%s: %s\n", msg, strerror(errno));
  exit(0);
}

int Accept(int s, struct sockaddr *addr, socklen_t *addrlen) 
{
  int rc;

  if ((rc = accept(s, addr, addrlen)) < 0) {
    unix_error("Accept error");
  }
  return rc;
}

void Close(int fd) 
{
  int rc;

  if ((rc = close(fd)) < 0) {
    unix_error("Close error");
  }
}

struct hostent *Gethostbyaddr(const char *addr, int len, int type) 
{
  struct hostent *p;

  if ((p = gethostbyaddr(addr, len, type)) == NULL) {
    dns_error("Gethostbyaddr error");
  }
  return p;
}

int open_listenfd(int port) 
{
  int listenfd, optval=1;
  struct sockaddr_in serveraddr;
  
  /* Create a socket descriptor */
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return -1;
  }
  /* Eliminates "Address already in use" error from bind. */
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
		 (const void *)&optval , sizeof(int)) < 0) {
    return -1;
  }
  /* Listenfd will be an endpoint for all requests to port
     on any IP address for this host */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET; 
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
  serveraddr.sin_port = htons((unsigned short)port); 
  if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0) {
    return -1;
  }
  /* Make it a listening socket ready to accept connection requests */
  if (listen(listenfd, LISTENQ) < 0) {
    return -1;
  }
  return listenfd;
}

int Open_listenfd(int port) 
{
  int rc;

  if ((rc = open_listenfd(port)) < 0) {
    unix_error("Open_listenfd error");
}
  return rc;
}

void rio_readinitb(rio_t *rp, int fd) 
{
  rp->rio_fd = fd;  
  rp->rio_cnt = 0;  
  rp->rio_bufptr = rp->rio_buf;
}

void Rio_readinitb(rio_t *rp, int fd)
{
  rio_readinitb(rp, fd);
} 

static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
  int cnt;

  while (rp->rio_cnt <= 0) {  /* refill if buf is empty */
    rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, 
		       sizeof(rp->rio_buf));
    if (rp->rio_cnt < 0) {
      if (errno != EINTR) { /* interrupted by sig handler return */
	return -1;
      }
    }
    else if (rp->rio_cnt == 0) {  /* EOF */
      return 0;
    }
    else{ 
      rp->rio_bufptr = rp->rio_buf; /* reset buffer ptr */
    }
  }

  /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
  cnt = n;          
  if (rp->rio_cnt < n) {   
    cnt = rp->rio_cnt;
  }
  memcpy(usrbuf, rp->rio_bufptr, cnt);
  rp->rio_bufptr += cnt;
  rp->rio_cnt -= cnt;
  return cnt;
}

ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
  int n, rc;
  char c, *bufp = usrbuf;

  for (n = 1; n < maxlen; n++) { 
    if ((rc = rio_read(rp, &c, 1)) == 1) {
      *bufp++ = c;
      if (c == '\n') {
	break;
      }
    } else if (rc == 0) {
      if (n == 1) {
	return 0; /* EOF, no data read */
      }
      else {
	break;    /* EOF, some data was read */
      }
    } else {
      return -1;  /* error */
    }
  }
  *bufp = 0;
  return n;
}

ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
  ssize_t rc;

  if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
    unix_error("Rio_readlineb error");
  return rc;
} 

ssize_t rio_writen(int fd, void *usrbuf, size_t n) 
{
  size_t nleft = n;
  ssize_t nwritten;
  char *bufp = usrbuf;

  while (nleft > 0) {
    if ((nwritten = write(fd, bufp, nleft)) <= 0) {
      if (errno == EINTR) {  /* interrupted by sig handler return */
	nwritten = 0;    /* and call write() again */
      }
      else {
	return -1;       /* errno set by write() */
      }
    }
    nleft -= nwritten;
    bufp += nwritten;
  }
  return n;
}

void Rio_writen(int fd, void *usrbuf, size_t n) 
{
  if (rio_writen(fd, usrbuf, n) != n) {
    unix_error("Rio_writen error");
  }
}

void Free(void *ptr) 
{
  free(ptr);
}

void *Malloc(size_t size) 
{
  void *p;

  if ((p  = malloc(size)) == NULL)
    unix_error("Malloc error");
  return p;
}

void Pthread_create(pthread_t *tidp, pthread_attr_t *attrp, 
		    void * (*routine)(void *), void *argp) 
{
  int rc;

  if ((rc = pthread_create(tidp, attrp, routine, argp)) != 0)
    posix_error(rc, "Pthread_create error");
}

void posix_error(int code, char *msg) /* posix-style error */
{
  fprintf(stderr, "%s: %s\n", msg, strerror(code));
  exit(0);
}
