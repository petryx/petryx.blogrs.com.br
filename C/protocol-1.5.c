/*
	Programa para transferencia de arquivos pela porta serial
    Copyright (C) 2008  Marlon Luis Petry marlonpetry@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <termio.h>
#include <errno.h>



#include <signal.h>
#include <sys/time.h> /*for timesub*/
#include <sys/sysmacros.h> /* For MIN, MAX, etc */
#include <sys/sysmacros.h> /* For MIN, MAX, etc */
#if ! defined(MIN)
# define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif
#if ! defined(MAX)
# define MAX(a, b)	((a) < (b) ? (b) : (a))
#endif

#define VERSION "1.2.5"
#include <utime.h>

#define MAX_PKT 255
#define MAX_PKT_DATA 250
#define MAX_SEQ 1




//CODIGO REMOVIDO DOS FONTES DO SCP
/* Progress meter bar */
#define BAR \
	"************************************************************"\
	"************************************************************"\
	"************************************************************"\
	"************************************************************"
#define MAX_BARLENGTH (sizeof(BAR) - 1)

#define STALLTIME	5
/* Visual statistics about files as they are transferred. */
void progressmeter(int);

/* Returns width of the terminal (for progress meter calculations). */
int getttywidth(void);

/* Time a transfer started. */
static struct timeval start;

/* Number of bytes of current file transferred so far. */
volatile unsigned long statbytes = 0;

/* Total size of current file. */
off_t totalbytes = 0;

/* Name of current file being transferred. */
char *curfile;

/*
 * ensure all of data on socket comes through. f==read || f==write
 */
ssize_t
atomicio(f, fd, _s, n)
	ssize_t (*f) ();
	int fd;
	void *_s;
	size_t n;
{
	char *s = _s;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = (f) (fd, s + pos, n - pos);
		switch (res) {
		case -1:
#ifdef EWOULDBLOCK
			if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
#else
			if (errno == EINTR || errno == EAGAIN)
#endif
				continue;
		case 0:
			return (res);
		default:
			pos += res;
		}
	}
	return (pos);
}
//fim

struct termio tnew,tsaved;

#define RETRY 10

//protocolo
#define HDT 0x01
#define DATA 0x02
#define HET 0x03
#define ACK  0x04
//#define NAK  0x05
#define TIMEOUT 0x06


/*Macro PARA INCREMENTAR A SEQ*/
#define inc(k) if (k < MAX_SEQ) k = k + 1; else k = 0

int DEBUG = 0;
int SIGNALS = 0;
struct termios options;
char *portName;
int ttyfd; /* File descriptor for the port */
unsigned seq = 0; //sequencia inicial
int flag_low;
char *pathFile;


void
updateprogressmeter(int ignore)
{
	
	progressmeter(0);
	
}



int
getttywidth(void)
{
	struct winsize winsize;

	if (ioctl(fileno(stdout), TIOCGWINSZ, &winsize) != -1)
		return (winsize.ws_col ? winsize.ws_col : 80);
	else
		return (80);
}

void
progressmeter(int flag)
{
	static const char prefixes[] = " KMGTP";
	static struct timeval lastupdate;
	static off_t lastsize;
	struct timeval now, td, wait;
	off_t cursize, abbrevsize;
	double elapsed;
	int ratio, barlength, i, remaining;
	char buf[256];

	if (flag == -1) {
		(void) gettimeofday(&start, (struct timezone *) 0);
		lastupdate = start;
		lastsize = 0;
	}
	
	(void) gettimeofday(&now, (struct timezone *) 0);
	cursize = statbytes;
	if (totalbytes != 0) {
		ratio = 100.0 * cursize / totalbytes;
		ratio = MAX(ratio, 0);
		ratio = MIN(ratio, 100);
	} else
		ratio = 100;

	snprintf(buf, sizeof(buf), "\r%-20.20s %3d%% ", curfile, ratio);

	barlength = getttywidth() - 51;
	barlength = (barlength <= MAX_BARLENGTH)?barlength:MAX_BARLENGTH;
	if (barlength > 0) {
		i = barlength * ratio / 100;
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			 "|%.*s%*s|", i, BAR, barlength - i, "");
	}
	i = 0;
	abbrevsize = cursize;
	while (abbrevsize >= 100000 && i < sizeof(prefixes)) {
		i++;
		abbrevsize >>= 10;
	}
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %5d %c%c ",
	     (int) abbrevsize, prefixes[i], prefixes[i] == ' ' ? ' ' :
		 'B');

	timersub(&now, &lastupdate, &wait);
	if (cursize > lastsize) {
		lastupdate = now;
		lastsize = cursize;
		if (wait.tv_sec >= STALLTIME) {
			start.tv_sec += wait.tv_sec;
			start.tv_usec += wait.tv_usec;
		}
		wait.tv_sec = 0;
	}
	timersub(&now, &start, &td);
	elapsed = td.tv_sec + (td.tv_usec / 1000000.0);

	if (statbytes <= 0 || elapsed <= 0.0 || cursize > totalbytes) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			 "   --:-- ETA");
	} else if (wait.tv_sec >= STALLTIME) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			 " - stalled -");
	} else {
		if (flag != 1)
			remaining =
			    (int)(totalbytes / (statbytes / elapsed) - elapsed);
		else
			remaining = elapsed;

		i = remaining / 3600;
		if (i)
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
				 "%2d:", i);
		else
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
				 "   ");
		i = remaining % 3600;
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			 "%02d:%02d%s", i / 60, i % 60,
			 (flag != 1) ? " ETA" : "    ");
	}
	atomicio(write, fileno(stdout), buf, strlen(buf));
