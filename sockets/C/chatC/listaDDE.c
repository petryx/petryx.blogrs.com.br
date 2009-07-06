#include <string.h>
#include <stdlib.h>
#include "listaDDE.h"

nodo *first = NULL, *last = NULL;

nodo *getFirst()
{
	printf("call getFirst()\n");
	return first;
}

nodo *getLast()
{
	return last;
}
void inseri(nodo **root, int socket, char nome[],char nick[],char host[])
{
	nodo *aux;
	//primeira inserção
	if(*root == NULL)
	{
		*root = (nodo *) malloc(sizeof (nodo));
		if(*root == NULL)
		{
			perror("erro de alocação\n");

		}
		(*root)->socket = socket;
		strcpy((*root)->nome , nome);
		strcpy((*root)->nick , nick);
	        strcpy((*root)->host , host);
		(*root)->prox = NULL;
		(*root)->ante = NULL;
		 first = (*root);
		last = first;

	}
	else
	{
		aux = (nodo *) malloc(sizeof (nodo));
		if(aux == NULL)
		{
			perror("erro de alocação\n");

		}
		aux->socket = socket;
		strcpy(aux->nome , nome);
		strcpy(aux->nick , nick);
	        strcpy(aux->host , host);

		aux->ante =last;
            	aux->prox = NULL;

                last->prox = aux;
            	last = aux;
		*root = last;
	}
}

void removi(nodo *primeiro,int dado)
{
   nodo *aux = primeiro;
   while(aux != NULL)
   {
       if(dado == aux->socket && aux == first) //Remove first element
      {

	 if(aux->prox != NULL)
	 {
		printf("!= null\n");
		aux->prox->ante = NULL;
	        first = aux->prox;
	 }
	 else
	 {
		printf("null\n");
		free(first);

		break;
	 }

      }
      else if(dado == aux->socket && aux->prox == NULL) //remove last
      {
      	 aux->ante->prox = NULL;
         last = aux->ante;
         free(aux);
         break;
      }
      else if(dado == aux->socket &&  aux->prox != NULL && aux->ante !=NULL) //remove middle
      {
      	aux->ante->prox = aux->prox;
         aux->prox->ante = aux->ante;
         free(aux);
         break;
      }

      aux = aux->prox;
   }
}

void print (nodo *ini)
{
   nodo *p = first;
   while(p != NULL)
   {   printf ("socket %d cliente %s\n", p->socket,p->nome);
	p = p->prox;    

   }
}

nodo *busca (int key)
{
   nodo *p = getFirst();
   int flag = 0;
   while(p != NULL)
   {
   	if(p->socket == key)
	{
		return p;
	}
	p = p->prox;    

   }
   return NULL;
}
