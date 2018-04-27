prog: main.c
	gcc -Wall -Werror main.c -o webserver

clean:
	rm -f webserver