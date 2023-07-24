#include "../include/header.c"
#include <sys/stat.h>
#define DEBUG

char* ip = "127.0.0.1";
int id=0,matrixid=0,kmeansid=0;
char filepath[1024];
char* filename;
FILE *fp;
int port;
int main(int argc, char *argv[])
{
    int pos=0;
    while(pos!=argc){
        if(strcmp(argv[pos],"-ip")==0){
            ip = argv[pos+1];
        }
        if(strcmp(argv[pos],"-p")==0){
            port=atoi(argv[pos+1]);
        }
        pos++;
    }
    printf("ip:%s port:%d\n",ip,port);
    int cd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    in_addr_t in_addr = inet_addr(ip);
    if (INADDR_NONE == in_addr)
    {
        perror("inet_addr() failed");
        abort(); /* or whatever error handling you choose. */
    }

    server.sin_addr.s_addr = in_addr;
    int c = connect(cd, (struct sockaddr *)&server, sizeof(server));
    if (c == -1)
    {
        printf("Error in connecting to server\n");
        perror("connect() failed");
        abort();

    }
    printf("Connected to server\n");

    char input_buffer[100];
    // receive id
    int r = read(cd, &id, sizeof(int));
    if (r == -1)
    {
        perror("read() failed");
        abort();
    }
    printf("id:%d\n",id);
    while(1){
        printf("Enter a command : ");
        gets(input_buffer);

        int w = write(cd, input_buffer, sizeof(input_buffer));
        if (w == -1)
        {
            perror("write() failed");
            abort();
        }
        if(strcmp(input_buffer,"exit")==0){
            break;
        }

        // first token
        char *token = strtok(input_buffer, " ");

        if(strcmp(token,"matinvpar") == 0){
            matrixid++;
            printf("matrixid:%d\n",matrixid);
            sprintf(filepath,"../computed_results/matinv_client%d_soln%d.txt",id,matrixid);
            filename = filepath+20;
            printf("received the solution file name:%s\n",filename);
        }
        if(strcmp(token,"kmeanspar") == 0){
            while(strcmp(token,"-f")!=0){
                token = strtok(NULL, " ");
            }
            token = strtok(NULL, " ");
            printf("token:%s\n",token);
            FILE *input_file = fopen(token,"r");
            if(input_file == NULL){
                printf("Error in opening file\n");
                exit(0);
            }
            send_file(cd,input_file);
            kmeansid++;
            printf("kmeansid:%d\n",kmeansid);
            sprintf(filepath,"../computed_results/kmeans_client%d_soln%d.txt",id,kmeansid);
            filename = filepath+20;
            printf("received the solution : %s\n",filepath);


        }
        
        fp = fopen(filepath,"w");
        if(fp==NULL){
            printf("Error in opening file\n");
            exit(0);
        }
        // read from server
        char buffer[1024];
        while(1){
            // receive size of line
            int r = read(cd, &w, sizeof(int));
            if (r == -1)
            {
                perror("read() failed");
                abort();
            }
            if(w==0){
                break;
            }
            // clear buffer
            memset(buffer,0,sizeof(buffer));
            // receive line
            r = read(cd, buffer, w);
            if (r == -1)
            {
                perror("read() failed");
                abort();
            }
            if(strcmp(buffer,"EOF")==0){
                break;
            }
            fprintf(fp,"%s",buffer);
            
        }
        fclose(fp);

    }
}

void send_file(int cd,FILE *fp){

    struct stat stats;
    fstat(fileno(fp), &stats);
    send(cd, &stats.st_size, sizeof(off_t), 0);
    if (sendfile(cd, fileno(fp), NULL, stats.st_size) < 0)
    {
        printf("Error in sending file\n");
        perror("sendfile");
        return;
    }
}