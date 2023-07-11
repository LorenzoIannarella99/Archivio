#!/usr/bin/env python3 


import sys,threading,logging,time,os,subprocess,concurrent.futures,socket,argparse,struct,signal
HOST = "127.0.0.1"
PORT = 57537
Pipe1="capolet"
Pipe2="caposc"

#configurazione del logging
logging.basicConfig(
    filename=os.path.basename(sys.argv[0])[:-3]+'.log',
    level=logging.DEBUG,
    datefmt='%d/%m/%y %H:%M:%S',
    format='%(asctime)s - %(levelname)s - %(message)s')


def main(t,r,w,host=HOST,port=PORT):
    logging.debug("Inizio esecuzione del server")
    fifo1= os.path.join(os.getcwd(),Pipe1)
    fifo2=os.path.join(os.getcwd(),Pipe2)
    logging.debug("creo ed apro le pipe capolet e caposc")
    if not (os.path.exists(fifo1)) and not (os.path.exists(fifo2)):
       os.mkfifo(fifo1)
       os.mkfifo(fifo2)
    fd1=os.open(fifo1,os.O_WRONLY ) 
    fd2=os.open(fifo2,os.O_WRONLY )   
    with socket.socket(socket.AF_INET,socket.SOCK_STREAM) as s:
        try:
            s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
            s.bind((host,port))
            s.listen()
            assert t > 0, "Il numero di thread deve essere positivo"
            with concurrent.futures.ThreadPoolExecutor(max_workers=t) as executor:
                while True:
                    logging.debug("il server attende un client")
                    conn,addr=s.accept()
                    executor.submit(gestisci_connessione,conn,addr,fd1,fd2)
        except KeyboardInterrupt:
           logging.debug("chiusura server")
           s.shutdown(socket.SHUT_RDWR)
           os.close(fd1)
           os.close(fd2)
           os.unlink(fifo1)
           os.unlink(fifo2)
           p_archivio.send_signal(signal.SIGTERM)

           

def gestisci_connessione(conn,addr,fd1,fd2):
   global bytes_scritti_A
   tipo_client=recv_all(conn,1)
   if tipo_client.decode()=='A':
      logging.debug(f"{threading.current_thread().name} contattato da {addr} con connessione di tipo A")
      logging.debug("il server ha aperto in scrittura la pipe capolet")
      while True:
        lung_seq=recv_all(conn,4)
        lunghezza=struct.unpack("!i",lung_seq)[0]
        if lunghezza==0:
           logging.debug(f"con connessione di tipo A il server ha scritto nella pipe capolet {bytes_scritti_A+4}")
           break
        bytes_lung_seq=struct.pack("<i",lunghezza)
        bytes_scritti_A+=4
        os.write(fd1,bytes_lung_seq)
        data_seq=recv_all(conn,lunghezza)
        bytes_scritti_A+=len(data_seq)
        pacco=struct.pack(f'{len(data_seq)}s',data_seq)
        os.write(fd1,pacco)
      bytes_scritti_A=0
      return 


   elif tipo_client.decode()=='B':
      global bytes_scritti_B
      logging.debug(f"{threading.current_thread().name} contattato da {addr} con connessione di tipo B")
      logging.debug("il server ha aperto in scrittura la pipe caposc")
      while True:
        lung_seq=recv_all(conn,4)
        lunghezza=struct.unpack("!i",lung_seq)[0] 
        assert lunghezza<=2048,"non è possibile trasferire questa sequenza di bytes"
        #logging.debug(f"{threading.current_thread().name} ha ricevuto che la lunghezza della stringa è {lunghezza}")
        if lunghezza==0:
           logging.debug(f"con connessione di tipo B il server ha scritto nella pipe caposc {bytes_scritti_B+4}")

           break        
        bytes_lung_seq=struct.pack("<i",lunghezza)
        with lock:
         bytes_scritti_B+=4
        sem.acquire()
        os.write(fd2,bytes_lung_seq)
        data_seq=recv_all(conn,lunghezza)
        bytes_scritti_B+=len(data_seq)
        pacco=struct.pack(f'{len(data_seq)}s',data_seq)
        os.write(fd2,pacco)
        sem.release()
      #os.close(fd2)
      bytes_scritti_B=0
      return

def recv_all(conn,n):
  chunks = b''
  bytes_recd = 0
  while bytes_recd < n:
    chunk = conn.recv(min(n - bytes_recd, 2048))
    if len(chunk) == 0:
      break
    chunks += chunk
    bytes_recd = bytes_recd + len(chunk)
  return chunks


if __name__ == '__main__':
   parser=argparse.ArgumentParser()
   parser.add_argument('t',type=int,help='threads')
   parser.add_argument('-r',help="threads lettori",type=int,default=3)
   parser.add_argument('-w',help='threads scrittori',type=int,default=3)
   parser.add_argument('-a',help='host address',type=str,default=HOST)
   parser.add_argument('-p',help='port',type=int,default=PORT)
   parser.add_argument('-v',action='store_true',help='abilita valgrind')
   args = parser.parse_args()

   logging.debug(f"Il server lancia il programma archivio con {args.r} lettori ed {args.w} scrittori ")
   client1=os.path.join(os.getcwd(),'client1')
   client2=os.path.join(os.getcwd(),'client2')
   subprocess.run(['chmod','+x',client1])
   subprocess.run(['chmod','+x',client2])
   assert args.r>0 and args.w>0, "I valori 'r' e 'w' devono essere positivi" 
   if args.v:
      p_archivio=subprocess.Popen(["valgrind","--leak-check=full", 
                        "--show-leak-kinds=all", 
                        "--log-file=valgrind-%p.log", 
                        "./archivio.out", f"{args.r}",f"{args.w}"])
   else :
      p_archivio=subprocess.Popen(["./archivio.out", f"{args.r}",f"{args.w}"])
   sem=threading.Semaphore(1)
   bytes_scritti_A=0
   bytes_scritti_B=0
   lock=threading.Lock()
   main(args.t,args.r,args.w,args.a,args.p)   

