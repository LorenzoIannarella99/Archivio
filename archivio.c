#include "xerrori.h"
#define QUI __LINE__,__FILE__
#define Num_elem 1000000
#define PC_buffer_len 10
#define Max_sequence_length 2048
#define STATIC_INIZIALIZER {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'}

// tabella hash definita in questo modo per poter utilizzare le funzione hsearch_r(),hcreate_r(),hdestroy_r()
//che richiedono tra i parametri l'indirizzo della tabella ed esso viene fornito mediante &hah_table
struct hsearch_data hash_table;
//struttura che utilizzo per lavorare con il thread gestore
typedef struct 
{
    int *numero_stringhe;
    pthread_t *fine_cl;
    pthread_t *fine_cs;
}dato_gestore;

//struttura per il thread capo lettore
typedef struct 
{
    int num_lett;
    int *index_bcl;
    char **buffer;
    pthread_mutex_t *mbuffl;
    sem_t *sem_free_slots;
    sem_t *sem_data_items;
    pthread_t *array;
}c_let;



//struttura per il thread capo scrittore
typedef struct 
{
    int num_scritt;
    int *index_bs;
    char **buffer;
    pthread_mutex_t *mbuffs;
    sem_t *sem_free_slots;
    sem_t *sem_data_items;
    pthread_t *array;
}c_scrt;


//struttura che utilizzo per risolvere il problema lettori/scrittori
typedef struct 
{
    int *lettori_attivi;
    int *lettori_attesa;
    int *scrittori_attivi;
    int *scrittori_attesa;
    pthread_mutex_t *mux;
    pthread_cond_t *readGo;
    pthread_cond_t *writeGo;
}ls;

// struct che definisce i thread lettori
typedef struct 
{
    int *index_bl;
    char **buffer;
    pthread_mutex_t *mbuffl;
    sem_t *sem_free_slots;
    sem_t *sem_data_items;
    FILE* file;
    pthread_mutex_t *muxf;
    ls *dato;
}lettori;

// struct che definisce i thread scrittori 
typedef struct 
{
    int *numero_stringhe;
    int *index_bs;
    char **buffer;
    pthread_mutex_t *mbuffs;
    sem_t *sem_free_slots;
    sem_t *sem_data_items;
    ls *dato;
}scrittori;



ENTRY *crea_entry(char *s, int n){
    ENTRY *e= malloc(sizeof(ENTRY));;
    if(e ==NULL) xtermina("errore malloc entry 1",QUI);;
    e->key =strdup(s);
    e->data=(int *) malloc(sizeof(int));
    if(e->key==NULL || e->data==NULL)
    xtermina("errore malloc entry 2",QUI);
    *((int *)e->data) = n;
  return e;
}

void distruggi_entry(ENTRY *e)
{
  free(e->key); 
  free(e->data); 
  free(e);
}


void aggiungi(char *s,int*n){
    ENTRY *res;
    ENTRY *e=crea_entry(s,1);
    if(hsearch_r(*e,FIND,&res,&hash_table)==0){
        if(hsearch_r(*e,ENTER,&res,&hash_table)==0) xtermina("errore nell'inserimento",QUI);
        *n+=1;
    }
    else{
        int *incr = (int*)res->data;
        (*incr)++;
        distruggi_entry(e);
    }
    
}

int conta(char *s){
    ENTRY *res;
    ENTRY *e=crea_entry(s,1);
    int *elem;

    if(hsearch_r(*e,FIND,&res,&hash_table)==0){
        distruggi_entry(e);
        return 0;
    }
    else{
        elem=(int *)res->data;
        distruggi_entry(e);
        return *elem;
    }
}

ssize_t readn(int fd, void *ptr, size_t n) {  
   size_t   nleft;
   ssize_t  nread;
 
   nleft = n;
   while (nleft > 0) {
     if((nread = read(fd, ptr, nleft)) < 0) {
        if (nleft == n) return -1; 
        else break; 
     } else if (nread == 0) break; 
     nleft -= nread;
     ptr   += nread;
   }
   return(n - nleft);
}


void read_lock(ls *d){
    pthread_mutex_lock(d->mux);
    *(d->lettori_attesa)+=1;
    while (*(d->scrittori_attivi)>0 ) 
        pthread_cond_wait(d->readGo,d->mux);
    *(d->lettori_attesa)-=1;
    *(d->lettori_attivi)+=1;
    pthread_mutex_unlock(d->mux);
}

