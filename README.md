# Progetto_lab2

Questo progetto è un'applicazione client-server multithread realizzata utilizzando sia il linguaggio C che Python.

## Dettagli Implementativi

Il sistema è strutturato per gestire letture e scritture concorrenti su un archivio. L'applicazione crea vari thread di lettura e scrittura ed inoltre un capo lettore e un capo scrittore. Questi thread sono lanciati dal thread main che lancia anche un thread gestore che gestisce i segnali SIGINT e SIGTERM. Infine il thread main attende la terminazione del thread gestore che a sua volta prima di terminare attende la terminazione del thread capolettore e del thread caposcrittore.

## Parte C

La porzione di codice scritta in C si occupa dell'inizializzazione e della gestione dei thread lettori e scrittori. I dati sono letti e scritti da un buffer. Il codice gestisce anche l'accesso sincrono al buffer utilizzando mutex e semafori per evitare condizioni di gara. Inoltre, il thread principale, attende la terminazione del thread gestore. Dopo di che, dealloca il tutto per poi terminare.


### 1. Tabella hash
Il programma c usa come archivio una tabella hash come struttura dati che viene manipolata e visionata tramite le funzioni **aggiungi** e **conta** che implicitamente usano la funzione ***hsearch_r***().
Ho scelto di usare hsearch_r() in quanto garantisce la thread safety e quindi non da problemi in caso di letture parallele effettuate dai lettori.
### 2. Thread scrittori e lettori
 Sono gli unici thread che operano sulla struttura dati, essi sono gestiti mediante il paradigma **lettori/scrittori** favorevole ai lettori. Per effettuare ciò, ho scelto di usare una struct **ls** che contiene tutto ciò che serve per gestire i thread mediante l'utilizzo delle funzioni ***read_lock(ls *d)***,***read_unlock(ls *d)***,***write_lock(ls *d)***,***write_unlock(ls *d)***.
 La struct **ls** contiene una variabile di mutex per la mutua esclusione, due variabili di condizione e due variabili intere che permettono di contare i thread che stanno accedendo alla tabella sia per lattura che per la scrittura in modo da poter effettuare la sincronizzazione dei thread mediante un corretto uso della ****wait(),signal(),brodcast()**** .
 ### 3. Thread caposcrittore
 Il thread caposcrittore ha il compito di leggere le sequenze di bytes dalla npipe ****caposc**** aperta dal programma server.py leggendo prima la lunghezza della stringa e poi la sequenza di bytes.
 Quest'ultima rappresenta la stringa che poi viene tokenizzata mediante la ****strtok_r()**** ed ogni token viene scritto in un buffer condiviso con i thread scrittori.
 Questo scambio di informazioni tra thread caposcrittore e thread scrittori avviene mediante paradigma produttori consumatori e per garantire che tutto avvenga correttamente, ho usato una variabile di lock e due semafori.
 Il thread caposcrittore tenta di leggere continuamente dalla npipe mediante la funzione ****readn()**** vista a lezione e ciò avviene con un ciclo while da cui è possibile uscire solo se la npipe caposc viene chiusa in scrittura dal programma server.py.
 All'uscita del while, la npipe viene chiusa in lettura e manda sul buffer il messaggio di terminazione ai thread scrittori ed attende la loro terminazione con la ****xpthread_join()****, per poi terminare.
### 4. Thread capolettore
Il thread capolettore legge dalla npipe ****capolet**** le sequenze di bytes inviate dal programma server.py, ricevendo prima le lunghezze delle sequenze e poi le sequenze che vengono tokenizzate mediante la ****strtok_r()****.
Ho scelto di usare la ****strtok_r()**** in quanto la funzione garantisce la thread safety. Successivamente ogni token viene condiviso ai thread lettori mediante un buffer condiviso.
Per far avvenire correttamente lo scambio d'informazioni, ho scelto di usare una variabile di mutex e due semafori.
Il thread capolettore tenta di leggere continuamente dalla npipe mediante la funzione ****readn()**** vista a lezione, ciò avviene con un ciclo while da cui è possibile uscire solo se npipe viene chiusa in scrittura.
 All'uscita del while la npipe viene chiusa in lettura e scrive sul buffer il messaggio di terminazione ai thread lettori ed attende la loro terminazione con la ****xpthread_join()****, per poi terminare.
### 5. Thread gestore 
Il thread gestore cattura tutti i segnali ed avviene poichè nel thread main, prima di lanciare i thread, sono stati bloccati tutti i segnali e quindi i thread generati hanno ereditato la maschera. Quindi effettuando ****sigfillset(&mask)**** sulla maschera dei segnali definita con ****sigset_t mask****, il thread gestore è l'unico che li riceve. Nella gestione del segnale SIGINT il thread gestore utilizza la funzione ****write****(in quanto è async-signal-safe) per stampare su stderr il numero di stringhe distinte contenute nella tabella hash.
Quando riceve il segnale SIGTERM il thread gestore blocca tutti i segnali con ****pthread_sigmask(SIG_BLOCK,&mask,NULL)****  in modo da non essere interrotto e attende con due ****xpthread_join()**** la terminazione dei thread caposcrittore e capolettore per poi stampare su stdout il numero di stringhe distinte nella tabella hash ,deallocare la tabella hash e terminare.

## Parte Python

Il codice Python implementa un server che gestisce connessioni di rete da vari client utilizzando il modulo ****concurrent.futures**** per gestire un pool di thread ognuno dei quali, gestisce una connessione client.

Ci sono due tipi di client, uno di tipo 'A' e uno di tipo 'B'. Il client di tipo 'A' invia richieste al server che vengono elaborate ed inoltrate al thread capolettore. Il client di tipo 'B' fa la stessa cosa ma le sue richieste sono inoltrate al thread caposcrittore.

Il server Python utilizza socket per la comunicazione di rete e named pipes per la comunicazione con il programma C archivio.

### 1. Server
Il server inizialmente lancia il programma archivio usando ****subprocess.Popen****, poi esegue il main. Il main effettua un controllo sull'esistenza delle npipe(capolet,caposc) se esse non esistono le crea e le apre in scrittura e poi usa ****concurrent.futures.ThreadPoolExecutor()****
per definire una pool di thread che gestiscono le varie connessioni. Ciò avviene mediante la funzione ****gestisci_connessione()**** che riceve un singolo byte per capire che tipo di connessione è e quindi di gestirla diversamente. Nel caso connessione di tipo B ho definito un semaforo per far avvenire correttamente le scritture nella npipe caposc in quanto più thread vi scrivono.

### 2. Client tipo2
Il client di tipo2 è in grando di gestire più file e comunica con il server mediante connessione di tipo B usando
****concurrent.futures.ThreadPoolExecutor()**** per definire dei thread ognuno dei quali legge da un file le stringhe e le invia al server, inviando prima la lunghezza e poi il la sequenza di bytes.

### 3. Client tipo1
Il client di tipo1 è in grado di gestire un solo file e comunica con il server mediante connessione di tipo A. Esso legge dal file le stringhe e le invia al server inviando primma la lunghezza e poi la sequenza di bytes.
