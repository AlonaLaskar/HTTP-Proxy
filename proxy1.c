#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
typedef struct{
    int  port;
    char hostName[100];
    char path[200];
}url_Details;
void splitTheUrl(url_Details* det,char *url){//Divides the url into hostName, port, path
    char* token;
    token= strtok(url, "/");//Start reading from the 7th place in the string that skip the "http: //"
    strcpy(det->hostName,token);
    token= strtok(NULL,"\0");
    strcpy(det->path, "/");
    if (token != NULL) {
        strcat(det->path, token);
    }
    int flag=0;//If we find a colon then there is a port and we will light the flag
    for(int i=0;i< strlen(det->hostName);i++){
        if(det->hostName[i]==':'){
            flag=1;
            char *porti=strtok(det->hostName,":");
            porti= strtok(NULL,"/");
            det->port=atoi(porti);
        }
    }


    if(flag==0) {//No port entered
        det->port = 80;
    }

}

int createSocket(url_Details det){
    struct hostent *hp; /*ptr to host info for remote*/
    struct sockaddr_in peeraddr;
    peeraddr.sin_family = AF_INET;
    hp = gethostbyname(det.hostName);
    if(hp==NULL){//cheak if gethostbynameis Succeeded
        herror("gethostbyname");
        exit(1);
    }
    peeraddr.sin_addr.s_addr = ((struct in_addr*)(hp->h_addr))->s_addr;

    int fd;		/* socket descriptor */
    if((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    peeraddr.sin_port = htons(det.port);

    if(connect(fd, (struct sockaddr*) &peeraddr, sizeof(peeraddr)) < 0) {//connection to socket
        perror("connect");
        exit(1);
    }
    return fd;
}

void createDirs(char* str) {//creat dirs
    char *lastSlash=strrchr(str,'/');//Find the last "/" and returns a pointer to its position
    char untilLastSlash[100];
    memset(untilLastSlash, 0, 100);//Resets the lastSlash array
    strncpy(untilLastSlash, str, lastSlash-str);
    char last[100];
    memset(last, 0, 100);
    char *token= strtok(untilLastSlash,"/");
    while (token !=NULL){
        strcat(last, token);
        strcat(last, "/");
        mkdir(last, 0777);
        token = strtok(NULL, "/");
    }
}

int main(int argc,char* argv[])
{
    if (argc != 2)
    {
        printf("Usage: proxy1 <URL>\n");
        exit(1);
    }
    url_Details det;
    memset(&det, 0, sizeof(det));
    char* url = argv[1];
    splitTheUrl(&det, url + 7);
    //printf("the hostName: %s\n",det.hostName);
    //printf("the port is: %d\n",det.port);
    //printf("the path is: %s\n",det.path);

    char host_and_path[300];
    memset(host_and_path, 0, 300); // reset array
    memcpy(host_and_path,det.hostName, strlen(det.hostName));//Copy block of memory
    char *host_and_path_pointer=host_and_path;//the array whit hostName and path only(without port)
    memcpy(host_and_path_pointer+ strlen(det.hostName),det.path, strlen(det.path));//Unifies hostName + path
    for (int i = 0; i < strlen(host_and_path_pointer); ++i) {
        if(i== strlen(host_and_path_pointer)-1 && host_and_path_pointer[i]=='/'){//
            strcat(host_and_path_pointer,"index.html");
        }
    }

    FILE *fd=fopen(host_and_path_pointer,"r");
    if(fd !=NULL){//if the file is exist
        fseek(fd, 0L, SEEK_END);
        int file_size = ftell(fd);
        rewind(fd);
        printf("File is given from local filesystem\n"
               "HTTP/1.0 200 OK\r\n"
               "Content-Length: %d\r\n\r\n",file_size);
        char str[50];
        memset(str, 0, 50);
        while(fread(str,1,50,fd)!=0){//read all the file
            printf("%s",str);
            memset(str, 0, 50);
        }

        int total_size=37+file_size+4;
        printf("\n Total response bytes: %d",total_size);
        fclose(fd);

    }
    else{//is the file is not exist
        int sd =createSocket(det);//socket descriptor
        char request[400];//buffer request
        memset(request,0,400);
        sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", det.path, det.hostName);//request

        printf("HTTP request =\n%s\nLEN = %d\n", request, (int)strlen(request));

        if (write(sd, request, 400) < 0) {
            perror("write error");
            exit(1);
        }

        unsigned char buff[100];
        int flag=0;
        int startWrite = 0;
        FILE *fd2 = NULL;
        int howMuchRead = 0;
        int readAmount = 0;
        memset(buff,0,100);
        while ((readAmount = read(sd,buff,100-1))>0){//Read to the end of the file
            howMuchRead += readAmount;
            printf("%s", buff);
            if (flag == 0 && strstr((char*) buff, "200 OK")) {
                flag = 1;
                createDirs(host_and_path_pointer);
                fd2 = fopen(host_and_path_pointer, "w");
                if (fd2 == NULL) {
                    perror("fopen");
                    exit(1);
                }
            }
            char* afterRnRn = strstr((char*) buff, "\r\n\r\n");//After the string / r / n/ r /n will start writing to the file
            if (fd2 != NULL && startWrite == 0 && afterRnRn != NULL) {
                startWrite = 1;
                int index = ((unsigned char*) afterRnRn) - buff + 4;
                fwrite(buff + index, 1, readAmount-index, fd2);
            } else if (startWrite == 1 && fd2 != NULL) {
                fwrite(buff, 1, readAmount, fd2);
            }
            memset(buff,0,100);
        }

        printf("\n Total response bytes: %d",howMuchRead);

        if (fd2 != NULL) {
            fclose(fd2);
        }

        close(sd);

    }


    return 0;
}
//http://www.alona/bar/orel   good
//https://www.ynet.co.il/   exit code 139 (interrupted by signal 11: SIGSEGV)
//http://www.ynet.com    Produces the folder and prints nothing,Writes in Chinese.
//http://www.ynet.com/alona/orel/   Process finished with exit code 139 (interrupted by signal 11: SIGSEGV)
//http://www.google.com  Produces the folder and prints nothing, exit code 0
//http://www.google.com (second time)
//File is given from local filesystem
//HTTP/1.0 200 OK
//Content-Length: 53800
//
//
//Process finished with exit code 139 (interrupted by signal 11: SIGSEGV)


