CC=gcc
CFLAGS=-std=c11 -Wall -g -O -pthread
LDLIBS=-lm -lrt -pthread

# elenco degli eseguibili da creare
EXECS= archivio



all: $(EXECS) clean


# regola per la creazioni degli eseguibili utilizzando xerrori.o
$(EXECS): $(EXECS).o xerrori.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# regola per la creazione di file oggetto che dipendono da xerrori.h
$(EXECS).o: $(EXECS).c xerrori.h
	$(CC) $(CFLAGS) -c $<
# rimuovo il file.o presenti 
clean: 
	rm -f *.o 
