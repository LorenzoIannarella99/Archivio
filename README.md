# Progetto_lab2

Questo progetto è un'applicazione client-server multithread realizzata utilizzando sia il linguaggio C che Python.

## Dettagli Implementativi

Il sistema è strutturato per gestire letture e scritture concorrenti su un archivio. In particolare, l'applicazione crea vari thread di lettura e scrittura, oltre a un capo lettore e un capo scrittore. Questi thread sono lanciati dal thread main , che lancia anche un thread gestore che gestisce i segnali SIGINT e SIGTERM, ed infine il thread main attende la terminazione del thread gestore prima di terminare e a sua volta il thread gestore attende la terminazione del thread capolettore e del thread caposcrittore .

## Parte C

La porzione di codice scritta in C si occupa dell'inizializzazione e della gestione dei thread lettori e scrittori. I dati sono letti e scritti da un buffer. Il codice gestisce anche l'accesso sincrono al buffer utilizzando mutex e semafori per evitare condizioni di gara. Inoltre, il thread principale attende la terminazione del thread gestore. Dopo di che, dealloca il tutto per poi terminare.


### 1. tabella hash
il programma c usa come archivio una tabella hash come struttura dati, che viene manipolata e visionata tramite le funzioni **aggiungi** e **conta** che implicimanente usano la funzione ***hsearch_r***().
ho scelto di usare hsearch_r() è thread-safe in quanto la tabella è acceduta parallelamente da dei thread lettori e concorrentemente da dei thread scrittori.
### 2. thread scrittori e lettori
 sono gli unici thread che operano sulla struttura dati, essi sono gestiti mediante il paradigma **lettori/scrittori** favorevole ai lettori,per effettuare ciò ho scelto di usare una struct **ls** che contiene tutto ciò che serve per gestire i thread ,mediante l'utilizzo delle funzioni ***read_lock(ls *d)***,***read_unlock(ls *d)***,***write_lock(ls *d)***,***write_unlock(ls *d)***.
 la struct **ls** contine un variabile di mutex per la mutua esclusione e due variabili di condizione e due variabili intere che permettono di contare il thread che stanno accedendo alla tabella sia per lattura che per la scrittura in modo da poter effettuare la sincronizzazione dei thread mediante un corretto uso della ****wait(),signal(),brodcast()**** .
 ### 3. thread caposcrittore
 il thread caposcrittore ha il compito di leggere le sequenze di bytes dalla npipe ****caposc**** aperta dal programma server.py , leggendo prima la lunghezza della stringa e poi la sequenza di bytes che rappresenta la stringa per poi tokenizzare ogni striga che legge dalla pipe mediante la ****strtok_r()**** e scrivere in un buffer condiviso con i thread scrittori.
 questo scambio di informazioni tra thread caposcrittore e thread scrittori avviene mediante paradigma produttori consumatori, e per garantire che tutto avvenga correttamente ho usato una variabile di lock e due semafori .
 il thread caposcrittore tenta di leggere continuamente dalla npipe mediante la funzione ****readn()**** vista a lezione , ciò avviene con un ciclo while da cui è possibile uscire solo se npipe viene chiusa in scrittura.
 All'uscita del while viene chiusa la npipe in lettura e manda sul buffer il messaggio di terminazione ai thread scrittori e poi attende la loro terminazione con la ****xpthread_join()****, per poi terminare.
### 4. thread capolettore
il thread capolettore legge dalla npipe ****capolet**** le sequenze di bytes inviate dal programma server.py, ricevendo prima le lunghezze delle sequenze e poi le sequenza che vengono tokenizzate mediante la ****strtok_r()**** in quanto garantisce la thread safety , e poi ogni token viene condivisio ai thread lettori mediante un buffer condiviso .
per far avvenire correttamente lo scambio d'informazioni ho scelto di usare una variabile di mutex e due semafori.
il thread capolettore tenta di leggere continuamente dalla npipe mediante la funzione ****readn()**** vista a lezione , ciò avviene con un ciclo while da cui è possibile uscire solo se npipe viene chiusa in scrittura.
 All'uscita del while viene chiusa la npipe in lettura e scrive sul buffer il messaggio di terminazione ai thread lettori e poi attende la loro terminazione con la ****xpthread_join()****, per poi terminare.
### 5. thread gestore 
il thread gestore cattura tutti i segnali ciò avviene perchè nel thread main prima di lanciare tutti i thread sono stati bloccati tutti i segnali e quindi i thread generati hanno ereditato la maschera,quindi effettuando ****sigfillset(&mask)**** sulla maschera dei segnali definita con ****sigset_t mask**** il thread gestore è l'unico che li riceve , e nella gestione del segnale SIGINT utilizza la funzione ****write****(in quanto è async-signal-safe) per stampare su stderr il numero di stringhe distinte contenute nella tabella hash.
Quando riceve il segnale SIGTERM il thread gestore blocca tutti i segnali con ****pthread_sigmask(SIG_BLOCK,&mask,NULL)****  in modo da non essere interrotto e attende con due ****xpthread_join()**** la terminazione dei thread caposcrittore e capolettore per poi stampare su stdout il numero di stringhe distinte nella tabella hash ,deallocare la tabella hash e terminare.

## Parte Python

Il codice Python implementa un server che gestisce connessioni di rete da vari client.Utilizza il modulo concurrent.futures per gestire un pool di thread, ognuno dei quali gestisce una connessione client.

Ci sono due tipi di client, uno di tipo 'A' e uno di tipo 'B'. Il client di tipo 'A' invia richieste al server, che vengono elaborate e inoltrate al thread capolettore. Il client di tipo 'B' fa la stessa cosa, ma le sue richieste sono inoltrate al thread caposcrittore.

Il server Python utilizza socket per la comunicazione di rete e named pipes per la comunicazione con il programma C archivio .

### 1. server
il server inizialmente lancia il programma archivio usando ****subprocess.Popen**** poi esegue il maai, il main effettuaun controllo sull'esistenza delle npipe(capolet,caposc) se esse non esistono le crea e le apre in scrittura e poi usa ****concurrent.futures.ThreadPoolExecutor()****
per definire una pool di thread per gestire le varie connessioni , ciò avviene mediante la funzione ****gestisci_connessione()**** che riceve un singolo byte per capire che tipo di connessione e quindi di gestirla diversamente ,nel caso di connessione di tipo B ho definito un semaforo per far avvenire correttamente le scritture nella pipe in qunato più thread scrivono nella npipe caposc.
ho definito una variabile di lock per per incrementare correttamente il numero di bytes che il server con connessione di tipo B gestita da più thread scrive nella npipe.

### 2.client tipo2
il client di tipo2 comunica con il server mediante connessione di tipo B ,usa 
****concurrent.futures.ThreadPoolExecutor()**** per definire dei thread dove ognuno di essi gestisce un file dal quale legge e invia ciò che legge al server ,inviando prima la lunghezza e poi il la sequenza di bytes .
ho usato una variabile global con una lock per contare i thread che finivano il loro operato in modo tale che l'ultimo che finiva di leggere e comunicare con il server, prima di terminare inviava un'ultima lunghezza "0" in modo da far capire al server la fine della comunicazione con il client di tipo2.