/*
	if (flag == -1) {
		struct sigaction sa;
		sa.sa_handler = updateprogressmeter;
		sigemptyset(&sa.sa_mask);
#ifdef SA_RESTART
		sa.sa_flags = SA_RESTART;
#endif
		sigaction(SIGALRM, &sa, NULL);
	//	alarmtimer(1);
	} else if (flag == 1) {
		alarmtimer(0);
//		(write, fileno(stdout), "\n", 1);
		statbytes = 0;
	}
*/
}



void fechaPorta()
{
	ioctl(ttyfd,TCSETA,&tsaved);
        close(ttyfd);
	if(DEBUG)
		puts("Porta Serial Fechada & volta as configurações antigas");	

	exit(1);
}


FILE* openFile (char *filePath, char *mode)
{
	FILE *temp_handler;

	temp_handler = fopen(filePath, mode);
	if (temp_handler == NULL)
	{
		printf("\n\n");
		printf("Problema ao abrir o arquivo %s", filePath);
		printf("\n\n");
		exit(0);
	}
	else
		return temp_handler;
}


int  open_port(char *port)
{
          ttyfd = open(port, O_RDWR);
          if (ttyfd == -1)
          {
           /*
           * 	* Could not open the port.
           * 		*/
        	perror("open_port: Unable to open");
                exit(0);
           }
       
	ioctl(ttyfd,TCGETA,&tsaved);
  	tnew.c_line = 0;
  	tnew.c_cflag &=~ CBAUD;
  	tnew.c_cflag |= B57600;                     /* baud rate 9600 */
  	//tnew.c_cflag &= ~CSTOPB;        /* 2 stop bits */
  	tnew.c_cflag &= ~CSIZE;         /* Mask the character size bits */
  	tnew.c_cflag |= CS8;            /* Select 8 data bits */
  	tnew.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  	tnew.c_oflag &= ~OPOST;
	
        ioctl(ttyfd,TCSETA,&tnew);
        if(DEBUG == 1)
	{	
		puts("Porta Serial Aberta");
		puts("Porta Serial Config velocidade 57600, 8n2");
	}
	return (ttyfd);
}

//calcula o crc da mensagem
unsigned short crc(unsigned char buf[],int start,int cnt)
{
   int      i,j;
   short temp,temp2,flag;

   temp=0xFFFF;

   for (i=start; i<cnt; i++){
      temp=temp ^ buf[i];

      for (j=1; j<=8; j++){
	 flag=temp & 0x0001;
	 temp=temp >> 1;
	 if (flag) temp=temp ^ 0xA001;
      }
   }

   
   temp2=temp >> 8;
   temp=(temp << 8) | temp2;
   return(temp);
}



//espera por um evento
int wait_for_event(char seq)
{
	char *resp = (char *) malloc(MAX_PKT * sizeof(char));
	
	int i=0;
	while(i < RETRY)
	{
		usleep(90000);
		int re = read(ttyfd,resp,5);
		if(re > 0)
		{	
				if(resp[0] == ACK && seq == resp[1])
				{
				
					free(resp);
					return ACK;
				}
		
		limpa_buffer();	
		}
		
		i++;
	}
	return	TIMEOUT;

}


