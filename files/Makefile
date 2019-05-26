CC = g++
LDFLAGS = -lpthread
CFLAGS = 
OBJS = netutils.o main.o
DEPS = netutils.h
SRCDIR = ./
TARGET = server
SRCDEPS = $(addprefix $(SRCDIR)/,$(DEPS))

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(OBJS): %.o: $(SRCDIR)/%.c $(SRCDEPS)
	$(CC) -g -c -o $@ $(CFLAGS) $<

.PHONY: clean
clean:
	rm -rf $(OBJS) $(TARGET)