void read_unlock(ls *d){
    pthread_mutex_lock(d->mux);
    *(d->lettori_attivi)-=1;
    if(*(d->lettori_attivi)==0 && *(d->scrittori_attesa)>0){
        pthread_cond_signal(d->writeGo);
    }
    pthread_mutex_unlock(d->mux);
}

void write_lock(ls *d){
    pthread_mutex_lock(d->mux);
    *(d->scrittori_attesa)+=1;
    while (*(d->scrittori_attivi)>0 || *(d->lettori_attivi)>0)
        pthread_cond_wait(d->writeGo,d->mux);
    *(d->scrittori_attesa)-=1;
    *(d->scrittori_attivi)+=1;
    pthread_mutex_unlock(d->mux);
}

void write_unlock(ls *d){
    pthread_mutex_lock(d->mux);
    *(d->scrittori_attivi)-=1;
    if(*(d->lettori_attesa)>0) pthread_cond_broadcast(d->readGo);
    else pthread_cond_signal(d->writeGo);
    pthread_mutex_unlock(d->mux);
}

//funzione che esegue  il thread capo lettore 
void *capo_let_body(void *arg){
    c_let *a=(c_let*) arg;
    char* token;
    char* t_checkpoint;
    int fd = open("capolet", O_RDONLY);
    if (fd < 0)
        xtermina("errore apertura pipe",QUI);
    while (true)
    {
        char *b=malloc(Max_sequence_length*(sizeof(char)));
        if(b==NULL) xtermina("malloc fallita",QUI);
        int32_t lung;
        ssize_t int_lung=read(fd,&lung,sizeof(lung));
        if(int_lung==-1) xtermina("errore scrittuta dell'intero nella pipe",QUI);
        if(int_lung==0){
            free(b);
            break;
        }else{
            ssize_t lung_seq=readn(fd,b,lung);
            if(lung_seq==-1) xtermina("errore lettura della sequenza dalla pipe",QUI);
            b[lung]='\0';
            b=realloc(b,(lung+1)*sizeof(char));
            token=strtok_r(b,".,:; \n\r\t",&t_checkpoint);
            while(token !=NULL){
                char *copia=strdup(token);
                xsem_wait(a->sem_free_slots,QUI);
                xpthread_mutex_lock(a->mbuffl,QUI);
                a->buffer[*(a->index_bcl)%PC_buffer_len]=copia;
                *(a->index_bcl)+=1;
                xpthread_mutex_unlock(a->mbuffl,QUI);
                xsem_post(a->sem_data_items,QUI);
                token=strtok_r(NULL,".,:; \n\r\t",&t_checkpoint);
            }
        }
        free(b); 
    }

    xclose(fd,QUI);
    // il thread capo_lettore comunica ai thread lettori che non vi è altro da leggere 
    // tale comunicazione avviene con la scrittura della stringa vuota nel buffer condiviso 
    for(int i=0;i<a->num_lett;i++){
        xsem_wait(a->sem_free_slots,QUI);
        xpthread_mutex_lock(a->mbuffl,QUI);
        a->buffer[*(a->index_bcl)%PC_buffer_len]="\0";
        *(a->index_bcl)+=1;
        xpthread_mutex_unlock(a->mbuffl,QUI);
        xsem_post(a->sem_data_items,QUI);       
    }
    // il thread capo_scrittore attende la terminazione dei thread lettori
    for(int i=0;i<a->num_lett;i++){
        xpthread_join(a->array[i],NULL,QUI);
    }

    pthread_exit(NULL);
}



//funzione che eseguono i thread lettori
void *leggi_val(void *arg){
    lettori *a = (lettori *)arg;
    int n;
    char *s;
    do{
        xsem_wait(a->sem_data_items,QUI);
        xpthread_mutex_lock(a->mbuffl,QUI);
        s=a->buffer[*(a->index_bl)%PC_buffer_len];
        *(a->index_bl)+=1;
        xpthread_mutex_unlock(a->mbuffl,QUI);
        xsem_post(a->sem_free_slots,QUI);
        if(strcmp(s,"\0")==0) break;

        read_lock(a->dato);
        n=conta(s);
        read_unlock(a->dato);

        xpthread_mutex_lock(a->muxf,QUI);
        fprintf(a->file,"%s %d\n",s,n);
        fflush(a->file);
        xpthread_mutex_unlock(a->muxf,QUI);
        // eseguo la free della copia che il capolettore ha inserito nel buffer condiviso
        free(s);
    }
    while (true);
    pthread_exit(NULL);
}

