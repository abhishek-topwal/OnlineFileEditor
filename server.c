#include <stdio.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>


#define SERVER_PORT 5000
#define TOTAL_CLIENTS 5
#define BACKLOG 5
#define BUFFER_SIZE 1000000
static int listen_fd = 0;

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

int get_total_bytes(FILE *file){
    fseek(file, 0, SEEK_END);
    int res = ftell(file);
    rewind(file);
    return res;
}

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
        bytes_to_write -= send_bytes;
        pos += send_bytes;
    }
    if(bytes_to_write != 0){
        write(socket,buffer+pos,bytes_to_write);
    }
}


typedef struct client_files{
    /*
        To maintain the client_ID of owner associated with a file.
        Helps in checking for duplucate file name.
    */
    bool isTaken;
    int client_ID;
    char filename[50];
}client_files;



typedef struct collaborator{
    /*
        To maintain the ID of collaborator and kind of permi ssion associated with the collaborator
    */
    bool isCollab;
    int collaborator_ID;
    char permission[10];
}collaborator;



typedef struct permission_record{
    /*
        To maintain all the permission w.r.t a file like its owner, collaborator and their respective permission
        Since the max clients can be 5, each file can have at most 4 collaborators
    */
    bool isFilled;
    char filename[50];
    int lines_count;
    int collaborator_count;
    collaborator collaborator[4];
}permission_record;


typedef struct invite_data{
    bool isInvited;
    int sender_sd;
    int sender_ID;
    int receiver_sd;
    int receiver_ID;
    int status; // 1->accepted , -1->rejected, 0->no reply
    char permission[10];
    char filename[50];
}invite_data;





typedef struct client{
    /*
        To maintain all the data w.r.t a client. Its ID, socket descriptor and its connection status.
        The server is desinged to handle at most 10 files per client. Hence each client will have 10
        permission records.
    */
    bool isConnected;
    int client_ID;
    int socket_des;
    int permission_count;
    permission_record permission_record[10];
}client_data;



typedef struct server{
    /*
        The server will maintain the status of all the clients.
        It will also maintain the owner client ID associated with a uploaded file.
    */
    int total_client;
    client_data client_list[TOTAL_CLIENTS];

    int file_count;
    client_files client_files[50];

    int invite_count;
    invite_data invite_data[50];
}server_data;


server_data server;


//..............TO WRITE FROM A BUFFER TO A FILE.........................

