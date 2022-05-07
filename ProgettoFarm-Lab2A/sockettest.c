#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>

#include <sys/un.h>       /* ind AF_UNIX */
#define UNIX_PATH_MAX 108 /* man 7 unix */
#define SOCKNAME "./mysock"
#define N 100
int main (void) {
int fd_skt, fd_c;
char buf[N];
struct sockaddr_un sa;
strncpy(sa.sun_path, SOCKNAME,UNIX_PATH_MAX);
sa.sun_family=AF_UNIX;
pid_t pid = fork();
if (pid != 0)
{ /* padre, server */
    if((fd_skt = socket(AF_UNIX,SOCK_STREAM,0))==-1)
    {
        perror("socket");
        return(EXIT_FAILURE);
    }
    printf("socket number :%d \n",fd_skt);
    if (bind(fd_skt, (struct sockaddr *)&sa, sizeof(sa)) == -1)
    {
        perror("bind");
        return (EXIT_FAILURE);
    }
    if (listen(fd_skt, SOMAXCONN) == -1)
    {
        perror("listen");
        return (EXIT_FAILURE);
    }
    if (fd_c = accept(fd_skt, NULL, 0) == -1)
    {
        perror("accept");
        return (EXIT_FAILURE);
    }
    if (read(fd_c, buf, N) == -1)
    {
        perror("read");
        return (EXIT_FAILURE);
    }
    printf("Server got: % s\n", buf);
    if (write(fd_c, "Bye !", 5) == -1)
    {
        perror("write");
        return (EXIT_FAILURE);
    }
    
    
    close(fd_skt);
    close(fd_c);
    exit(EXIT_SUCCESS);
}
else
{ /* figlio, client */

    if ((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        return (EXIT_FAILURE);
    }
    while (connect(fd_skt, (struct sockaddr *)&sa, sizeof(sa)) == -1)
    {
        if (errno == ENOENT)

            sleep(1); /* sock non esiste */

        else
            exit(EXIT_FAILURE);
    }
    write(fd_skt, "Hallo !", 7) ;
    
   
    read(fd_skt, buf, N);
    printf("Client got : % s\n", buf);
    close(fd_skt);
    exit(EXIT_SUCCESS);
}
}
