#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include "socket.h"
#include "jobprotocol.h"
#define QUEUE_LENGTH 5
#define MAX_CLIENTS 20

#ifndef JOBS_DIR
    #define JOBS_DIR "jobs/"
#endif
int sigint_recieved;
static int cont = 1;
void sigint_handler(int code){
	cont = 0;
}
void sigchld_handler(int code){
	
}

int setup_new_client(int listen_fd, Client *clients){
	
	int user_index = 0;
	while (user_index < MAX_CLIENTS && clients[user_index].socket_fd != -1) {
        	user_index++;
	}

	int client_socket = accept_connection(listen_fd);
	if (client_socket < 0) {
	        return -1;
    	}
	if (user_index >= MAX_JOBS) {
        	fprintf(stderr, "server: max concurrent connections\n");
        	close(client_socket);
        	return -1;
    	}
	clients[user_index].socket_fd = client_socket;
	//&clients[user_index].buffer = malloc(sizeof(Buffer));
	//clients[user_index].buffer.buf = malloc(BUFSIZE);
	
	return client_socket;
}
void choose_command(Client *client, int client_index, JobList *job_list){
	char *args[10];
	int size =splitWords(client[client_index].buffer.buf, args);
	
	int command = get_job_command(args[0]);
	
	//printf("command: %d\n", CMD_LISTJOBS);	
	if (command == CMD_LISTJOBS){
		list_jobs(job_list, *client);	
	}
	else if (command == CMD_RUNJOB){
	//printf("%s\n", args[0]);
	//printf("%s\n", args[1]);
	if (job_list->count <= MAX_JOBS){
	JobNode *job = start_job(args[1], args, size);
	//printf("sup");
	char create[BUFSIZE+1];
	add_job(job_list, job);
	sprintf(create, "[SERVER] Job %d created\r\n", job->pid);
	write(client[client_index].socket_fd, create, strlen(create));
	printf("%s", create);
	write_to_client(job, client, client_index);
	//add_job(job_list, job);
	}
	else{
		char max[BUFSIZE+1];
		sprintf(max, "[SERVER] MAX JOBS exceeded\r\n");
		write(client[client_index].socket_fd, max, strlen(max));
		printf("%s", max);
	}		
	}
	else if (command == CMD_KILLJOB){	
	kill_job(job_list, strtol(args[1], NULL, 10),  client[client_index].socket_fd);
	}
	else if (command == CMD_WATCHJOB){
/*	char *args[10];
	find_str(buf, args);
	job = find_job(job_list, strtol(args[1], NULL, 10));
	add_watcher(job->watcher_list, client[client_index].socket_fd);
*/	}
	else if (command == CMD_EXIT){
		close(client[client_index].socket_fd);
		client[client_index].socket_fd = -1;	
	}
	else if (command == CMD_INVALID){
		char invalid[BUFSIZE+1];
		sprintf(invalid, "[SERVER] Invalid command: %s\r\n", client[client_index].buffer.buf);
		write(client[client_index].socket_fd, invalid, strlen(invalid));
		printf("%s", invalid);
	}
	strcpy(client[client_index].buffer.buf, "");
}
int read_buf(int client_index, Client *client, JobList *joblist){
	int fd = client[client_index].socket_fd;
	//printf("dsasddasdsadsdasdas\n");
	//int num_read = read(fd, client[client_index].buffer.buf, BUFSIZE);
	//printf("%s\n", client[client_index].buffer.buf);
	//client[client_index].buffer.buf[num_read] = '\0';
	int num_read = buffer_msg(fd, client[client_index].buffer.buf);
	
	if (num_read == 0) {
		client[client_index].socket_fd = -1;
		
		return fd;
	}
	char client_msg[BUFSIZE+1];
	sprintf(client_msg, "[CLIENT %d] %s\r\n", client[client_index].socket_fd, client[client_index].buffer.buf);
	printf("%s", client_msg);
	choose_command(client, client_index, joblist);
	return 0;
/*
	int nbytes;
	int room = sizeof(buf);
	char *after = buf;
	while ((nbytes = read(client.client_fd, after, room)) > 0) {
                client.buffer.inbuf += nbytes;
                int where;
		while ((where = find_network_newline(buf, client.buffer.inbuf)) > 0) {
			buf[where-2] = '\0';
                	printf("Next message: %s\n", &(buf[0]));
			choose_command(client, buf);
			client.inbuf -= where;
			memmove(buf, &(buf[where]), client.buffer.inbuf);
		}
		room = sizeof(buf) - client.buffer.inbuf;
		after = &(buf[client.buffer.inbuf]);
	}		
*/
}

