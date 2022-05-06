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
#include <sys/un.h>
#define UNIX_PATH_MAX 108 /* man 7 unix */
#define SOCKNAME "./mysock"
#define N 100

   
#include <util.h>

#define MAX_FILENAME_LENGTH 255

typedef struct threadArgs
{
    int thid;
    BQueue_t *q;
    char **regularfiles;
    int nregularfiles;
    int delay
} threadArgs_t;

void *Master(void *arg)
{
    
    int myid = ((threadArgs_t *)arg)->thid;

    BQueue_t *q = ((threadArgs_t *)arg)->q;
    char **regularfiles=((threadArgs_t*)arg)->regularfiles;
    int nregularfiles = ((threadArgs_t *)arg)->nregularfiles;
    int delay = ((threadArgs_t *)arg)->delay;
    int microsecondsdelay=delay*1000;
    

    

    for (int i = 0; i < nregularfiles; i++)
    {
        char **data = malloc(sizeof(char)*MAX_FILENAME_LENGTH);
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
        printf("Master %d pushed <%s>\n", myid,*data);
        usleep(microsecondsdelay);
    }
    
    printf("The master finished\n");
    
    return NULL;
}
void *Worker(void *arg)
{
    
   // printf("ITS ME ... WORKER\n");
    int myid = ((threadArgs_t *)arg)->thid;

    BQueue_t *q = ((threadArgs_t *)arg)->q;
    size_t consumed = 0;
    long  num=0;
    long sum=0;
    long i=0;
    while (1)
    {
       
        char **data;
        data = pop(q);
       // printf("Worker %d popped : <%s>\n",myid,data);
        assert(data);
       // printf("AFTER pop in worker\n");
        if (strcmp(data , "-1")==0)
        {
            printf("BEFORE free in worker\n");

            free(data);
            break;
        }
    
        ++consumed;

        printf("Worker %d extracted <%s>\n", myid, *data);

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
            
           sum+=(num*i);
           i++;
        }
        
        // inserisco l'EOS nella coda di uscita
    
        fclose(in);

       
        free(data);
    }

    printf("Worker %d summed a total of <%ld>\n", myid, sum);

    printf("Worker %d consumed <%ld> files\n", myid, consumed);
    
   printf("Worker %d finished\n",myid);
    return NULL;
}


void MasterWorker(int nthread,int qlen,int delay,int nfiles,char **filenames)
{
   size_t filesize;
   char **regularfiles = malloc(MAX_FILENAME_LENGTH * sizeof(char) * 1000);
   int nregularfiles=0;

   int fd_skt, fd_c;
   char buf[N];
   struct sockaddr_un sa;
   strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
   sa.sun_family = AF_UNIX;

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
      
       regularfiles[nregularfiles]=filenames[i];
       printf("Regular filename: %s\n", regularfiles[nregularfiles]);
       nregularfiles++;
    }

   
printf("Allocating memory for threads and threadargs\n");
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
//setting args for Master and Workers
printf("Setting args\n");
    thArgs[0].thid = 0;
    thArgs[0].q = q;
    thArgs[0].regularfiles=regularfiles;
    thArgs[0].nregularfiles=nregularfiles;
    thArgs[0].delay=delay;



    for (int i = 0; i < nthread ; i++)
    {
        thArgs[i+1].thid = i+1;
        thArgs[i+1].q = q;
        
    }

    
    printf("creating threads\n");
    
//creating Master and Workers
    
        if (pthread_create(&th[0], NULL, Master, &thArgs[0]) != 0)
        {
            printf("Error in master creation");
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < nthread; i++)
        {
            if (pthread_create(&th[i + 1], NULL, Worker,&thArgs[i+1]) != 0)
            {
                printf("Error in worker creation");
                exit(EXIT_FAILURE);
            }
        }

    printf("creating collector \n");
     //creating Collector process
        pid_t pid = fork();

        if (pid == -1)
        {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        else if (pid != 0)
        {
            //MasterWorker
            fd_skt = socket(AF_UNIX, SOCK_STREAM, 0);
            bind(fd_skt, (struct sockaddr *)&sa,

                 sizeof(sa));

            listen(fd_skt, SOMAXCONN);
            fd_c = accept(fd_skt, NULL, 0);
            read(fd_c, buf, N);
            printf("Server got: % s\n", buf);
            write(fd_c, "Bye !", 5);
            close(fd_skt);
            close(fd_c);
            exit(EXIT_SUCCESS);

            
            int status;
            // Wait for Collector
            waitpid(pid, &status, 0);

            
        }
        else
        {
            // Collector
            fd_skt = socket(AF_UNIX, SOCK_STREAM, 0);
            while (connect(fd_skt, (struct sockaddr *)&sa,

                           sizeof(sa)) == -1)
            {
                if (errno == ENOENT)

                    sleep(1); /* sock non esiste */

                else
                    exit(EXIT_FAILURE);
            }

            write(fd_skt, "Hallo !", 7);
            read(fd_skt, buf, N);
            printf("Client got: % s\n", buf);
            close(fd_skt);

            exit(EXIT_SUCCESS);
        }

    // waiting master and workers
    printf("About to wait for master and workers\n");
    pthread_join(th[0], NULL);
    
  char *end="-1";
    for (int i = 0; i < nthread; i++)
    {

        char *eos = malloc(sizeof(char)*MAX_FILENAME_LENGTH);
        strcpy(eos ,end);
        push(q, eos);
    }
    for (int i = 0; i < nthread; i++)
    {
        pthread_join(th[i + 1], NULL);
    }

    deleteBQueue(q,NULL);
    free(th);
    free(thArgs);
        
        printf("END OF MASTER WORKER\n");
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
    int delay=0;
    int nfiles=0;
    extern char *optarg;
    int  opt;
    int listBeginning=1;

    // parsing degli argomenti
    while ((opt = getopt(argc, argv, "n:q:t:")) != -1)
    {
        switch (opt)
        {
        case 'n':
            listBeginning+=2;
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
    nfiles=argc-listBeginning;
    printf("Number of arguments %d\n",argc);
    if (argc < 2)
    {
        usage(argv[0]);
    }
    printf("n=%d q=%d t=%d \n",nthread,qlen,delay);

    for(int i=listBeginning;i<argc;i++)
    {
       
        filenames[i-listBeginning]=argv[i];
    }
    for (int i =0; i < nfiles; i++)
    {
        printf("Filename: %s\n", filenames[i]);
        
    }

    MasterWorker(nthread, qlen, delay, nfiles, filenames);

        printf("End of main\n");

    return 0;
}
