CC=gcc
CFLAGS=-std=c11 -Wall -g -O -pthread
LDLIBS=-lm -lrt -pthread

# elenco degli eseguibili da creare
EXECS= archivio



all: $(EXECS) 


# regola per la creazioni degli eseguibili utilizzando xerrori.o
archivio: archivio.o xerrori.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# regola per la creazione di file oggetto che dipendono da xerrori.h
archivio.o: archivio.c xerrori.h
	$(CC) $(CFLAGS) -c $<

