sound: main.o
	gcc -o sound main.o -lwiringPi -g -DDEBUG
main.o:

.PHONY: clean install
clean:
	rm sound main.o
install:
	cp sound /usr/bin -f         \
	cp source ~/records/ -rf