//funzione che esegue il thread capo scrittore
void *capo_scrittore_body(void *arg){
    c_scrt *a=(c_scrt *)arg; 
    char* token;
    char* t_checkpoint;
    int fd = open("caposc", O_RDONLY);
    if (fd < 0)
        xtermina("errore apertura pipe",QUI);
    while (true)
    {
        char *b=malloc(Max_sequence_length*(sizeof(char)));
        if(b==NULL) xtermina("malloc fallita",QUI);
        int32_t lung;
        ssize_t int_lung=readn(fd,&lung,sizeof(lung));
        if(int_lung==-1) xtermina("errore scrittuta dell'intero nella pipe",QUI);
        else if(int_lung==0){
            free(b);
            break;
        }else{
            ssize_t lung_seq=readn(fd,b,lung);
            if(lung_seq==-1) xtermina("errore lettura della sequenza dalla pipe",QUI);
            b[lung]='\0';
            token=strtok_r(b,".,:; \n\r\t",&t_checkpoint);
            while(token !=NULL){
                char *copia=strdup(token);
                xsem_wait(a->sem_free_slots,QUI);
                xpthread_mutex_lock(a->mbuffs,QUI);
                a->buffer[*(a->index_bs)%PC_buffer_len]=copia;
                *(a->index_bs)+=1;
                xpthread_mutex_unlock(a->mbuffs,QUI);
                xsem_post(a->sem_data_items,QUI);
                token=strtok_r(NULL,".,:; \n\r\t",&t_checkpoint); 
            }
        }
        free(b);
    }
    xclose(fd,QUI);
    // il thread capo_scrittore comunica ai thread scrittori che non vi è altro da leggere 
    // tale comunicazione avviene con la scrittura della stringa vuota nel buffer condiviso 
    for(int i=0;i<a->num_scritt;i++){
        xsem_wait(a->sem_free_slots,QUI);
        xpthread_mutex_lock(a->mbuffs,QUI);
        a->buffer[*(a->index_bs)%PC_buffer_len]="\0";
        *(a->index_bs)+=1;
        xpthread_mutex_unlock(a->mbuffs,QUI);
        xsem_post(a->sem_data_items,QUI);        
    }
    // il thread capo_scrittore attende la terminazione dei thread scrittori 
    for(int i=0;i<a->num_scritt;i++){
        xpthread_join(a->array[i],NULL,QUI);
    }
    pthread_exit(NULL);
}  

// funzione che eseguono i thread scrittori
void *scrivi_val(void *arg){
    scrittori *a=(scrittori *) arg;
    char *s;
    do
    {
        xsem_wait(a->sem_data_items,QUI);
        xpthread_mutex_lock(a->mbuffs,QUI);
        s=a->buffer[*(a->index_bs)%PC_buffer_len];
        *(a->index_bs)+=1;
        xpthread_mutex_unlock(a->mbuffs,QUI);
        xsem_post(a->sem_free_slots,QUI);

        if(strcmp(s,"\0")==0) break;

        write_lock(a->dato);
        aggiungi(s,a->numero_stringhe);
        write_unlock(a->dato);
// eseguo la free della copia che il caposcrittore ha inserito nel buffer condiviso
        free(s);
    } while (true);
    pthread_exit(NULL);
}


