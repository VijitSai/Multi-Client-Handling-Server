
mathserver: ./src/ 
	gcc -w -O2 -o server ./src/server.c -lpthread
	gcc -w -O2 -o client ./src/client.c -lpthread
	gcc -w -O2 -o ./src/matinvpar ./src/matinvpar.c -lpthread
	gcc -w -O2 -o ./src/kmeanspar ./src/kmeanspar.c -lpthread
clean: 
	rm -rf ./objects/server
	rm -rf ./objects/client
	rm -rf ./objects/matinvpar
	rm -rf ./objects/kmeanspar
