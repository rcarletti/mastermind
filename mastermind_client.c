
#include "structures.h"

void show_help();
struct queue *queue_add(struct queue ** , int , unsigned short , unsigned int , char *);
struct queue *queue_search(struct queue *, int);
void queue_remove(struct queue **,struct queue**);
int send_msg(struct queue *);
int rec_msg(int, struct queue *);
int send_to (int ,struct queue * ,struct info *);
int rec_from(int ,struct queue* ,struct info * );
void read_comb(char *);
void handle_combination(char *);

void read_cmd(void);
void handle_incoming_connect(struct queue *p);
void handle_incoming_accept(struct queue *p);

void flush_in(void);

int sd, cd;
fd_set read_set, write_set;
char  bb[100];

int isYourTurn = 0;

int correct =0;
int wrong = 0;

struct queue *queue_l;
struct info player_info, opponent_info;

int turn;
char comb[5];


int main(int argc, char **argv)
{
    struct sockaddr_in server_addr;
    int ret, max_fd, i;
    int invalid, sent;
    fd_set read_tmp, write_tmp;
    int msg_len;
    char * msg;
    
    char attempt[5];

    queue_l = 0;
    invalid = 0;
    sent = 0;

    if(argc != 3) {
        printf("Sinstassi: mastermind_client <host> <porta>\n");
        exit(1);
    }

    //connessione al server

    FD_ZERO(&read_set);
    FD_ZERO(&write_set);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;	//ipv4
    server_addr.sin_port = htons(atoi(argv[2]));
    ret = inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    if(ret==0) {
        printf("Indirizzo non valido\n");
        exit(1);
    }

    sd = socket(PF_INET, SOCK_STREAM, 0);
    if(sd < 0) {
        printf("Errore creazione socket\n");
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
            printf("Errore connessione: %s\n",strerror(ok) );
            exit(1);
        }
        FD_CLR(sd, &write_set);
    }
    FD_SET(sd, &read_set);


    printf("Connessione al server %s (porta %d) effettuata con successo\n",
            argv[1], atoi(argv[2]));
    show_help();

    for(;;) {

        printf("Inserisci il tuo nome: ");
        fflush(stdout);

        scanf("%s",&player_info.name);
        flush_in();

        printf("Inserisci la porta di ascolto UDP (>1024): ");
        fflush(stdout);

        scanf("%hu", &player_info.port);
        flush_in();

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
                        printf("Server disconnesso\n");
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
            printf("Info inviate correttamente\n");
            break;
        }
        else {
            printf("Nome già in uso\n");
        }

    }
    		//risveglio il descrittore relativo alla tastiera
    
    
    FD_SET(fileno(stdin), &read_set);
   
    cd = socket(PF_INET, SOCK_DGRAM, 0);        //connessione UDP
    bind(cd, (struct sockaddr*)&player_info.addr, sizeof(player_info.addr));

    max_fd = (max_fd < cd)?cd : max_fd;

    FD_SET(cd, &read_set);


    while(1) {
    	fflush(stdout);

    	read_tmp = read_set;
    	write_tmp = write_set;
    	ret = select(max_fd+1, &read_tmp, &write_tmp, NULL, NULL);
    	if(ret < 0) {
    		printf("Errore select\n");
    		continue;
    	}

    	for(i = 0; i < max_fd+1; i++) {

    		if(FD_ISSET(i, &read_tmp)) {

                if(i == fileno(stdin)) { //nuovi dati dall'utente
                    read_cmd();
                }

                else if(i == sd) {  //nuovi dati dal server
                    int ret;
                    struct queue *p;

                    p = queue_search(queue_l, i);
                    if(p==0){
                        p = queue_add(&queue_l, i, CL_UNDEF, 0, 0);
                    }
                    
                    ret = rec_msg(sd, p);
                    if(ret < 0) {
                        printf("Server disconnesso\n");
                        close(sd);
                        exit(0);
                    }

                    if(p->step == 4) {

                        switch(p->flags)
                        {
                        case CL_WHO:
                            printf("%s\n", p->buffer);
                            queue_remove(&queue_l, &p);
                            break;

                        case CL_CONN:
                            handle_incoming_connect(p);
                            break;

                        case CL_REF:
                            printf("%s ha rifiutato la partita\n", p->buffer);
                            queue_remove(&queue_l, &p);
                            break;

                        case CL_ACC:
                            handle_incoming_accept(p);
                            printf("Digita la combinazione segreta: ");
                            fflush(stdout);
                            read_comb(comb);

                            break;

                        case CL_DISC:
                            printf("Il tuo avversario si è disconnesso\n");
                            FD_CLR(cd, &write_set);
                            queue_remove(&queue_l, &p);
                            break;

                        default:
                            break;
                        }
                    }
                }

                else //ricevo dati da un altro client tramite porta UDP
                {  
                    struct queue* p;

                    p = queue_search(queue_l, i);
                    if(p == 0){
                        p = queue_add(&queue_l, i, CL_UNDEF, 0, 0);

                    }
                    
                    ret = rec_from(i, p, &opponent_info);
                    if(ret < 0) {
                        printf("Client disconnesso, termino la partita\n");
                        FD_CLR(i, &write_set);
                    }

                    if(p->step == 4) {

                        switch(p->flags)
                        {
                        case CL_DISC:
                                
                            FD_CLR(cd, &write_set);
                            break;

                        case CL_INS:
                            printf("%s ha inserito la combinazione, la partita può cominciare\n", opponent_info.name);
                            FD_CLR(cd, &write_set);
                            printf("E' il tuo turno\n");
                            isYourTurn = 1;

                            break;

                        case CL_COMB:
                            
                            FD_CLR(cd, &write_set);
                            isYourTurn = 1;
                            printf("è il tuo turno\n");
                            handle_combination(p->buffer);
                            
                            break;
                    
                        case CL_ANS:{

                            printf("%s\n", p->buffer );
                            printf("è il turno di %s\n",opponent_info.name );
                            FD_CLR(cd, &write_set);
                            break; 
                        }

                        case CL_WIN:
                            printf("hai vinto!\n");
                            FD_CLR(cd, &write_set);
                            FD_CLR(cd, &read_set);
                            queue_add(&queue_l, sd, CL_WIN, 0, 0);
                            FD_SET(sd, &write_set);
                            break;

                        default:
                            break;
                        }

                        queue_remove(&queue_l, &p);
                        
                    }
                }
    		}

            if(FD_ISSET(i, &write_tmp)) {
                
                if(i == sd) {  //server pronto a ricevere
                    struct queue * p;

                    p = queue_search(queue_l, i);

                    ret = send_msg(p);
                    if(ret < 0){
                        printf("Errore ret send_msg\n");
                        close(sd);
                        exit(1);
                    }

                    if(p->step == 4) {

                        switch(p->flags)
                        {
                        case CL_QUIT:
                            close(sd);
                            close(cd);
                            printf("Disconnesso con successo\n");
                            queue_remove(&queue_l, &p);
                            exit(0);

                        case CL_ACC:
                            printf("Digita la combinazione segreta: ");
                            fflush(stdout);
                            read_comb(comb);

                            isYourTurn = 0;

                            queue_add(&queue_l, cd, CL_INS, 0, 0);
                            FD_SET(cd, &write_set);

                            printf("E' il turno di %s\n", opponent_info.name);
                            
                            break;

                        default: break;



                        }
                       
                        queue_remove(&queue_l, &p);
                        FD_CLR(sd, &write_set);
                    }
                }

                else {       //client pronto a ricevere
                    struct queue *p;

                    p = queue_search(queue_l, i);
                    ret = send_to(i, p, &opponent_info);

                    if(p->step == 4) {

                        switch(p->flags)
                        {
                        case CL_DISC:                            
                            break;


                        default:
                            break;
                        }

                        queue_remove(&queue_l, &p);
                        FD_CLR(i, &write_set);
                    }
                }
            }
    	}
    }

    return 0;
}