void *gestore(void *arg){
    dato_gestore *d = (dato_gestore *) arg;
    sigset_t mask;
    sigfillset(&mask);
    int s;
    while(true){
        int e = sigwait(&mask,&s);
        if(e!=0) perror("errore sigwait");
        printf("Thread gestore svegliato dal segnale %d\n",s);
       
        if(s==SIGINT){ 
            char b[16]=STATIC_INIZIALIZER;
            int lung=0;
            unsigned int num_s = (unsigned int) *(d->numero_stringhe); 
            do
            {
                b[lung++]=(num_s%10)+'0';
                num_s/=10;
            } while (num_s>0);
            b[++lung]='\n';
            int inizio=0;
            int fine=lung-2;
            while (inizio<fine){
                char swap=b[inizio];
                b[inizio]=b[fine];
                b[fine]=swap;
                inizio++;
                fine--;
            }
            ssize_t e= write(STDERR_FILENO,b,16);
            if(e==-1) xtermina("write fallita",QUI);
            }

        else if(s==SIGTERM) {
            // BLOCCA TUTTI I SEGNALI IN QUANTO NON NE DEVONO ESSERE GESTITI ALTRI DURANTE LA GESTIONE DI SIGTERM  
            pthread_sigmask(SIG_BLOCK,&mask,NULL); 
            // prima di stampare e far terminare il programma,il thread gestore attende la terminazione del capolettore e del caposcrittore 
            xpthread_join(*(d->fine_cs),NULL,QUI);
            xpthread_join(*(d->fine_cl),NULL,QUI);
            //fprintf(stdout,"\n numero di stringhe distinte è %d\n",*(d->numero_stringhe));
            char b[16]=STATIC_INIZIALIZER;
            int lung=0;
            unsigned int num_s = (unsigned int) *(d->numero_stringhe); 
            do
            {
                b[lung++]=(num_s%10)+'0';
                num_s/=10;
            } while (num_s>0);
            b[++lung]='\n';
            int inizio=0;
            int fine=lung-2;
            while (inizio<fine){
                char swap=b[inizio];
                b[inizio]=b[fine];
                b[fine]=swap;
                inizio++;
                fine--;
            }
            ssize_t e= write(STDOUT_FILENO,b,16);
            if(e==-1) xtermina("write fallita",QUI);
            hdestroy_r(&hash_table);
            break;
        }
    }
    pthread_exit(NULL);
}
 
