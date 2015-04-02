CFLAGS=-Wall
LIBS=-lutil

objects=$(patsubst %.c, bin/%.o, $(wildcard *.c))
target=bin/prompt
all: $(target)

$(target): $(objects)
	cc $(CFLAGS) -o $@ $^ $(LIBS)

bin/%.o: %.c | bin
	cc -c $(CFLAGS) -o $@ $<

bin:
	mkdir bin

clean:
	rm -rf bin
