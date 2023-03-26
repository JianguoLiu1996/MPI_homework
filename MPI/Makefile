#
# Makefile to build example MPI programs
#

CC=mpicc
CFLAGS=-c -Wall
SOURCE=blocking  broadcast  dartboard_pi_send  deadlock  hello_world non-blocking pi_trapezoid reduce_trapezoid  scatter_gather  send_recv  send_trapezoid  serial_trapezoid MatrixMatrix

all: $(SOURCE)

###########################################################
# linking 
blocking: blocking.o
	$(CC) blocking.o -o blocking.e    
	\rm -f *.o 

# -c for Compile and assemble, but do not link.
#blocking.o: blocking.c
#	$(CC) $(CFLAGS) blocking.c

##########################################################
# linking 
broadcast: broadcast.o
	$(CC) broadcast.o -o broadcast.e    
	\rm -f *.o 

# -c for Compile and assemble, but do not link.
broadcast.o: broadcast.c
	$(CC) $(CFLAGS) broadcast.c

##########################################################
# linking 
dartboard_pi_send: dartboard_pi_send.o
	$(CC) dartboard_pi_send.o -o dartboard_pi_send.e    
	\rm -f *.o 

# -c for Compile and assemble, but do not link.
dartboard_pi_send.o: dartboard_pi_send.c
	$(CC) $(CFLAGS) dartboard_pi_send.c

##########################################################
# linking 
deadlock: deadlock.o
	$(CC) deadlock.o -o deadlock.e  
	\rm -f *.o 

# -c for Compile and assemble, but do not link.
deadlock.o: deadlock.c
	$(CC) $(CFLAGS) deadlock.c

##########################################################
# linking 
hello_world: hello_world.o
	$(CC) hello_world.o -o hello_world.e    
	\rm -f *.o 

# -c for Compile and assemble, but do not link.
hello_world.o: hello_world.c
	$(CC) $(CFLAGS) hello_world.c

##########################################################
# linking 
non-blocking: non-blocking.o
	$(CC) non-blocking.o -o non-blocking.e    
	\rm -f *.o 

# -c for Compile and assemble, but do not link.
non-blocking.o: non-blocking.c
	$(CC) $(CFLAGS) non-blocking.c

##########################################################
# linking 
reduce_trapezoid: reduce_trapezoid.o
	$(CC) reduce_trapezoid.o -o reduce_trapezoid.e    
	\rm -f *.o 

# -c for Compile and assemble, but do not link.
reduce_trapezoid.o: reduce_trapezoid.c
	$(CC) $(CFLAGS) reduce_trapezoid.c

##########################################################
# linking
pi_trapezoid: pi_trapezoid.o
	$(CC) pi_trapezoid.o -o pi_trapezoid.e
	\rm -f *.o

# -c for Compile and assemble, but do not link.
pi_trapezoid.o: pi_trapezoid.c
	$(CC) $(CFLAGS) pi_trapezoid.c
	
##########################################################
# linking 
scatter_gather: scatter_gather.o
	$(CC) scatter_gather.o -o scatter_gather.e    
	\rm -f *.o 

# -c for Compile and assemble, but do not link.
scatter_gather.o: scatter_gather.c
	$(CC) $(CFLAGS) scatter_gather.c

##########################################################
# linking 
send_recv: send_recv.o
	$(CC) send_recv.o -o send_recv.e    
	\rm -f *.o 

# -c for Compile and assemble, but do not link.
send_recv.o: send_recv.c
	$(CC) $(CFLAGS) send_recv.c

##########################################################
# linking 
send_trapezoid: send_trapezoid.o
	$(CC) send_trapezoid.o -o send_trapezoid.e    
	\rm -f *.o 

# -c for Compile and assemble, but do not link.
send_trapezoid.o: send_trapezoid.c
	$(CC) $(CFLAGS) send_trapezoid.c

##########################################################
# linking 
serial_trapezoid: serial_trapezoid.o
	$(CC) serial_trapezoid.o -o serial_trapezoid.e  
	\rm -f *.o 

# -c for Compile and assemble, but do not link.
serial_trapezoid.o: serial_trapezoid.c
	$(CC) $(CFLAGS) serial_trapezoid.c

##########################################################
# linking 
MatrixMatrix: MatrixMatrix.o
	$(CC) MatrixMatrix.o -o MatrixMatrix.e  
	\rm -f *.o 

# -c for Compile and assemble, but do not link.
MatrixMatrix.o: MatrixMatrix.c
	$(CC) $(CFLAGS) MatrixMatrix.c

##########################################################

clean:
	\rm -f *.e
	\rm -f *.o 