//le o arquivo para enviar
void sender(char *pathArquivo)
{
	struct stat statbuf;
	int tamFile;



	FILE *f = openFile(pathArquivo,"rt");
	curfile = pathArquivo;
	int lido =0;
	if(f)
	{	
		//coleta statisticas do arquivo
		if(stat(pathArquivo,&statbuf))
		{
			printf("Erro ao abrir arquivo %s para statiticas.\n\n");
			exit(0);
		}
		else
			tamFile = statbuf.st_size;	
				
		printf("Tamanho: %ld Bytes\n",tamFile);
		
		//coleta informações para troca de arquivo
			

		//envia cabecalho HDT
		for_physical_layer(HDT,pathArquivo,strlen(pathArquivo),0);

		totalbytes = tamFile;
		progressmeter(-1);

		while (!feof(f))
		{
			//aloca buffer de dados
		//	printf("read File\n");
			unsigned char *dataBuffer = (unsigned char *)malloc(MAX_PKT_DATA * sizeof(unsigned char));
			if(dataBuffer == NULL)
			{
				printf("Sem Memória para alocar Buffer de dados\n\n");
				exit(0);
			}

			lido = fread(dataBuffer,sizeof(unsigned char), MAX_PKT_DATA, f);
			if(lido != 0)
			{
				if(DEBUG == 1)
					printf("\nArquivo lido %ld B\n",lido);
			 	for_physical_layer(DATA,dataBuffer,lido);
				//grava arquivo para teste
//				char *file = "lido.c";
//				receiveData(dataBuffer,lido,file);
				free(dataBuffer);
				progressmeter(0);
				
			}
			
		}
		
			//	if(DEBUG == 1)
			//		printf("\nArquivo lido %ld B\n",lido);
				for_physical_layer(HET,NULL,0);	
			//	progressmeter(0);
		//		free(dataBuffer);
				
			
			
			
	}

}


int for_physical_layer( int function,unsigned  char *data, unsigned int lenghtData,int optSeq)
{

	if(function == DATA) //envia dados do arquivo
	{	
		
		usleep(lenghtData * 1000);
		int rec = sendDataFile(seq,&data[0],lenghtData);
		statbytes +=rec;
		if( rec == lenghtData+5)
		{
			int event=-1;
			int t=0;
			do
			{
				if(SIGNALS == 1)
				{
					break;
				}
				if(DEBUG==1)
					printf("loop event\n");
			        
				event = wait_for_event(seq);
				if(event == ACK)
				{
					if(DEBUG==1)
						printf("ACK\n");
					inc(seq); //inverte a sequencia
					break;			
				}
				else if(event == TIMEOUT)
				{
					if(DEBUG==1)
						printf("TIMEOUT Reenvia\n");	
					limpa_buffer();
					sendDataFile(seq,data,lenghtData);
					limpa_buffer();
					t++;
					if(t == RETRY)
					{	
						printf("\nTIMEOUT EXP\n");
						fechaPorta();

					}
				}
				
			}while(t < RETRY);
			
		}   //envia

	}
	else if(function == HDT)
	{
		puts("envia HDT");
		printf("%s\n",data);
		printf("%d\n",lenghtData);
		sendHDT(data,lenghtData);	
	}
	else if(function == HET)
	{
		usleep(lenghtData * 10000);
		sendHeaderEndTransfer();
	}
	else if(function == ACK)
	{
		sendAck(optSeq);
	}
	return 1;
	
}



//envia o cabecalho de transferencia de arquivo
//ajusta a seq para 0
int sendHDT(char *path, int lenPath)
{
	lenPath += 1; //soma mais um para colocar a sequencia

		limpa_buffer();
		unsigned char *env = ( unsigned char *) malloc(MAX_PKT * sizeof(unsigned char));
		int respWrite;
		if(env == NULL)
		{
			printf("Erro alocar memoria\n\n");
			fechaPorta();

		}
		
		
		env[0] = HDT;
		env[1] = seq; 
        	env[2] = lenPath+1;
	        env[3] = seq; //seq comeca com zero
		
		
		lenPath = lenPath+1;

		strcpy(&env[4],path);
		//calcula o crc
		int temp= crc(&env[0],0,lenPath+3);

		unsigned char crc_hi = temp >> 8;
    		unsigned char crc_lo = temp & 0x00FF;

		env[lenPath+3]= crc_lo;
		env[lenPath+4]= crc_hi;
		
		
		respWrite =  write(ttyfd,env,lenPath+5);
		limpa_buffer();
	
		free(env);
		return respWrite;

}

