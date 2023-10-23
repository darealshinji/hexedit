CFLAGS ?= -Wall -Wextra -O3
LDFLAGS ?= -s

BIN := hexedit
OBJS = hexedit.o


all: $(BIN)

clean:
	-rm -f $(BIN) $(OBJS)

distclean: clean

$(BIN): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