int write_to_file(char *FileName, char *buffer)
{
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

void handle_getusers_request(int client_sd){
    char buffer[1024];
    bzero(buffer,1024);
    sprintf(buffer,"%s","/users");
    write_data(client_sd,buffer,100);
    bzero(buffer,1024);
    for(int i=0; i<server.total_client; i++){
        char ID[10];
        if(server.client_list[i].isConnected){
            sprintf(ID,"%d",server.client_list[i].client_ID);
            strcat(buffer,ID);
            sprintf(ID,"%s"," || ");
            strcat(buffer,ID);
        }
    }
    write_data(client_sd,buffer,100);
}

//................TO HANDLE UPLOAD FILE REQUEST........................

int handle_uploadfile_request(char* client_input, int client_sd,int index){
    char instruction[50];
    char filename[50];
    sscanf(client_input,"%[^' '] %[^\n]",instruction,filename);

    for(int i=0; i<server.file_count; i++){
        if(server.client_files[i].isTaken){
            if(strcmp(server.client_files[i].filename,filename)==0){
                return -1;
            }
        }
    }

    if(server.file_count==0){
        server.file_count = server.file_count + 1;
        server.client_files[0].isTaken = true;
        sprintf(server.client_files[0].filename, "%s", filename);
        server.client_files[0].client_ID = server.client_list[index].client_ID;
    }
    else{
        server.file_count = server.file_count +1;
        for(int i=0; i<server.file_count; i++){
            if(!server.client_files[i].isTaken){
                server.file_count = server.file_count + 1;
                server.client_files[i].isTaken= true;
                sprintf(server.client_files[i].filename,"%s",filename);
                server.client_files[i].client_ID = server.client_list[index].client_ID;
                break;
            }
        }
    }

    char buffer[1024];

    // send acknowledgemnt to client
    bzero(buffer,1024);
    sprintf(buffer,"%d",1);
    write_data(client_sd,buffer,100);

    // receive total lines from the client
    int total_lines;
    bzero(buffer,1024);
    read_data(client_sd,buffer,100);
    total_lines = atoi(buffer);

    // server.client_list[index].permission_count = server.client_list[index].permission_count + 1;
    // for(int i=0; i<server.client_list[index].permission_count;i++){
    //     if(!server.client_list[index].permission_record->isFilled){
    //         server.client_list[index].permission_record->isFilled = true;
    //         sprintf(server.client_list[index].permission_record->filename,"%s",filename);
    //         server.client_list[index].permission_record->lines_count= total_lines;
    //         break;
    //     }
    // }

    for(int i=0; i<server.total_client; i++){
        if(server.client_list[i].isConnected){
            if(server.client_list[i].client_ID == server.client_list[index].client_ID){
                server.client_list[i].permission_count = server.client_list[i].permission_count + 1;
                for(int j=0; j<server.client_list[i].permission_count;j++){
                    if(!server.client_list[i].permission_record[j].isFilled){
                        server.client_list[i].permission_record[j].isFilled = true;
                        server.client_list[i].permission_record[j].lines_count = total_lines;
                        sprintf(server.client_list[i].permission_record[j].filename,"%s",filename);
                        break;
                    }
                }
            }
        }
    }

    // receive total bytes from client
    int count;
    bzero(buffer,1024);
    read_data(client_sd,buffer,100);
    count = atoi(buffer);

    // send upload command to client
    bzero(buffer,1024);
    sprintf(buffer,"%s","/upload");
    write_data(client_sd,buffer,100);

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

    // save file content in the file
    if(write_to_file(filename,file_buffer)<0){
        return -2;
    }

    // send completion message to client
    bzero(buffer,1024);
    sprintf(buffer,"%s","SERVER: Your file has been uploaded successfully");
    write_data(client_sd,buffer,100);
}


//................TO HANDLE INVITE CLIENT REQUEST......................

int handle_invite_request(char* client_input, int client_sd,int index){
    char instruction[50];
    char filename[20];
    char client_id[10];
    char permission[5];
    sscanf(client_input, "%[^' '] %[^' '] %[^' '] %[^\n]", instruction, filename, client_id, permission);

    int invite_client_ID = atoi(client_id);
    // printf("\n%d",invite_client_ID);
    // fflush(stdout);
    int sender_client_ID = server.client_list[index].client_ID;

    if(invite_client_ID == sender_client_ID){
        return -3;
    }

    // check if invite_client_ID is valid
    int flag=0;
    for(int i=0; i<server.total_client; i++){
        if(server.client_list[i].isConnected){
            if(server.client_list[i].client_ID == invite_client_ID){
                flag =1 ;
            }
        }
    }
    if(flag==0){
        return -3;
    }


    //check for if file was uploaded before or not.... or the client is the owner of file or not
    int file_flag=0;
    for(int i=0; i<server.file_count; i++){
        if(server.client_files[i].isTaken){
            if(strcmp(server.client_files[i].filename,filename)==0){
                file_flag = 1;
                if(server.client_files[i].client_ID != sender_client_ID){
                    return -1;
                }
            }
        }
    }
    if(file_flag == 0 ){
        return -2;
    }

    // send invitation to the client
    int invite_client_sd;
    for(int i=0; i<server.total_client; i++){
        if(server.client_list[i].isConnected){
            if(server.client_list[i].client_ID == invite_client_ID){
                invite_client_sd = server.client_list[i].socket_des;
            }
        }
    }


    server.invite_count = server.invite_count + 1;
    for(int i=0; i<server.invite_count ; i++){
        if(!server.invite_data[i].isInvited){
            server.invite_data[i].isInvited = true;
            server.invite_data[i].sender_ID = sender_client_ID;
            server.invite_data[i].sender_sd = client_sd;
            server.invite_data[i].receiver_ID = invite_client_ID;
            server.invite_data[i].receiver_sd = invite_client_sd;
            sprintf(server.invite_data[i].filename,"%s",filename);
            sprintf(server.invite_data[i].permission,"%s",permission);
            server.invite_data[i].status = 0;
            break;
        }
    }

    char buffer[1024];
    char permission_type[15];
    if(strcmp(permission,"V")==0){
        sprintf(permission_type,"%s","Viewer");
    }
    else{
        sprintf(permission_type,"%s","Editor");
    }

    char invite_buffer[500];
    bzero(invite_buffer,500);
    sprintf(invite_buffer,"%s","inv");
    write_data(invite_client_sd,invite_buffer,100);
    // fflush(stdout);
    bzero(invite_buffer,500);
    snprintf(invite_buffer,500,"SERVER: Invitation from client <%d> to join in the file || %s || as %s",sender_client_ID,filename,permission_type);
    write_data(invite_client_sd,invite_buffer,100);

    bzero(buffer,1024);
    sprintf(buffer,"%d",1);
    write_data(client_sd,buffer,100);
}

//................TO HANDLE INVITE RESPONSE REQUEST.....................

int handle_invite_response_request(char* client_input, int client_sd,int index){
    char instruction[20];
    char sender_client_ID[20];
    sscanf(client_input,"%[^' '] %[^\n]",instruction,sender_client_ID);

    int sender_clientID = atoi(sender_client_ID);

    int receiver_client_ID = server.client_list[index].client_ID;

    if(sender_clientID == receiver_client_ID){
        return -1;
    }

    //check if sender_clientID is valid or not
    int client_valid_flag = 0;
    for(int i=0; i<server.total_client; i++){
        if(server.client_list[i].isConnected){
            if(server.client_list[i].client_ID == sender_clientID){
                client_valid_flag = 1;
                break;
            }
        }
    }
    if(client_valid_flag==0){
        return -1;
    }

    // check if sender_clientID has sent a colloboration request before or not
    int colloboration_valid_flag=0;
    int invite_data_index;
    for(int i=0; i<server.invite_count; i++){
        if(server.invite_data[i].isInvited){
            if(server.invite_data[i].status==0){
                if(server.invite_data[i].receiver_sd == client_sd && server.invite_data[i].sender_ID == sender_clientID){
                    invite_data_index = i;
                    colloboration_valid_flag = 1;
                    break;
                }
            }
        }
    }
    if(colloboration_valid_flag==0){
        return -2;
    }

    char namefile[50];
    bzero(namefile, 50);
    sprintf(namefile, "%s", server.invite_data[invite_data_index].filename);
    char access[10];
    bzero(access, 10);
    sprintf(access, "%s", (strcmp(server.invite_data[invite_data_index].permission, "V") == 0) ? "Viewer" : "Editor");

    if(strcmp(instruction,"/YES")==0){
        server.invite_data[invite_data_index].status = 1;
        for(int i=0; i<server.total_client; i++){
            if(server.client_list[i].isConnected){
                if(server.client_list[i].client_ID == sender_clientID){
                    for(int j=0; j<server.client_list[i].permission_count; j++){
                        if(server.client_list[i].permission_record[j].isFilled){
                            if(strcmp(server.client_list[i].permission_record[j].filename,server.invite_data[invite_data_index].filename)==0){
                                server.client_list[i].permission_record[j].collaborator_count =  server.client_list[i].permission_record[j].collaborator_count +1;
                                for(int k=0; k<server.client_list[i].permission_record[j].collaborator_count; k++){
                                    if(!server.client_list[i].permission_record[j].collaborator[k].isCollab){
                                        server.client_list[i].permission_record[j].collaborator[k].isCollab = true;
                                        server.client_list[i].permission_record[j].collaborator[k].collaborator_ID = server.invite_data[invite_data_index].receiver_ID;
                                        sprintf(server.client_list[i].permission_record[j].collaborator[k].permission,"%s", server.invite_data[invite_data_index].permission);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }


        int sender_sd = server.invite_data[invite_data_index].sender_sd;
        char buffer[1000];
        bzero(buffer,1000);
        sprintf(buffer,"%s","yes");
        write_data(sender_sd,buffer,100);

        bzero(buffer,1000);
        snprintf(buffer,1000,"Your collaboration request for file || %s || as a %s has been accepted by %d.",namefile,access,server.invite_data[invite_data_index].receiver_ID);
        write_data(sender_sd,buffer,1000);

        bzero(buffer,1000);
        sprintf(buffer,"%s","2");
        write_data(client_sd,buffer,100);

        bzero(buffer,1000);
        snprintf(buffer,1000,"SERVER: You are now a %s in file || %s || with the owner %d.",access,namefile,sender_clientID);
        write_data(client_sd,buffer,1000);

        return 2;
    }
    else{
        server.invite_data[invite_data_index].status = -1;
        int sender_sd = server.invite_data[invite_data_index].sender_sd;
        char buffer[1024];
        bzero(buffer,1024);
        sprintf(buffer,"%s","no");
        write_data(sender_sd,buffer,100);

        bzero(buffer,1024);
        snprintf(buffer,1024,"Your collaboration request for file || %s || as a %s has been rejected by %d.",namefile,access,server.invite_data[invite_data_index].receiver_ID);
        write_data(sender_sd,buffer,100);
        return 1;
    }
}

//................TO HANDLE GET FILE INFO REQUEST......................

void handle_getfiles_request(char* client_input, int client_sd,int index){
    char main_buffer[5000];
    char buffer[1000];

    for(int i=0; i<server.total_client; i++){
        if(server.client_list[i].isConnected){
            char owner_info[50];
            bzero(owner_info,50);
            snprintf(owner_info,50,"Owner-%d",server.client_list[i].client_ID);

            for(int j=0; j<server.client_list[i].permission_count;j++){
                int collab_flag = 0;
                if(server.client_list[i].permission_record[j].isFilled){
                    char file_name[100];
                    char line_count[50];
                    bzero(file_name,100);
                    snprintf(file_name,100,"Filename-%s",server.client_list[i].permission_record[j].filename);

                    char namefile[100];
                    sprintf(namefile,"%s",server.client_list[i].permission_record[j].filename);
                    FILE* fp = fopen(namefile,"r+");
                    int total_lines = NLINEX(fp);
                    fclose(fp);

                    bzero(line_count,50);
                    snprintf(line_count,50,"Total Lines-%d",total_lines);

                    for(int k=0; k<server.client_list[i].permission_record[j].collaborator_count; k++){
                        if(server.client_list[i].permission_record[j].collaborator[k].isCollab){
                            collab_flag =1;
                            char collab_info[50];
                            char collab_permission[50];

                            bzero(collab_info,50);
                            snprintf(collab_info,50,"Collaborator ID-%d",server.client_list[i].permission_record[j].collaborator[k].collaborator_ID);

                            bzero(collab_permission,50);
                            snprintf(collab_permission,50,"Collaborator Permission-%s",(strcmp(server.client_list[i].permission_record[j].collaborator[k].permission,"V")==0)?"Viewer":"Editor");

                            bzero(buffer,1000);
                            snprintf(buffer,1000,"%s || %s || %s || %s || %s\n",owner_info,file_name,line_count,collab_info,collab_permission);
                            strcat(main_buffer,buffer);
                        }
                    }
                    if(collab_flag == 0){
                        bzero(buffer,1000);
                        snprintf(buffer,1000,"%s || %s || %s\n",owner_info,file_name,line_count);
                        strcat(main_buffer,buffer);
                    }
                }
            }
        }
    }

    write(client_sd,main_buffer,5000);
    bzero(main_buffer,5000);
}

//...............TO HANDLE DOWNLOAD FILE REQUEST.......................

int handle_downloadfile_request(char* client_input, int client_sd,int index){
    char instruction[20];
    char filename[100];
    sscanf(client_input,"%[^' '] %[^\n]",instruction,filename);

    printf("\n%ld",strlen(filename));
    fflush(stdout);

    //check if given file exist or not
    int file_flag =0;
    int owner_client_ID;
    for(int i=0; i<server.file_count;i++){
        if(server.client_files[i].isTaken){
            if(strcmp(server.client_files[i].filename,filename)==0){
                owner_client_ID = server.client_files[i].client_ID ;
                file_flag = 1;
                break;
            }
        }
    }
    if(file_flag == 0){
        return -1;
    }



    //get ID of client who has requested for download
    int command_client_ID;
    for(int i=0; i<server.total_client; i++){
        if(server.client_list[i].isConnected){
            if(server.client_list[i].socket_des == client_sd){
                command_client_ID = server.client_list[i].client_ID;
                break;
            }
        }
    }

    if(command_client_ID == owner_client_ID){

        char ready_msg[100];
        bzero(ready_msg,100);
        sprintf(ready_msg,"%d",1);
        write_data(client_sd,ready_msg,100);

        FILE* fp = fopen(filename,"r+");

        char buffer[100];
        int count = get_total_bytes(fp);

        //send bytes to be send
        bzero(buffer,100);
        sprintf(buffer,"%d",count);
        write_data(client_sd,buffer,100);

        //send client ID
        bzero(buffer,100);
        sprintf(buffer,"%d",command_client_ID);
        write_data(client_sd,buffer,100);


        //send filename
        bzero(buffer,100);
        sprintf(buffer,"%s",filename);
        write_data(client_sd,buffer,100);

        //send file content
        char file_buffer[BUFFER_SIZE];
        int get_count = fread(file_buffer, sizeof(char), count, fp);
        if(get_count != count){
            return -3;
        }
        int chunk = 256;
        int pos = 0;
        while (count >= chunk){
            int send_bytes = write(client_sd, file_buffer + pos, chunk);
            count -= send_bytes;
            pos += send_bytes;
        }
        if (count != 0){
            write(client_sd, file_buffer + pos, count);
        }

        return 1;
    }
    else{
        // check if the client is collaborator or not
        int collaborator_flag=0;
        for(int i=0; i<server.total_client; i++){
            if(server.client_list[i].isConnected){
                for(int j=0; j<server.client_list[i].permission_count;j++){
                    if(server.client_list[i].permission_record[j].isFilled){
                        if(strcmp(server.client_list[i].permission_record[j].filename,filename)==0){
                            for(int k=0; k<server.client_list[i].permission_record[j].collaborator_count;k++){
                                if(server.client_list[i].permission_record[j].collaborator[k].isCollab){
                                    if(server.client_list[i].permission_record[j].collaborator[k].collaborator_ID == command_client_ID){
                                        collaborator_flag = 1;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if(collaborator_flag == 0){
            // the client is not a collaborator
            return -2;
        }
        else{
            // the client is a collaborator
            char ready_msg[100];
            bzero(ready_msg,100);
            sprintf(ready_msg,"%d",1);
            write_data(client_sd,ready_msg,100);

            FILE* fp = fopen(filename,"r+");

            char buffer[100];
            int count = get_total_bytes(fp);

            //send bytes to be send
            bzero(buffer,100);
            sprintf(buffer,"%d",count);
            write_data(client_sd,buffer,100);

            //send client ID
            bzero(buffer,100);
            sprintf(buffer,"%d",command_client_ID);
            write_data(client_sd,buffer,100);

            //send filename
            bzero(buffer,100);
            sprintf(buffer,"%s",filename);
            write_data(client_sd,buffer,100);

            //send file content
            char file_buffer[BUFFER_SIZE];
            int get_count = fread(file_buffer, sizeof(char), count, fp);
            if(get_count != count){
                return -3;
            }
            int chunk = 256;
            int pos = 0;
            while (count >= chunk){
                int send_bytes = write(client_sd, file_buffer + pos, chunk);
                count -= send_bytes;
                pos += send_bytes;
            }
            if (count != 0){
                write(client_sd, file_buffer + pos, count);
            }

            return 1;
        }
    }
}

//...............HELPER FUNCTIONS FOR READ FILE REQUEST.................

void read_entire_file(char* filename, int client_sd){
    char buffer[100];
    bzero(buffer,100);
    sprintf(buffer,"%d",1);
    write_data(client_sd,buffer,100);

    FILE *fp = fopen(filename,"r+");
    int count = get_total_bytes(fp);

    bzero(buffer,100);
    sprintf(buffer,"%d",count);
    write_data(client_sd,buffer,100);

    char file_buffer[BUFFER_SIZE];
    int get_count = fread(file_buffer, sizeof(char), count, fp);
    int chunk = 256;
    int pos = 0;
    while (count >= chunk){
        int send_bytes = write(client_sd, file_buffer + pos, chunk);
        count -= send_bytes;
        pos += send_bytes;
    }
    if (count != 0){
        write(client_sd, file_buffer + pos, count);
    }
    fclose(fp);
}

int read_index_line(char* filename, int search_index,int client_sd){


    FILE *fp = fopen(filename,"r+");
    int total_lines = NLINEX(fp);

    if (search_index < (-1 * total_lines) || search_index >= total_lines){
        fclose(fp);
        return -1;
    }
    char buffer[1024];
    bzero(buffer,1024);
    sprintf(buffer,"%d",2);
    write_data(client_sd,buffer,100);

    char chr;
    char msg[1000];
    bzero(msg,1000);
    int index_reached = 0;
    if (search_index < 0){
        search_index = search_index + total_lines;
    }
    rewind(fp);
    while (index_reached != search_index){
        chr = fgetc(fp);
        if (chr == '\n'){
            index_reached += 1;
        }
    }
    int j = 0;
    chr = fgetc(fp);
    while (chr != '\n'){
        if (chr == EOF){
            break;
        }
        msg[j++] += chr;
        chr = fgetc(fp);
    }
    rewind(fp);

    bzero(buffer,1024);
    sprintf(buffer,"%s",msg);
    write_data(client_sd,buffer,1000);

    return 1;
}

int read_lines(char* filename, int begin_index,int end_index,int client_sd){

    FILE *fp = fopen(filename,"r+");
    int total_lines = NLINEX(fp);

    if (begin_index < (-1 * total_lines) || begin_index >= total_lines){
        fclose(fp);
        return -1;
    }

    if (end_index < (-1 * total_lines) || end_index >= total_lines){
        fclose(fp);
        return -1;
    }

    char chr;
    char msg[BUFFER_SIZE];
    bzero(msg,BUFFER_SIZE);
    if (begin_index < 0){
        begin_index = begin_index + total_lines;
    }

    if (end_index < 0){
        end_index = end_index + total_lines;
    }

    if(begin_index>end_index){
        fclose(fp);
        return -1;
    }

    char buffer[1024];
    bzero(buffer,1024);
    sprintf(buffer,"%d",3);
    write_data(client_sd,buffer,100);

    int required_lines = end_index - begin_index + 1;

    rewind(fp);
    int index_reached = 0;
    while (index_reached != begin_index){
        chr = fgetc(fp);
        if (chr == '\n'){
            index_reached += 1;
        }
    }

    int lines_read = 0;
    int j = 0;
    chr = fgetc(fp);
    while (lines_read != required_lines){
        if (chr == EOF){
            break;
        }
        msg[j++] += chr;
        chr = fgetc(fp);
        if(chr == '\n'){
            lines_read+= 1;
        }
    }
    rewind(fp);

    write_data(client_sd,msg,BUFFER_SIZE);
    bzero(msg,BUFFER_SIZE);

    return 1;
}

//...............TO HANDLE READ FILE REQUEST...........................

int handle_readfile_request(char* client_input, int client_sd,int index){
    char instruction[30];
    char command[200];
    char filename[100];
    char start_index[20];
    char end_index[20];
    int arg_count = sscanf(client_input,"%[^' '] %[^' '] %[^' '] %[^\n]",instruction,filename,start_index,end_index);
    if(arg_count == 2){
        sscanf(client_input,"%[^' '] %[^\n]",instruction,filename);
    }
    else if(arg_count == 3){
        sscanf(client_input,"%[^' '] %[^' '] %[^\n]",instruction,filename,start_index);
    }
    else if(arg_count == 4){
        sscanf(client_input,"%[^' '] %[^' '] %[^' '] %[^\n]",instruction,filename,start_index,end_index);
    }

    //check if given file exist or not
    int file_flag =0;
    int owner_client_ID;
    for(int i=0; i<server.file_count;i++){
        if(server.client_files[i].isTaken){
            if(strcmp(server.client_files[i].filename,filename)==0){
                owner_client_ID = server.client_files[i].client_ID ;
                file_flag = 1;
                break;
            }
        }
    }
    if(file_flag == 0){
        return -1;
    }

    //get ID of client who has requested for download
    int command_client_ID;
    for(int i=0; i<server.total_client; i++){
        if(server.client_list[i].isConnected){
            if(server.client_list[i].socket_des == client_sd){
                command_client_ID = server.client_list[i].client_ID;
                break;
            }
        }
    }

    if(command_client_ID == owner_client_ID){
        if(arg_count== 2){
            read_entire_file(filename,client_sd);
        }
        else if(arg_count == 3){
            int line_index = atoi(start_index);
            if(read_index_line(filename,line_index,client_sd)<0){
                return -2;
            }
        }
        else if(arg_count == 4){
            int begin_index = atoi(start_index);
            int last_index = atoi(end_index);
            if(begin_index == last_index){
                if(read_index_line(filename,begin_index,client_sd)<0){
                    return -2;
                }
            }
            else{
                if(read_lines(filename,begin_index,last_index,client_sd)<0){
                    return -2;
                }
            }
        }
        return 1;
    }
    else{
        // check if the client is collaborator or not
        int collaborator_flag=0;
        for(int i=0; i<server.total_client; i++){
            if(server.client_list[i].isConnected){
                for(int j=0; j<server.client_list[i].permission_count;j++){
                    if(server.client_list[i].permission_record[j].isFilled){
                        if(strcmp(server.client_list[i].permission_record[j].filename,filename)==0){
                            for(int k=0; k<server.client_list[i].permission_record[j].collaborator_count;k++){
                                if(server.client_list[i].permission_record[j].collaborator[k].isCollab){
                                    if(server.client_list[i].permission_record[j].collaborator[k].collaborator_ID == command_client_ID){
                                        collaborator_flag = 1;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if(collaborator_flag == 0){
            // the client is not a collaborator
            return -3;
        }
        else
        {
            if (arg_count == 2){
                read_entire_file(filename, client_sd);
            }
            else if (arg_count == 3){
                int line_index = atoi(start_index);
                if (read_index_line(filename, line_index, client_sd) < 0){
                    return -2;
                }
            }
            else if (arg_count == 4){
                int begin_index = atoi(start_index);
                int last_index = atoi(end_index);
                if (begin_index == last_index){
                    if (read_index_line(filename, begin_index, client_sd) < 0){
                        return -2;
                    }
                }
                else{
                    if (read_lines(filename, begin_index, last_index, client_sd) < 0){
                        return -2;
                    }
                }
            }
            return 1;
        }
    }
}

//................HELPER FUNCTION FOR DELETE FILE REQUEST...............

void delete_file_content(char* filename,int client_sd,int start_index,int end_index,int flag){
    // FILE *temp_file = tmpfile();// create a temporary file
    FILE *temp_file = fopen("temp_file.txt", "w+");
    FILE *file = fopen(filename, "r+");
    int count = 0;
    char file_buff[1000];
    bzero(file_buff, 1000);
    int total_lines = NLINEX(file);
    rewind(file);
    while (count < total_lines){
        if (count >= 0 && count < start_index){
            fgets(file_buff, 1000, file);
            fputs(file_buff, temp_file);
            bzero(file_buff, 1000);
            count++;
        }
        else if (count > end_index && count < total_lines){
            fgets(file_buff, 1000, file);
            fputs(file_buff, temp_file);
            bzero(file_buff, 1000);
            count++;
        }
        else{
            fgets(file_buff, 1000, file);
            bzero(file_buff, 1000);
            count++;
        }
    }

    file = freopen(filename, "w+", file);
    rewind(file);
    rewind(temp_file);
    bzero(file_buff, 1000);
    while (!feof(temp_file)){
        fgets(file_buff, 1000, temp_file);
        fputs(file_buff, file);
        bzero(file_buff, 1000);
    }
    fclose(temp_file);
    remove("temp_file.txt");
    rewind(file);

    if(flag==0){
        fclose(file);
        char resp[50];
        bzero(resp,50);
        sprintf(resp,"%d",1);
        write_data(client_sd,resp,50);
    }
    else{
        char resp[50];
        bzero(resp,50);
        sprintf(resp,"%d",2);
        write_data(client_sd,resp,50);

        char file_buffer[BUFFER_SIZE];
        int byte_count = get_total_bytes(file);

        //send byte_count
        char sendbytes[50];
        bzero(sendbytes,50);
        sprintf(sendbytes,"%d",byte_count);
        write_data(client_sd,sendbytes,50);

        int get_count = fread(file_buffer, sizeof(char), byte_count, file);
        int chunk = 256;
        int pos = 0;
        while (byte_count >= chunk){
            int send_bytes = write(client_sd, file_buffer + pos, chunk);
            byte_count -= send_bytes;
            pos += send_bytes;
        }
        if (byte_count != 0){
            write(client_sd, file_buffer + pos, byte_count);
        }
        fclose(file);
    }
}

//...............TO HANDLE DELETE FILE REQUEST............................

int handle_deletefile_request(char* client_input, int client_sd,int index){
    char instruction[30];
    char command[200];
    char filename[100];
    char start_index[20];
    char end_index[20];
    int arg_count = sscanf(client_input,"%[^' '] %[^' '] %[^' '] %[^\n]",instruction,filename,start_index,end_index);
    if(arg_count == 2){
        sscanf(client_input,"%[^' '] %[^\n]",instruction,filename);
    }
    else if(arg_count == 3){
        sscanf(client_input,"%[^' '] %[^' '] %[^\n]",instruction,filename,start_index);
    }
    else if(arg_count == 4){
        sscanf(client_input,"%[^' '] %[^' '] %[^' '] %[^\n]",instruction,filename,start_index,end_index);
    }

    //check if given file exist or not
    int file_flag =0;
    int owner_client_ID;
    for(int i=0; i<server.file_count;i++){
        if(server.client_files[i].isTaken){
            if(strcmp(server.client_files[i].filename,filename)==0){
                owner_client_ID = server.client_files[i].client_ID ;
                file_flag = 1;
                break;
            }
        }
    }
    if(file_flag == 0){
        return -1;
    }

    //get ID of client who has requested for delete
    int command_client_ID;
    for(int i=0; i<server.total_client; i++){
        if(server.client_list[i].isConnected){
            if(server.client_list[i].socket_des == client_sd){
                command_client_ID = server.client_list[i].client_ID;
                break;
            }
        }
    }

    FILE *fp = fopen(filename, "r+");
    int total_lines = NLINEX(fp);
    fclose(fp);

    if(command_client_ID == owner_client_ID){
        if(arg_count==2){
            delete_file_content(filename,client_sd,0,total_lines-1,0);
            return 1;
        }
        else if(arg_count==3){
            int begin_index = atoi(start_index);
            if (begin_index < (-1 * total_lines) || begin_index >= total_lines){
                return -2;
            }
            if (begin_index < 0){
                begin_index = begin_index + total_lines;
            }
            delete_file_content(filename,client_sd,begin_index,begin_index,1);
            return 1;
        }
        else if(arg_count==4){
            int begin_index = atoi(start_index);
            int last_index = atoi(end_index);
            if (begin_index < (-1 * total_lines) || begin_index >= total_lines){
                return -2;
            }
            if (last_index < (-1 * total_lines) || last_index >= total_lines){
                return -2;
            }
            if (begin_index < 0){
                begin_index = begin_index + total_lines;
            }
            if (last_index < 0){
                last_index = last_index + total_lines;
            }
            if(begin_index>last_index){
                return -2;
            }
            delete_file_content(filename,client_sd,begin_index,last_index,1);
            return 1;
        }
    }
    else{
        // check if the client is collaborator or not
        int collaborator_flag=0;
        for(int i=0; i<server.total_client; i++){
            if(server.client_list[i].isConnected){
                for(int j=0; j<server.client_list[i].permission_count;j++){
                    if(server.client_list[i].permission_record[j].isFilled){
                        if(strcmp(server.client_list[i].permission_record[j].filename,filename)==0){
                            for(int k=0; k<server.client_list[i].permission_record[j].collaborator_count;k++){
                                if(server.client_list[i].permission_record[j].collaborator[k].isCollab){
                                    if(server.client_list[i].permission_record[j].collaborator[k].collaborator_ID == command_client_ID){
                                        if(strcmp(server.client_list[i].permission_record[j].collaborator[k].permission,"E")==0){
                                            collaborator_flag = 1;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        if(collaborator_flag==0){
            return -3;
        }
        else{
            if(arg_count==2){
                delete_file_content(filename,client_sd,0,total_lines-1,0);
                return 1;
            }
            else if(arg_count==3){
                int begin_index = atoi(start_index);
                if (begin_index < (-1 * total_lines) || begin_index >= total_lines){
                    return -2;
                }
                if (begin_index < 0){
                    begin_index = begin_index + total_lines;
                }
                delete_file_content(filename,client_sd,begin_index,begin_index,1);
                return 1;
            }
            else if(arg_count==4){
                int begin_index = atoi(start_index);
                int last_index = atoi(end_index);
                if (begin_index < (-1 * total_lines) || begin_index >= total_lines){
                    return -2;
                }
                if (last_index < (-1 * total_lines) || last_index >= total_lines){
                    return -2;
                }
                if (begin_index < 0){
                    begin_index = begin_index + total_lines;
                }
                if (last_index < 0){
                    last_index = last_index + total_lines;
                }
                if(begin_index>last_index){
                    return -2;
                }
                delete_file_content(filename,client_sd,begin_index,last_index,1);
                return 1;
            }
        }
    }
}

//...............HELPER FUNCTION FOR INSERT IN FILE REQUEST.............................

void insert_file_content(char* filename,int client_sd,int insert_index, FILE* temp_file,int flag){
    FILE* fp = fopen(filename,"r+");
    FILE *pfile = tmpfile();// create a temporary file

    int file_count = NLINEX(fp);

    char file_buff[1000];
    bzero(file_buff, 1000);

    if(flag ==0){
        int count = NLINEX(temp_file);
        fseek(fp, 0, SEEK_END);
        if(file_count!=0){
            fputc('\n', fp);
        }
        while(count!=0){
            fgets(file_buff, 1000, temp_file);
            fputs(file_buff, fp);
            bzero(file_buff, 1000);
            count--;
        }
        rewind(fp);
    }
    else{
        int current_index = -1;
        char chr;

        while (current_index != insert_index - 1){
            // go to the index to be inserted
            fgets(file_buff, 1000, fp);
            fputs(file_buff, pfile);
            bzero(file_buff, 1000);
            current_index += 1;
        }
        int count = NLINEX(temp_file);
        while(count!=0){
            fgets(file_buff, 1000, temp_file);
            fputs(file_buff, pfile);
            bzero(file_buff, 1000);
            count--;
        }
        fputs("\n", pfile);
        int rem_count = file_count - (insert_index-1);
        while (rem_count!=0){
            // store content after current position in a current file
            fgets(file_buff, 1000, fp);
            fputs(file_buff, pfile);
            bzero(file_buff, 1000);
            rem_count--;
        }
        rewind(pfile);
        rewind(fp);

        int temp_file_lines = NLINEX(pfile);
        while(temp_file_lines!=0){
            fgets(file_buff, 1000, pfile);
            fputs(file_buff, fp);
            bzero(file_buff, 1000);
            temp_file_lines--;
        }
        rewind(fp);
    }

    int total_bytes = get_total_bytes(fp);

    //send response
    char buffer[100];
    bzero(buffer,100);
    sprintf(buffer,"%d",1);
    write_data(client_sd,buffer,100);

    //send bytes
    bzero(buffer,100);
    sprintf(buffer,"%d",total_bytes);
    write_data(client_sd,buffer,100);

    char file_buffer[BUFFER_SIZE];
    bzero(file_buffer, BUFFER_SIZE);
    int count = fread(file_buffer, sizeof(char), total_bytes, fp);

    // send file in chunks
    int chunk = 256;
    int pos = 0;
    while (count >= chunk){
        int send_bytes = write(client_sd, file_buffer + pos, chunk);
        count -= send_bytes;
        pos += send_bytes;
    }
    if (count != 0){
        write(client_sd, file_buffer + pos, count);
    }
}

//...............TO HANDLE INSERT IN FILE REQUEST ................................

int handle_insertfile_request(char* client_input, int client_sd,int index){
    char instruction[30];
    char filename[100];
    char insertindex[20];
    char message[1000];
    int arg_count = sscanf(client_input,"%[^' '] %[^' '] %[^' '] %[^\n]",instruction,filename,insertindex,message);
    if(arg_count == 3){
        sscanf(client_input,"%[^' '] %[^' '] %[^\n]",instruction,filename,message);
    }
    else if(arg_count == 4){
        sscanf(client_input,"%[^' '] %[^' '] %[^' '] %[^\n]",instruction,filename,insertindex,message);
    }

    //check if given file exist or not
    int file_flag =0;
    int owner_client_ID;
    for(int i=0; i<server.file_count;i++){
        if(server.client_files[i].isTaken){
            if(strcmp(server.client_files[i].filename,filename)==0){
                owner_client_ID = server.client_files[i].client_ID ;
                file_flag = 1;
                break;
            }
        }
    }
    if(file_flag == 0){
        return -1;
    }

    //check if index is valid
    FILE* fp = fopen(filename,"r+");
    int total_lines = NLINEX(fp);
    fclose(fp);

    char client_message[1000];
    sscanf(message,"\"%[^\"]\"",client_message);

    int msg_len = strlen(client_message);
    char final_message[1000];
    FILE *temp_file = fopen("temp_file.txt", "w+");
    int i=0;
    while(i<msg_len){
        if(client_message[i]==92 && client_message[i+1]=='n'){
            fputc('\n',temp_file);
            i += 2;
        }
        else{
            fputc(client_message[i],temp_file);
            i++;
        }
    }
    rewind(temp_file);

    //get ID of client who has requested for insert
    int command_client_ID;
    for(int i=0; i<server.total_client; i++){
        if(server.client_list[i].isConnected){
            if(server.client_list[i].socket_des == client_sd){
                command_client_ID = server.client_list[i].client_ID;
                break;
            }
        }
    }

    if(command_client_ID == owner_client_ID){
        if(arg_count==3){
            insert_file_content(filename,client_sd,total_lines-1,temp_file,0);
            return 1;
        }
        else if(arg_count==4){
            int insert_index = atoi(insertindex);
            if (insert_index < (-1 * total_lines) || insert_index >= total_lines){
                return -2;
            }
            if(insert_index<0){
                insert_index = insert_index + total_lines;
            }
            insert_file_content(filename,client_sd,insert_index,temp_file,1);
            return 1;
        }
    }
    else{
        // check if the client is collaborator or not
        int collaborator_flag=0;
        for(int i=0; i<server.total_client; i++){
            if(server.client_list[i].isConnected){
                for(int j=0; j<server.client_list[i].permission_count;j++){
                    if(server.client_list[i].permission_record[j].isFilled){
                        if(strcmp(server.client_list[i].permission_record[j].filename,filename)==0){
                            for(int k=0; k<server.client_list[i].permission_record[j].collaborator_count;k++){
                                if(server.client_list[i].permission_record[j].collaborator[k].isCollab){
                                    if(server.client_list[i].permission_record[j].collaborator[k].collaborator_ID == command_client_ID){
                                        if(strcmp(server.client_list[i].permission_record[j].collaborator[k].permission,"E")==0){
                                            collaborator_flag = 1;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        if(collaborator_flag==0){
            return -3;
        }
        else{
            if(arg_count==3){
                insert_file_content(filename,client_sd,total_lines-1,temp_file,0);
                return 1;
            }
            else if(arg_count==4){
                int insert_index = atoi(insertindex);
                if (insert_index < (-1 * total_lines) || insert_index >= total_lines){
                    return -2;
                }
                if(insert_index<0){
                    insert_index = insert_index + total_lines;
                }
                insert_file_content(filename,client_sd,insert_index,temp_file,1);
                return 1;
            }
        }
    }
}

//...............TO HANDLE EXIT REQUEST.......................................

void handle_exit_request(char* client_input, int client_sd,int index){
    //get ID of client who has requested for exit
    char namefile[50];
    int exit_client_ID;

    // remove the contents of the exit client
    for(int i=0; i<server.total_client; i++){
        if(server.client_list[i].isConnected){
            if(server.client_list[i].socket_des == client_sd){
                exit_client_ID = server.client_list[i].client_ID;
                for(int j=0; j<server.client_list[i].permission_count;j++){
                    if(server.client_list[i].permission_record[j].isFilled){
                        bzero(namefile,50);
                        sprintf(namefile,"%s",server.client_list[i].permission_record[j].filename);
                        remove(namefile);
                        server.client_list[i].permission_record[j].isFilled = false;
                        server.client_list[i].permission_count = server.client_list[i].permission_count-1;
                    }
                }
                server.client_list[i].isConnected = false;
                server.total_client = server.total_client - 1;
            }
        }
    }


    // also remove the colloborations of the exit client
    for(int i=0; i<server.total_client;i++){
        if(server.client_list[i].isConnected){
            for(int j=0; j<server.client_list[i].permission_count;j++){
                if(server.client_list[i].permission_record[j].isFilled){
                    for(int k=0;k<server.client_list[j].permission_record[k].collaborator_count;k++){
                        if(server.client_list[i].permission_record[j].collaborator[k].collaborator_ID == exit_client_ID){
                            server.client_list[i].permission_record[j].collaborator[k].isCollab = false;
                            server.client_list[i].permission_record[j].collaborator_count = server.client_list[i].permission_record[j].collaborator_count -1;
                        }
                    }
                }
            }

        }
    }

    // remove the entries of the file owned by the exit client in client_files
    for(int i=0; i<server.file_count;i++){
        if(server.client_files[i].isTaken){
            if(server.client_files[i].client_ID == exit_client_ID){
                // bzero(namefile, 50);
                // sprintf(namefile, "%s", server.client_files[i].filename);
                // remove(namefile);
                server.client_files[i].isTaken = false;
                server.file_count = server.file_count - 1;
            }
        }
    }


    for(int i=0 ;i<server.invite_count; i++){
        if(server.invite_data[i].isInvited){
            if(server.invite_data[i].sender_ID == exit_client_ID){
                server.invite_data[i].isInvited = false;
                server.invite_count = server.invite_count - 1;
            }
        }
    }

    printf("\n CLIENT %d is disconnected.\n",index);
}


//..............TO HANDLE CLIENT INPUT.........................................

void handle_client_input(char* client_input,int client_sd,int index){
    if(strncmp(client_input,"/users",5)==0){
        handle_getusers_request(client_sd);
    }
    else{
        if(strncmp(client_input,"/upload",7)==0){
            if(handle_uploadfile_request(client_input, client_sd,index)<0){
                char buffer[1024];
                bzero(buffer,1024);
                sprintf(buffer,"%d",-1);
                write(client_sd,buffer,strlen(buffer));
            }
        }
        else{
            if(strncmp(client_input,"/invite",7)==0){
                int response = handle_invite_request(client_input, client_sd,index);
                char buffer[1024];
                if(response<0){
                    if(response == -1){
                        // invite sender client is not the owner of the file
                        bzero(buffer,1024);
                        sprintf(buffer,"%d",-1);
                        write(client_sd,buffer,strlen(buffer));
                    }
                    if(response == -2){
                        // case when the file was not uploaded before
                        bzero(buffer,1024);
                        sprintf(buffer,"%d",-2);
                        write(client_sd,buffer,strlen(buffer));
                    }

                    if(response== -3){
                        // case when the given client ID is not valid
                        bzero(buffer,1024);
                        sprintf(buffer,"%d",-3);
                        write(client_sd,buffer,strlen(buffer));

                    }
                }
            }
            else{
                if((strncmp(client_input,"/YES",4)==0) || (strncmp(client_input,"/NO",3)==0)){
                    int response = handle_invite_response_request(client_input,client_sd,index);
                    char buffer[1024];
                    if(response == 1){
                        // the response was handled properly
                        bzero(buffer,1024);
                        sprintf(buffer,"%d",1);
                        write_data(client_sd,buffer,100);
                    }
                    if(response == -1){
                        // the client ID was not valid
                        bzero(buffer,1024);
                        sprintf(buffer,"%d",-1);
                        write_data(client_sd,buffer,100);
                    }

                    if(response == -2){
                        // the client was never asked for collaboration
                        bzero(buffer,1024);
                        sprintf(buffer,"%d",-2);
                        write_data(client_sd,buffer,100);
                    }
                }
                else{
                    if(strncmp(client_input,"/files",6)==0){
                        handle_getfiles_request(client_input,client_sd,index);
                    }
                    else{
                        if(strncmp(client_input,"/download",9)==0){
                            int response = handle_downloadfile_request(client_input,client_sd,index);
                            char buffer[1024];
                            if(response == 1){
                                //send completion message
                                bzero(buffer,100);
                                sprintf(buffer,"%s","done");
                                write_data(client_sd,buffer,100);
                            }

                            if(response == -1){
                                // file does not exist
                                bzero(buffer,100);
                                sprintf(buffer,"%d",-1);
                                write_data(client_sd,buffer,100);
                            }
                            if(response == -2){
                                // No collaboration access
                                bzero(buffer,100);
                                sprintf(buffer,"%d",-2);
                                write_data(client_sd,buffer,100);
                            }

                            if(response == -3){
                                // Reading Error
                                bzero(buffer,100);
                                sprintf(buffer,"%d",-3);
                                write_data(client_sd,buffer,100);
                            }
                        }
                        else{
                            if(strncmp(client_input,"/read",5)==0){
                                int response = handle_readfile_request(client_input,client_sd,index);
                                char buffer[1024];
                                if(response == -1){
                                    // file does not exist
                                    bzero(buffer,100);
                                    sprintf(buffer,"%d",-1);
                                    write_data(client_sd,buffer,100);
                                }

                                if(response == -2){
                                    // invalid line numbers
                                    bzero(buffer,100);
                                    sprintf(buffer,"%d",-2);
                                    write_data(client_sd,buffer,100);
                                }
                                if(response == -3){
                                    // You are not a collaborator
                                    bzero(buffer,100);
                                    sprintf(buffer,"%d",-3);
                                    write_data(client_sd,buffer,100);
                                }
                            }
                            else{
                                if(strncmp(client_input,"/delete",7)==0){
                                    int response = handle_deletefile_request(client_input,client_sd,index);
                                    char buffer[1024];
                                    if(response == -1){
                                        // file does not exist
                                        bzero(buffer,100);
                                        sprintf(buffer,"%d",-1);
                                        write_data(client_sd,buffer,50);
                                    }

                                    if(response == -2){
                                        // invalid line numbers
                                        bzero(buffer,100);
                                        sprintf(buffer,"%d",-2);
                                        write_data(client_sd,buffer,50);
                                    }
                                    if(response == -3){
                                        // You are not a collaborator
                                        bzero(buffer,100);
                                        sprintf(buffer,"%d",-3);
                                        write_data(client_sd,buffer,50);
                                    }
                                }
                                else{
                                    if(strncmp(client_input,"/insert",7)==0){
                                        int response = handle_insertfile_request(client_input,client_sd,index);
                                        char buffer[1024];
                                        if(response == -1){
                                            // file does not exist
                                            bzero(buffer,100);
                                            sprintf(buffer,"%d",-1);
                                            write_data(client_sd,buffer,100);
                                        }

                                        if(response == -2){
                                            // invalid line numbers
                                            bzero(buffer,100);
                                            sprintf(buffer,"%d",-2);
                                            write_data(client_sd,buffer,100);
                                        }
                                        if(response == -3){
                                            // You are not a collaborator
                                            bzero(buffer,100);
                                            sprintf(buffer,"%d",-3);
                                            write_data(client_sd,buffer,100);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}



//..................TO HANDLE A NEW CONNECTION.........................
int ID_count;
int handle_new_client(int listen_fd) {
    int new_socket_fd = 0;
    struct sockaddr_in client_addr;
    int len = sizeof(struct sockaddr);
    bzero(&client_addr, sizeof(client_addr));
    if ((new_socket_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &len)) < 0){
        error("ERROR : accept failed\n");
    }
    server.total_client = server.total_client + 1;
    if (server.total_client > TOTAL_CLIENTS){
        /*** send exceeding message***/
        server.total_client = server.total_client - 1;
        char close_command[20] = {0};
        snprintf(close_command, 20, "%s", "close");
        int n = write(new_socket_fd, close_command, strlen(close_command)); // writes message to the socket descriptor
        if (n < 0){
            error("SERVER: Error on writing\n");
        }
    }
    else{
        char welcome_mesg[200] = {0};
        for (int i = 0; i < server.total_client; i++){
            if (!server.client_list[i].isConnected){
                printf("\n[CLIENT %d] is connected\n", i);
                server.client_list[i].isConnected = true;
                server.client_list[i].client_ID= 12345+ID_count++;
                server.client_list[i].socket_des = new_socket_fd;
                bzero(welcome_mesg,200);
                snprintf(welcome_mesg, 200, "Welcome to Online File Editor !!! \nYour ID is %d ",server.client_list[i].client_ID);
                break;
            }
        }
        write(new_socket_fd, welcome_mesg, strlen(welcome_mesg));
    }
    return new_socket_fd;
}



//....................TO CREATE THE MASTER SOCKET........................
int server_create_socket(int *listen_fd){
    struct sockaddr_in server_addr;
    if((*listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("ERROR : socket creation failed");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(*listen_fd,(struct sockaddr*)&server_addr,sizeof(struct sockaddr))<0){
         perror("ERROR : socket bind failed\n");
         return -1;
    }

    if(listen(*listen_fd, BACKLOG )<0) {
         perror("ERROR : socket listen failed\n");
         return -1;
    }
    return 0;
}


int main(){

    int new_socket = 0;

    //set of socket descriptors
    fd_set readfds;

    int max_fd= 0;

    memset(&server,0,sizeof(server_data));
    printf(" SERVER STARTED SUCCESSFULLY !!!\n");

    if(server_create_socket(&listen_fd) != 0) {
        error("ERROR : creation socket failed\n");
    }

    max_fd = listen_fd;
    char buffer[BUFFER_SIZE];
    char client_input[100];
    int n;
    int loop_count;

    while(1) {

        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);

        for(int i = 0; i<server.total_client; i++) {
            if(server.client_list[i].isConnected){
                FD_SET(server.client_list[i].socket_des,&readfds);
            }
        }

        int activity = select(max_fd+1,&readfds,NULL,NULL,NULL);

        if(activity <= 0) {
            error("ERROR: Select error");
        }

        //check the server listenfd
        if(FD_ISSET(listen_fd,&readfds)) {
            int new_fd = handle_new_client(listen_fd);
            if(new_fd> max_fd){
                max_fd = new_fd;
            }
        }


        for(int i = 0; i<server.total_client; i++) {
            if(FD_ISSET(server.client_list[i].socket_des,&readfds)){
                int client_sd= server.client_list[i].socket_des;
                // take input from client
                bzero(client_input, sizeof(client_input));
                read(client_sd,client_input,sizeof(client_input));
                printf("\nCLIENT %d: %s\n",i,client_input);

                if (strncmp(client_input, "/exit", 5) == 0){
                    handle_exit_request(client_input, client_sd, i);
                    FD_CLR(client_sd, &readfds);
                    continue;
                }

                handle_client_input(client_input, client_sd,i);
            }
        }
    }
}












