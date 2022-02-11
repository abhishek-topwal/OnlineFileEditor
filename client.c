#include<stdio.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<stdbool.h>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>

#define SERVER_PORT 5000
#define BUFFER_SIZE 1000000

int invite_flag=0;

void debug(int i){
    printf("\nCP-> %d",i);
    fflush(stdout);
}


void error(char *msg){
    /*
        To show any error while forming sockets.
        It will terminate the program showing the error message.
    */
    perror(msg);
    exit(1);
}

//..................TO READ DATA IN CHUNKS.................

void read_data(int socket, char* buffer, int bytes_to_read){
    int chunk = 100;
    int pos =0;
    while(bytes_to_read>chunk){
        int rec_bytes = read(socket,buffer+pos,chunk);
        bytes_to_read -= rec_bytes;
        pos += rec_bytes;
    }
    if(bytes_to_read>0){
        read(socket,buffer+pos,bytes_to_read);
    }
}

//..................TO WRITE DATA IN CHUNKS.................

void write_data(int socket, char* buffer, int bytes_to_write){
    int chunk=100;
    int pos=0;
    while(bytes_to_write>=chunk){
        int send_bytes = write(socket,buffer+pos,chunk);
        bytes_to_write    -= send_bytes;
        pos += send_bytes;
    }
    if(bytes_to_write != 0){
        read(socket,buffer+pos,bytes_to_write);
    }
}

typedef struct client_files{
    bool isUploaded;
    char filename[50];
}client_files;

typedef struct client_data{
    int curr_file_count;
    client_files client_files[10];
}client_data;

client_data client;

typedef struct client_request{
    /*
        This strucutre maintains the necessary details at the client end.
        Details like client instruction, filename, line count, total bytes etc is maintained.
    */
    char instruction[30];
    FILE *file[1];
    char filename[20];
    int total_lines;
    int total_bytes;
    char client_id[10];
    char permission[5];
    int start_index;
    int end_index;
    int insert_index;
    char message[1000];
    char error_message[150];
}client_request;

int NLINEX(FILE* file){
    /*
        To find the number of lines currently in the file.
        Invoked by "NLINEX" command
    */
    char chr = fgetc(file);
    if(chr==EOF){
        return 0;
    }
    int count_lines=0;
    int flag = 0;
	while (chr != EOF){
	//Count whenever new line is encountered
        if (chr == '\n'){
            flag = 1;
            count_lines = count_lines + 1;
        }
        //take next character from file.
        chr = fgetc(file);
    }
    rewind(file);
    return (flag==0)?1:count_lines+1;
}


int get_total_bytes(FILE *file){
    // to get total bytes in a file
    fseek(file, 0, SEEK_END);
    int res = ftell(file);
    rewind(file);
    return res;
}

//..............TO WRITE FROM A BUFFER TO A FILE.........................

