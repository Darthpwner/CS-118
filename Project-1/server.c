/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */

#include <string.h>
#include <sys/stat.h>

void error(char *msg)
{
    perror(msg);
    exit(1);
}

//Parses the HTTP request from the browser
/*void parse(HTTPRequest x)*/

//Read the first request of file name, use fstream to get the file name
   //After getting the file name, I have to check if I have the file or not
    //If I have a file, I have to make a response messaage
   //If the server doesn't have the file, it should generate the 404 error message
   //Once I build the response message, I can use the write function to send it to the client
   //Should show request file on the screen of the file 
   //Follow Week 2 discussion
   //Only make changes to server.c code

//Parse HTTP request message
   //browser variable should could contain the message request header, so I can get some info

//Get the file name here
void parse(int browser) {

  int n;
  char buffer[512]; 
  char file[512];
  
  memset(buffer, 0, sizeof(buffer));  //Fill the buffer in with 0's
  memset(file, 0, sizeof(file));  //Fill the file in with 0's

  //1) Read 1st request of file name, use fstream to get file name
  //FILE *infile = fopen(browser, "r");
  n = read(browser, buffer, 255);
  if(n < 0) {
    error("ERROR reading from socket");
  }

  printf("Message: %s\n", buffer);
  //2) After getting file name, check if I have the file or not
  char* start = strstr(buffer, "GET /");
  if(start == buffer) {
    start += 5;
  } else {
    write(browser, "HTTP/1.1 ", 9);
    write(browser, "500 Internal Error\n", 17);
    error("ERROR request type is not supported");
    return;
  }
  //3) If you have the file, make a response message
  char* end = strstr(start, " HTTP/");
  int length = end - start;
  strncpy(file, start, length);
  file[length] = '\0';
  printf("File: %s\n", file);

  write(browser, "HTTP/1.1", 9);

  //4) Otherwise, generate the 404 error message
  struct stat b;
  if(length <= 0 || stat(file, &b) != 0) {
    printf("404: File Not Found");
    write(browser, "404 Not Found\n", 14);
    write(browser, "Content-Language: en-US\n", 24);
    write(browser, "Content-Length: 0\n", 18);
    write(browser, "Content-Type: text/html\n", 22);
    write(browser, "Connection: Close\n\n", 19);
    return;
  }

  //File found
  write(browser, "200 OK\n", 5);
  write(browser, "Content-Language: en-US\n", 24);
  FILE *infile = fopen(file, "r"); 

  //Get size of the file
  fseek(infile, 0L, SEEK_END);
  int fSize = (int) ftell(infile);
  fseek(infile, 0L, SEEK_SET);

  //Send the length of the content
  sprintf(buffer, "Content-Length: %d\n", fSize);
  printf("Size: %d\n", fSize);
  write(browser, buffer, strlen(buffer));

  //Load file into memory
  char* memory = (char* ) malloc(sizeof(char) * fSize);
  fread(memory, 1, fSize, infile);

  if(ferror(infile)) {
    error("ERROR Reading File");
  }

  //5) Call write function after building the response message
  write(browser, "Connection: Close\n\n", 19);
  
  n = write(browser, "Message received", 18);
  if (n < 0) { 
    error("ERROR writing to socket");
  }
  //6) Close the file for good practice
  free(buffer);
  free(file);
  free(memory);  
  fclose(infile);
  return;
  ////////////////////////////////////////////////////////
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     //struct sigaction sa; //for signal SIGCHILD

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);	//create socket
     if (sockfd < 0) 
        error("ERROR opening socket");
     memset((char *) &serv_addr, 0, sizeof(serv_addr));	//reset memory
     //fill in address info
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);	//5 simultaneous connection at most
     
     //accept connections
     newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
     if (newsockfd < 0) 
       error("ERROR on accept");
         
     int n;
   	 char buffer[256];
   			 
   	 memset(buffer, 0, 256);	//reset memory
      
 		 //read client's message
   	 n = read(newsockfd,buffer,255);
   	 if (n < 0) error("ERROR reading from socket");
   	 printf("Here is the message: %s\n",buffer);
   	 
   	 //reply to client

   	 // n = write(newsockfd,"I got your message",18);
   	 // if (n < 0) error("ERROR writing to socket");
         
     //Add Parse here
     parse(n);
     //
     
     close(newsockfd);//close connection 
     close(sockfd);
         
     return 0; 
}

