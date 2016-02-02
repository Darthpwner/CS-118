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
void parse(const char* browser) {
  //Use fstream to get the terminal output
  
  FILE *infile;
  infile = fopen(browser, "r");

  fscanf(infile, "%d", -1);

  fclose(infile);

  //

  char* dummy;

  //Call write to send it to the client
  int n = write(-1, dummy, sizeof(dummy));
  if (n < 0) error("ERROR writing to socket");
}
//

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;

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
   	 n = write(newsockfd,"I got your message",18);
   	 if (n < 0) error("ERROR writing to socket");
         
     //Add Parse here
     parse(n);
     //
     
     close(newsockfd);//close connection 
     close(sockfd);
         
     return 0; 
}

