/**
 * Tyler Socket Application
 * AESD
 */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
// #include <syslog.h>
#include <time.h>
#include <unistd.h>

#define PORT 9000
#define LOG_FILE "/var/tmp/aesdsocketdata.txt"

static int error = -1;

/* File Scope Variables */
bool d_mode = false;
FILE *fout;
pid_t pid;

bool kill_thread = false;
int loc = 0;
pthread_mutex_t mutex;

struct thread_info {
  pthread_t thread_id;
  SLIST_ENTRY(thread_info) entries;
  int client;
  bool done;
};
SLIST_HEAD(thread_list, thread_info) threads = SLIST_HEAD_INITIALIZER(threads);

static void *write_timestamp(void *arg) {
  time_t now;
  struct tm *time_info;
  char timestamp[128];

  sleep(10);

  while (!kill_thread) {
    time(&now);
    time_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %T %z\n",
             time_info);
    printf("Writing timestamp %s\n", timestamp);
    pthread_mutex_lock(&mutex); // LOCK
    fseek(fout, loc, SEEK_SET);
    fprintf(fout, "%s\n", timestamp);
    loc = (int)ftell(fout) - 1;
    pthread_mutex_unlock(&mutex); // UNLOCK

    sleep(10);
  }
}

const char NL = '\n';
static void *thread_server(void *arg) {
  char *clientRxBuf = (char *)malloc(512);
  char *clientTxBuf = (char *)malloc(512);
  char *ptr;
  char val;
  int nrx, ntx, counter = 0;

  pthread_mutex_lock(&mutex);
  struct thread_info *threadInfo = arg;
  int client = threadInfo->client;
  pthread_mutex_unlock(&mutex);

  while (!kill_thread) {

    while (ptr == NULL) {
      memset(clientRxBuf, 0, 512);
      nrx = (int)recv(client, clientRxBuf, 511, 0);
      ptr = strchr(clientRxBuf, NL);
      if (nrx <= 0) {
        break;
      } else if ((nrx == 511) && (NULL == ptr)) {
        /* Full buffer, dump */
        pthread_mutex_lock(&mutex);
        fseek(fout, loc, SEEK_SET);
        fprintf(fout, "%s\n", clientRxBuf);
        loc = (int)ftell(fout) - 1;
        pthread_mutex_unlock(&mutex);
      }
    }

    if (nrx <= 0) {
      if (nrx == 0) {
        shutdown(client, SHUT_RDWR);
        break;
      } else {
        shutdown(client, SHUT_RDWR);
        free(clientRxBuf);
        free(clientTxBuf);
        printf(
            "ERROR: Thread %ld aesdsocket failed to recv from client socket\n",
            pthread_self());
        pthread_exit(&error);
      }
    } else {
      /* Data is good */
      pthread_mutex_lock(&mutex);
      fseek(fout, loc, SEEK_SET);
      fprintf(fout, "%s\n", clientRxBuf);
      loc = (int)ftell(fout) - 1;

      /* Read Entire File Out */
      fseek(fout, 0, SEEK_SET);
      do {
        memset(clientTxBuf, 0, 512);
        val = '\0';
        for (counter = 0; counter < 511 && val != '\n'; counter++) {
          val = fgetc(fout);
          if (feof(fout)) {
            break;
          }
          clientTxBuf[counter] = val;
        }
        if (counter > 1) {
          ntx = (int)send(client, clientTxBuf, counter, 0);
        }
        if (ntx == -1) {
          printf(
              "ERROR: Thread %ld aesdsocket failed to send to client socket\n",
              pthread_self());
          shutdown(client, SHUT_RDWR);
          free(clientRxBuf);
          free(clientTxBuf);
          pthread_mutex_unlock(&mutex);
          pthread_exit(&error);
        }

      } while (!feof(fout));
      pthread_mutex_unlock(&mutex);
    }
  }
  /* Cleanup and leave */
  shutdown(client, SHUT_RDWR);
  free(clientRxBuf);
  free(clientTxBuf);
  printf("Thread %ld Completed connection from %d\n", pthread_self(), client);

  pthread_mutex_lock(&mutex);
  threadInfo->done = true;
  pthread_mutex_unlock(&mutex);

  pthread_exit(NULL);
}

/* Handle signal and exit */
static void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    kill_thread = true;
    fclose(fout);
    // remove(LOG_FILE);
    printf("Caught signal, exiting");
    exit(-1);
  }
}

int main(int argc, char **argv) {

  struct sockaddr_in svr;
  struct sockaddr_in clt;
  socklen_t addr_size;

  int serverfd, acceptfd, status;

  // register signal handlers
  printf("Starting Up \n");
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  pthread_mutex_init(&mutex, NULL);

  // setup server
  serverfd = socket(AF_INET, SOCK_STREAM, 0);
  if (serverfd == -1) {
    printf("socket error = %d\n", errno);
    exit(-1);
  } else {
    printf("socket success\n");
  }

  svr.sin_addr.s_addr = INADDR_ANY;
  svr.sin_family = AF_INET;
  svr.sin_port = htons(PORT);

  // bind server
  status =
      bind(serverfd, (struct sockaddr_in *)&svr, sizeof(struct sockaddr_in));
  if (status == -1) {
    printf("bind error = %d\n", errno);
    exit(-1);
  } else {
    printf("bind success\n");
  }

  // determine daemon mode
  if (argc > 1 && strcmp(argv[1], "-d") == 0) {
    d_mode = true;
  }

  // daemon mode
  if (d_mode) {
    pid = fork();
    if (pid == -1) {
      printf("fork error = %d\n", errno);
      return -1;
    } else if (pid != 0) {
      exit(0);
    }
  }

  status = listen(serverfd, 10);
  if (status == -1) {
    printf("listen error = %d\n", errno);
    exit(-1);
  } else {
    printf("listen success\n");
  }

  fout = fopen(LOG_FILE, "w+");
  if (fout == NULL) {
    printf("Failed to open %s\n", fout);
    exit(-1);
  }

  struct thread_info *thread_info_i;

  pthread_t time_thread;
  pthread_create(&time_thread, NULL, write_timestamp, NULL);

  while (!kill_thread) {
    addr_size = sizeof(clt);
    acceptfd = accept(serverfd, (struct sockaddr_in *)&clt, &addr_size);
    if (acceptfd == -1) {
      printf("accept error = %d\n", errno);
      fclose(fout);
      pthread_join(time_thread, NULL);
      pthread_mutex_destroy(&mutex);
      shutdown(acceptfd, SHUT_RDWR);
      shutdown(serverfd, SHUT_RDWR);
      exit(-1);
    }

    if (!kill_thread) {
      printf("accept success\n");
      printf("Accepted connection from : %s\n", inet_ntoa(clt.sin_addr));

      struct thread_info *info = malloc(sizeof(struct thread_info));
      info->done = false;
      info->client = acceptfd;
      pthread_create(&info->thread_id, NULL, thread_server, (void *)info);

      pthread_mutex_lock(&mutex);
      SLIST_INSERT_HEAD(&threads, info, entries);

      SLIST_FOREACH(thread_info_i, &threads, entries) {
        if (thread_info_i->done) {
          pthread_join(thread_info_i->thread_id, NULL);
          SLIST_REMOVE(&threads, thread_info_i, thread_info, entries);
        }
      }
      pthread_mutex_unlock(&mutex);
    }
  }

  pthread_join(time_thread, NULL);
  fclose(fout);
  shutdown(acceptfd, SHUT_RDWR);
  shutdown(serverfd, SHUT_RDWR);
  pthread_mutex_destroy(&mutex);
  return 0;
}