int remove_client(int listen_fd, int client_index, Client *clients, JobList *job_list){
	close(listen_fd);
	int i = 0;
	while (clients[i].socket_fd != client_index){
		if (clients[i].socket_fd < 0){
			return -1;
		}
		i += 1;
	}	
	free(&clients[i]);
	clients[i].socket_fd = -1;
	return 0;
}
void close_clients(Client *clients){
	char buf[BUFSIZE+1];
	strcpy(buf, "[SERVER] Shutting down\r\n"); 
	for (int i = 0; i < MAX_CLIENTS; i++){
                if (clients[i].socket_fd != -1){
			write(clients[i].socket_fd, buf, strlen(buf));
			close(clients[i].socket_fd);
			clients[i].socket_fd = -1;
		}
        }
	printf("%s", buf);

}
void free_jobs(JobList *job_list){
	JobNode *job = job_list->first;
	JobNode *temp;
	while(job != NULL){
		temp = job;
		kill(job->pid, SIGKILL);
		job = job->next;
		free(temp);
	}
	free(job_list);
}
void clean_exit(int listen_fd, Client *clients, JobList *job_list, int exit_status){
	close_clients(clients);
	free_jobs(job_list);
}
int main(void) {
    // This line causes stdout and stderr not to be buffered.
    // Don't change this! Necessary for autotesting.
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    
    /* TODO: Initialize job and client tracking structures, start
     * accepting connections. Listen for messages from both clients
     * and jobs. Execute client commands if properly formatted. 
     * Forward messages from jobs to appropriate clients. 
     * Tear down cleanly.
     */

    /* Here is a snippet of code to create the name of an executable
     * to execute:
     *
     * char exe_file[BUFSIZE];
     * snprintf(exe_file, BUFSIZE, "%s/%s", JOBS_DIR, <job_name>);
     */
    // TODO: Set up SIGCHLD handler
	signal(SIGCHLD, sigchld_handler);
    // TODO: Set up SIGINT handler
	/*struct sigaction exiting;
    	exiting.sa_handler = sigint_handler;
    	exiting.sa_flags = 0;
    	sigemptyset(&exiting.sa_mask);
    	sigaction(SIGINT, &exiting, NULL);
	*///sigaction(SIGCHLD, &exiting, NULL);
	signal(SIGINT, sigint_handler);
    // TODO: Set up server socket
    struct sockaddr_in *self = init_server_addr(PORT);
    int listen_fd = setup_server_socket(self, QUEUE_LENGTH);

    // TODO: Initialize client tracking structure (array list)
	Client clients[MAX_CLIENTS + 1];
	//Client *clients[] = malloc(sizeof(Client)*MAX_CLIENTS);
	for (int i = 0; i < MAX_CLIENTS; i++){
		clients[i].socket_fd = -1;
	}
	
    // TODO: Initialize job tracking structure (linked list)
	JobList *joblist = malloc(sizeof(JobList));
	joblist->count = 0;
	joblist->first = NULL;
	
    // TODO: Set up fd set(s) that we want to pass to select()
	fd_set readfds, listen_fds;
	
	int max_fd;
	max_fd = listen_fd;
	//FD_ZERO(&readfds);
	FD_ZERO(&listen_fds);
	FD_SET(listen_fd, &listen_fds);
	
    while (cont) {
        // Use select to wait on fds, also perform any necessary checks
        // for errors or received signals
	readfds = listen_fds;
	//printf("dasad");
	int nready = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (nready == -1) {
            //perror("server: select");
        	perror("[SERVER] select");
	
		cont = 0;
        }
	
	
	if(cont && FD_ISSET(listen_fd, &readfds)){
        // Accept incoming connections
		int client_socket = setup_new_client(listen_fd, clients);
		if (client_socket < 0){
			continue;
		}
		if (client_socket > max_fd){
			max_fd = client_socket;
		}
		FD_SET(client_socket, &listen_fds);
	//	printf("%d\n", client_socket);
	}
	
	for (int i = 0; i < MAX_CLIENTS; i++){
		if (clients[i].socket_fd > -1 && FD_ISSET(clients[i].socket_fd, &readfds)){
			//printf("past_jobs");
			int client_closed = read_buf(i, clients, joblist);
			if (client_closed > 0){
				FD_CLR(client_closed, &listen_fds);
				if (close(client_closed) == -1){
					perror("client closed");
				}
				printf("[Client %d] Connection closed\n", client_closed);
			}
		}
	}
        // Check our job pipes, update max_fd if we got children

        // Check on all the connected clients, process any requests
        // or deal with any dead connections etc.
    }


    free(self);
    clean_exit(listen_fd, clients, joblist, 0);
    close(listen_fd);
    return 0;
}

