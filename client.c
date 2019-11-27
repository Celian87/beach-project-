//CLIENT
/*Componenti gruppo di lavoro:
    - Vantaggiato Giulia    130117
    - Damiano Dalila        131946
*/

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#ifndef STAMPA_MSG
#define STAMPA_MSG 1
#endif
#define BUFF_SIZE 256
#define DELIM " -/"

//variabili globali
int sockfd;

//prototipi
void send_msg(int socket_descriptor, char* msg);
void receive_msg(int socket_descriptor, char* msg);
int estraiNum(char *buffer);
int controlloDate(char *array);
void sighand (int sig);

//####################################### MAIN #############################################

int main (int argc, char *argv[]){

    struct sockaddr_in serv_addr;
    char buffer[BUFF_SIZE];

    if (argc < 2){
        puts("Errore, argomenti insufficienti!");
        puts("Usare una delle seguenti opzioni:");
        puts("  ./client BOOK -->richiesta ombrellone;");
        puts("  ./client AVAILABLE -->conoscere disponibilità ombrelloni;");
        puts("  ./client AVAILABLE 1 --> ombrelloni disponibili prima fila;");
        puts("  ./client CANCEL $OMBRELLONE --> cancellare prenotazione.");
        exit(EXIT_SUCCESS);
    }
    //gestione dei segnali
    signal(SIGINT, sighand);
    signal(SIGTERM, sighand);
    signal(SIGQUIT, sighand);
    
    //crea un punto socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if ( sockfd < 0 ){
        perror("Creazione socket FALLITA");
        exit(EXIT_FAILURE);
    }

    //pulire la socketaddr_in structure
    bzero((char *)&serv_addr, sizeof(serv_addr));
   
    //inizializzazione dell'indirizzo del server
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(12687);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    
    //richiesta di connessione al server
    while (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1){
        if ( errno == ENOENT){
            sleep(1); //socket non ancora disponibile, attende 
        }else{
            puts("Errore di connessione, server non disponibile");
            exit (EXIT_FAILURE);
        }
    }
    
    if (argc == 3){ //AVALILABLE1
        //invio al server AVAILABLE 1/CANCEL $OMBRELLONE
        sprintf(buffer, "%s %s", argv[1], argv[2]);
        send_msg (sockfd, buffer);
         
        int n = strcmp(argv[1], "AVAILABLE");
        if(n == 0){ //invia AVAILABLE 1
            
            //il server invia il numero di ombrelloni disp in prima fila
            receive_msg(sockfd, buffer);
            int t;
            t = estraiNum(buffer); //estraggo il numero disponibile
             
            //lo stampo a video
            printf ( "\nN° ombrelloni disponibili in prima fila: %d\n", t);
            close(sockfd);
        }else { //invia CANCEL $OMBRELLONE
            //riceve OK/NOK da server
            receive_msg(sockfd, buffer);
            int x; 
            x = strcmp(buffer, "OK"); 
            if ( x == 0 ){
                //manda ESITO 
                strcpy(buffer, "ESITO?");
                send_msg(sockfd, buffer);
                 
                //riceve CANCEL OK dal server e chiude la connessione 
                receive_msg (sockfd, buffer);
                //controllo numero ombrellone inserito 
                if(strcmp(buffer, "Ombrellone non esistente!")==0){
                    puts("\nErrore scelta ombrellone, riprovare.");
                    puts("Inserire numero da 1 a 30");
                    close(sockfd);
                }else{
                    puts("\nCancellazione avvenuta con successo.");
                    puts("Grazie ed arrivederci.");
                    close(sockfd);
                } 
                
            }else{
                //se il server non è pronto, chiude la connessione
                puts("\nServer non disponibile.");
                close(sockfd);
            }
            
        }
        //fine if (argc == 3)    

    }else if (argc < 3){ //caso BOOK / AVAILABLE 
        strcpy(buffer, argv[1]);
        //invio il messaggio al server= BOOK/AVAILABLE
        send_msg(sockfd, buffer); 
        int m = strcmp(argv[1], "BOOK");
        if (m == 0){
            //riceve OK/NOK 
            receive_msg(sockfd, buffer);
            int n; 
            n = strcmp(buffer, "OK");
             
            //se riceve OK
            if ( n == 0 ){
            //OGGI/FUTURO
            int prenotazione;
		    do{
                puts("\nTipo di prenotazione: ");
                puts(" 1 --> prenotazione con data di oggi");
                puts(" 2 --> prenotazione data futura");
                printf("%s", "Inserire numero scelta: ");
                scanf("%d", &prenotazione);
		    }while(prenotazione < 1 || prenotazione > 2); //controllo scelta utente

            switch(prenotazione){        
                case 1:
                    { 
                    //mando OGGi al server 
                    strcpy(buffer, "OGGI");
                    send_msg(sockfd, buffer);
                         
                    //ricevo PROSEGUI dal server 
                    receive_msg(sockfd, buffer);
                    int scelta = 0;
                    do {
                    puts("\nRichiesta numero ombrellone da prenotare: ");
                    printf("%s", "Inserire numero ombrellone [1-30]: ");
                    scanf ( "%d",&scelta);
                    } while (scelta < 1 || scelta > 30); //controllo scelta utente

                    //manda BOOK OMBRELLONE
                    sprintf(buffer, "BOOK %d",scelta);
                    send_msg(sockfd, buffer); 
                    //riceve AVAILABLE O NAVAILABLE 
                    receive_msg(sockfd, buffer);
                    int n;
                    n = strcmp ( buffer, "AVAILABLE"); 
                    //se riceve AVAILABLE
                    if ( n == 0 ){
                        char date[6];
				        printf("%s", "Inserire data: ");
				        scanf("%s", date);
                        //effettua controllo correttezza data inserita dall'utente
                        int controllo = controlloDate(date);
                        while (!controllo){
                            puts("Inserire data valida!");
                            printf("%s", "Inserire data: ");
                            scanf("%s", date);
                            controllo = controlloDate(date);
                        }
                            
                    //scelgo se CONFERMARE la prenotazione o CANCELLARE
			        int n_scelta;
                    do{
                        puts ("\nConferma prenotazione? ");
                        puts ( "  1 = conferma.");
                        puts ( "  2 = CANCEL.");
                        printf("%s", "Inserire numero scelta: ");
                        scanf("%d", &n_scelta);
		            }while(n_scelta < 1 || n_scelta > 2); //controllo scelta utente

                    if ( n_scelta == 1 ){ //mando BOOK OMBRELLONE DATE
                        //invio dati al server  BOOK $OMBRELLONE DATE
				        sprintf(buffer, "BOOK %d %s", scelta, date);
                        send_msg ( sockfd, buffer );         
                        //riceve la risposta dal server PRENOTAZIONE EFFETTUATA
                        receive_msg(sockfd, buffer);         
                        //chiudo la connessione
                        puts("\nPrenotazione avvenuta con successo.");
                        puts("Grazie ed arrivederci!");
                        close(sockfd);

                    }else { //CANCELLO        
                        //invio CANCEL al server per disdire una prenotazione 
                        strcpy(buffer, "CANCEL");
                        send_msg (sockfd, buffer); 
                        //riceve risposta di avvenuta cancellazione
                        puts("\nPrenotazione non eseguita.");
                        puts("Arrivederci.");
                        close(sockfd);

                    }
                    //se l'ombrellone non è disponibile   
                    }else{ //NAVAILABLE
                        //riceve comunicazione dal server e chiude la connessione
                        puts("\nSpiacenti, ombrellone non disponibile.");
                        puts("Arrivederci.");
                        close(sockfd);
                    }
                    break;
                    }
    
                case 2: 
                    {
                    //mando FUTURO al server 
                    strcpy(buffer, "FUTURO");
                    send_msg(sockfd, buffer);
                         
                    //ricevo PROSEGUI dal server 
                    receive_msg(sockfd, buffer); 
                    int scelta;
                    puts("\nRichiesta numero ombrellone da prenotare:");
                    do{
                        printf("%s", "Inserire numero ombrellone [1-30]: ");
                        scanf("%d", &scelta);
                    }while(scelta < 1 || scelta > 30 ); //controllo scelta utente
                    sprintf(buffer,"BOOK %d",scelta);
                    send_msg(sockfd,buffer);

                    receive_msg(sockfd,buffer);
                    if (strcmp(buffer,"NAVAILABLE") == 0) {
                        puts("\nSpiacenti, ombrellone non disponibile.");
                        puts("Grazie ed arrivederci.");
                        close(sockfd);
                        break;
                    }
                    //riceve prima data dall'utente
                    char date_i[6];
			        printf("%s", "Inserire data inizio prenotazione: ");
			        scanf("%s", date_i);
                    //effettua controllo correttezza data inserita dall'utente
                    int controllo2 = controlloDate(date_i);
                        while (!controllo2){
                            puts("Inserire data valida!");
                            printf("%s", "Inserire data: ");
                            scanf("%s", date_i);
                            controllo2 = controlloDate(date_i);
                        }
                    //riceve seconda data dall'utente
                    char date_f[6];
			        printf("%s", "Inserire data fine prenotazione: ");
			        scanf("%s", date_f);
                    //effettua controllo correttezza data inserita dall'utente
                    int controllo3 = controlloDate(date_f);
                        while (!controllo3){
                            puts("Inserire data valida!");
                            printf("%s", "Inserire data: ");
                            scanf("%s", date_f);
                            controllo3 = controlloDate(date_f);
                        }
                    //invia dati al server    
                    sprintf(buffer, "BOOK %d %s %s", scelta, date_i, date_f);
                    send_msg ( sockfd, buffer );
                         
                    //riceve la risposta dal server AVAILABLE/NAVAILABLE 
                    receive_msg(sockfd, buffer);
                    int ritorno = strcmp(buffer, "AVAILABLE"); 
                    if (ritorno == 0) { //AVAILABLE
                        //chiudo la connessione
                        puts("\nPrenotazione effettuata!");
                        puts("Grazie ed arrivederci!");
                        close(sockfd);

                    }else{ //NAVAILABLE
                        puts("\nSpiacenti, ombrellone non disponibile in quelle date.");
                        puts("Grazie ed arrivederci.");
                        close(sockfd);
                    }
                    break;
                    }
                            
                }//fine 2° switch

            }else{ //riceve NOK
                //se il serve non è pronto, chiude la connessione
                puts("Server non disponibile.");
                close(sockfd);
            }

        }else{
            //riceve risposta dal server 
            receive_msg (sockfd, buffer);
            int compare;
            compare = strcmp(buffer, "NAVAILABLE");
             
            if ( compare == 0 ){ //non ci sono ombrelloni
                puts("\nSiamo spiacenti: non ci sono ombrelloni disponibili.");
                puts("Arrivederci.");
                close(sockfd);

            }else{
                int num_disp; 
                sscanf (buffer, "AVAILABLE %d", &num_disp);
                //estraggo il numero di ombrelloni disponibili
                int t;
                t = estraiNum (buffer); 
                //lo stampo a video
                printf ("\nN° ombrelloni disponibili: %d\n", t);
                close(sockfd);
            }
        }
        
    }//fine else-if

    return 0;
}