//envia o cabecalho de Final da Transferencia transferencia
int sendHeaderEndTransfer()
{
	limpa_buffer();
	if(DEBUG==1)
		printf("SEND HET\n");
	int length = 5;	
	unsigned char *resp = (unsigned char *) malloc(length * sizeof(unsigned char));
	if(resp == NULL)
	{
		perror("Erro ao Alocar, l520");	
		fechaPorta();

	}		

		
		int respWrite;
		resp[0] = HET;
		resp[1] = seq;
		resp[2] = 3;
		//calcula crc para enviar
		int temp2 = crc(&resp[0],0,length);
		unsigned char crc_hi2 = temp2 >> 8;
    		unsigned char crc_lo2 = temp2 & 0x00FF;    
		resp[3] = crc_lo2;
		resp[4] = crc_hi2;
		
		respWrite = write(ttyfd,resp,length);
		limpa_buffer();
 	
		free(resp);
		return respWrite;

}

//envia ACK
int sendAck(int seqACK)
{
	limpa_buffer();
 	if(DEBUG==1)
		printf("SEND ACK\n");
	int length = 5;	
	unsigned char *resp = (unsigned char *) malloc(length * sizeof(unsigned char));
	if(resp == NULL)
	{
		perror("Erro ao Alocar, l270");	
		fechaPorta();

	}		

		
		int respWrite;
		resp[0] = ACK;
		resp[1] = seqACK;
		resp[2] = 0;
		//calcula crc para enviar
		int temp2 = crc(&resp[0],0,length);
		unsigned char crc_hi2 = temp2 >> 8;
    		unsigned char crc_lo2 = temp2 & 0x00FF;    
		resp[3] = crc_lo2;
		resp[4] = crc_hi2;
		
		respWrite = write(ttyfd,resp,length);
		limpa_buffer();
 	
		free(resp);
		return respWrite;			


}


int sendDataFile(int seq,unsigned  char data[], unsigned int lenghtData)
{
		if(DEBUG==1)
			printf("\nSend DATA File\n");
		limpa_buffer();
		unsigned char *env = ( unsigned char *) malloc(MAX_PKT * sizeof(unsigned char));
		int respWrite;
		if(env == NULL)
		{
			printf("Erro alocar memoria\n\n");
			fechaPorta();

		}
		
		env[0] = DATA;
		env[1] = seq; 
        	env[2] = lenghtData ;
		//inseri os dados no vetor de envio
		int i=0,j=0;
		//concatena vetor 
		//for(i=3;i <= lenghtData;i++)
		//	env[i] = *data++;
		
		//strcat(env,data);
		memcpy(&env[3],data,lenghtData);
		//calcula o crc
		int temp= crc(&env[0],0,lenghtData+3);

		unsigned char crc_hi = temp >> 8;
    		unsigned char crc_lo = temp & 0x00FF;

		env[lenghtData+3]= crc_lo;
		env[lenghtData+4]= crc_hi;
		
		if(DEBUG==1)
		{		
			printf("\nSHOW STATS\n");
			printf("\nsize env %x", strlen(env));
			printf("\nfnct = %x\t",env[0]);
			printf("seq = %x\t",env[1]);
			printf("length = %x\t",env[2]);
			printf("crc lo - %x\t",env[lenghtData+3]);
			printf("crc hi - %x\t",env[lenghtData+4]);
			
			
		}
		
		respWrite =  write(ttyfd,env,lenghtData+5);
		limpa_buffer();
 		

		//grava arquivo para teste
		char *file = "testeAntesEnviar.c";
		receiveData(&env[3],lenghtData,file);
		
		free(env);
		return respWrite;
}		

int limpa_buffer()
{
	ioctl(ttyfd,TCFLSH,0);   //limpa buffer
        ioctl(ttyfd,TCFLSH,1);
	ioctl(ttyfd,TCFLSH,0);
	ioctl(ttyfd,TCFLSH,1);
	ioctl(ttyfd,TCFLSH,0);
	if(DEBUG==1)
		puts("limpa buffer");		
	
}


