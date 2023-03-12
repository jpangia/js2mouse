#author: James Pangia

#compile
compile: js2mouse.c
	gcc -Wall -o js2mouse js2mouse.c
#run without args
run: js2mouse
	./js2mouse

rebuild: js2mouse.c
	gcc -Wall -o js2mouse js2mouse.c
	./js2mouse
#clean
clean:
	rm js2mouse