int main(int argc, char *argv[]){
    if (argc!=3) xtermina("errore nel passaggio dei parametri",QUI);
    int let =atoi(argv[1]);
    int scrit =atoi(argv[2]);
    int numero_di_stringhe_distinte=0;
// creo l'hash table e controllo se ciò è avvenuto correttamente
    if(hcreate_r(Num_elem,&hash_table)==0) xtermina("errore creazione tabella hash",QUI);
// creo il file lettori.log dove i thread lettori scriveranno la stringa ed il numero di occorrenze 
    FILE *f = fopen("lettori.log", "wt");
    if (f == NULL) xtermina("apertura  file fallita",QUI);
// blocco i segnali in modo da far si che se ne occupi un thread dedicato
    sigset_t mask;
    sigfillset(&mask);
    pthread_sigmask(SIG_BLOCK,&mask,NULL);

//definisco le variabili condivise trai vari thread scrittori/lettori
    int read_act=0;
    int read_wait=0;
    int write_act=0; 
    int write_wait=0;

//definisco la variabile di lock per la muta esclusione sulla hash table
    pthread_mutex_t m_hash = PTHREAD_MUTEX_INITIALIZER;
    
//definisco le variabili di condizione per effutare la sicornizzazione tra lettori e scrittori
    pthread_cond_t readGo=PTHREAD_COND_INITIALIZER;
    pthread_cond_t writeGo=PTHREAD_COND_INITIALIZER;
    
// inizializzo la struttura dati per lettori/scrittori
    ls dato;
    dato.mux=&m_hash;
    dato.readGo=&readGo;
    dato.writeGo=&writeGo;
    dato.lettori_attesa=&read_wait;
    dato.lettori_attivi=&read_act;
    dato.scrittori_attesa=&write_wait;
    dato.scrittori_attivi=&write_act;

    
//inizializzazione buffer di stringhe e indice per effettuare il paradigma produttore consumatore
    char *buffl [PC_buffer_len];
    char *buffs[PC_buffer_len];
    int index_buff_l=0;
    int index_buff_cl=0;
    int index_buff_s=0;
    int index_buff_cs=0;

// definizione variabile di lock per la mutua esclusione sul buffer sia per i lettori che per gli scrittori
    pthread_mutex_t m_buffl=PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t m_buffs=PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t m_file=PTHREAD_MUTEX_INITIALIZER;

//definisco ed inizializzo i semafori per la sincronizzare i thread sia lettori che scrittori
    sem_t sem_free_slots_l, sem_data_items_l,sem_free_slots_s, sem_data_items_s;
    xsem_init(&sem_free_slots_l,0,PC_buffer_len,QUI);
    xsem_init(&sem_data_items_l,0,0,QUI);
    xsem_init(&sem_free_slots_s,0,PC_buffer_len,QUI);
    xsem_init(&sem_data_items_s,0,0,QUI);
    
//definisco il threads capo scrittore e lettore ed il thread gestore 
    pthread_t cap_lett;
    pthread_t cap_scritt;
    pthread_t t_gestore;
    pthread_t ts[scrit];
    pthread_t tl[let];

//variabile che permette al thread gestore di stamapare il numero corretto di strighe
//inizializzo la struct dato_gestore e lancio il thread gestore
    dato_gestore dg;
    dg.numero_stringhe=&numero_di_stringhe_distinte;
    dg.fine_cl=&cap_lett;
    dg.fine_cs=&cap_scritt;
    //puts("thread gestore lanciato");
    xpthread_create(&t_gestore,NULL,gestore,&dg,QUI);

//inizializzo e lancio il thread capo_scrittore
    c_scrt dcs;
    dcs.array=ts; 
    dcs.buffer=buffs;
    dcs.index_bs=&index_buff_cs;
    dcs.mbuffs=&m_buffs;
    dcs.num_scritt=scrit;
    dcs.sem_data_items=&sem_data_items_s;
    dcs.sem_free_slots=&sem_free_slots_s;
    //puts("trhead_caposcrittore lanciato");
    xpthread_create(&cap_scritt,NULL,capo_scrittore_body,&dcs,QUI);

//inizializzo e lancio il thread capo_lettore
    c_let dcl;
    dcl.array=tl;
    dcl.buffer=buffl;
    dcl.index_bcl=&index_buff_cl;
    dcl.mbuffl=&m_buffl;
    dcl.num_lett=let;
    dcl.sem_data_items=&sem_data_items_l;
    dcl.sem_free_slots=&sem_free_slots_l;
    //puts("thread capolettore lanciato");
    xpthread_create(&cap_lett,NULL,capo_let_body,&dcl,QUI);
    
//inizializzo i thread lettori 
    lettori apl[let];
    for(int i=0 ; i<let ; i++){
        apl[i].dato = &dato;
        apl[i].mbuffl = &m_buffl;
        apl[i].index_bl = &index_buff_l;
        apl[i].buffer = buffl;
        apl[i].sem_data_items = &sem_data_items_l;
        apl[i].sem_free_slots = &sem_free_slots_l;
        apl[i].file= f;
        apl[i].muxf = &m_file;
        xpthread_create(&tl[i],NULL,leggi_val,apl+i,QUI);
    }
//inizializzo i thread scrittori 
    scrittori aps[scrit];
    for(int i=0 ; i<scrit ; i++){
        aps[i].dato=&dato;
        aps[i].buffer=buffs;
        aps[i].mbuffs=&m_buffs;
        aps[i].index_bs=&index_buff_s;
        aps[i].sem_data_items=&sem_data_items_s;
        aps[i].sem_free_slots=&sem_free_slots_s;
        aps[i].numero_stringhe=&numero_di_stringhe_distinte;
        xpthread_create(&ts[i],NULL,scrivi_val,aps+i,QUI);
    }

// attende la terminazione del thread gestore che avviene solo quando riceve il segnale SIGTERM
    xpthread_join(t_gestore,NULL,QUI);
// il thread main dealloca tutto e termina 
    if(fclose(f)==EOF) xtermina("chiusura file fallita",QUI);
    xpthread_cond_destroy(&readGo,QUI);
    xpthread_cond_destroy(&writeGo,QUI);
    xpthread_mutex_destroy(&m_buffl,QUI);
    xpthread_mutex_destroy(&m_buffs,QUI);
    xpthread_mutex_destroy(&m_file,QUI);
    xpthread_mutex_destroy(&m_hash,QUI);
    xsem_destroy(&sem_free_slots_l,QUI);
    xsem_destroy(&sem_free_slots_s,QUI);
    xsem_destroy(&sem_data_items_l,QUI);
    xsem_destroy(&sem_data_items_s,QUI);
    return 0;

}