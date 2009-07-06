#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAXBUFFER 2048

typedef struct cel
{
	int socket;
	char nome[50];
	char nick[50];
	char host[50];
	char buff[MAXBUFFER];
	struct cel *ante;
       	struct cel *prox;
}nodo;

nodo *getFirst();
nodo *getLast();
nodo *busca(int key);

void inseri(nodo **root, int socket,char nome[],char nick[],char host[]);

void removi(nodo *root, int socket);
void print (nodo *ini);

