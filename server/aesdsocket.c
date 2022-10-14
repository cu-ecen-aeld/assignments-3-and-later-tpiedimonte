/**
 * Tyler Socket Application
 * AESD
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <syslog.h>


 
#define PORT 9000
#define LOG_FILE "/var/tmp/aesdsocketdata"
 
 
 /* File Scope Variables */
 bool d_mode = false;
 int filefd, serverfd;
 pid_t pid;
  

/* Handle signal and exit */
static void signal_handler(int signal)
{
    if(signal == SIGINT || signal == SIGTERM)
    {
        close(filefd);
        close(serverfd);
        remove(LOG_FILE);
        syslog(LOG_ERR, "Caught signal, exiting");
        exit(-1);
        
    }
}

int main(int argc, char **argv) 
{

    char * write_packet;
    char * read_packet;
    char server_buf[256];
    
    struct sockaddr_in svr;
    struct sockaddr_in clt;
    socklen_t addr_size;
    
    int status, recv_bytes, acceptfd, max_buf_size, saved_bytes, send_bytes = 0, read_bytes = 0;
    
    //register signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    
    // setup server
    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverfd == -1)
    {
        syslog(LOG_ERR, "socket error = %d\n", errno);
        exit(-1);
    }
    else
    {
        syslog(LOG_DEBUG, "socket success\n");
    }
    
    svr.sin_addr.s_addr = INADDR_ANY;
    svr.sin_family = AF_INET;
    svr.sin_port = htons(PORT);
    
    //bind server
   status = bind(serverfd , (struct sockaddr_in *)&svr , sizeof(struct sockaddr_in));
   if(status == -1)
   {
        syslog(LOG_ERR, "bind error = %d\n", errno);
        exit(-1);
   } 
   else
   {
       syslog(LOG_DEBUG, "bind success\n");
   }
   
   //determine daemon mode
   if(argc>1 && strcmp(argv[1],"-d") == 0)
   {
       d_mode = true;
   }
   
   //daemon mode
   if(d_mode)
   {
	pid = fork();
	if(pid == -1)
	{
	    syslog(LOG_ERR, "fork error = %d\n",errno);
	    return -1;
	}
	else if(pid != 0)
	{
			exit(0);
	}
		
	status = setsid();
	if(status == -1)
	{
	    syslog(LOG_ERR, "setsid error = %d\n",errno);
	    return -1;
	}
	else
	{
	    syslog(LOG_DEBUG, "setsid success\n");
	}
		
	status = chdir("/");
	if(status == -1)
	{
	    syslog(LOG_ERR, "chdir error = %d\n",errno);
	    return -1;
	}
		
	open("/dev/null", O_RDWR);
	dup(0);
	dup(0);
   }
   
    status = listen(serverfd, 10);
    if(status == -1)
    {
        syslog(LOG_ERR, "listen error = %d\n",errno);
        exit(-1);
    }
    else
    {
        syslog(LOG_DEBUG, "listen success\n");
    }
    
    
    filefd = open(LOG_FILE, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
        
    while(1)
    {
        addr_size = sizeof(clt);
        acceptfd = accept(serverfd, (struct sockaddr_in *) &clt, &addr_size);
	if(acceptfd == -1)
	{
		syslog(LOG_ERR, "accept error = %d\n",errno);
		exit(-1);
	}
	else
	{
		syslog(LOG_DEBUG, "accept success\n");
		syslog(LOG_DEBUG, "Accepted connection from : %s\n", inet_ntoa(clt.sin_addr));
	}
	
        max_buf_size = 256;
        saved_bytes = 0;


        write_packet = malloc(sizeof(char) * 256);

        bool packet_collected = false;
        while(!packet_collected)
        {
            recv_bytes = recv(acceptfd, server_buf, 256, 0);

            if (recv_bytes == 0 || (strchr(server_buf, '\n') != NULL))
            {
                packet_collected = true;
            }

            if ((max_buf_size - saved_bytes) < recv_bytes)
            {
                max_buf_size += recv_bytes;
                write_packet = (char *) realloc(write_packet, sizeof(char) * max_buf_size);
            }

            memcpy(write_packet + saved_bytes, server_buf, recv_bytes);
            saved_bytes += recv_bytes;
        }

        write(filefd, write_packet, saved_bytes);
        lseek(filefd, 0, SEEK_SET);


        send_bytes += saved_bytes;

        read_packet = (char*)malloc(sizeof(char) * send_bytes);
        if(read_packet == NULL)
	{
		syslog(LOG_ERR, "malloc error = %d\n",errno);
		exit(-1);
	}
	else
	{
		syslog(LOG_DEBUG, "malloc success\n");
	}

        read_bytes = read(filefd, read_packet, send_bytes);

        status = send(acceptfd, read_packet, read_bytes , 0);
        if(status == -1)
	{
		syslog(LOG_ERR, "send error = %d\n",errno);
		exit(-1);
	}
	else
	{
		syslog(LOG_DEBUG, "send success\n");
	}
	syslog(LOG_DEBUG, "Closed connection from : %s\n", inet_ntoa(clt.sin_addr));
        free(read_packet);
        free(write_packet);
    }


    return 0;
}


























