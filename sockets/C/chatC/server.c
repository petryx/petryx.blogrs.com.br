#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <stddef.h>
#include <errno.h>
#include <sys/un.h>


#include "listaDDE.h"
#define MYPORT 5800    // the port users will be connecting to

#define BACKLOG 10     // how many pending connections queue will hold
#define MAXLINE 1024
#define NUM_THREADS     50
fd_set rmask;
pthread_mutex_t mutexsum;
pthread_mutex_t mutexLogin;
int fd_max;
int maxfds;

int sockfd;
fd_set read_fds;
fd_set master;
nodo *root = NULL;
static int	read_cnt;
static char	*read_ptr;
static char	read_buf[MAXLINE];

static void *sendMensagem(void *arg);
void sigchld_handler(int s);
static void *sendALL(char msg[],int soks);
static void *sendMensagem(void *arg);
static void *newClientConnect(int arg);

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void *sendALL(char msg[],int soks)
{
	nodo *aux = getFirst();
	while(aux != NULL)
	{
		int soc =aux->socket;

		if(soc != soks)
			{
	      	       		if(write(soc, msg, strlen(msg)) == -1)
		    			perror("send");
				printf("send %s %s\n",msg,aux->nick);

			}
		aux = aux->prox;
	}
}

void *sendMensagem(void *arg)
{
  char *buff;
  int fd = (*(int *) arg);
  int nbytes;
  char tmp[5000];

  printf("\nthread call %d",fd);
  newClientConnect(fd);
  printf("\n entry LOOP\n");
  nodo *cliente = busca(fd);

while(1)
{

     buff = (char *) calloc( MAXLINE,sizeof(char));
     if(FD_ISSET(fd,&master))
     {
         nbytes = recv(fd,buff, MAXLINE -1 , 0);
	 if(nbytes > 1)
	 {

	   if(!tmp)
	   {
		perror("alocation:");
		exit(-1);
	   }
           strcat(tmp, cliente->nick);
	   strcat(tmp, " >> ");
	   strcat(tmp, buff);
           sendALL(tmp,fd);
	 }
	 else
	 {
	       if (nbytes == 0) {
                       printf ("servidor: socket %d desligado\n",  fd);
               } else {
                       perror ("recv");
               }
              close (fd); // BYE!!
              FD_CLR( fd , &master);
	      removi(getFirst(),fd);
	      pthread_exit(NULL);
	  }

     }
     free(buff);

}

}

static void *newClientConnect(int arg)
{
	int fd = arg;
	pthread_t tid;
	struct sockaddr_in their_addr;
	char welcome[] = "Welcome Chat SistDistribuidos 0.1 \n";
	char msg1[] = "Login: ";
	char msg2[] = "NickName: ";
	char nome[100];
	char nick[100];
	int nbytes;
	socklen_t sin_size;

	pthread_mutex_lock (&mutexLogin);

		printf("\naccept \n");
		if (write(fd, welcome, sizeof welcome - 1) != sizeof welcome - 1) {
	   		perror("write:");
			exit(1);
		}		

		printf("write %s \n",welcome);
		usleep(1000);

		if (write(fd, msg1, strlen(msg1)) != strlen(msg1)) {

	   		perror("write:");
			exit(1);
		}

		printf("write %s \n",msg1);
		usleep(1000);
		if ((nbytes = recv(fd, nome, 100,0)) < 0) {
		perror("read:");
	        exit(1);
	   	return;
		}
		nome[nbytes -1] = '\0';
		printf("Read %d %s\n",strlen(nome),nome);
		usleep(1000);
		if (write(fd, msg2, sizeof msg2 - 1) != sizeof msg2 - 1) {
	        	perror("write:");
	   	exit(1);
		}

		if ((nbytes =recv(fd, nick, 100,0)) < 0) {
	  	perror("read:");
		 exit(1);
		}
		nick[nbytes-1]='\0';
		printf("Read %d %s\n",strlen(nick),nick);		

		inseri(&root,fd,nome,nick, inet_ntoa(their_addr.sin_addr));
		FD_SET(fd, &master);		/* listen to this socket too */

	printf("end welcome \n");

	pthread_mutex_unlock (&mutexLogin);

}

int main(void)
{
      // listen on sock_fd, new connection on new_fd
    int j,new_fd;
    struct timeval waitd;
    int nbytes;
    char buf[256];
    int fd_max; //numero maximo de descritores

    struct sockaddr_in my_addr;    // my address information
    struct sockaddr_in their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    pthread_mutex_init(&mutexsum, NULL);
    pthread_mutex_init(&mutexLogin,NULL);

    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    if ( setsockopt ( sockfd , SOL_SOCKET , SO_REUSEADDR , &yes , sizeof ( int ) ) == -1 )
    {
		perror ( " setsockopt " );
		exit ( 1 );
    }

    my_addr.sin_family = AF_INET;         // host byte order
    my_addr.sin_port = htons(MYPORT);     // short, network byte order
    my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
    memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof my_addr) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    FD_SET(sockfd, &master);

    fd_max = sockfd;	

    int rc =0;	 

    int i = 0;
    int ret;

    int fd;
      waitd.tv_sec = 1;
      waitd.tv_usec = 0;

     while(1)
     {  

        read_fds = master;
        if (select (fd_max+1,  &read_fds, NULL, NULL, NULL)== -1) {
                perror ("select");
                exit(1);
        }

        int addrlen = sizeof (their_addr);
        if (( new_fd = accept(sockfd,(struct sockaddr *)&their_addr,&addrlen)) == -1)  {
            perror ("accept");
        } else {

            FD_SET ( new_fd , &master);

            printf ("server : new connection %s socket %d\n",
                              inet_ntoa (their_addr.sin_addr),new_fd);

	    pthread_t tred;
	    rc = pthread_create( &tred, NULL,sendMensagem, (void *) &new_fd);
       	    if (rc){
        	printf("ERROR; return code from pthread_create() is %d\n", rc);
       		exit(-1);
            }

        }
      } 

pthread_mutex_destroy(&mutexsum);
pthread_mutex_destroy(&mutexLogin);

    return 0;
}
