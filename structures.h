#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>

#define MAXPENDING 20
#define NAME_SIZE 20

#define CL_OK 		0x00
#define CL_INFO 	0x01		//invio informazioni connessione
#define CL_FREE		0X02		//client libero
#define CL_BUSY		0x03		//client occupato
#define CL_WHO		0x04		//comando !who
#define CL_CONN		0x05		//comando !connect
#define CL_NEC		0x06		//client non esistente
#define CL_REF		0x07		//partita rifiutata
#define CL_ACC		0x08		//richiesta accettata
#define CL_QUIT		0x09		//comando quit
#define CL_DISC		0x0A		//client disconnesso
#define CL_UNDEF	0x0B		//indefinito
#define CL_COMB		0x0C		//comando combinazione
#define CL_INS		0x0D		//combinazione inserita



struct sockaddr_in sockaddr_in;

struct client_t{
	int id;
	int opponent_id;
	struct in_addr ip;
	char name[NAME_SIZE + 2];
	unsigned short port;
	int busy;
	struct client_t *next;
} client_t;

struct info{
	struct sockaddr_in addr;
	unsigned short port;
	char name[NAME_SIZE];
} info;

struct queue{
	int sd; 		//descrittore del socket elemento della coda
	unsigned short flags;
	int length;			//lunghezza del messaggio che si deve aspettare
	char * buffer;
	unsigned short step; 	//iterazione
	struct queue * next;
	
} queue;