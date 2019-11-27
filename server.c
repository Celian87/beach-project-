//SERVER 
/*Componenti gruppo di lavoro:
    - Vantaggiato Giulia    130117
    - Damiano Dalila        131946
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h> //link con -lpthread
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#ifndef STAMPA_MSG
#define STAMPA_MSG 1
#endif
#define DIM 30
#define BUFF_SIZE 256
#define K 10
#define DELIM " /-"
#define MAXCONNESSIONI 3


typedef struct{
    int giorno_i;
    int mese_i;
    int giorno_f;
    int mese_f;
} data;

typedef struct {
    int num; //numero ombrellone
    int disp; //disponibilità: 0 = occupato, 1 = disponibile, 2 = temporaneamente occupato
    data scadenza; //data di scadenza
    int fila; //a 1 a 3. 10 ombrelloni per fila
} ombrellone;

ombrellone prenotazioni[DIM]; //array di strutture
int pronto = 1; //se == 1, libera. se == 0 manda NOK
int fd; //variabile file descriptor
int msd; //master socket descriptor
int conteggio = 0; //conta numero connessioni

//definizione dei mutex
pthread_mutex_t M_ombr;
pthread_mutex_t M_file;
pthread_mutex_t M_cont;

pthread_t master; //master thread descriptor globale per poterlo fermare con handler segnali

//PROTOTIPI
void* client_thread (void *csocket_desk); //funzione chiamata da ogni thread_worker
void* master_thread(void);
int send_msg(int socket_descriptor, char* msg);
int receive_msg(int socket_descriptor, char* msg);
void riempiArray (ombrellone *prenotazioni);
void aggiornaFile(ombrellone *prenotazioni);
int estraiNum(char *buffer);
int* estraiDate(char *buffer, int *a);
void sighand(int sig);

//############################# MAIN ###########################################
int main (void) {
    struct sockaddr_in serv_addr;
    
    //mutex_init
    pthread_mutex_init (&M_ombr, NULL);
    pthread_mutex_init (&M_file, NULL);
    pthread_mutex_init (&M_cont, NULL);

    signal(SIGINT, sighand);
    signal(SIGTERM, sighand);
    signal(SIGQUIT, sighand);

    //creazione del master socket
    msd = socket(AF_INET, SOCK_STREAM, 0);
    if ( msd == -1 ) {
        puts ("Creazione socket FALLITA"); 
        exit(-1); 
    }
    puts ("Socket creato");

    //"pulire" la sockaddr_in structure
    bzero((char *) &serv_addr, sizeof(struct sockaddr_in));

    //preparazione della sockaddr_in structure
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(12687);

    //bind, legare la socket con l'indirizzo 
    if (bind(msd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
        //stampo messaggio di errore
        perror("Bind fallita, ERRORE");
        return 1;
    }
    puts("Bind eseguita");

    //Riempimento array di strutture con dati da file
    riempiArray(prenotazioni);

    //avvio master thread
    pthread_create(&master,NULL,(void*)master_thread(),NULL);
    pthread_join(master,NULL);

    printf("ERROREEEEE\n");
}

//############################# CLIENT ###########################################
void* client_thread (void *csocket_desk){

    //prendere il socket descriptor
    int sock = *(int *)csocket_desk;
    int retval;
    char client_message[BUFF_SIZE]; //buffer comunicazione client/server 
    
    retval = 1;
    while ( (retval = receive_msg(sock, client_message)) > 0) {

        //ALTRI CASI
        if (strcmp(client_message, "AVAILABLE") == 0) {
            //CONTROLLA LA DISPONIBILITÀ DEGLI OMBRELLONI
            //printf("Ricevuto AVAILABLE\n");
            int conta_disp = 0;
            for ( int j = 0; j < DIM; j++){
                if ( prenotazioni[j].disp == 1 ){
                    conta_disp++;
                }
                        
            }
            if ( conta_disp > 0 ){
                //manda AVAILABLE e numero ombrelloni disponibili
                sprintf(client_message, "AVAILABLE %d", conta_disp);
                retval = send_msg(sock, client_message);
                
            }else{ //manda NAVAILABLE 
                strcpy(client_message, "NAVAILABLE");
                retval = send_msg(sock, client_message);
                
            }
        }
        else if(strcmp (client_message, "AVAILABLE 1") == 0) {
            //controlla gli ombrelloni disponibili nella prima fila
            //printf("Ricevuto AVAILABLE 1\n");
            int conta_disp_fila = 0;
            for ( int j = 0; j < DIM; j++){
                if (prenotazioni[j].disp == 1 && prenotazioni[j].fila == 1){
                        conta_disp_fila++;
                }
            }
            //invio disponibilità ombrelloni prima fila
            sprintf(client_message, "AVAILABLE %d", conta_disp_fila);
            retval = send_msg(sock, client_message);   
        }

        if (strcmp(client_message, "BOOK") == 0){
            //può ricevere: BOOK
            if (pronto){ //BOOK
                //manda OK
                //printf("Ricevuto BOOK\n");
                strcpy(client_message, "OK");
                //Manda il messaggio indietro al cliente
                retval = send_msg(sock, client_message);
                    
                //riceve OGGI/DOMANI
                retval = receive_msg(sock, client_message);
                                        
                if ( strcmp (client_message, "OGGI") == 0){ //riceve OGGI
                    //mando PROSEGUI
                    strcpy(client_message, "PROSEGUI");
                    retval = send_msg(sock, client_message);

                    //ricevo BOOK OMBRELLONE
                    retval = receive_msg(sock, client_message);
                    //printf("retval = %d\n",retval);
                    int m = estraiNum(client_message);
                        
                    //se l'ombrellone è disponibile
                    if ( m > 0 && m <= DIM && prenotazioni[m-1].disp == 1 ){
                        pthread_mutex_lock(&M_ombr);
                        prenotazioni[m-1].disp = 2; //2 = temporanemanete prenotato
                        pthread_mutex_unlock(&M_ombr);
                        //aggiorna file
                        pthread_mutex_lock(&M_file);
                        aggiornaFile(prenotazioni);
                        pthread_mutex_unlock(&M_file);

                        //manda AVAILABLE al cliente
                        strcpy(client_message, "AVAILABLE");
                        retval = send_msg(sock, client_message);
                            
                        //riceve CANCEL/BOOK OMBRELLONE DATE
                        retval = receive_msg(sock, client_message);
                        int cancel;
                        cancel = strcmp (client_message, "CANCEL");
                            
                        if(cancel == 0){
                            //tolgo il temporaneamente prenotato
                            pthread_mutex_lock(&M_ombr);
                            prenotazioni[m-1].disp = 1;
                            pthread_mutex_unlock(&M_ombr);
                            pthread_mutex_lock(&M_file);
                            aggiornaFile(prenotazioni);
                            pthread_mutex_unlock(&M_file);
                                
                        }
                        else{ //se è BOOK OMBRELLONE DATE
                                        
                            int omb;
                            int giorno;
                            int mese;
                            int a[K];
                            //estrae i valori
                            estraiDate(client_message, a);
                            omb = a[1];
                            giorno = a[2];
                            mese = a[3];
                                 
                            //modifico la disponibilità
                            pthread_mutex_lock(&M_ombr);
                            prenotazioni[omb-1].disp = 0;
                            prenotazioni[omb-1].scadenza.giorno_i = giorno;
                            prenotazioni[omb-1].scadenza.mese_i = mese;
                            prenotazioni[omb-1].scadenza.giorno_f = giorno;
                            prenotazioni[omb-1].scadenza.mese_f = mese;
                            pthread_mutex_unlock(&M_ombr);
                            //AGGIORNA FILE
                            pthread_mutex_lock(&M_file);
                            aggiornaFile(prenotazioni);
                            pthread_mutex_unlock(&M_file);
                            //MANDA PRENOTAZIONE EFFETTUATA
                            strcpy(client_message, "PRENOTAZIONE EFFETTUATA");
                            retval = send_msg (sock, client_message);

                        }
                    }
                    else{ //se l'ombrellone non è disponibile
                        //manda messaggio NAVAILABLE 
                        strcpy(client_message, "NAVAILABLE");
                        retval = send_msg (sock, client_message);
                        puts("Fine comunicazione: ombrellone non disponibile");
                    }
                }
                    
                if (strcmp (client_message, "FUTURO") == 0){ //riceve FUTURO
                    //mando PROSEGUI
                    strcpy(client_message, "PROSEGUI");
                    retval = send_msg(sock, client_message);
                        
                    //riceve BOOK OMBRELLONE (per bloccare l'ombrellone)
                    retval = receive_msg(sock, client_message);
                    int omb = estraiNum(client_message);
                    if ( omb > 0 && omb <= DIM && prenotazioni[omb-1].disp == 1 ){
                        pthread_mutex_lock(&M_ombr);
                        prenotazioni[omb-1].disp = 2; //2 = temporanemanete prenotato
                        pthread_mutex_unlock(&M_ombr);
                        //aggiorna file
                        pthread_mutex_lock(&M_file);
                        aggiornaFile(prenotazioni);
                        pthread_mutex_unlock(&M_file);

                        //manda AVAILABLE al cliente
                        strcpy(client_message, "AVAILABLE");
                        retval = send_msg(sock, client_message);
                    }
                    else {
                        strcpy(client_message, "NAVAILABLE");
                        retval = send_msg(sock, client_message);
                    }

                    //riceve BOOK OMBRELLONE DATESTART DATEEND
                    retval = receive_msg(sock, client_message);
                    //int omb;                        
                    int giorno_i, mese_i;
                    int giorno_f, mese_f;
                    int a[K];

                    //estrae i valori 
                    estraiDate(client_message, a);
                    omb = a[1];
                    giorno_i = a[2];
                    mese_i = a[3];
                    giorno_f = a[4];
                    mese_f = a[5];
                    //controllo ombrelloni liberi
                    //controllo mese
                    if (omb > 0 && omb <= DIM) { //se numero di ombrellone valido
                        if (prenotazioni[omb-1].scadenza.mese_f < mese_i){
                            //prenoto ombrellone
                            pthread_mutex_lock(&M_ombr);
                            prenotazioni[omb-1].disp = 0; //occupato
                            prenotazioni[omb-1].scadenza.giorno_i = giorno_i;
                            prenotazioni[omb-1].scadenza.mese_i = mese_i;
                            prenotazioni[omb-1].scadenza.giorno_f = giorno_f;
                            prenotazioni[omb-1].scadenza.mese_f = mese_f;
                            pthread_mutex_unlock(&M_ombr);
                            puts ("Ombrellone prenotato");

                            //aggiorno il file
                            pthread_mutex_lock(&M_file);
                            aggiornaFile(prenotazioni);
                            pthread_mutex_unlock(&M_file);

                            //mando conferma al cliente
                            strcpy(client_message, "AVAILABLE");
                            retval = send_msg(sock, client_message);
                                    
                        }
                        else { //controllo mese e giorno
                            if (prenotazioni[omb-1].scadenza.mese_f == mese_i){
                                //controllo giorno
                                if (prenotazioni[omb-1].scadenza.giorno_f < giorno_i ){
                                    //prenoto ombrellone
                                    pthread_mutex_lock(&M_ombr);
                                    prenotazioni[omb-1].disp = 0;
                                    prenotazioni[omb-1].scadenza.giorno_i = giorno_i;
                                    prenotazioni[omb-1].scadenza.mese_i = mese_i;
                                    prenotazioni[omb-1].scadenza.giorno_f = giorno_f;
                                    prenotazioni[omb-1].scadenza.mese_f = mese_f;
                                    pthread_mutex_unlock(&M_ombr);
                                    puts ( "Ombrellone prenotato");
                                        
                                    //mando conferma al cliente
                                    strcpy(client_message, "AVAILABLE");
                                    retval = send_msg(sock, client_message);
                                    
                                    //aggiorno il file
                                    pthread_mutex_lock(&M_file);
                                    aggiornaFile(prenotazioni);
                                    pthread_mutex_unlock(&M_file);
                                }
                                else{
                                    puts("Ombrellone non disponibile");
                                    //comunico l'indisponibilità al cliente
                                    strcpy(client_message, "NAVAILABLE");
                                    retval = send_msg (sock, client_message);
                                }
                            }else{
                                    puts("Ombrellone non disponibile");
                                    //comunico l'indisponibilità al cliente
                                    strcpy(client_message, "NAVAILABLE");
                                    retval = send_msg (sock, client_message);
                            }
                        }
                    }
                    else{ //ombrellone non esistente = non disponibile ;)
                        puts("Ombrellone non disponibile");
                        //comunico l'indisponibilità al cliente
                        strcpy(client_message, "NAVAILABLE");
                        retval = send_msg (sock, client_message);
                    }

                }
                //fine BOOK
            }
            else{ //se il server è occupato (a salvare su file) manda NOK    
                strcpy(client_message, "NOK");
                send_msg(sock, client_message);
                puts("Server occupato");
            }
                    
        }
        else {
           int k = estraiNum(client_message); //va messo qui! o la strcmp sotto non funziona!
            if (strcmp(client_message, "CANCEL") == 0) {
                //printf("Ricevuto CANCEL\n");
                if (pronto){ //CANCEL OMBRELLONE
                    //manda OK 
                    strcpy(client_message, "OK");
                    //Manda il messaggio indietro al cliente
                    retval = send_msg(sock, client_message);
                    //printf("\tCANCEL k = %d\n",k); 
                    if (k > 0 && k <= DIM) { 
                        //aggiorno array struttura cancellando la prenotazione
                        pthread_mutex_lock(&M_ombr);
                        prenotazioni[k-1].disp = 1;
                        prenotazioni[k-1].scadenza.giorno_i = 0;
                        prenotazioni[k-1].scadenza.mese_i = 0;
                        prenotazioni[k-1].scadenza.giorno_f = 0;
                        prenotazioni[k-1].scadenza.mese_f = 0;
                        pthread_mutex_unlock(&M_ombr);

                        //aggiorno il file
                        pthread_mutex_lock(&M_file);
                        aggiornaFile(prenotazioni);
                        pthread_mutex_unlock(&M_file);

                        //riceve ESITO
                        receive_msg(sock, client_message);
                        //mando conferma di cancellazione al cliente
                        strcpy(client_message, "CANCEL OK");
                        send_msg(sock, client_message);
                    }else{
                        //riceve ESITO
                        receive_msg(sock, client_message);
                        //mando conferma di cancellazione al cliente
                        strcpy(client_message, "Ombrellone non esistente!");
                        send_msg(sock, client_message);
                    }
   
                }
                else{ //se il server è occupato (a salvare su file) NOK    
                    strcpy(client_message, "NOK");
                    send_msg(sock, client_message);
                    puts("Server occupato");
                }
            }
        }
    }
        
    if (retval == 0){
        puts ("Cliente disconnesso");
        close(sock);
        pthread_mutex_lock(&M_cont);
        conteggio --;
        if (conteggio < MAXCONNESSIONI) pronto = 1;
        pthread_mutex_unlock(&M_cont);
        fflush(stdout);
    }
    else if (retval == -1){
        perror("recv fallita.");
    }
    pthread_exit(NULL);
}//fine funzione  

//############################# MASTER THREAD ###########################################
void* master_thread(void) {
    int csd; //client socket descriptor
    struct sockaddr_in; 
    pthread_t tid; //thread_id

    while (1){
        //listen per attendere le connessioni
        listen (msd, 5); //5 dim. max coda associata alla socket per deposito rischieste connessione
        //accetta ed inizia la connessione
        puts("Aspettando richieste di connessione...");
	//serve le richieste di connessione...
        csd = accept(msd,NULL,0); //se la coda è vuota, sospensione del server
        if (csd < 0){
            //perror("accept FALLITA");
            exit(1);
        }
	//creazione del canale virtuale avvenuta, risveglio del processo client in attesa sulla connect
        puts ("Connessione accettata");
        int ret = pthread_create( &tid, NULL, client_thread, (void*)&csd);

        if ( ret < 0 ){
            perror("Non può creare il thread");
            exit(1);
        }else{
            pthread_mutex_lock(&M_cont);
            conteggio ++;
            if (conteggio > MAXCONNESSIONI) pronto = 0;
            pthread_mutex_unlock(&M_cont);
        }
    }
    pthread_exit(NULL);
}


//############################### FUNZIONI UTILI#################################

//funzione per inviare i messaggi
int send_msg(int socket_descriptor, char* msg){
    int retval;
    retval = send(socket_descriptor, msg, BUFF_SIZE, 0);
    if (retval < 0){
        perror("Errore invio messaggio.");
        exit(EXIT_FAILURE);
    }else{
        if (STAMPA_MSG){
            printf("(%d) > Messaggio inviato: %s\n", socket_descriptor, msg);
        }
    }
    return retval;
    
}
//funzione per ricevere i messaggi
int receive_msg(int socket_descriptor, char* msg){
    int retval;
    bzero(msg, BUFF_SIZE);
    retval = recv(socket_descriptor, msg, BUFF_SIZE, 0);
    if (retval < 0){
        perror("Errore invio messaggio.");
        exit(EXIT_FAILURE);
    }else{
        if (STAMPA_MSG){
            printf("(%d) > Messaggio ricevuto: %s\n", socket_descriptor, msg);
        }
    }
    return retval;
}
//funzione per rimepire l'array di strutture
void riempiArray (ombrellone *prenotazioni){
    FILE *f;
    f = fopen ( "file.txt", "r");
    if (f == NULL){
        puts ("File could not be opened");
        exit(-1);
    }else{
        int i = 0;
    
        while (!feof(f)) {
            fscanf(f,"%d\t%d\t%d/%d\t%d/%d\t%d\n", &prenotazioni[i].num, &prenotazioni[i].disp, &prenotazioni[i].scadenza.giorno_i, &prenotazioni[i].scadenza.mese_i, &prenotazioni[i].scadenza.giorno_f, &prenotazioni[i].scadenza.mese_f, &prenotazioni[i].fila);
            i++;
        }
    }

    fclose(f);
}
//funzione per aggiornare il file 
void aggiornaFile(ombrellone *prenotazioni){
    
    pthread_mutex_lock(&M_cont);
    pronto = 0;
    pthread_mutex_unlock(&M_cont);
    sleep(3);
    FILE *f;
    f = fopen ( "file.txt", "w");
    if (f == NULL){
        puts ("File could not be opened");
        exit(-1);
    }else{
        int i;
        printf("Salvataggio file...\n");
        for (i = 0; i < DIM; i++){
            if (prenotazioni[i].disp == 2) //se una prenotazione è temporanea, non salvarla
                fprintf(f, "%d\t1\t0/0\t0/0\t%d\n",  prenotazioni[i].num, prenotazioni[i].fila);
            else 
                fprintf(f, "%d\t%d\t%d/%d\t%d/%d\t%d\n",  prenotazioni[i].num, prenotazioni[i].disp, prenotazioni[i].scadenza.giorno_i, prenotazioni[i].scadenza.mese_i, prenotazioni[i].scadenza.giorno_f, prenotazioni[i].scadenza.mese_f, prenotazioni[i].fila);
        }

        fclose(f);
    }

    pthread_mutex_lock(&M_cont);
    pronto = 1;
    pthread_mutex_unlock(&M_cont);
}
//funzione per estrarre numero ombrellone
int estraiNum(char *buffer){
    char *token;
    int i;

    for (token = strtok(buffer, DELIM); token!= NULL; token = strtok(NULL, DELIM)){
        i = atoi (token);
    }
    return i;
}
//funzione per estrarre le date
int* estraiDate(char *buffer, int *a){
    char *token;
    int j = 0;

    for (token = strtok(buffer, DELIM); token!= NULL; token = strtok(NULL, DELIM)){
        a[j] = atoi (token);
        j++;
    }
    return a;
}
//funzione per la gestione dei segnali
void sighand (int sig){
    if (sig == SIGINT ) printf("\nRicevuto signale SIGINT\n");
    if (sig == SIGTERM) printf("\nRicevuto signale SIGTERM\n");
    if (sig == SIGQUIT) printf("\nRicevuto signale SIGQUIT\n");

    pthread_mutex_lock(&M_cont);
    pronto = 0;
    pthread_mutex_unlock(&M_cont);
    printf("Chiusura del msd\n");
    pthread_cancel(master);
    close(msd); //impedisco la creazione di nuovi thread worker (il server si sta chiudendo, bisogna evitare di avere ulteriori richieste e sbrigare quelle già arrivate)
    sleep(2); //la uso per simulare una prova di richiesta di un client che dovrà essere rifiutata
    pthread_mutex_lock(&M_file);
    aggiornaFile(prenotazioni);
    pronto = 0;//perchè aggiornaFile rimette pronto=1 e "dato che la sfiga ci vede benissimo"...
    //si potrebbe avere un client che finisce la prenotazione dopo che hai aggiornato il file ma 
    //prima che il server termini, ed andrebbe persa.
    pthread_mutex_unlock(&M_file);
    
}