int write_to_file(char *FileName, char *buffer){
    FILE *fp = fopen(FileName,"w+");
    if(fp == NULL){
        return -1;
    }
    int count = fwrite(buffer,sizeof(char),strlen(buffer),fp);
    if(count == BUFFER_SIZE){
        bzero(buffer,BUFFER_SIZE);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    bzero(buffer,BUFFER_SIZE);
    return 1;
}

//................TO HANDLE GET USERS REQUEST...........................

client_request* process_getusers_request(char* instruction){
    client_request* req = malloc(sizeof(client_request));
    sprintf(req->instruction,"%s",instruction);
    return req;
}

//................TO HANDLE GET FILE INFO REQUEST......................

client_request* process_getfiles_request(char* instruction){
    client_request* req = malloc(sizeof(client_request));
    sprintf(req->instruction,"%s",instruction);
    return req;
}

//................TO HANDLE UPLOAD FILE REQUEST........................

client_request* process_uploadfile_request(char* instruction, char* filename){
    client_request* req = malloc(sizeof(client_request));
    FILE *fp1= fopen(filename, "r+");
    if (fp1 == NULL){
        sprintf(req->error_message,"%s","Not able to open file.");
        return req;
    }
    sprintf(req->instruction,"%s",instruction);
    sprintf(req->filename,"%s",filename);
    req->total_lines = NLINEX(fp1);
    req->total_bytes = get_total_bytes(fp1);
    req->file[0]=fp1;
    return req;
}

//...............TO HANDLE DOWNLOAD FILE REQUEST.......................

client_request* process_downloadfile_request(char* instruction, char* filename){
    client_request* req = malloc(sizeof(client_request));
    sprintf(req->instruction,"%s",instruction);
    sprintf(req->filename,"%s",filename);
    return req;
}

//................TO HANDLE INVITE CLIENT REQUEST......................

client_request* process_invite_request(char* instruction, char* filename,char* client_id, char* permission){
    client_request* req = malloc(sizeof(client_request));
    FILE *fp1= fopen(filename, "r+");
    if (fp1 == NULL){
        sprintf(req->error_message,"%s","Not able to open file.");
        return req;
    }
    sprintf(req->instruction,"%s",instruction);
    sprintf(req->filename,"%s",filename);
    sprintf(req->permission,"%s",permission);
    sprintf(req->client_id,"%s",client_id);
    req->file[0]=fp1;
    return req;
}

//................TO HANDLE INVITE RESPONSE REQUEST.....................

client_request* process_invite_response_request(char* instruction, char* sender_client_ID){
    client_request* req = malloc(sizeof(client_request));
    sprintf(req->instruction,"%s",instruction);
    sprintf(req->client_id,"%s",sender_client_ID);
    return req;
}

//...............TO HANDLE READ FILE REQUEST...........................

client_request* process_readfile_request(char* instruction, char* filename,char *start_index,char* end_index,int flag){
    if(flag == 1){
        client_request* req = malloc(sizeof(client_request));
        sprintf(req->instruction,"%s",instruction);
        sprintf(req->filename,"%s",filename);
        return req;
    }
    else if(flag == 2){
        client_request* req = malloc(sizeof(client_request));
        sprintf(req->instruction,"%s",instruction);
        sprintf(req->filename,"%s",filename);
        req->start_index = atoi(start_index);
        return req;
    }
    else if(flag==3){
        client_request* req = malloc(sizeof(client_request));
        sprintf(req->instruction,"%s",instruction);
        sprintf(req->filename,"%s",filename);
        req->start_index = atoi(start_index);
        req->end_index = atoi(end_index);
        return req;
    }
}

//...............TO HANDLE DELETE FILE REQUEST............................

client_request* process_deletefile_request(char* instruction, char* filename,char *start_index,char* end_index,int flag){
    if(flag == 1){
        client_request* req = malloc(sizeof(client_request));
        sprintf(req->instruction,"%s",instruction);
        sprintf(req->filename,"%s",filename);
        return req;
    }
    else if(flag == 2){
        client_request* req = malloc(sizeof(client_request));
        sprintf(req->instruction,"%s",instruction);
        sprintf(req->filename,"%s",filename);
        req->start_index = atoi(start_index);
        return req;
    }
    else if(flag==3){
        client_request* req = malloc(sizeof(client_request));
        sprintf(req->instruction,"%s",instruction);
        sprintf(req->filename,"%s",filename);
        req->start_index = atoi(start_index);
        req->end_index = atoi(end_index);
        return req;
    }
}

//...............TO HANDLE INSERT IN FILE REQUEST ................................

client_request* process_insertfile_request(char* instruction, char* filename,char *index,char* message,int flag){
    if(flag == 1){
        client_request* req = malloc(sizeof(client_request));
        sprintf(req->instruction,"%s",instruction);
        sprintf(req->filename,"%s",filename);
        sprintf(req->message,"%s",message);
        return req;
    }
    else if(flag == 2){
        client_request* req = malloc(sizeof(client_request));
        sprintf(req->instruction,"%s",instruction);
        sprintf(req->filename,"%s",filename);
        sprintf(req->message,"%s",message);
        return req;
    }
}

//...............TO HANDLE EXIT REQUEST.......................................

client_request* process_exit_request(char* instruction){

    client_request* req = malloc(sizeof(client_request));
    sprintf(req->instruction,"%s",instruction);
    return req;
}



//................TO CHECK THE INPUT BY CLIENT IN STDIN.........
client_request* parse_client_input(char *client_input){
    /*
        The client input is parsed. If the input is in proper format,
        client request structure is returned, else NULL is returned.
    */
    char instruction[30];
    if(strncmp(client_input,"/users",6)==0){
        sscanf(client_input,"%[^\n]",instruction);
        if(strcmp(instruction,"/users")==0){
            client_request* req = process_getusers_request(instruction);
            return req;
        }
        else{
            return NULL;
        }
    }

    if(strncmp(client_input,"/files",6)==0){
        sscanf(client_input,"%[^\n]",instruction);
        if(strcmp(instruction,"/files")==0){
            client_request* req = process_getfiles_request(instruction);
            return req;
        }
        else{
            return NULL;
        }
    }

    if(strncmp(client_input,"/upload",7)==0){
        char filename[20];
        int arg_count = sscanf(client_input,"%[^' '] %[^\n]",instruction,filename);
        if(arg_count == 2 && strlen(filename) > 0 && strlen(instruction) > 0){
            if(strcmp(instruction,"/upload")==0){
                client_request* req = process_uploadfile_request(instruction,filename);
                return req;
            }
        }
        else{
            return NULL;
        }
    }

    if(strncmp(client_input,"/download",9)==0){
        char filename[20];
        int arg_count = sscanf(client_input,"%[^' '] %[^\n]",instruction,filename);
        if(arg_count == 2 && strlen(filename) > 0 && strlen(instruction) > 0){
            if(strcmp(instruction,"/download")==0){
                client_request* req = process_downloadfile_request(instruction,filename);
                return req;
            }
        }
        else{
            return NULL;
        }
    }

    if(strncmp(client_input,"/invite",7)==0){
        char filename[20];
        char client_id[10];
        char permission[5];
        int arg_count = sscanf(client_input,"%[^' '] %[^' '] %[^' '] %[^\n]",instruction,filename,client_id,permission);
        if(arg_count == 4 && strlen(filename) > 0 && strlen(instruction) > 0 && strlen(client_id)==5 && strlen(permission)==1 && ((strcmp(permission,"V")==0) || (strcmp(permission,"E")==0))){
            if(strcmp(instruction,"/invite")==0){
                client_request* req = process_invite_request(instruction,filename,client_id,permission);
                return req;
            }
        }
        else{
            return NULL;
        }
    }

    if((strncmp(client_input,"/YES",4)==0) || (strncmp(client_input,"/NO",3)==0)) {
        char sender_client_ID[10];
        int arg_count = sscanf(client_input,"%[^' '] %[^\n]",instruction,sender_client_ID);
        if(arg_count == 2 && strlen(sender_client_ID)==5){
            client_request * req = process_invite_response_request(instruction,sender_client_ID);
            return req;
        }
        else{
            return NULL;
        }
    }

    if(strncmp(client_input,"/read",5)==0){
        char command[100];
        int arg_count = sscanf(client_input,"%[^' '] %[^\n]",instruction,command);
        if(arg_count==2 && strlen(command)>0){
            char filename[50];
            char start_index[10];
            char end_index[10];
            int flag = 0;
            int count_arg = sscanf(command,"%[^' '] %[^' '] %[^\n]",filename,start_index,end_index);
            if(count_arg==1 && strlen(filename)>0){
                flag = 1;
                sscanf(command,"%[^\n]",filename);
                client_request * req = process_readfile_request(instruction,filename,start_index,end_index,flag);
                return req;
            }
            else if(count_arg == 2 && strlen(filename)>0 && strlen(start_index)>0){
                flag = 2;
                sscanf(command,"%[^' '] %[^\n]",filename,start_index);
                client_request * req = process_readfile_request(instruction,filename,start_index,end_index,flag);
                return req;
            }
            else if(count_arg==3 && strlen(filename)>0 && strlen(start_index)>0 && strlen(end_index)>0){
                flag =3;
                sscanf(command,"%[^' '] %[^' '] %[^\n]",filename,start_index,end_index);
                client_request * req = process_readfile_request(instruction,filename,start_index,end_index,flag);
                return req;
            }
            else{
                return NULL;
            }

        }
        else{
            return NULL;
        }

    }


    if(strncmp(client_input,"/delete",7)==0){
        char command[100];
        int arg_count = sscanf(client_input,"%[^' '] %[^\n]",instruction,command);
        if(arg_count==2 && strlen(command)>0){
            char filename[50];
            char start_index[10];
            char end_index[10];
            int flag = 0;
            int count_arg = sscanf(command,"%[^' '] %[^' '] %[^\n]",filename,start_index,end_index);
            if(count_arg==1 && strlen(filename)>0){
                flag = 1;
                sscanf(command,"%[^\n]",filename);
                client_request * req = process_deletefile_request(instruction,filename,start_index,end_index,flag);
                return req;
            }
            else if(count_arg == 2 && strlen(filename)>0 && strlen(start_index)>0){
                flag = 2;
                sscanf(command,"%[^' '] %[^\n]",filename,start_index);
                client_request * req = process_deletefile_request(instruction,filename,start_index,end_index,flag);
                return req;
            }
            else if(count_arg==3 && strlen(filename)>0 && strlen(start_index)>0 && strlen(end_index)>0){
                flag =3;
                sscanf(command,"%[^' '] %[^' '] %[^\n]",filename,start_index,end_index);
                client_request * req = process_deletefile_request(instruction,filename,start_index,end_index,flag);
                return req;
            }
            else{
                return NULL;
            }

        }
        else{
            return NULL;
        }

    }

    if(strncmp(client_input,"/insert",7)==0){
        char command[100];
        int arg_count = sscanf(client_input,"%[^' '] %[^\n]",instruction,command);
        if(arg_count==2 && strlen(command)>0){
            char filename[50];
            char index[20];
            char message[1000];
            int flag =0;
            int count_arg = sscanf(command,"%[^' '] %[^' '] %[^\n]",filename,index,message);
            if(count_arg == 2 && strlen(filename)>0){
                flag = 1;
                sscanf(command,"%[^' '] %[^\n]",filename,message);
                client_request * req = process_insertfile_request(instruction,filename,index,message,flag);
                return req;
            }
            else if(count_arg == 3 && strlen(filename)>0 && strlen(index)>0 && strlen(message)){
                flag = 2;
                sscanf(command,"%[^' '] %[^' '] %[^\n]",filename,index,message);
                client_request * req = process_insertfile_request(instruction,filename,index,message,flag);
                return req;
            }
            else{
                return NULL;
            }
        }
        else{
            return NULL;
        }
    }

    if(strncmp(client_input,"/exit",5)==0){
        int arg_count = sscanf(client_input,"%[^\n]",instruction);
        if(strcmp(instruction,"/exit")==0){
            client_request * req = process_exit_request(instruction);
            return req;
        }
        else{
            return NULL;
        }


    }
    return NULL;
}


//..........TO HANDLE USERS COMMAND OUTPUT BY SERVER................
void handle_getusers_command(int client_sd){
    char server_command[100];
    bzero(server_command,100);
    read_data(client_sd,server_command,100);
    if(strcmp(server_command,"/users")==0){
        char buffer[1000];
        bzero(buffer,1000);
        read_data(client_sd,buffer,100);
        printf("\nThe IDs of all active clients are: ");
        fflush(stdout);
        printf("%s\n",buffer);
        fflush(stdout);
    }
}


//..........TO HANDLE UPLOAD FILE COMMAND OUTPUT BY SERVER................

void handle_uploadfile_command(client_request* req, int client_sd){
    // receive message from server if the filname is available
    char buffer[1024];
    bzero(buffer,1024);
    read_data(client_sd,buffer,100);
    if(strcmp(buffer,"1")==0){
        //send the number of lines in the file
        int line_count = req->total_lines;
        char linecount[10];
        sprintf(linecount,"%d",line_count);
        write_data(client_sd,linecount,100);

        //send the number of bytes in the file to the server
        char file_buffer[BUFFER_SIZE];
        int count = fread(file_buffer, sizeof(char), req->total_bytes, req->file[0]);
        char byte_count[10];
        sprintf(byte_count,"%d",count);
        write_data(client_sd,byte_count,100);

        // receive upload from server
        char server_command[100];
        read_data(client_sd,server_command,100);
        if(strcmp(server_command,"/upload")==0){
            // send file in chunks
            int chunk = 256;
            int pos=0;
            while(count >= chunk){
                int send_bytes = write(client_sd,file_buffer+pos,chunk);
                count -= send_bytes;
                pos += send_bytes;
            }
            if(count!=0){
                write(client_sd,file_buffer+pos,count);
            }
        }

        // receive completion message
        bzero(buffer,1024);
        read_data(client_sd,buffer,100);
        printf("\n%s\n",buffer);
    }
    else{
        // case when same name file exist
        printf("\nSERVER: SORRY!! There exist a file with same name.\n");
        fflush(stdout);
    }
}

//..........TO HANDLE INVITE CLIENT COMMAND OUTPUT BY SERVER................

void handle_invite_command(client_request* req, int client_sd){
    // receive message from server if the filname is available
    char buffer[1024];
    bzero(buffer,1024);
    read_data(client_sd,buffer,100);
    if(strcmp(buffer,"1")==0){
        printf("\nYour invitation was sent successfully.\n");
        fflush(stdout);
    }
    else{
        if(strcmp(buffer,"-1")==0){
            printf("\nSERVER: SORRY!! You are not the owner of the file.\n");
            fflush(stdout);
        }
        else{
            if(strcmp(buffer,"-2")==0){
                printf("\nSERVER: SORRY!! You have to first upload the file to send any invitation.\n");
                fflush(stdout);
            }
            else{
                printf("\nSERVER: SORRY!! The given Client ID is not valid.\n");
                fflush(stdout);
            }
        }
    }

}

//..........TO HANDLE INVITE RESPONSE COMMAND OUTPUT BY SERVER................

void handle_invite_response_command(client_request* req, int client_sd){
    // receive message from server if the response was valid
    char buffer[1024];
    bzero(buffer,1024);
    read_data(client_sd,buffer,100);
    if (strcmp(buffer, "1") == 0){
        invite_flag = 0;
        printf("\nYour response was sent.\n");
        fflush(stdout);
    }
    else if (strcmp(buffer, "-1") == 0){
        printf("\nSERVER: SORRY!! The client ID is not valid.\n");
        fflush(stdout);
    }
    else if (strcmp(buffer, "-2") == 0){
        printf("\nSERVER: SORRY!! There was no collaboration request from this ID.\n");
        fflush(stdout);
    }
    else if (strcmp(buffer, "2") == 0){
        invite_flag=0;
        bzero(buffer,1024);
        read_data(client_sd,buffer,1000);
        fprintf(stdout, "\n");
        printf("\n%s\n", buffer);
        fflush(stdout);
    }
}

//..........TO HANDLE FILES INFO COMMAND OUTPUT BY SERVER................

void handle_getfiles_command(client_request* req, int client_sd){
    char buffer[5000];
    bzero(buffer,5000);
    read_data(client_sd,buffer,5000);
    printf("\n%s",buffer);
    fflush(stdout);
}

//..........TO HANDLE DOWNLOAD FILE COMMAND OUTPUT BY SERVER................

void handle_downloadfile_command(client_request* req, int client_sd){
    char buffer[1024];
    bzero(buffer,1024);
    read_data(client_sd,buffer,100);
    char filename[100];

    if(strcmp(buffer,"1")==0){

        // get number of bytes to read
        bzero(buffer,1024);
        read_data(client_sd,buffer,100);
        int count = atoi(buffer);

        //get client ID
        char client_ID[100];
        bzero(client_ID,100);
        read_data(client_sd,client_ID,100);
        int clientID = atoi(client_ID);

        // get filename
        bzero(filename,100);
        read_data(client_sd,filename,100);

        char client_filename[250];
        bzero(client_filename,250);
        snprintf(client_filename,250,"%d_%s",clientID,filename);

        // start receiving file content in chunks
        char file_buffer[BUFFER_SIZE];
        bzero(file_buffer,BUFFER_SIZE);
        int chunk = 256;
        int pos = 0;
        while(count>chunk){
            int rec_bytes = read(client_sd,file_buffer+pos,chunk);
            count -= rec_bytes;
            pos += rec_bytes;
        }
        if(count>0){
            read(client_sd,file_buffer+pos,count);
        }

        // save file content in a file
        write_to_file(client_filename,file_buffer);

        // receive successfull download message
        bzero(buffer,1024);
        read_data(client_sd,buffer,100);
        if(strcmp(buffer,"done")==0){
            bzero(buffer,1024);
            snprintf(buffer,1024,"SERVER: The file %s has been downloaded.",filename);
            printf("\n%s\n",buffer);
            fflush(stdout);
        }
    }
    else if(strcmp(buffer,"-1")==0){
        bzero(buffer,1024);
        snprintf(buffer,1024,"SERVER: SORRY!! %s does not exist.",filename);
        printf("\n%s",buffer);
        fflush(stdout);
    }
    else if(strcmp(buffer,"-2")==0){
        bzero(buffer,1024);
        snprintf(buffer,1024,"SERVER: SORRY!! You don't have collaborator permission for file %s.",filename);
        printf("\n%s\n",buffer);
        fflush(stdout);
    }
    else if(strcmp(buffer,"-3")==0){
        bzero(buffer,1024);
        sprintf(buffer,"%s","SERVER: SORRY!! An error occured while downloading.");
        printf("\n%s\n",buffer);
        fflush(stdout);
    }
}

//..........TO HANDLE READ FILE COMMAND OUTPUT BY SERVER................

void handle_readfile_command(client_request* req, int client_sd){
    char buffer[100];
    bzero(buffer,100);
    read_data(client_sd,buffer,100);


    if(strcmp(buffer,"1")==0){
        // receive total bytes from client
        int count;
        bzero(buffer,100);
        read_data(client_sd,buffer,100);
        count = atoi(buffer);

        // start receiving file content in chunks
        char file_buffer[BUFFER_SIZE];
        bzero(file_buffer,BUFFER_SIZE);
        int chunk = 256;
        int pos = 0;
        while(count>chunk){
            int rec_bytes = read(client_sd,file_buffer+pos,chunk);
            count -= rec_bytes;
            pos += rec_bytes;
        }
        if(count>0){
            read(client_sd,file_buffer+pos,count);
        }

        printf("\n%s",file_buffer);
        fflush(stdout);
        printf("\n");
        fflush(stdout);
    }
    else if(strcmp(buffer,"2")==0){
        char line[1024];
        bzero(line,1024);
        read_data(client_sd,line,1000);

        printf("\n%s",line);
        fflush(stdout);
        printf("\n");
        fflush(stdout);
    }
    else if(strcmp(buffer,"3")==0){
        char msg[BUFFER_SIZE];
        bzero(msg,BUFFER_SIZE);
        read_data(client_sd,msg,BUFFER_SIZE);

        printf("\n%s",msg);
        fflush(stdout);
        printf("\n");
        fflush(stdout);
    }
    else if(strcmp(buffer,"-1")==0){
        printf("\nSERVER: SORRY!! This file does not exist.\n");
    }
    else if(strcmp(buffer,"-2")==0){
        printf("\nSERVER: INVALID LINE NUMBERS.\n");
    }
    else if(strcmp(buffer,"-3")==0){
        printf("\nSERVER: SORRY!! You are not a collaborator in this file.\n");
    }
}

//..........TO HANDLE DELETE FILE COMMAND OUTPUT BY SERVER................

void handle_deletefile_command(client_request* req, int client_sd){
    char buffer[100];
    bzero(buffer,100);
    read_data(client_sd,buffer,50);
    if(strcmp(buffer,"2")==0){
        bzero(buffer,100);
        read_data(client_sd,buffer,50);

        int count = atoi(buffer);

        // start receiving file content in chunks
        char file_buffer[BUFFER_SIZE];
        int chunk = 256;
        int pos = 0;
        while (count > chunk){
            int rec_bytes = read(client_sd, file_buffer + pos, chunk);
            count -= rec_bytes;
            pos += rec_bytes;
        }
        if (count > 0){
            read(client_sd, file_buffer + pos, count);
        }
        printf("\n%s", file_buffer);
        fflush(stdout);
        // puts("\n");
        bzero(file_buffer, BUFFER_SIZE);
    }
    else if(strcmp(buffer,"1")==0){
        printf("\nSERVER: The file contents were deleted successfully.\n");
    }
    else if(strcmp(buffer,"-1")==0){
        printf("\nSERVER: SORRY!! This file does not exist.\n");
    }
    else if(strcmp(buffer,"-2")==0){
        printf("\nSERVER: INVALID LINE NUMBERS.\n");
    }
    else if(strcmp(buffer,"-3")==0){
        printf("\nSERVER: SORRY!! You are do not have Editor permission in this file.\n");
    }
}

//..........TO HANDLE INSERT IN FILE COMMAND OUTPUT BY SERVER................

void handle_insertfile_command(client_request* req, int client_sd){
    char buffer[100];
    bzero(buffer,100);
    read_data(client_sd,buffer,100);
    if(strcmp(buffer,"1")==0){
        bzero(buffer,100);
        read_data(client_sd,buffer,100);
        int count = atoi(buffer);
        // start receiving file content in chunks
        char file_buffer[BUFFER_SIZE];
        bzero(file_buffer, BUFFER_SIZE);
        int chunk = 256;
        int pos = 0;
        while (count > chunk){
            int rec_bytes = read(client_sd, file_buffer + pos, chunk);
            count -= rec_bytes;
            pos += rec_bytes;
        }
        if (count > 0){
            read(client_sd, file_buffer + pos, count);
        }
        printf("\n%s", file_buffer);
        fflush(stdout);
        printf("\n");
        fflush(stdout);
    }
    else if(strcmp(buffer,"-1")==0){
        printf("\nSERVER: SORRY!! This file does not exist.\n");
        fflush(stdout);
    }
    else if(strcmp(buffer,"-2")==0){
        printf("\nSERVER: INVALID LINE NUMBERS.\n");
        fflush(stdout);
    }
    else if(strcmp(buffer,"-3")==0){
        printf("\nSERVER: SORRY!! You are do not have Editor permission in this file.\n");
        fflush(stdout);
    }
}



//Create a client socket
int client_create_socket(int *client_sd){
    struct sockaddr_in server_addr;

    if((*client_sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("ERROR : socket creation failed");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(*client_sd,(struct sockaddr *)&server_addr,sizeof(struct sockaddr))<0) {
        perror("ERROR : connect failed");
        return -1;
    }
    return 0;
}


int main(){

    int client_sd = 0;
    int new_socket = 0;
    int max_fd = 0;
    fd_set readfds;


    if(client_create_socket(&client_sd) != 0) {
        error("ERROR : socket creation failed");
    }

    max_fd = client_sd;

    char initial_message[200]={0};
    read(client_sd, initial_message, sizeof(initial_message));
    if(strncmp("close",initial_message,5)==0){
        printf("SERVER: SORRY, no more clients allowed\n");
        close(client_sd);
        exit(0);
    }
    else{
        printf("%s\n",initial_message);
    }


    char client_input[200];
    char buffer[BUFFER_SIZE];

    while(1){
        printf("\nTYPE YOUR REQUEST: ");
        fflush(stdout);
        FD_ZERO(&readfds);
        FD_SET(client_sd, &readfds);
        FD_SET(STDIN_FILENO,&readfds);
        // fcntl(STDIN_FILENO,F_SETFL,O_NONBLOCK);
        int activity = select(max_fd+1,&readfds,NULL,NULL,NULL);

        if(activity <= 0) {
            error("ERROR: Select error");
        }

        if(FD_ISSET(client_sd,&readfds)){
            // to read from server while being blocked at stdin
            char invite_buffer[1000];
            bzero(invite_buffer,1000);
            read_data(client_sd,invite_buffer,100);

            if(strcmp(invite_buffer,"inv")==0){
                invite_flag = 1;
                bzero(invite_buffer,1000);
                read_data(client_sd,invite_buffer,100);
                fprintf(stdout,"\n");
                printf("\n%s\n",invite_buffer);
                fflush(stdout);
                printf("\nTO ACCEPT TYPE: /YES <sender_client_ID> (without <>)\n");
                fflush(stdout);
                printf("\nTO REJECT TYPE: /NO <sender_client_ID> (without <>)\n");
                fflush(stdout);
            }
            else if(strcmp(invite_buffer,"yes")==0){
                bzero(invite_buffer,1000);
                read_data(client_sd,invite_buffer,1000);
                fprintf(stdout,"\n");
                printf("\n%s\n",invite_buffer);
                fflush(stdout);
            }
            else if(strcmp(invite_buffer,"no")==0){
                bzero(invite_buffer,1000);
                read_data(client_sd,invite_buffer,100);
                fprintf(stdout,"\n");
                printf("\n%s\n",invite_buffer);
                fflush(stdout);
            }
        }

        if(FD_ISSET(STDIN_FILENO,&readfds)){
            // to handle the message input by client at stdin
            bzero(client_input, 200);
            fgets(client_input,200,stdin);

            client_request *req = parse_client_input(client_input);
            if (req == NULL){
                printf("\n%s\n", "ERROR!! INVALID INPUT");
                continue;
            }

            if (strlen(req->error_message) > 0){
                printf("\n%s\n", req->error_message);
                continue;
            }

            if(invite_flag == 1){
                if ((strncmp(req->instruction, "/YES", 4) == 0) || (strncmp(req->instruction, "/NO", 3) == 0)){
                    write(client_sd, client_input, strlen(client_input));
                    handle_invite_response_command(req, client_sd);
                    continue;
                }else{
                    printf("\nPlease respond to the invite\n");
                    fflush(stdout);
                    continue;
                }
            }

            if(strcmp(req->instruction,"/users")==0){
                write(client_sd,req->instruction,strlen(req->instruction));
                handle_getusers_command(client_sd);
                continue;
            }

            if(strcmp(req->instruction,"/upload")==0){
                write(client_sd,client_input,strlen(client_input));
                handle_uploadfile_command(req,client_sd);
                continue;
            }

            if(strcmp(req->instruction,"/invite")==0){
                write(client_sd,client_input,strlen(client_input));
                handle_invite_command(req,client_sd);
                continue;
            }

            if((strncmp(req->instruction,"/YES",4)==0) || (strncmp(req->instruction,"/NO",3)==0)){
                write(client_sd,client_input,strlen(client_input));
                handle_invite_response_command(req,client_sd);
                continue;
            }

            if(strcmp(req->instruction,"/files")==0){
                write(client_sd,client_input,strlen(client_input));
                handle_getfiles_command(req,client_sd);
                continue;
            }

            if(strcmp(req->instruction,"/download")==0){
                write(client_sd,client_input,strlen(client_input));
                handle_downloadfile_command(req,client_sd);
                continue;
            }

            if(strcmp(req->instruction,"/read")==0){
                write(client_sd,client_input,strlen(client_input));
                handle_readfile_command(req,client_sd);
                continue;
            }

            if(strcmp(req->instruction,"/delete")==0){
                write(client_sd,client_input,strlen(client_input));
                handle_deletefile_command(req,client_sd);
                continue;
            }
            if(strcmp(req->instruction,"/insert")==0){
                write(client_sd,client_input,strlen(client_input));
                handle_insertfile_command(req,client_sd);
                continue;
            }
            if(strcmp(req->instruction,"/exit")==0){
                write(client_sd,client_input,strlen(client_input));
                printf("\nClosing Connection....\n");
                close(client_sd);
                exit(0);
            }
        }
    }
}