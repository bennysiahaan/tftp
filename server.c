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

int ls(FILE *f) 
{ 
	struct dirent **dirent; int n = 0;

	if ((n = scandir(".", &dirent, NULL, alphasort)) < 0) { 
		perror("Scan error."); 
		return -1; 
	}
        
	while (n--) {
		fprintf(f, "%s\n", dirent[n]->d_name);	
		free(dirent[n]); 
	}
	
	free(dirent); 
	return 0; 
}                                             

static void print_error(const char *msg, ...)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

/*-------------------------------------------Main loop-----------------------------------------*/

int main(int argc, char **argv)
{
	/*check for appropriate commandline arguments*/
	if ((argc < 2) || (argc > 2)) {				
		printf("Usage --> ./[%s] <port>\n", argv[0]);		//Should have a port number > 5000
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in sv_addr, cl_addr;
	struct stat st;
	struct frame_t frame;
	struct timeval t_out = {0, 0};

	char msg_recv[BUF_SIZE];
	char flname_recv[20];         
	char cmd_recv[10];
	
	char username[USERNAME_SIZE];
	char password[PASSWORD_SIZE];
	int returnStatus;

	ssize_t numRead;
	ssize_t length;
	off_t f_size; 	
	long int ack_num = 0;    //Receive frame acknowledgement
	int ack_send = 0;
	int sfd;

	FILE *fptr;
	
	int logged = 0;

	/*Clear the server structure - 'sv_addr' and populate it with port and IP address*/
	memset(&sv_addr, 0, sizeof(sv_addr));
	sv_addr.sin_family = AF_INET;
	sv_addr.sin_port = htons(atoi(argv[1]));
	sv_addr.sin_addr.s_addr = INADDR_ANY;

	if ((sfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		print_error("Server: socket");

	if (bind(sfd, (struct sockaddr *) &sv_addr, sizeof(sv_addr)) == -1)
		print_error("Server: bind");

	int addrlen = sizeof(cl_addr);
	for(;;) {
		printf("Server: Waiting for a client...\n");

		length = sizeof(cl_addr);
		if (logged == 0) {
			/*----------------------------------------------------------Login Credentials------------------------------------------------------------------*/
			memset(msg_recv, 0, sizeof(msg_recv));
			returnStatus = recvfrom(sfd, msg_recv, BUF_SIZE, 0, (struct sockaddr *)&cl_addr, &addrlen);
			if (returnStatus == -1) {
				fprintf(stderr, "Could not receive \"READY\" status from client.\n");
				exit(1);
			}
			do {
				memset(username, 0, sizeof(username));
				memset(password, 0, sizeof(password));
				sendto(sfd, "Username: ", strlen("Username: "), 0, (struct sockaddr *)&cl_addr, addrlen);
				recvfrom(sfd, username, USERNAME_SIZE, 0, (struct sockaddr *) &cl_addr, &addrlen);
				sendto(sfd, "Password: ", strlen("Password: "), 0, (struct sockaddr *)&cl_addr, addrlen);
				recvfrom(sfd, password, PASSWORD_SIZE, 0, (struct sockaddr *) &cl_addr, &addrlen);
				if (strncmp(username, AUTH_USERNAME, sizeof(AUTH_USERNAME)) == 0) {
					if (strncmp(password, AUTH_PASSWORD, sizeof(AUTH_PASSWORD)) == 0) {
						returnStatus = sendto(sfd, "Login success.\n", strlen("Login success.\n"), 0, (struct sockaddr *)&cl_addr, addrlen);
						if (returnStatus == -1) {
							fprintf(stderr, "Could not send login confirmation.\n");
							exit(1);
						}
						logged = 1;
						break;
					}
				}
				returnStatus = sendto(sfd, "Login failed.\n", strlen("Login failed.\n"), 0, (struct sockaddr *)&cl_addr, addrlen);
				if (returnStatus == -1) {
					fprintf(stderr, "Could not send login confirmation.\n");
					exit(1);
				}
			} while (1);
			/*---------------------------------------------------------------------------------------------------------------------------------------------*/
		}
		memset(msg_recv, 0, sizeof(msg_recv));
		memset(cmd_recv, 0, sizeof(cmd_recv));
		memset(flname_recv, 0, sizeof(flname_recv));
		if((numRead = recvfrom(sfd, msg_recv, BUF_SIZE, 0, (struct sockaddr *) &cl_addr, (socklen_t *) &length)) == -1)
			print_error("Server: receive");

		//print_msg("Server: Received %ld bytes from %s\n", numRead, cl_addr.sin_addr.s_addr);
		printf("Server: The received message: %s\n", msg_recv);

		sscanf(msg_recv, "%s %s", cmd_recv, flname_recv);
/*----------------------------------------------------------------------"get case"-------------------------------------------------------------------------*/
		if ((strcmp(cmd_recv, "get") == 0) && (flname_recv[0] != '\0')) {

			printf("Server: Get called with file name: \"%s\"\n", flname_recv);

			if (access(flname_recv, F_OK) == 0) {			//Check if file exist
				
				int total_frame = 0, resend_frame = 0, drop_frame = 0, t_out_flag = 0;
				long int i = 0;
					
				stat(flname_recv, &st);
				f_size = st.st_size;			//Size of the file

				t_out.tv_sec = 2;			
				t_out.tv_usec = 0;
				setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval));   //Set timeout option for recvfrom

				fptr = fopen(flname_recv, "rb");        //open the file to be sent
					
				if ((f_size % BUF_SIZE) != 0)
					total_frame = (f_size / BUF_SIZE) + 1;				//Total number of frames to be sent
				else
					total_frame = (f_size / BUF_SIZE);

				printf("Total number of packets: %d\n", total_frame);
					
				length = sizeof(cl_addr);

				sendto(sfd, &(total_frame), sizeof(total_frame), 0, (struct sockaddr *) &cl_addr, sizeof(cl_addr));	//Send number of packets (to be transmitted) to reciever
				recvfrom(sfd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *) &cl_addr, (socklen_t *) &length);

				while (ack_num != total_frame)		//Check for the acknowledgement
				{
					/*keep Retrying until the ack matches*/
					sendto(sfd, &(total_frame), sizeof(total_frame), 0, (struct sockaddr *) &cl_addr, sizeof(cl_addr)); 
					recvfrom(sfd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *) &cl_addr, (socklen_t *) &length);

					resend_frame++;

					/*Enable timeout flag even if it fails after 20 tries*/
					if (resend_frame == 20) {
						t_out_flag = 1;
						break;
					}
				}

				/*transmit data frames sequentially followed by an acknowledgement matching*/
				for (i = 1; i <= total_frame; i++)
				{
					memset(&frame, 0, sizeof(frame));
					ack_num = 0;
					frame.ID = i;
					frame.length = fread(frame.data, 1, BUF_SIZE, fptr);

					sendto(sfd, &(frame), sizeof(frame), 0, (struct sockaddr *) &cl_addr, sizeof(cl_addr));		//send the frame
					recvfrom(sfd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *) &cl_addr, (socklen_t *) &length);	//Receive the acknowledgement

					while (ack_num != frame.ID)  //Check for ack
					{
						/*keep retrying until the ack matches*/
						sendto(sfd, &(frame), sizeof(frame), 0, (struct sockaddr *) &cl_addr, sizeof(cl_addr));
						recvfrom(sfd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *) &cl_addr, (socklen_t *) &length);
						printf("#Packet: %ld	dropped, %d times\n", frame.ID, ++drop_frame);
						
						resend_frame++;

						printf("#Packet: %ld	dropped, %d times\n", frame.ID, drop_frame);

						/*Enable the timeout flag even if it fails after 200 tries*/
						if (resend_frame == 200) {
							t_out_flag = 1;
							break;
						}
					}

					resend_frame = 0;
					drop_frame = 0;

					/*File transfer fails if timeout occurs*/
					if (t_out_flag == 1) {
						printf("File not sent.\n");
						break;
					}

					printf("#Packet: %ld	#ACK: %ld\n", i, ack_num);

					if (total_frame == ack_num)
						printf("File sent.\n");
				}
				fclose(fptr);

				t_out.tv_sec = 0;
				t_out.tv_usec = 0;
				setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&t_out, sizeof(struct timeval)); //Disable the timeout option
			}
			else {	
				printf("Invalid file name.\n");
			}
		}

/*--------------------------------------------------------------------"exit case"----------------------------------------------------------------------------*/

		else if (strcmp(cmd_recv, "exit") == 0) {
			close(sfd);   //close the server on exit call
			exit(EXIT_SUCCESS);
		}

/*--------------------------------------------------------------------"Invalid case"-------------------------------------------------------------------------*/

		else {
			printf("Server: Unknown command. Please try again.\n");
		}
	}
	
	close(sfd);
	exit(EXIT_SUCCESS);
}
