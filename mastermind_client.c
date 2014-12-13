
#include "structures.h"

void Show_help();
struct queue *queue_add(struct queue ** , int , unsigned short , unsigned int , char *);
struct queue *queue_search(struct queue *, int);
void queue_remove(struct queue **,struct queue**);
int send_msg(struct queue *);
int rec_msg(int, struct queue *);
int send_to (int ,struct queue * ,struct info *);
int rec_from(int ,struct queue* ,struct info * );


int main(int argc, char **argv) {

    struct sockaddr_in server_addr;
    int ret, sd, max_fd, i, cd;
    int invalid, sent;
    int turn;
    fd_set read_set, write_set, read_tmp, write_tmp;
    struct info player_info, opponent_info;
    int msg_len;
    char * msg;
    struct queue * queue_l = 0;
    char cmdbuff[256];
    char comb[4];


    if(argc != 3) {
        printf("sinstassi: mastermind_client <host> <porta>\n");
        exit(1);

    }

    //connessione al server

    FD_ZERO(&read_set);
    FD_ZERO(&write_set);

    invalid = 0;
    sent = 0;

    //ciao

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;	//ipv4
    server_addr.sin_port = htons(atoi(argv[2]));
    ret = inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    if(ret==0) {
        printf("indirizzo non valido\n");
        exit(1);
    }

    sd = socket(PF_INET, SOCK_STREAM, 0);
    if(sd < 0) {
        printf("errore creazione socket\n");
        exit(1);

    }

    ret = fcntl(sd, F_GETFL, 0);	//ritorna la lista dei flag
    fcntl(sd, F_SETFL, ret| O_NONBLOCK);

    connect(sd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    max_fd = sd;
    FD_SET(sd, &write_set);

   
    

    memset(&player_info.addr, 0, sizeof(player_info.addr));
    player_info.addr.sin_family = AF_INET;

    write_tmp = write_set;
    select(max_fd+1, NULL, &write_tmp, NULL, NULL);
    if(FD_ISSET(sd, &write_tmp)) {
        int ok;
        socklen_t len = sizeof(ok);
        getsockopt(sd, SOL_SOCKET, SO_ERROR, (void*)&ok, &len);
        if(ok!=0) {
            printf("errore connessione: %s\n",strerror(ok) );
            exit(1);
        }
        FD_CLR(sd, &write_set);
    }
    FD_SET(sd, &read_set);



    printf("connessione al server %s (porta %d) effettuata con successo\n",argv[1], atoi(argv[2]) );

    Show_help();
    for(;;) {

        printf("inserisci il tuo nome:\n");
        scanf("%s",&player_info.name);



        printf("inserisci la porta di ascolto UDP (>1024):\n");
        scanf("%hu", &player_info.port);

        player_info.addr.sin_port = htons(player_info.port);
        player_info.addr.sin_addr.s_addr = htonl(INADDR_ANY);

        msg_len = sizeof(player_info.port) + strlen(player_info.name)+1 ;
        msg = (char*)malloc(msg_len);
        memset(msg, 0, msg_len);


        memcpy(msg, &player_info.port, sizeof(player_info.port));
        memcpy(msg+sizeof(player_info.port), player_info.name, strlen(player_info.name));

        printf("%hu | %s\n", *(unsigned short *) msg, &msg[2]);


        queue_add(&queue_l, sd, CL_INFO, msg_len, msg );

        free(msg);

        FD_SET(sd, &write_set);

        sent = 0;

        for(;;) {

            read_tmp = read_set;
            write_tmp = write_set;
            select(max_fd+1, &read_tmp, &write_tmp, NULL, NULL);
            for(i=0; i<max_fd+1; i++) {

                if(FD_ISSET(i, &write_tmp)) {

                    struct queue *p = queue_search(queue_l, sd);
                    ret = send_msg(p);

                    if(p->step == 4) {
                       
                        queue_remove(&queue_l, &p);
                        FD_CLR(sd, &write_set);
                    }
                }

                if(FD_ISSET(i, &read_tmp)) {
                    struct queue *p = queue_search(queue_l, sd);
                    if(p==0)
                        p=queue_add(&queue_l, sd, CL_INFO, 0, 0);

                    ret = rec_msg(sd, p);
                    if(ret < 0) {
                        printf("server disconnesso\n");
                        close(sd);
                        exit(1);
                    }

                    if(p->step == 4) {
                        switch(p->flags) {
                        case CL_OK:
                            sent = 1;
                            invalid = 0;
                            break;

                        case CL_INFO:
                            sent = 1;
                            invalid = 1;

                        default:
                            break;

                        }

                        queue_remove(&queue_l, &p);

                    }
                }
            }
            if(sent == 1)
                break;
        }

        if(invalid == 0) {
            printf("info inviate correttamente\n");
            break;
        }
        else {
            printf("nome già in uso\n");
        }

    }
    		//risveglio il descrittore relativo alla tastiera
    
    
   FD_SET(fileno(stdin), &read_set);
   
    cd = socket(PF_INET, SOCK_DGRAM, 0);        //connessione UDP
    bind(cd, (struct sockaddr*)&player_info.addr, sizeof(player_info.addr));

    max_fd = (max_fd < cd)?cd : max_fd;

    FD_SET(cd, &read_set);



    while(1){
    	fflush(stdout);

    	read_tmp = read_set;
    	write_tmp = write_set;
    	ret = select(max_fd+1, &read_tmp, &write_tmp, NULL, NULL);
    	if(ret < 0){
    		printf("errore select\n");
    		continue;
    	}

    	for(i = 0; i < max_fd+1; i++){
            

    		if(FD_ISSET(i, &read_tmp)){

                if(i == fileno(stdin)){
    			 
                    	//nuovi dati dall'utente
    				memset(cmdbuff, 0, 256);
    				ret = read(STDIN_FILENO, (void*)cmdbuff, 256);
    				if(ret < 0){
    					printf("errore in lettura, riprova");
    					continue;
    				}

    				if((strncmp(cmdbuff, "!help", 5)==0)){
    					Show_help();
    					printf(" ");
    					continue;
    				}

    				if((strncmp(cmdbuff, "!who", 4)==0)){
                       
    					queue_add(&queue_l, sd, CL_WHO, 0, 0);
    					FD_SET(sd, &write_set);
    					continue;
    				}

                    if((strncmp(cmdbuff, "!connect", 8)==0)){
                        //aggiungere errore sintassi ed errore se sei già in partita
                        char* buff = (char*) malloc(strlen(cmdbuff)-9);     //alloco lo spazio per un buffer di lunghezza username
                        strncpy(buff, cmdbuff+9, strlen(cmdbuff)-10 );      //copio il nome dentro buff
                        buff[strlen(cmdbuff)-10] = '\0';

                        printf("%s\n",buff );

                        //aggiungere errore "non puoi giocare contro te stesso"

                        printf("connessione in corso con %s\n",buff );
                        queue_add(&queue_l, sd, CL_CONN, strlen(buff)+1, buff);
                        free(buff);
                        FD_SET(sd, &write_set);
                        continue;
                    }

                    if((strncmp(cmdbuff, "!quit", 5))==0){
                        queue_add(&queue_l, sd, CL_QUIT, 0, 0 );
                        FD_SET(sd, &write_set);
                        continue;
                            
                    }

                    if((strncmp(cmdbuff, "!disconnect", 11))==0){
                       
                        queue_add(&queue_l, sd, CL_DISC, 0, 0);
                        queue_add(&queue_l, cd, CL_DISC, 0, 0);
                        FD_SET(sd, &write_set);
                        FD_SET(cd, &write_set);
                        printf("ti sei disconnesso\n");
                        continue;
                    }
                }

                else if(i == sd){       //nuovi dati dal server
                    int ret;

                    struct queue *p;
                    p = queue_search(queue_l, i);
                   if(p==0){
                        p = queue_add(&queue_l, i, CL_UNDEF, 0, 0);
                    }
                    
                    ret = rec_msg(sd, p);
                    if(ret < 0){
                        printf("server disconnesso\n");
                        close(sd);
                        exit(0);
                    }

                    if(p->step == 4){
                        switch(p->flags){
                            case CL_WHO:
                                
                                printf("%s\n",p->buffer );
                                queue_remove(&queue_l, &p);
                                break;

                            case CL_CONN:
                            {
                                struct in_addr *ip;
                                unsigned short *port;
                                char * name;
                                char ans;

                                ip = (struct in_addr *)p->buffer;
                                port = (unsigned short *)(p->buffer + sizeof(struct in_addr));
                                name = (char *)(p->buffer+ sizeof(struct in_addr)+ sizeof(unsigned short));

                                

                                do{
                                    char x;
                                    printf("il client %s, ip %s, porta %hu, vuole giocare. accetti la richiesta? [Y|N]\n", name, inet_ntoa(*ip), *port );
                                    fflush(stdout);

                                    do{
                                        x = getchar();
                                    } while (x!='\n'  && x!= EOF);

                                    scanf("%c",&ans );

                                    do{
                                        x = getchar();
                                    } while (x!='\n'  && x!= EOF);

                                } while (ans!= 'y'&& ans != 'Y' && ans != 'n' && ans != 'N');


                            

                                if(ans == 'N'||ans == 'n'){
                                   
                                    printf("richiesta rifiutata\n");
                                    p->flags = CL_REF;
                                }

                                else if(ans == 'y'||ans == 'Y' ){

                                   

                                    memset(&opponent_info.addr, 0, sizeof(opponent_info.addr));
                                    opponent_info.addr.sin_family = AF_INET;
                                    opponent_info.addr.sin_port = htons(*port);
                                    opponent_info.addr.sin_addr = *ip;
                                    strcpy(opponent_info.name, name);

                                
                                    printf("richiesta accettata\n");
                                    printf("è il turno di %s\n",opponent_info.name );

                                    printf("digita la combinazione segreta\n");

                                    turn = 0;
                                    p->flags = CL_ACC;





                                }

                                p->length = strlen(name)+1;
                                p->buffer = (char*)realloc(p->buffer, p->length);
                                strcpy(p->buffer, name);
                                p->sd = sd;
                                p->step = 1;
                                FD_SET(sd, &write_set);
                                break;

                            }

                            case CL_REF:{
                                printf("%s ha rifiutato la partita\n",p->buffer);
                                queue_remove(&queue_l, &p);
                                break;
                            }

                            case CL_ACC:{

                                struct in_addr * ip;
                                unsigned short * port;
                                char * name;

                                ip = (struct in_addr *)p->buffer;
                                port = (unsigned short *)(p->buffer + sizeof(struct in_addr));
                                name = (char *)(p->buffer+ sizeof(struct in_addr)+ sizeof(unsigned short));



                                printf("%s ha accettato la partita\n",(p->buffer+ sizeof(unsigned short)+ sizeof(struct in_addr)));

                                printf("digita la combinazione segreta\n");

                                

                                printf("è il tuo turno\n");

                                memset(&opponent_info.addr, 0, sizeof(opponent_info.addr));
                                opponent_info.addr.sin_family = AF_INET;
                                opponent_info.addr.sin_port = htons(*port);
                                opponent_info.addr.sin_addr = *ip;
                                strcpy(opponent_info.name, name);

                                /*cd = socket(PF_INET, SOCK_DGRAM, 0);        //connessione UDP
                                bind(cd, (struct sockaddr*)&player_info.addr, sizeof(player_info.addr));

                                max_fd = (max_fd < cd) ? cd : max_fd;
                                FD_SET(cd, &read_set);*/

                                queue_remove(&queue_l, &p);



                                break;
                            }

                            case CL_DISC:
                                
                                
                                printf("il tuo avversario si è disconnesso\n");
                                //FD_CLR(cd, &read_set);
                                FD_CLR(cd, &write_set);
                                //close(cd);
                                queue_remove(&queue_l, &p);
                                
                                break;




                            default: break;

                            

                        }
                    }

                }

                else        //ricevo dati da un altro client tramite porta UDP
                {  
                    struct queue* p = queue_search(queue_l, i);

                    if(p == 0){
                        p = queue_add(&queue_l, i, CL_UNDEF, 0, 0);
                    }
                    
                    ret = rec_from(i, p, &opponent_info);

                        if(ret < 0){
                        printf("client disconnesso, termino la partita\n");
                        close(i);
                        FD_CLR(i, &read_set);
                        FD_CLR(i, &write_set);
                    }

                    if(p->step == 4){
                        

                        switch(p->flags){

                            case CL_DISC:
                                printf("il tuo avversario si è disconnesso\n");
                                //close(cd);
                                //FD_CLR(cd, &read_set);
                                FD_CLR(cd, &write_set);
                                break;

                            default: break;

                        }
                    }



                }
    		}
            if(FD_ISSET(i, &write_tmp)){
                
                if(i == sd){        //server pronto a ricevere
                    struct queue * pun;
                    for(pun = queue_l; pun!=0 && pun->sd!=i; pun = pun->next);

                    ret = send_msg(pun);
                    if(ret < 0){
                        printf("errore ret send_msg\n");
                        close(sd);
                        exit(1);
                    }

                    if(pun->step == 4) {

                        switch(pun->flags){
                            case CL_QUIT:
                                close(sd);
                                FD_CLR(sd, &write_set);
                                FD_CLR(sd, &read_set);
                                printf("disconnesso con successo\n");
                                queue_remove(&queue_l, &pun);
                                exit(0);
                        }
                       
                        queue_remove(&queue_l, &pun);
                        FD_CLR(sd, &write_set);
                    }

                }

                else {       //client pronto a ricevere

                    struct queue *pun;
                    for(pun = queue_l; pun!=0 && pun->sd!=i; pun = pun->next); 

                    ret = send_to(i, pun, &opponent_info);
                    

                    if(pun->step == 4)
                    {
                        switch(pun->flags){

                            case CL_DISC:
                                //close(cd);
                                //FD_CLR(i, &read_set);
                                
                                
                                break;

                            default: break;
                        }

                        queue_remove(&queue_l, &pun);
                        FD_CLR(i, &write_set);
                    }


                }
            }
    		       
    	}
    }
    return 0;
}













void Show_help(){
	printf("Sono disponibili i seguenti comandi:\n"
		"* !help --> mostra l'elenco dei comandi disponibili\n"
		"* !who --> mostra l'elenco dei client connessi al server\n"
		"* !connect nome_client --> avvia una partita con l'utente nome_client\n"
		"* !disconnect --> disconnette il client dall'attuale partita intrapresa con un altro peer\n"
		"* !quit --> disconnette il client dal server\n"
		"* !combinazione comb -> permette al client di fare un tentativo con la combinazione comb\n ");
}

struct queue * queue_add(struct queue ** queue_t, int sd, unsigned short flags, unsigned int length, char *buffer) {

   
   
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

struct queue * queue_search(struct queue * queue_l, int sd) {
    struct queue *pun;
    for(pun = queue_l; pun!=0 && pun->sd!=sd; pun=pun->next);
    return pun;
}

int send_msg(struct queue *p) {
    int ret;
    
    switch(p->step) {			//sto inviando i flag
    case 1:
        ret = send(p->sd, (void*)&p->flags, sizeof(p->flags), 0);
        break;
    case 2:
        ret = send(p->sd, (void*)&p->length, sizeof(p->length),0);
        if(p->length == 0)
            p->step++;
        break;
    case 3:
        ret = send(p->sd, (void*)p->buffer, p->length, 0);
        break;

    default:
        break;
    }
    p->step++;
    return ret;
}

int rec_msg(int sd,struct queue * p) {
    int ret;
    
    
    switch(p->step) {
    case 1:
        ret = recv(sd, (void*) &p->flags, sizeof(p->flags), 0);
        break;
    case 2:
        ret = recv(sd, (void*) &p->length, sizeof(p->length), 0);
        if(p->length == 0)
            p->step++;
        break;
    case 3:
        p->buffer = (char*)malloc(p->length);
        ret = recv(sd, (void*)p->buffer, p->length, 0);
        break;
    default:
        break;
    }

    p->step++;
    return ret;
}

int send_to (int i,struct queue * p,struct info * opponent_info)
{

    
    int ret;
    socklen_t len = sizeof(opponent_info->addr);


    switch(p->step)
    {
    case 1:
        ret = sendto(i, (void *) &p->flags, sizeof(p->flags), 0, (struct sockaddr *) &opponent_info->addr, len);
        printf("%hu \n",p->flags );
        break;

    case 2:
        ret = sendto(i, (void *) &p->length, sizeof(p->length), 0, (struct sockaddr *) &opponent_info->addr, len);
        if(p->length == 0)
            p->step++;
        break;

    case 3:
        ret = sendto(i, (void *) p->buffer, p->length, 0, (struct sockaddr *) &opponent_info->addr, len);
        break;

    default:
        break;
    }

    p->step++;





    return ret;
}

int rec_from(int i,struct queue* p,struct info * opponent_info){

    int ret;
    socklen_t len = sizeof(opponent_info->addr);

   

    switch(p->step)
    {
    case 1:
        ret = recvfrom(i, (void *) &p->flags, sizeof(p->flags), 0,(struct sockaddr *) &opponent_info->addr, &len);
        
        break;

    case 2:
        ret = recvfrom(i, (void *) &p->length, sizeof(p->length), 0, (struct sockaddr *) &opponent_info->addr, &len);
        if(p->length == 0)
            p->step++;
        break;

    case 3:
        ret = recvfrom(i, (void *) p->buffer, p->length, 0, (struct sockaddr *) &opponent_info->addr, &len);
        break;

    default:
        break;
    }

    p->step++;

    return ret;
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

