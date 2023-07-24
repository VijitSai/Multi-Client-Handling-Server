#include "../include/header.c"

#include <sys/mman.h>
#define DEBUG
#define MAXZ 1400
#define MAX_POINTS 4096
#define MAX_CLUSTERS 32
#define FORK 1

int port,strategy,damonize;

static int* client_no;
char *filename;
// K-Means
typedef struct Client {
    int id;
    int matid;
    int kmeansid;
}Client;

int main(int argc, char **argv[])
{
    // flags for server setup
    // read server arguments

    int pos = 0;
    client_no = mmap(NULL, sizeof *client_no, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *client_no = 1;
    while(pos< argc){
        if(strcmp(argv[pos], "-p") == 0){
            port = atoi(argv[pos+1]);
        }
        else if(strcmp(argv[pos], "-s") == 0){
            if(strcmp(argv[pos+1], "fork") == 0){
                strategy = FORK;
            }
            
        }
        else if(strcmp(argv[pos], "-d") == 0){
            damonize = 1;
        }
        pos++;
    }

    // setup server
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // bind socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    int addrlen = sizeof(address);
    int i = 0;
    printf("listening on port %d\n", port);
    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            exit(EXIT_FAILURE);
        }
        if (strategy == FORK)
        {
            if (fork() == 0)
            {
                close(server_fd);
                Client *client = malloc(sizeof(Client));
                client->id = *client_no;
                client->matid = 0;
                client->kmeansid = 0;
                send(new_socket, &client->id, sizeof(int), 0);
                printf("client %d connected\n", client->id);
                (*client_no)++;
                while(1){
                    char buffer[1024] = {0};
                    valread = read(new_socket, buffer, 1024);
                    if(strcmp(buffer, "exit") == 0){
                        break;
                    }
                    else{
                        char cmd[1024];
                        char buffer_cpy[1024];
                        strcpy(buffer_cpy, buffer);
                        char *type = strtok(buffer_cpy, " ");

                        if(type == NULL){
                            printf("client %d disconnected\n", client->id);
                            break;
                        }
                        
                        char file_details[100];
                        char full_cmd[1024];
                        char data_file[100];
                        data_file[0] = '\0';
                        if(strcmp(type, "matinvpar") == 0){
                            client->matid++;
                            sprintf(file_details, "matinv_client%d_soln%d.txt", client->id, client->matid);
                            strcpy(cmd, "./src/");
                            strcat(cmd, buffer);
                            strcat(cmd, " -r ");
                            strcat(cmd, file_details);
                            strcpy(full_cmd, cmd);
                        }
                        else if(strcmp(type, "kmeanspar") == 0){
                            client->kmeansid++;
                            sprintf(file_details, "kmeans_client%d_soln%d.txt", client->id, client->kmeansid);
                            // receive data file
                            sprintf(data_file, "kmeans_client%d_data%d.txt", client->id, client->kmeansid);
                            FILE *data_fp = fopen(data_file, "w");
                            
                            // read file from client
                            char data_buffer[1024];
                            // receive file size
                            off_t file_size;
                            recv(new_socket, &file_size, sizeof(off_t), 0);
                            int total_bytes_read = 0;
                            int bytes_read;
                            do
                            {
                                bytes_read = recv(new_socket, data_buffer, sizeof(data_buffer), 0);
                                total_bytes_read += bytes_read;
                                if (bytes_read > 0)
                                {
                                    fwrite(data_buffer, 1, bytes_read, data_fp);
                                }
                                if (total_bytes_read == file_size)
                                    break;
                            } while (bytes_read > 0);
                            fclose(data_fp);
                            while(strcmp(type,"-k")!=0){
                                type = strtok(NULL, " ");
                            }
                            
                            int k = atoi(strtok(NULL, " "));
                            sprintf(full_cmd, "./src/kmeanspar -f %s -k %d -r %s", data_file, k, file_details);
                        }
                        system(full_cmd);
                        if(data_file[0] != '\0'){
                            remove(data_file);
                        }
                        printf("sending solution file %s\n", file_details);
                        FILE *fp = fopen(file_details, "r");
                        char *line = NULL;
                        size_t len = 0;
                        ssize_t read;

                        while ((read = getline(&line, &len, fp)) != -1) {
                            // send size of line
                            int size = strlen(line);
                            send(new_socket, &size, sizeof(int), 0);
                            send(new_socket, line, strlen(line), 0);
                        }
                        
                        fclose(fp);
                        int size = strlen("EOF");
                        send(new_socket, &size, sizeof(int), 0);
                        send(new_socket, "EOF", 3, 0);
                        // delete file
                        remove(file_details);

                    }
                    // if client disconnects
                    if(valread == 0){
                        break;
                    }
                }
            }
            else
            {
                close(new_socket);
            }
        }
        
    }
    return 0;


    
}
