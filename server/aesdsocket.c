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

#define LOG_FILE "/var/tmp/aesdsocketdata.txt"

static int error = -1;

/* File Scope Variables */
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
  (void *)(arg);
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
  printf("Server Thread for client %d up and running", client);

  while (!kill_thread) {

    do {
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
    } while (ptr == NULL);

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
  const char *daemonArg = argv[1];
  bool d_mode = false;

  // register signal handlers
  printf("Starting Up \n");
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  pthread_mutex_init(&mutex, NULL);

  fout = fopen(LOG_FILE, "w+");
  if (fout == NULL) {
    printf("Failed to open %s\n", LOG_FILE);
    exit(-1);
  }

  // determine daemon mode
  if (argc > 1 && strcmp(daemonArg, "-d") == 0) {
    d_mode = true;
  }

  int nsocket, nbind, getaddr, nlisten, client;
  struct addrinfo hints, *servinfo;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  getaddr = getaddrinfo(NULL, "9000", &hints, &servinfo);

  if (getaddr != 0) {
    printf("Error - failed to getAddrInfo, returned %d", getaddr);
    freeaddrinfo(servinfo);
    fclose(fout);
    pthread_mutex_destroy(&mutex);
    return -1;
  }

  nsocket =
      socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  if (nsocket == -1) {
    printf("Error - failed to get socket, returned %d", nsocket);
    freeaddrinfo(servinfo);
    fclose(fout);
    pthread_mutex_destroy(&mutex);
    return -1;
  }

  do {
    nbind = bind(nsocket, servinfo->ai_addr, servinfo->ai_addrlen);
    close(nsocket);
    sleep(1);
    nsocket = socket(servinfo->ai_family, servinfo->ai_socktype,
                     servinfo->ai_protocol);
    if (nsocket == -1) {
      printf("Error - failed to get socket, returned %d", nsocket);
      freeaddrinfo(servinfo);
      fclose(fout);
      pthread_mutex_destroy(&mutex);
      return -1;
    }
  } while (nbind == -1 && errno == EADDRINUSE);
  freeaddrinfo(servinfo);

  int yes = 1;
  if (setsockopt(nsocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
    printf("Error - failed to setsockopt\n");
    shutdown(nsocket, SHUT_RDWR);
    fclose(fout);
    pthread_mutex_destroy(&mutex);
    return -1;
  }

  // daemon mode
  if (d_mode) {
    pid = fork();
    if (pid == -1) {
      printf("fork error = %d\n", errno);
      shutdown(nsocket, SHUT_RDWR);
      fclose(fout);
      pthread_mutex_destroy(&mutex);
      return -1;
    } else if (pid != 0) {
      exit(0);
    }
  }

  nlisten = listen(nsocket, SOMAXCONN);
  if (nlisten != 0) {
    printf("Error - failed to listen\n");
    shutdown(nsocket, SHUT_RDWR);
    fclose(fout);
    pthread_mutex_destroy(&mutex);
  } else {
    printf("Listening...\n");
  }

  struct thread_info *thread_info_i;
  pthread_t time_thread;
  pthread_create(&time_thread, NULL, write_timestamp, NULL);

  while (!kill_thread) {
    client = accept(nsocket, servinfo->ai_addr, &servinfo->ai_addrlen);
    if (client == -1) {
      printf("accept error = %d\n", errno);
      fclose(fout);
      pthread_join(time_thread, NULL);
      pthread_mutex_destroy(&mutex);
      shutdown(client, SHUT_RDWR);
      shutdown(nsocket, SHUT_RDWR);
      return -1;
    }

    if (!kill_thread) {
      printf("accept success\n");
      printf("Accepted connection from : %d\n", client);

      struct thread_info *info = malloc(sizeof(struct thread_info));
      info->done = false;
      info->client = client;
      pthread_create(&info->thread_id, NULL, thread_server, (void *)info);

      pthread_mutex_lock(&mutex);
      SLIST_INSERT_HEAD(&threads, info, entries);

      SLIST_FOREACH(thread_info_i, &threads, entries) {
        if (thread_info_i->done) {
          pthread_join(thread_info_i->thread_id, NULL);
          SLIST_REMOVE(&threads, thread_info_i, thread_info, entries);
          free(thread_info_i);
        }
      }
      pthread_mutex_unlock(&mutex);
    }
  }

  pthread_join(time_thread, NULL);
  fclose(fout);
  shutdown(client, SHUT_RDWR);
  shutdown(nsocket, SHUT_RDWR);
  pthread_mutex_destroy(&mutex);
  return 0;
}
