CFLAGS = -Wall -O2 -g -std=c++11

CC = c++
#CC = clang++
#CC = clang++-3.9

OBJSDIR = objs
$(shell mkdir -p $(OBJSDIR))

OBJS = $(OBJSDIR)/server.o \
	$(OBJSDIR)/queue2.o \
	$(OBJSDIR)/classes.o \
	$(OBJSDIR)/send_headers.o \
	$(OBJSDIR)/config.o \
	$(OBJSDIR)/threads_manager.o \
	$(OBJSDIR)/request.o \
	$(OBJSDIR)/response.o \
	$(OBJSDIR)/send_files.o \
	$(OBJSDIR)/create_socket.o \
	$(OBJSDIR)/percent_decoding.o \
	$(OBJSDIR)/rd_wr.o \
	$(OBJSDIR)/functions.o \
	$(OBJSDIR)/log.o \
	$(OBJSDIR)/cgi.o \
	$(OBJSDIR)/fcgi.o \
	$(OBJSDIR)/index.o \

server: $(OBJS)
	$(CC) $(CFLAGS) -o $@  $(OBJS) -lpthread

$(OBJSDIR)/server.o: server.cpp main.h
	$(CC) $(CFLAGS) -c server.cpp -o $@

$(OBJSDIR)/queue2.o: queue2.cpp main.h
	$(CC) $(CFLAGS) -c queue2.cpp -o $@

$(OBJSDIR)/classes.o: classes.cpp main.h classes.h String.h
	$(CC) $(CFLAGS) -c classes.cpp -o $@

$(OBJSDIR)/send_headers.o: send_headers.cpp main.h classes.h String.h
	$(CC) $(CFLAGS) -c send_headers.cpp -o $@

$(OBJSDIR)/config.o: config.cpp main.h String.h
	$(CC) $(CFLAGS) -c config.cpp -o $@

$(OBJSDIR)/threads_manager.o: threads_manager.cpp main.h
	$(CC) $(CFLAGS) -c threads_manager.cpp -o $@

$(OBJSDIR)/request.o: request.cpp main.h
	$(CC) $(CFLAGS) -c request.cpp -o $@

$(OBJSDIR)/response.o: response.cpp main.h classes.h String.h
	$(CC) $(CFLAGS) -c response.cpp -o $@

$(OBJSDIR)/send_files.o: send_files.cpp main.h
	$(CC) $(CFLAGS) -c send_files.cpp -o $@

$(OBJSDIR)/create_socket.o: create_socket.cpp main.h
	$(CC) $(CFLAGS) -c create_socket.cpp -o $@

$(OBJSDIR)/percent_decoding.o: percent_decoding.cpp main.h
	$(CC) $(CFLAGS) -c percent_decoding.cpp -o $@

$(OBJSDIR)/rd_wr.o: rd_wr.cpp main.h
	$(CC) $(CFLAGS) -c rd_wr.cpp -o $@

$(OBJSDIR)/functions.o: functions.cpp main.h
	$(CC) $(CFLAGS) -c functions.cpp -o $@

$(OBJSDIR)/log.o: log.cpp main.h String.h
	$(CC) $(CFLAGS) -c log.cpp -o $@

$(OBJSDIR)/cgi.o: cgi.cpp main.h classes.h String.h
	$(CC) $(CFLAGS) -c cgi.cpp -o $@

$(OBJSDIR)/fcgi.o: fcgi.cpp main.h classes.h String.h fcgi.h
	$(CC) $(CFLAGS) -c fcgi.cpp -o $@

$(OBJSDIR)/index.o: index.cpp main.h classes.h String.h
	$(CC) $(CFLAGS) -c index.cpp -o $@


clean:
	rm -f server
	rm -f $(OBJSDIR)/*.o