void show_help(){
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
    
    switch(p->step)
    {			//sto inviando i flag
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
    
    switch(p->step)
    {
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


int rec_from(int i,struct queue* p,struct info * opponent_info) {


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
        p->buffer = (char*)malloc(p->length);
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


void read_comb(char * comb)
{
    int invalid=0;

    do {
        int i;

        invalid = 0;

        scanf("%4s", comb);
        flush_in();

        for(i=0; i<4 ; i++)
            if(comb[i]>'9' || comb[i]<'0')
                invalid = 1;

    } while (invalid == 1);
}


void flush_in(void)
{
    char ch;
    while ((ch = getchar()) != EOF && ch != '\n') ;
}


void read_cmd()
{
    int ret;
    char cmd[256];

    memset(cmd, 0, 256);

    ret = read(STDIN_FILENO, (void*) cmd, 256);

    if(ret < 0) {
        printf("errore in lettura, riprova");
    }

    else if((strncmp(cmd, "!help", 5)==0)) {
        show_help();
        printf(" ");
    }

    else if((strncmp(cmd, "!who", 4)==0)) {
        queue_add(&queue_l, sd, CL_WHO, 0, 0);
        FD_SET(sd, &write_set);
    }

    else if((strncmp(cmd, "!connect", 8)==0)) {
        //aggiungere errore sintassi ed errore se sei già in partita
        char* buff = (char*) malloc(strlen(cmd)-9);     //alloco lo spazio per un buffer di lunghezza username
        strncpy(buff, cmd+9, strlen(cmd)-10 );      //copio il nome dentro buff
        buff[strlen(cmd)-10] = '\0';

        printf("%s\n", buff);

        //aggiungere errore "non puoi giocare contro te stesso"

        //aggiungere errore se nn esiste il client

        printf("connessione in corso con %s\n",buff );
        queue_add(&queue_l, sd, CL_CONN, strlen(buff)+1, buff);
        free(buff);

        FD_SET(sd, &write_set);
    }

    else if((strncmp(cmd, "!quit", 5))==0) {
        queue_add(&queue_l, sd, CL_QUIT, 0, 0 );
        FD_SET(sd, &write_set);
    }

    else if((strncmp(cmd, "!disconnect", 11))==0) {
        queue_add(&queue_l, sd, CL_DISC, 0, 0);
        queue_add(&queue_l, cd, CL_DISC, 0, 0);
        FD_SET(sd, &write_set);
        FD_SET(cd, &write_set);
        printf("ti sei disconnesso\n");
    }

    else if((strncmp(cmd, "!combinazione", 13))==0) {

        if(isYourTurn == 0){
            printf("non è il tuo turno, aspetta l'altro giocatore\n");
        }

        else{
        char comb_buff[5];
        comb_buff[0] = cmd[14];
        comb_buff[1] = cmd[15];
        comb_buff[2] = cmd[16];
        comb_buff[3] = cmd[17];
        comb_buff[4] = '\0';
 

        isYourTurn = 0;
        queue_add(&queue_l, cd, CL_COMB, 5 , comb_buff);
        FD_SET(cd, &write_set);
        
        }   
    }


}


void handle_incoming_connect(struct queue *p)
{
    struct in_addr *ip;
    unsigned short *port;
    char *name;
    char ans;

    ip   = (struct in_addr *) p->buffer;
    port = (unsigned short *)(p->buffer + sizeof(struct in_addr));
    name = (char *)(p->buffer+ sizeof(struct in_addr) + sizeof(unsigned short));

    do{
        char ch;

        printf("Il client %s, IP %s, porta %hu, vuole giocare. "
               "Accetti la richiesta? [Y|N]: ", name, inet_ntoa(*ip), *port);
        fflush(stdout);

        scanf("%c", &ans);
        flush_in();

    } while (ans != 'y' && ans != 'Y' && ans != 'n' && ans != 'N');


    if(ans == 'N' || ans == 'n')
    {   
        printf("Richiesta rifiutata\n");
        p->flags = CL_REF;
    }
    else if(ans == 'y' || ans == 'Y' )
    {
        memset(&opponent_info.addr, 0, sizeof(opponent_info.addr));
        opponent_info.addr.sin_family = AF_INET;
        opponent_info.addr.sin_port = htons(*port);
        opponent_info.addr.sin_addr = *ip;
        strcpy(opponent_info.name, name);

        printf("Richiesta accettata\n");
        
        turn = 0;
        p->flags = CL_ACC;
    }

    p->length = strlen(name)+1;
    p->buffer = (char*)realloc(p->buffer, p->length);
    strcpy(p->buffer, name);
    p->sd = sd;
    p->step = 1;
    FD_SET(sd, &write_set);
}


void handle_incoming_accept(struct queue *p)
{
    struct in_addr * ip;
    unsigned short * port;
    char * name;

    ip   = (struct in_addr *) p->buffer;
    port = (unsigned short *)(p->buffer + sizeof(struct in_addr));
    name = (char *)(p->buffer+ sizeof(struct in_addr)+ sizeof(unsigned short));

    printf("%s ha accettato la partita\n", name);

    memset(&opponent_info.addr, 0, sizeof(opponent_info.addr));
    opponent_info.addr.sin_family = AF_INET;
    opponent_info.addr.sin_port = htons(*port);
    opponent_info.addr.sin_addr = *ip;
    strcpy(opponent_info.name, name);

    queue_remove(&queue_l, &p);
}


void handle_combination(char * buff){


    int i, j;

    wrong = 0;
    correct = 0;
    

    for(i=0; i<4; i++){
        if(buff[i]==comb[i])
            correct++;
        else{
            for(j=0; j<4; j++)
                if(buff[j]==comb[i])
                    wrong++;
            
        }
    }


    if(correct==4){
        queue_add(&queue_l, cd, CL_WIN, 0, 0 );
        FD_SET(cd, &write_set);

        printf("mi dispiace, hai perso\n");
    }
    
    else{

    snprintf(bb, 100, "%s dice: cifre giuste al posto giusto: %d , cifre giuste al posto sbagliato %d ", player_info.name, correct, wrong);

    queue_add(&queue_l, cd, CL_ANS, sizeof(bb), bb);
    FD_SET(cd, &write_set);
    }
       

    
}