//################################### FUNZIONI UTILI ########################################

//funzione invio messaggi
void send_msg(int socket_descriptor, char* msg){
    int retval;
    retval = send(socket_descriptor, msg, BUFF_SIZE, 0);
    if (retval < 0){
        perror("Errore invio messaggio.");
        exit(EXIT_FAILURE);
    }
    if (STAMPA_MSG){
        printf("(%d) > Messaggio inviato: %s\n", socket_descriptor, msg);
    }
}

//funzione ricezione messaggi
void receive_msg(int socket_descriptor, char* msg){
    int retval;
    bzero(msg, BUFF_SIZE);
    retval = recv(socket_descriptor, msg, BUFF_SIZE, 0);
    if (retval < 0){
        perror("Errore invio messaggio.");
        exit(EXIT_FAILURE);
    }
    if (STAMPA_MSG){
        printf("(%d) > Messaggio ricevuto: %s\n", socket_descriptor, msg);
    } 
}

//estrarre numero 
int estraiNum(char *buffer){
    char *token;
    int i;
    for (token = strtok(buffer, DELIM); token!= NULL; token = strtok(NULL, DELIM)){
        i = atoi (token);
    }
    return i;
}

//controllo validità date inserite
int controlloDate(char *array){
    char *gg, *mm; 
    char datestring[20];
	strcpy(datestring,array); //pe evitare corruzione di "date"

    gg = strtok(datestring, DELIM);
    mm = strtok(NULL,DELIM);

    int giorno = 0, mese = 0;
    if (mm != NULL && gg != NULL) {
        giorno = atoi(gg);
        mese = atoi(mm);
    }
    if ( mese <= 0 || giorno <= 0 ) return 0;
    if ( mese > 12 || giorno > 31 ) return 0;
    if ( mese == 11 || mese == 4 || mese == 6 || mese == 9 ){
        if ( giorno > 30 ) return 0;
    }
    //ci interessano solo i mesi di giugno, luglio, agosto e settembre
    //i lidi sono aperti solo in quei periodi quindi niente controllo anni ed anni bisestili
    return 1;
}

//funzione gestione segnali
void sighand (int sig){
    if (sig == SIGINT ) printf("\nRicevuto signale SIGINT\n");
    if (sig == SIGTERM) printf("\nRicevuto signale SIGTERM\n");
    if (sig == SIGQUIT) printf("\nRicevuto signale SIGQUIT\n");

    send(sockfd, "exit", 5, 0);
    puts("Interruzione comunicazioni.");
	close(sockfd);
	exit(0);
}

