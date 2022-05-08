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
#include <boundedqueue.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <signal.h>
#define UNIX_PATH_MAX 108 /* man 7 unix */
#define SOCKNAME "./farm.sck"
#define N 100
#define EC(sc, m)           \
    if (sc == -1)           \
    {                       \
        perror(m);          \
        exit(EXIT_FAILURE); \
    }

#include <util.h>

#define MAX_FILENAME_LENGTH 255

void handlerINT(int s)
{
    remove("farm.sck");
    _exit(EXIT_SUCCESS);
}
void handlerHUP(int s)
{
    remove("farm.sck");
    _exit(EXIT_SUCCESS);
}
void handlerQUIT(int s)
{
    remove("farm.sck");
    _exit(EXIT_SUCCESS);
}
void handlerTERM(int s)
{
    remove("farm.sck");
    _exit(EXIT_SUCCESS);
}

typedef struct threadArgs
{
    int thid;
    BQueue_t *q;
    char **regularfiles;
    int nregularfiles;
    int delay;
    struct sockaddr *psa
} threadArgs_t;

void *Master(void *arg)
{

    int myid = ((threadArgs_t *)arg)->thid;

    BQueue_t *q = ((threadArgs_t *)arg)->q;
    char **regularfiles = ((threadArgs_t *)arg)->regularfiles;
    int nregularfiles = ((threadArgs_t *)arg)->nregularfiles;
    int delay = ((threadArgs_t *)arg)->delay;
    int microsecondsdelay = delay * 1000;

    for (int i = 0; i < nregularfiles; i++)
    {
        char **data = malloc(sizeof(char) * MAX_FILENAME_LENGTH);
        if (data == NULL)
        {
            perror("Producer malloc");
            pthread_exit(NULL);
        }
        *data = regularfiles[i];
        if (push(q, data) == -1)
        {
            fprintf(stderr, "Errore: push\n");
            pthread_exit(NULL);
        }

        usleep(microsecondsdelay);
    }

    return NULL;
}
void *Worker(void *arg)
{

    // printf("ITS ME ... WORKER\n");
    int myid = ((threadArgs_t *)arg)->thid;

    BQueue_t *q = ((threadArgs_t *)arg)->q;
    struct sockaddr *psa = ((threadArgs_t *)arg)->psa;

    size_t consumed = 0;
    long num = 0;
    long sum = 0;
    long i = 0;

    int fd_skt;
    char buf[N];

    if ((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        return (EXIT_FAILURE);
    }

    // try to connect to collector
    while (connect(fd_skt, (struct sockaddr *)psa,

                   sizeof(*psa)) == -1)
    {
        if (errno == ENOENT)
            sleep(1);
        else
            exit(EXIT_FAILURE);
    }

    while (1)
    {

        char resstring[MAX_FILENAME_LENGTH];
        char **data;
        data = pop(q);
        // printf("Worker %d popped : <%s>\n",myid,data);
        assert(data);
        // printf("AFTER pop in worker\n");
        if (strcmp(data, "-1") == 0)
        {

            free(data);
            break;
        }

        sum = 0;
        i = 0;
        ++consumed;

        FILE *in = NULL;
        char *line = NULL;

        if ((in = fopen(*data, "rb")) == NULL)
        {
            perror("fopen");
            return NULL;
        }

        // finche' ho linee da leggere vado avanti
        while (fread(&num, sizeof(long), 1, in) != 0)
        {

            sum += (num * i);
            i++;
        }

        sprintf(resstring, "%ld %s", sum, *data);

        if (write(fd_skt, resstring, 21) == -1)
        {
            perror("write");
            return (EXIT_FAILURE);
        }

        if (read(fd_skt, buf, N) == -1)
        {
            perror("worker read");
            return (EXIT_FAILURE);
        }

        fclose(in);

        free(data);
    }
    close(fd_skt);

    return NULL;
}

void Collector(struct sockaddr *psa)
{

    int fd_sk, fd_c, fd_num = 0, fd;
    char buf[N];
    fd_set set, rdset;
    int nread;
    if ((fd_sk = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        return (EXIT_FAILURE);
    }
    if (bind(fd_sk, (struct sockaddr *)psa, sizeof(*psa)) == -1)
    {
        perror("bind");
        return (EXIT_FAILURE);
    }
    if (listen(fd_sk, SOMAXCONN) == -1)
    {
        perror("listen");
        return (EXIT_FAILURE);
    }
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
            for (fd = 0; fd <= fd_num + 1; fd++)
            {
                if (FD_ISSET(fd, &rdset))
                {
                    if (fd == fd_sk)
                    { /* sock connect pronto */

                        if ((fd_c = accept(fd_sk, NULL, 0)) == -1)
                        {
                            perror("accept");
                            return (EXIT_FAILURE);
                        }
                        FD_SET(fd_c, &set);
                        if (fd_c > fd_num)
                            fd_num = fd_c;
                    }
                    else
                    { /* sock I/0 pronto */

                        if ((nread = read(fd, buf, N)) == -1)
                        {
                            perror("Collector read");
                            return (EXIT_FAILURE);
                        }
                        if (nread == 0)
                        { /* EOF client finito */
                            FD_CLR(fd, &set);
                            while (!FD_ISSET(fd_num, &set))
                                fd_num--;
                            close(fd);
                        }
                        else
                        { /* nread !=0 */
                            if (nread > 0)
                            {
                                buf[nread] = '\0';
                                printf("% s\n", buf);

                                if (write(fd, "Ty !", 4) == -1)
                                {
                                    perror("write");
                                    return (EXIT_FAILURE);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
void MasterWorker(int nthread, int qlen, int delay, int nfiles, char **filenames)
{
    size_t filesize;
    char **regularfiles = malloc(MAX_FILENAME_LENGTH * sizeof(char) * 1000);
    int nregularfiles = 0;

    struct sockaddr_un sa; /* ind AF_UNIX */
    strcpy(sa.sun_path, SOCKNAME);
    sa.sun_family = AF_UNIX;

    struct sigaction s;
    sigset_t set;

    EC(sigfillset(&set), "fillset");
    EC(pthread_sigmask(SIG_SETMASK, &set, NULL), "sigmask");

    EC(sigemptyset(&set), "emptyset");
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGQUIT);

    EC(sigaction(SIGINT, NULL, &s), "sigaction1");

    s.sa_mask = set;           // s.sa_mask contains signals to mask during handler execution
    s.sa_handler = handlerHUP; // sa_handler can be SIG_IGN to ignore a signal..or a function
    EC(sigaction(SIGHUP, &s, NULL), "sigaction4");

    s.sa_handler = handlerINT;
    EC(sigaction(SIGINT, &s, NULL), "sigaction2");

    s.sa_handler = handlerTERM;
    EC(sigaction(SIGTERM, &s, NULL), "sigaction3");

    s.sa_handler = handlerQUIT;
    EC(sigaction(SIGQUIT, &s, NULL), "sigaction5");

    EC(sigemptyset(&set), "emptyset");

    EC(pthread_sigmask(SIG_SETMASK, &set, NULL), "sigmask");

    for (int i = 0; i < nfiles; i++)
    {
        if (isRegular(filenames[i], &filesize) != 1)
        { // check if the file is regular ,and get the file size
            if (errno == 0)
            {
                fprintf(stderr, "%s non e' un file regolare\n", filenames[i]);
                continue;
            }
            perror("isRegular");
            continue;
        }

        regularfiles[nregularfiles] = filenames[i];

        nregularfiles++;
    }

    pthread_t *th;
    threadArgs_t *thArgs;
    th = malloc((nthread + 1) * sizeof(pthread_t));
    thArgs = malloc((nthread + 1) * sizeof(threadArgs_t));

    if (!th || !thArgs)
    {
        printf("Malloc error");
        exit(EXIT_FAILURE);
    }
    BQueue_t *q = initBQueue(qlen);

    if (!q)
    {
        printf("Queue creation error");
        exit(EXIT_FAILURE);
    }
    // setting args for Master and Workers

    thArgs[0].thid = 0;
    thArgs[0].q = q;
    thArgs[0].regularfiles = regularfiles;
    thArgs[0].nregularfiles = nregularfiles;
    thArgs[0].delay = delay;

    for (int i = 0; i < nthread; i++)
    {
        thArgs[i + 1].thid = i + 1;
        thArgs[i + 1].q = q;
        thArgs[i + 1].psa = &sa;
    }

    // creating Master and Workers

    if (pthread_create(&th[0], NULL, Master, &thArgs[0]) != 0)
    {
        printf("Error in master creation");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < nthread; i++)
    {
        if (pthread_create(&th[i + 1], NULL, Worker, &thArgs[i + 1]) != 0)
        {
            printf("Error in worker creation");
            exit(EXIT_FAILURE);
        }
    }

    // creating Collector process
    pid_t pid = fork();

    if (pid == -1)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        // Collector

        s.sa_handler = SIG_IGN;
        EC(sigaction(SIGINT, &s, NULL), "sigaction1collector");
        EC(sigaction(SIGHUP, &s, NULL), "sigaction2collector");
        EC(sigaction(SIGQUIT, &s, NULL), "sigaction3collector");
        EC(sigaction(SIGTERM, &s, NULL), "sigaction4collector");
        EC(sigaction(SIGPIPE, &s, NULL), "sigaction5collector");

        Collector(&sa);

        exit(EXIT_SUCCESS);
    }
    else
    {
        // MasterWorker
        int status;

        pthread_join(th[0], NULL);

        char *end = "-1";
        for (int i = 0; i < nthread; i++)
        {

            char *eos = malloc(sizeof(char) * MAX_FILENAME_LENGTH);
            strcpy(eos, end);
            push(q, eos);
        }
        for (int i = 0; i < nthread; i++)
        {
            pthread_join(th[i + 1], NULL);
        }

        deleteBQueue(q, NULL);
        free(th);
        free(thArgs);

        // Wait for Collector
        waitpid(pid, &status, 0);
        remove("farm.sck");
    }

    // waiting master and workers

    return NULL;
}

void usage(char *pname)
{

    fprintf(stderr, "\nUsage: %s filename1 filename 2 filename3 ...optional: -n nthreads -q qlen -t delay\n", pname);
    exit(EXIT_FAILURE);
}
int main(int argc, char *argv[])
{
    char **filenames = malloc(MAX_FILENAME_LENGTH * sizeof(char) * 1000);
    int nthread = 4;
    int qlen = 8;
    int delay = 0;
    int nfiles = 0;
    extern char *optarg;
    int opt;
    int listBeginning = 1;

    // parsing degli argomenti
    while ((opt = getopt(argc, argv, "n:q:t:")) != -1)
    {
        switch (opt)
        {
        case 'n':
            listBeginning += 2;
            nthread = atoi(optarg);
            break;
        case 'q':
            listBeginning += 2;
            qlen = atoi(optarg);
            break;
        case 't':
            listBeginning += 2;
            delay = atoi(optarg);
            break;
        default:

            break;
        }
    }
    nfiles = argc - listBeginning;

    if (argc < 2)
    {
        usage(argv[0]);
    }
    printf("n=%d q=%d t=%d \n", nthread, qlen, delay);

    for (int i = listBeginning; i < argc; i++)
    {

        filenames[i - listBeginning] = argv[i];
    }
    for (int i = 0; i < nfiles; i++)
    {
    }

    MasterWorker(nthread, qlen, delay, nfiles, filenames);

    return 0;
}
