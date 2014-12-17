

#include "structures.h"

//functions

struct queue *queue_add(struct queue ** , int , unsigned short , unsigned int , char *);
struct queue *queue_search(struct queue *, int);
void client_append(struct client_t**, struct client_t **);
void queue_remove(struct queue **,struct queue**);
void client_remove(int, struct client_t**, struct queue*, fd_set*);


int main(int argc, char **argv){

	int ret, max_fd, listener, new_fd;
	int existing;
	int count = 0;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	struct client_t * client_list;
	int yes = 1;
	fd_set read_set, write_set, read_tmp, write_tmp;
	struct queue * queue_l;

	if(argc!=3){
		printf("sintassi: mastermind_server <host> <porta>\n");
		exit(1);	
	}

	client_list = 0;
	queue_l = 0;

	ret=atoi(argv[2]);
	if(ret<1024){
		printf("errore nel numero porta, usare un numero maggiore di 1024\n");
		exit(1);

	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;	//ipv4
	server_addr.sin_port = htons(ret);
	ret = inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

	if(ret==0){
		printf("indirizzo non valido\n");
		exit(1);
	}

	FD_ZERO(&read_set);		//azzero i set
	FD_ZERO(&write_set);

	listener = socket(PF_INET, SOCK_STREAM, 0);		//restituisce il descrittore
	if(listener==-1){
			printf("errore nella creazione del socket\n");
			exit(1);
	}

	FD_SET(listener, &read_set);	//metto a 1 il read_set di indice listener
	max_fd = listener;		//indice maggiore


	ret = setsockopt(listener,SOL_SOCKET,SO_REUSEADDR, &yes ,sizeof(yes));	//manipola le opzioni associate con un socket
	if(ret<0){
		printf("errore setsockopt\n");
		exit(1);
	}

	ret = bind(listener,(struct sockaddr *)&server_addr,sizeof(server_addr));	//collego il socket ad un indirizzo locale
	if(ret<0){
		printf("Bind error\n");
		exit(1);
	}

	ret = listen(listener, MAXPENDING);	//mette il socket in attesa di connessioni
	if(ret<0){
		printf("listen error\n");
		exit(1);
	}

	while(1){
		int i;
		read_tmp = read_set;
		write_tmp = write_set;
		ret = select(max_fd+1, &read_tmp, &write_tmp, NULL, NULL);
		if(ret < 0){
			printf("errore sulla select\n");
			continue;
		}

		for(i = 0; i < max_fd+1; i++){

			if(FD_ISSET(i, &read_tmp)){				//controllo se è settato l'indice i

				if(i==listener){
					new_fd = accept(listener, (struct sockaddr *) &client_addr,	&client_len);	//descrittore del socket
					if(new_fd < 0){
						printf("errore accept\n");
					}


					else{



						struct client_t* client;
						max_fd = (new_fd > max_fd)? new_fd : max_fd;
						FD_SET(new_fd, &read_set);		//aggiungo il descrittore al read_set
						client = (struct client_t*)malloc(sizeof(struct client_t));		//alloco l'elemento
						client->id = new_fd;
						client->opponent_id = -1;
						client->ip = client_addr.sin_addr;
						client->port = ntohs(client_addr.sin_port);
						memset(client->name, 0, NAME_SIZE+2);
						client->busy = 0;
						client->next = 0;

						client_append(&client, &client_list);

						printf("connessione stabilita con il client %s: %d\n",inet_ntoa(client->ip), client->port );
					}
					
				}

				else{		//messaggio da parte del client
					struct queue * p = queue_search(queue_l, i);

					if(p==0){
						p = queue_add(&queue_l, i, CL_OK, 0, 0);
					}



					switch(p->step){
						
						case 1:
							ret = recv(i, (void*) &p->flags, sizeof(p->flags), 0);		//ricevo i flag
							if(ret < 0){
								perror("errore ret step 1");
								client_remove(i, &client_list, queue_l, &write_set);
								close(i);
								FD_CLR(i, &write_set);
								FD_CLR(i, &read_set);
								queue_remove(&queue_l, &p);
							}

							break;

						case 2:
							ret = recv(i, (void*)&p->length, sizeof(p->length), 0);		//ricevo la lunghezza del msg
							if(ret < 0){
								perror("errore ret step 1");
								client_remove(i, &client_list, queue_l, &write_set);
								close(i);
								FD_CLR(i, &write_set);
								FD_CLR(i, &read_set);
								queue_remove(&queue_l, &p);
							}
							else if(p->length==0)
								p->step++;
							break;

						case 3:
							p->buffer = (char *)malloc(p->length);
							ret = recv(i, (void*)p->buffer, p->length, 0);		//ricevo il buffer
							if(ret < 0){
								perror("errore ret step 1");
								client_remove(i, &client_list, queue_l, &write_set);
								close(i);
								FD_CLR(i, &write_set);
								FD_CLR(i, &read_set);
								queue_remove(&queue_l, &p);
							}

							break;


						default: break;

					}
					p->step++;


					if(p->step==4){
						switch(p->flags){

							case CL_INFO:
							{	
								unsigned short *port;
								char* name;
								struct client_t* cl = 0;
								int existing = 0;

								

								port =(unsigned short* )p->buffer;
								name = (char*)(p->buffer + sizeof(unsigned short));

								
							
								for(cl = client_list; cl!=0; cl = cl->next){
									if(strcmp(cl->name, name)==0){
										existing = 1;
										free(p->buffer);
										p->buffer = 0;
										p->sd = i;
										p->flags = CL_INFO;
										p->length = 0;
										p->step = 1;
										FD_SET(i, &write_set);
										break;
									}
								}
								if(existing==1)
									break;

								
								for(cl = client_list; cl!=0 && cl->id!=i; cl = cl->next);
									


								cl->port = *port;

								for(count = 0; count < (p->length - sizeof(unsigned short)); count ++)
								cl->name[count] = name[count];

								

								printf("%s si è connesso con porta %hu\n", cl->name, cl->port );

								p->sd = i;
								p->flags = CL_OK;
								p->length = 0;
								free(p->buffer);
								p->buffer = 0;
								p->step = 1;
								FD_SET(i, &write_set);
								
								break;

							}
							case CL_WHO:
							{	
								int m=9;
								struct client_t *cl;
								
								for(cl = client_list; cl!=0;  cl = cl->next)
									m+=strlen(cl->name)+1;
								p->buffer = (char*)realloc(p->buffer, m);
								memset(p->buffer, 0, m);
								strcat(p->buffer, "clients: ");
								for(cl =client_list; cl!=0; cl = cl->next){
									strcat(p->buffer, cl->name);
									strcat(p->buffer, " ");}
								p->buffer[m-1]='\0';
								p->flags = CL_WHO;
								p->length = m;
								p->sd = i;
								p->step = 1;

								FD_SET(i, &write_set); 		//riscrivo al client
								break;

							}

							case CL_CONN:
							{
								struct client_t* cl;

								printf("richiesta partita con %s\n",p->buffer );

								for(cl = client_list; cl !=0 && (strcmp(cl->name, p->buffer)!=0);cl = cl->next);		//cerco il client
								if(cl == 0){								//client non esistente, mando il flag di client non trovato
									printf("il client non esiste\n");
									p->flags = CL_NEC;
									p->length = 0;
									free(p->buffer);
									p->buffer =0;
									p->sd = i;
									p->step = 1;
									FD_SET(i, &write_set);
								}

								else if(cl->busy == 1){						//client occupato, mando il flag di client occupato
									printf("il client è occupato\n");		
									p->flags = CL_BUSY;
									p->length = 0;
									free(p->buffer);
									p->buffer = 0;
									p->sd = i;
									p->step = 1;
									FD_SET(i, &write_set);

								}

								else{
									struct client_t *from, *to;
									struct in_addr *ip;
									unsigned short *port;

									printf("invio richiesta a %s\n",p->buffer );
									for(to = client_list; to!=0 && (strcmp(to->name, p->buffer)!=0); to = to->next);
									for(from = client_list; from !=0 && from->id !=i; from = from->next);

									to->busy = 1;			//i due giocatori sono ora occupati in una partita
									from->busy = 1;

									p->length = sizeof(struct in_addr) + sizeof(unsigned short)+ strlen(from->name)+1;
									p->buffer = (char *)realloc(p->buffer, p->length);
									memset(p->buffer, 0, p->length);

									memcpy(p->buffer, &from->ip, sizeof(from->ip));
									memcpy(p->buffer+ sizeof(from->ip), &from->port, sizeof(from->port));
									memcpy(p->buffer+ sizeof(from->ip)+ sizeof(from->port), &from->name, strlen(from->name));



									//ho inserito nel buffer l'ip, la porta e il nome del richiedente

									p->flags = CL_CONN;
									p->sd = to->id;
									p->step = 1;

									FD_SET(to->id, &write_set);			//mando la richiesta al client a cui mi voglio connettere

								}
								break;

							}
							case CL_REF:
							{
								struct client_t *from, *to;

								for(to = client_list; to!=0 && (strcmp(to->name, p->buffer)!=0); to = to->next);
								for(from = client_list; from !=0 && from->id !=i; from = from->next);

								from->busy = 0;
								to->busy = 0;

								p->flags = CL_REF;
								p->length = strlen(from->name)+1;
								p->buffer = (char*)realloc(p->buffer,p->length);
								strcpy(p->buffer, from->name);
								p->sd = to->id;
								p->step = 1;

								printf("partita rifiutata\n");

								FD_SET(to->id, &write_set);

								break;
							}

							case CL_ACC:
							{
								struct in_addr *ip;
								unsigned short *port;
								char * name;
								struct client_t * to, *from;

								for(to = client_list; to!=0 && (strcmp(to->name, p->buffer)!=0); to = to->next);
								for(from = client_list; from !=0 && from->id !=i; from = from->next);

								p->length = sizeof(struct in_addr)+ sizeof(unsigned short)+ strlen(from->name)+1;
								p->buffer = realloc(p->buffer, p->length);
								memset(p->buffer, 0, p->length);

								memcpy(p->buffer, &from->ip, sizeof(from->ip));
								memcpy(p->buffer+ sizeof(from->ip), &from->port, sizeof(from->port));
								memcpy(p->buffer+ sizeof(from->ip)+ sizeof(from->port), &from->name, strlen(from->name));

								p->sd = to->id;
								p->flags = CL_ACC;
								p->step = 1;

								printf("%s e %s stanno giocando\n",to->name, from->name);

								to->busy = 1;
								from ->busy = 1;

								to->opponent_id = from->id;
								from->opponent_id = to->id;

								FD_SET(to->id, &write_set);

								break;

							}

							case CL_QUIT:
								printf("client disconnesso\n");
								close(i);
								FD_CLR(i, &write_set);
								FD_CLR(i, &read_set);
								client_remove(i, &client_list, queue_l, &write_set);
								queue_remove(&queue_l, &p);
								break;

							case CL_WIN:
							case CL_DISC:
							{
								FD_CLR(i, &read_set);

								struct client_t *from, *opp;

								for(from = client_list; from ->id!=i; from = from->next);		//client
								for(opp = client_list; opp->id !=from->opponent_id; opp = opp->next);		//avversario

								printf("%s e %s hanno terminato la partita\n", from->name, opp->name);
								from->busy = 0;
								opp->busy = 0;

								from->opponent_id = -1;
								opp->opponent_id = -1;


								queue_remove(&queue_l, &p);
								break;

							}

						}	
					}

				}
			}
			else if(FD_ISSET(i, &write_tmp)){

				if(i!=listener)	{		//client pronto a ricevere

				struct queue *pun;
				for(pun = queue_l; pun!=0 && pun->sd !=i; pun = pun->next);

					switch(pun->step){
						case 1:

							
							send(i, (void*)&pun->flags, sizeof(pun->flags), 0);
							break;
						case 2:

							
							send(i, (void*)&pun->length, sizeof(pun->length), 0);
							if(pun->length ==0)
								pun->step++;
							break;
						case 3:

							send(i, (void*)pun->buffer, pun->length, 0);
							break;
						default: break;
					}
					
					pun->step++;
					if(pun->step == 4){

						
						queue_remove(&queue_l, &pun);
						FD_CLR(i, &write_set);
					}

				}

			}

		}

	}

	

	return 0;
}

//functions


struct queue * queue_add(struct queue ** queue_t, int sd, unsigned short flags, unsigned int length, char *buffer){
	struct queue * tmp;
	struct queue * pun;
	char *buff;
	int i;

	if (length > 0)
	{
		buff = (char*)malloc(length);
		for(i=0; i<length;i++)
				buff[i]=buffer[i];
	}
	else
	{
		buff = 0;
	}
	

	tmp = (struct queue *)malloc(sizeof(struct queue));
	tmp->sd = sd;
	tmp->flags = flags;
	tmp->length = length;
	tmp->step = 1;
	tmp->buffer = buff;
	tmp->next = 0;

	if(*queue_t ==0)
		*queue_t = tmp;
	else
	{
		for(pun = *queue_t; pun->next!=0; pun = pun->next);
		pun->next = tmp;
	}


	return tmp;

	
}

struct queue * queue_search(struct queue * queue_l, int sd){
	struct queue *pun;
	for(pun = queue_l; pun!=0 && pun->sd!=sd; pun=pun->next);
	return pun;
}

void client_append(struct client_t ** client_p,struct client_t ** client_list_p)
{
	struct client_t * tmp;
	


	if(*client_list_p == 0)
	{	
		*client_list_p = *client_p;
	}
	else
	{	
		for(tmp = *client_list_p; tmp->next != 0; tmp = tmp->next);
		tmp->next = *client_p;
	}



	

}


void client_remove(int i, struct client_t ** client_list,struct queue * queue_l, fd_set *write_set){
	struct client_t * tmp;

	//rivedi funzione
	

	for (tmp = *client_list; tmp!=0; tmp = tmp->next){	
			//controllo se il client stava giocando con qualcuno
		if(tmp->opponent_id == i){
			printf("gioco con qualcuno\n");
			tmp->busy = 0;
			tmp->opponent_id = -1;
			queue_add(&queue_l, tmp->id, CL_DISC, 0, 0);
			FD_SET(tmp->id, write_set);

		}

	}

	if((*client_list)->id ==i){

		printf("%s si è disconnesso\n", (*client_list)->name);
		tmp = *client_list;
		*client_list = (*client_list)->next;
		free(tmp);

	}

	else {

		struct client_t *tmp2;

		for(tmp = *client_list; tmp->next!=0 && tmp->next->id !=i; tmp = tmp->next);
		tmp2 = tmp->next;

		printf("%s si è disconnesso\n",tmp2->name );
		tmp->next = tmp2->next;
		free(tmp2);





	}
}

void queue_remove(struct queue ** queue_l, struct queue ** pun) {
	
    struct queue *tmp;
    if(*queue_l == *pun) {
        *queue_l = (*pun)->next;
        free((*pun)->buffer);
        free(*pun);
    }

    else {
        for(tmp = *queue_l; tmp !=0  && tmp->next!= *pun; tmp = tmp->next);
        tmp->next = (*pun)->next;

        free((*pun)->buffer);
        free(*pun);

    }

    

}