int from_physical_layer()
{
	printf("Listen Serial Port\n");
		//implementar a gravação do arquivo;
	int oldseq;
	while(1)
	{
		
		unsigned char *s = (unsigned  char *) malloc(MAX_PKT * sizeof(unsigned char));
		usleep(200000);
		
		//puts("le a porta");
		if(ttyfd < 0)
		{	
			if(DEBUG==1)
				puts("Reopen port");
			open_port(portName);
		}	
		int re = read(ttyfd,s,MAX_PKT);
		
		limpa_buffer();

		//printf("\n%d lido",re);
		if(re > 0)
		{
			int length = s[2];
			//valida crc
			if(valid_crc(&s[0],length) == 1)
			{
				if(seq == s[1])
				{
					if(DEBUG==1)
					{
						printf("fnct = %x\t",s[0]);
						printf("seq = %x\t",s[1]);
					}
					if(s[0] == DATA)
					{
				
						if(DEBUG==1)
						{	
							puts("DATA");
							printf("length = %x\t",s[2]);
						}
					
					int auxRX;
					int j=3;
					char *file = "testeFile.c";
						


					//envia os dados para funcao receiveDATA
					//sinaliza o inicio e o fim dos dados 
					 char *tmp = ( char *) malloc(length * sizeof( char));
					memcpy(tmp,&s[3],length);
					receiveData(tmp,length,pathFile);
					printf("seq %d\n",seq);
					for_physical_layer(ACK,NULL,0,seq);
     					//incrementa seq
					oldseq = seq;
					inc(seq);

					
					}
					else if(s[0] == HDT)
					{
						//Informa que irá começar o troca de arquivo
 						//ajusta a seq inicial
						//o nome do arquivo que será gravado
						//informa o caminho onde o arquivo será gravado
						if(DEBUG==1)
						{
							printf("Receive HDT\n");
							printf("length = %x\t",s[2]);
						}
						int auxRX;
						int j=3;
					
						//valida crc
						for_physical_layer(ACK,NULL,0,seq);
						//incrementa seq
						//envia os dados para funcao receiveDATA
						//sinaliza o inicio e o fim dos dados 
						seq = s[3];
						//inc(seq);
                	                	pathFile = ( char *) malloc((length+3) * sizeof( char));
                                        	//int tam = length -1;
						strcpy(pathFile,&s[4]);
						strcat(pathFile,".rec");
						printf("Recebendo arquivo %s\n",pathFile); 
					}
				}
				else //não é a sequencia esperada
				{
					//tratar o erro de sequencia
					//if(portClose == 1)
					//{	
						for_physical_layer(ACK,NULL,0,oldseq);
					//	portClose =0;
					//}
					printf("Erro seq %d\n",seq);
					puts("Erro seq errada");
				}
			}
			else if(s[0] == HET)
			{
						printf("\nArquivo recebido [ok]\n");
						exit(0);	
			}
			else
			{
				puts("erro crc");
			}
		}
		if(s != NULL)
		{
			
			free(s);
		}

		
		
	}
	
}

//recebe os dados implementar a gravação do arquivo
//baseado no hdt
int receiveData(unsigned char *data, int length,char *file)
{

	FILE *f = openFile(file ,"a");
  	
	int i = 0;
	fwrite(data,sizeof(unsigned char),length,f);
 	fclose(f);
	
	
        
}

//Valida a mensagem referente ao crc
int valid_crc(unsigned char frameRec[], int length)
{
	//calcula o crc do frame
	int temp = crc(&frameRec[0],0,length+3);
	unsigned short crc_hiCalc = temp >> 8;
    	unsigned short crc_loCalc = temp & 0x00FF;   
	if(DEBUG ==1)
	{	
		printf("\ncrc h %x crc l %x\n",crc_loCalc,crc_hiCalc);
		printf("\ncrc rh %x crc rl %x\n",frameRec[length+3],frameRec[length+4]);
	}

	if(crc_loCalc == frameRec[length+3] && crc_hiCalc == frameRec[length+4])
		return 1; /*crc ok*/
	else
		return 0; /* crc error */
									
}


int main(int argc, char *argv[])
{
int ch;
//printf("Version %s\n",VERSION);
while ((ch = getopt(argc, argv, "s:D:d:t:r")) != -1)

	switch(ch) {
		case 's':
			//disabilita sinais de controle
			SIGNALS =1;
			//puts("Disabilita sinais de controle");
			break;
		case 'D':
			//modo debug
			DEBUG=1;
			break;
		case 'd':
			//configura o device
			portName = optarg;
			open_port(portName);
			//printf("Port Open Ok\n");		
			break;
		case 't':
			//chama a funcao de transferencia
			//puts("Transferência de arquivo");
			sender(optarg);
			break;
		case 'r':
			//escuta a porta serial
			from_physical_layer();
			break;
		
		
	}

}

