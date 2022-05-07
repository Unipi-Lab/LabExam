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
#define SOCKNAME "./farm.sck"
#define N 100

static void run_server(struct sockaddr *psa)
{
    int fd_sk, fd_c, fd_num = 0, fd;
    char buf[N];
    fd_set set, rdset;
    int nread;
    fd_sk = socket(AF_UNIX, SOCK_STREAM, 0);
    bind(fd_sk, (struct sockaddr *)psa, sizeof(*psa));
    listen(fd_sk, SOMAXCONN);
    if (fd_sk > fd_num)
        fd_num = fd_sk;
    FD_ZERO(&set);
    FD_SET(fd_sk, &set);
    while (1)
    {
        rdset = set;
        if (select(fd_num + 1, &rdset, NULL, NULL, NULL) == -1)
        { /* gest errore */
            perror("select");
            exit(EXIT_FAILURE);
        }
        else
        { /* select OK */
            for (fd = 0; fd <= fd_num; fd++)
            {
                if (FD_ISSET(fd, &rdset))
                {
                    if (fd == fd_sk)
                    { /* sock connect pronto */
                        fd_c = accept(fd_sk, NULL, 0);
                        FD_SET(fd_c, &set);
                        if (fd_c > fd_num)
                            fd_num = fd_c;
                    }
                    else
                    { /* sock I/0 pronto */
                        nread = read(fd, buf, N);
                        if (nread == 0)
                        { /* EOF client finito */
                            FD_CLR(fd, &set);
                            while (!FD_ISSET(fd_num, &set))
                                fd_num--;
                            close(fd);
                        }
                        else
                        { /* nread !=0 */
                            printf("Server got : % s\n", buf);
                            write(fd, "Bye !", 5);
                        }
                    }
                }
            }
        }
    }
}

static void run_client(struct sockaddr *psa)
{
    if (fork() == 0)
    { /* figlio, client */

        int fd_skt;
        char buf[N];
        fd_skt = socket(AF_UNIX, SOCK_STREAM, 0);
        while (connect(fd_skt, (struct sockaddr *)psa,

                       sizeof(*psa)) == -1)
        {
            if (errno == ENOENT)
                sleep(1);
            else
                exit(EXIT_FAILURE);
        }
        write(fd_skt, "Hallo !", 7);
        read(fd_skt, buf, N);
        printf("Client got: % s\n", buf);
        close(fd_skt);
        exit(EXIT_SUCCESS);
    }
}

int main(void)
{

    struct sockaddr_un sa; /* ind AF_UNIX */
    strcpy(sa.sun_path, SOCKNAME);
    sa.sun_family = AF_UNIX;
    for (int i = 0; i < 4; i++)
        run_client(&sa); /* attiv client*/
    run_server(&sa);     /* attiv server */
    return 0;
}

/*
int fd_skt, fd_c;
char buf[N];
struct sockaddr_un sa;
strncpy(sa.sun_path, SOCKNAME,UNIX_PATH_MAX);
sa.sun_family=AF_UNIX;
pid_t pid = fork();
if (pid != 0)
{ // padre, server
if ((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
{
    perror("socket");
    return (EXIT_FAILURE);
}
printf("socket number :%d \n", fd_skt);
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
if ((fd_c = accept(fd_skt, NULL, 0)) == -1)
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
{ // figlio, client
    int n;
    if ((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        return (EXIT_FAILURE);
    }
    while (connect(fd_skt, (struct sockaddr *)&sa, sizeof(sa)) == -1)
    {
        if (errno == ENOENT)

            sleep(1); // sock non esiste

        else
            exit(EXIT_FAILURE);
    }

    if ((n = write(fd_skt, "Hallo !", 7)) == -1)
    {
        perror("write");
        exit(EXIT_FAILURE);
    }

    read(fd_skt, buf, N);
    printf("Client got : % s\n", buf);
    close(fd_skt);
    remove("farm.sck");
    exit(EXIT_SUCCESS);
}
*/