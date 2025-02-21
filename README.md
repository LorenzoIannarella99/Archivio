
**This project is a client-server multi-threaded application developed using both C and Python.**

### **Implementation Details**

The system is designed to handle concurrent read and write operations on an archive. The application creates multiple read and write threads, as well as a head reader and a head writer. These threads are launched by the main thread, which also spawns a manager thread to handle SIGINT and SIGTERM signals. Finally, the main thread waits for the termination of the manager thread, which in turn waits for the head reader and head writer threads before completing.

### **C Part**

The portion of the code written in C is responsible for initializing and managing the reader and writer threads. Data is read and written from a buffer. The code also handles synchronous access to the buffer using mutexes and semaphores to prevent race conditions. Additionally, the main thread waits for the termination of the manager thread. After that, it deallocates resources before terminating.

### **1. Hash Table**

The C program uses a hash table as an archive data structure, which is manipulated and accessed through the functions **add** and **count**, both of which implicitly use the function ***hsearch_r***(). I chose to use ***hsearch_r()*** because it guarantees thread safety, thus preventing issues in the case of parallel reads performed by the reader threads.

### **2. Reader and Writer Threads**

These are the only threads that operate on the data structure and are managed using the **reader/writer** paradigm, which favors readers. To achieve this, I used a struct **ls** containing everything needed to manage the threads through the functions ***read_lock(ls *d)***, ***read_unlock(ls *d)***, ***write_lock(ls *d)***, and ***write_unlock(ls *d)***.  
The **ls** struct contains a mutex variable for mutual exclusion, two condition variables, and two integer variables that allow counting the threads accessing the table for reading or writing, enabling synchronization using ***wait()***, ***signal()***, and ***broadcast()***.

### **3. Head Writer Thread**

The head writer thread is responsible for reading byte sequences from the named pipe ****caposc**** opened by the server program. It first reads the length of the string and then the byte sequence. The string is tokenized using ****strtok_r()****, and each token is written to a shared buffer with the writer threads.  
This exchange of information between the head writer and writer threads follows the producer-consumer paradigm. To ensure correct operation, I used a lock variable and two semaphores.  
The head writer continuously attempts to read from the named pipe using the ****readn()**** function (as discussed in the lesson), and this occurs in a while loop, which can only be exited when the named pipe ***caposc*** is closed for writing by the server program.  
Upon exiting the while loop, the named pipe is closed for reading, and a termination message is sent to the writer threads via the buffer. The head writer thread then waits for the termination of the writer threads with ***xpthread_join()*** before finishing.

### **4. Head Reader Thread**

The head reader thread reads byte sequences sent from the server program via the named pipe ****capolet****, receiving the length of the sequences first, followed by the sequences, which are then tokenized using ****strtok_r()****.  
I chose to use ****strtok_r()**** because the function guarantees thread safety. Each token is subsequently shared with the reader threads through a shared buffer.  
For correct information exchange, I used a mutex variable and two semaphores.  
The head reader continuously attempts to read from the named pipe using the ****readn()**** function (as discussed in the lesson). This occurs in a while loop, which can only be exited when the named pipe is closed for writing.  
Upon exiting the while loop, the named pipe is closed for reading, and a termination message is written to the buffer for the reader threads. The head reader thread then waits for their termination with ***xpthread_join()*** before finishing.

### **5. Manager Thread**

The manager thread captures all signals because, in the main thread, all signals were blocked before launching the other threads, and thus the generated threads inherit the signal mask. By using ***sigfillset(&mask)*** on the signal mask defined with ***sigset_t mask***, the manager thread is the only one that receives them.  
When the SIGINT signal is received, the manager thread uses the function ***write*** (since it is async-signal-safe) to print the number of distinct strings in the hash table to stderr.  
Upon receiving the SIGTERM signal, the manager thread blocks all signals with ***pthread_sigmask(SIG_BLOCK, &mask, NULL)*** to avoid interruption and waits for the termination of the head reader and head writer threads using two ***xpthread_join()*** calls before printing the number of distinct strings in the hash table, deallocating the hash table, and terminating the program.

### **Python Part**

The Python code implements a server that manages network connections from various clients using the **concurrent.futures** module to handle a thread pool, where each thread manages a client connection.

There are two types of clients: 'A' type and 'B' type. The 'A' type client sends requests to the server, which processes and forwards them to the head reader thread. The 'B' type client does the same but forwards its requests to the head writer thread.

The Python server uses sockets for network communication and named pipes for communication with the C-based archive program.

### **1. Server**

The server initially launches the archive program using **subprocess.Popen**, then runs the main program. The main program checks the existence of the named pipes (capolet, caposc); if they do not exist, it creates and opens them for writing, then uses **concurrent.futures.ThreadPoolExecutor()** to define a thread pool that handles the various connections. This is done through the function **gestisci_connessione()**, which receives a single byte to determine the type of connection and handle it accordingly. In the case of a 'B' type connection, I defined a semaphore to ensure proper writing to the **caposc** named pipe, as multiple threads are writing to it.

### **2. Client Type B**

The 'B' type client can handle multiple files and communicates with the server using a 'B' type connection. It uses **concurrent.futures.ThreadPoolExecutor()** to define threads, each of which reads strings from a file and sends them to the server, first sending the length and then the byte sequence.

### **3. Client Type A**

The 'A' type client can handle a single file and communicates with the server using an 'A' type connection. It reads strings from the file and sends them to the server, first sending the length and then the byte sequence.
