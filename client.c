#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <limits.h>

#define BUF_SIZE		1024		//Max buffer size of the data in a frame
#define USERNAME_SIZE	16
#define PASSWORD_SIZE	16

char AUTH_USERNAME[] = "rootadmin";
char AUTH_PASSWORD[] = "123windows!@#";

/*A frame packet with unique id, length and data*/
struct frame_t {
	long int ID;
	long int length;
	char data[BUF_SIZE];
};

static void print_error(char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

/*----------------------------------------Main loop-----------------------------------------------*/

int main(int argc, char **argv)
{
	if ((argc < 3) || (argc >3)) {
		printf("Client: Usage --> ./[%s] <ipaddress> <port>\n", argv[0]);  //Should have the IP of the server
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in send_addr, from_addr;
	struct stat st;
	struct frame_t frame;
	struct timeval t_out = {0, 0};

	char cmd_send[50];
	char flname[20];
	char cmd[10];
	char ack_send[4] = "ACK";
	
	ssize_t numRead = 0;
	ssize_t length = 0;
	off_t f_size = 0;
	long int ack_num = 0;
	int cfd, ack_recv = 0;

	FILE *fptr;

	/*Clear all the data buffer and structure*/
	memset(ack_send, 0, sizeof(ack_send));
	memset(&send_addr, 0, sizeof(send_addr));
	memset(&from_addr, 0, sizeof(from_addr));

	/*Populate send_addr structure with IP address and Port*/
	send_addr.sin_family = AF_INET;
	send_addr.sin_port = htons(atoi(argv[2]));
	send_addr.sin_addr.s_addr = inet_addr(argv[1]);

	if ((cfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		print_error("Client: socket");
	
	/* Login Credentials */
	// Send "READY" status to server
	unsigned int timeout = UINT_MAX;
	int returnStatus;
	do {
		if (timeout == UINT_MAX) {
			returnStatus = sendto(cfd, "READY", strlen("READY"), 0, (struct sockaddr *)&send_addr, sizeof(send_addr));
			if (returnStatus == -1) {
				printf("Could not send \"READY\" status to server. Retrying...\n");
				timeout = 0;
			}
		}
		timeout++;
	} while (returnStatus == -1);
	char username[USERNAME_SIZE];
	char password[PASSWORD_SIZE];
	char message[BUF_SIZE];
	memset(username, 0, sizeof(username));
	memset(password, 0, sizeof(password));
	do {
		memset(message, 0, sizeof(message));
		recvfrom(cfd, message, BUF_SIZE, 0, (struct sockaddr *)&from_addr, (socklen_t *) &length);
		if (strncmp(message, "Username: ", strlen("Username: ")) == 0) {
			printf("%s", message);
			scanf("%s", username);
			returnStatus = sendto(cfd, username, strlen(username), 0, (struct sockaddr *)&send_addr, sizeof(send_addr));
			if (returnStatus == -1) {
				fprintf(stderr, "Could not send username.\n");
				exit(1);
			}
		}
		memset(username, 0, sizeof(username));
		memset(message, 0, sizeof(message));
		recvfrom(cfd, message, BUF_SIZE, 0, (struct sockaddr *)&from_addr, (socklen_t *) &length);
		if (strncmp(message, "Password: ", strlen("Password: ")) == 0) {
			printf("%s", message);
			scanf("%s", password);
			returnStatus = sendto(cfd, password, strlen(password), 0, (struct sockaddr *)&send_addr, sizeof(send_addr));
			if (returnStatus == -1) {
				fprintf(stderr, "Could not send password.\n");
				exit(1);
			}
		}
		memset(password, 0, sizeof(password));
		memset(message, 0, sizeof(message));
		recvfrom(cfd, message, BUF_SIZE, 0, (struct sockaddr *)&from_addr, (socklen_t *) &length);
		printf("%s", message);
	} while (strncmp(message, "Login failed.\n", strlen("Login failed.\n")) == 0);

	for (;;) {

		memset(cmd_send, 0, sizeof(cmd_send));
		memset(cmd, 0, sizeof(cmd));
		memset(flname, 0, sizeof(flname));

		printf("\n Menu \n 1. get [file_name] \n 2. exit \n");		
		scanf(" %[^\n]%*c", cmd_send);

		//printf("----> %s\n", cmd_send);
		
		sscanf(cmd_send, "%s %s", cmd, flname);		//parse the user input into command and filename

		if (sendto(cfd, cmd_send, sizeof(cmd_send), 0, (struct sockaddr *) &send_addr, sizeof(send_addr)) == -1) {
			print_error("Client: send");
		}

/*----------------------------------------------------------------------"get case"-------------------------------------------------------------------------*/

		if ((strcmp(cmd, "get") == 0) && (flname[0] != '\0' )) {

			long int total_frame = 0;
			long int bytes_rec = 0, i = 0;

			t_out.tv_sec = 2;
			setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval)); 	//Enable the timeout option if client does not respond

			recvfrom(cfd, &(total_frame), sizeof(total_frame), 0, (struct sockaddr *) &from_addr, (socklen_t *) &length); //Get the total number of frame to recieve

			t_out.tv_sec = 0;
                	setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval)); 	//Disable the timeout option
			
			if (total_frame > 0) {
				sendto(cfd, &(total_frame), sizeof(total_frame), 0, (struct sockaddr *) &send_addr, sizeof(send_addr));
				printf("Total number of packets: %ld\n", total_frame);
				
				char newflname[20];
				strncpy(newflname, "new-", sizeof("new-"));
				strncat(newflname, flname, strlen(flname));
				fptr = fopen(newflname, "wb");	//open the file in write mode

				/*Receive all the frames and send the acknowledgement sequentially*/
				for (i = 1; i <= total_frame; i++)
				{
					memset(&frame, 0, sizeof(frame));

					recvfrom(cfd, &(frame), sizeof(frame), 0, (struct sockaddr *) &from_addr, (socklen_t *) &length);  //Recieve the frame
					sendto(cfd, &(frame.ID), sizeof(frame.ID), 0, (struct sockaddr *) &send_addr, sizeof(send_addr));	//Send the ack

					/*Drop the repeated frame*/
					if ((frame.ID < i) || (frame.ID > i))
						i--;
					else {
						fwrite(frame.data, 1, frame.length, fptr);   /*Write the recieved data to the file*/
						printf("Packet ID: %ld	Packet Length: %ld\n", frame.ID, frame.length);
						bytes_rec += frame.length;
					}

					if (i == total_frame) {
						printf("File received.\n");
					}
				}
				printf("Total bytes received: %ld bytes\n", bytes_rec);
				fclose(fptr);
			}
			else {
				printf("File is empty.\n");
			}
		}

/*----------------------------------------------------------------------"exit case"-------------------------------------------------------------------------*/

		else if (strcmp(cmd, "exit") == 0) {

			exit(EXIT_SUCCESS);

		}

/*--------------------------------------------------------------------"Invalid case"-------------------------------------------------------------------------*/

		else {
			printf("--------Invalid Command!----------\n");
		}


	}
		
	close(cfd);

	exit(EXIT_SUCCESS);
}
