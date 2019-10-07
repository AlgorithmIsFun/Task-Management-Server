#include <string.h>
#include "jobprotocol.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
// TODO: Use this file for helper functions (especially those you want available
// to both executables.

/* Example: Something like the function below might be useful

   // Find and return the location of the first newline character in a string
   // First argument is a string, second argument is the length of the string
   int find_newline(const char *buf, int len);
  
*/
JobCommand get_job_command(char * command){
	struct convert converts[] = { 
	{CMD_LISTJOBS, "jobs"},
        {CMD_RUNJOB, "run"},
        {CMD_KILLJOB, "kill"},
        {CMD_WATCHJOB, "watch"},
        {CMD_EXIT, "exit"}
	};
	if (command == NULL){
		return CMD_INVALID;
	}
	for (int i = 0; i < n_job_commands; i++){
		if (!strcmp(command, converts[i].str)){
			return i;
		}
	}
	return CMD_INVALID;
} 
JobNode *start_job(char *com, char * args[], int size){
	int fd[2];
	int err_fd[2];
	int r;
	char exe_file[BUFSIZE];
	sprintf(exe_file, "jobs/%s", args[1]);
//	printf("%s\n", exe_file);
	pipe(fd);
	pipe(err_fd);
	r = fork();
	if (r == 0){
		close(fd[0]);
		close(err_fd[0]);
		dup2(fd[1], 1);
		dup2(err_fd[1], 2);
		close(fd[1]);
		close(err_fd[1]);
		char *args2[10];
		int i;
		for (i = 1; i < size; i++){
			memcpy(&args2[i-1], &args[i], sizeof(char *));
		}
		args[i] = NULL;
		memcpy(&args2[i-1], &args[i], sizeof(char *));
		
		if (execv(exe_file, args2) == 0){
			printf("execv failed\n");
		}
		
		//close(fd[1]);
		exit(0);
	}
	else if (r > 0){
		close(fd[1]);
		close(err_fd[1]);
		JobNode *job = malloc(sizeof(JobNode));
		job->pid = r;
		job->next = NULL;
		job->stdout_fd = fd[0];
		job->stderr_fd = err_fd[0];
//		printf("fd: %d %d\n", fd[0], err_fd[0]);
		//wait(&(job->wait_status));
		return job;
	}
	return NULL;
}

void list_jobs(JobList *jobs, Client client){
	//printf("%d\n", jobs->count);
	char string[BUFSIZE+1];
	if (jobs->count == 0){
		strcat(string, "[SERVER] No currently running jobs");
	}else{
		strcpy(string, "[SERVER]");
		JobNode *job = jobs->first;
		for (int i = 0; i < jobs->count; i++){
			if (job->pid > 0){
				char pid[10];
				sprintf(pid, " %d", job->pid);
				strcat(string, pid);
				job = job->next;
			}
		}
	}	
	strcat(string, "\r\n");
	write(client.socket_fd, string, strlen(string));
	printf("%s", string);
}

void msg_all_clients(JobNode job, char *buf){
	WatcherNode *wc = job.watcher_list.first;
	for (int i = 0; i < job.watcher_list.count; i++){
		write(wc->client_fd, buf, strlen(buf));
		wc = wc->next;
	}
}
int buffer_msg(int fd, char *buf){
        int inbuf = 0;           // How many bytes currently in buffer?
        int room = BUFSIZE;  // How many bytes remaining in buffer?
        //char *after = buf;       // Pointer to position after the data in buf

        
        if ((inbuf = read(fd, buf, room)) > 0) {
            // Step 1: update inbuf (how many bytes were just added?)
            

            int where;
	    if ((where = find_network_newline(buf, inbuf)) > 0) {
		buf[where-2] = '\0';
	//	printf("network newline\n");
		//msg_all_clients(job, buf);
		//inbuf = inbuf - where;
                //memmove(buf, &(buf[where]), inbuf);
	    }
	    else{
		buf[inbuf] = '\0';
		}
            // Step 5: update after and room, in preparation for the next re
          //  after = &(buf[inbuf]);
	}
	//printf("buffer read: %s\n", buf);
	return inbuf;
}

void write_to_client(JobNode * job, Client * client, int client_index){
	//printf("buffer after forking: %s\n", client[client_index].buffer.buf);
	//fflush(client[client_index].socket_fd);
	char buf[BUFSIZE+1] = {'\0'};
	int inbuf = 0;
	int room = BUFSIZE;
	char *after;
	int nbytes;
	int done;
	int fd[] = {job->stdout_fd, job->stderr_fd};
	int max;
	if (fd[0] > fd[1]){
		max = fd[0];
	}else{
		max = fd[1];
	}
	fd_set all_fds, listen_fds;
    	FD_ZERO(&all_fds);
    	FD_SET(fd[0], &all_fds);
    	FD_SET(fd[1], &all_fds);
	while ((done = waitpid(job->pid, &job->wait_status, WNOHANG)) == 0){
	listen_fds = all_fds;
	//write(client[client_index].socket_fd, "hi\r\n", 4);
        int ready = select(max+1, &listen_fds, NULL, NULL, NULL);

        if (ready < 0)
        {
            perror("select");
        }
	for (int i = 0; i<2; i++){
//	printf("%d\n", fd[i]);
	memset(buf, 0, sizeof(buf));

	inbuf = 0;
        after = buf;
        nbytes = 0;
	
 	if (fd[i] > -1 && FD_ISSET(fd[i], &listen_fds))
        {
	while ((nbytes = read(fd[i], after, room)) > 0){
		inbuf += nbytes;
		int where;
		//printf("%s", after);
		//write(client[client_index].socket_fd, buf, nbytes);



		while ((where = find_network_newline(buf, inbuf)) > 0){
			buf[where-2] = '\0';
			char msg[BUFSIZE+1];
			if (i == 0){
				snprintf(msg, BUFSIZE, "[JOB %d] %s\r\n", job->pid, buf);
			}else{
				snprintf(msg, BUFSIZE, "*(JOB %d)* %s\r\n", job->pid, buf);
			}
			printf("%s", msg);
			//printf("%ld\n", sizeof("1"));;
			write(client[client_index].socket_fd, msg, strlen(msg));
			inbuf -= where;
			memmove(buf, &(buf[where]), inbuf);
		}
		room = sizeof(buf) - inbuf;
		after = &(buf[inbuf]);
	}
	}
	}
}
	close(fd[0]);
	close(fd[1]);
	char temp[BUFSIZE+1];
	int exit_len;
	if (WIFEXITED(job->wait_status)){
		exit_len = sprintf(temp, "[JOB %d] Exited with status %d.\r\n", job->pid, WEXITSTATUS(job->wait_status));
		write(client[client_index].socket_fd, temp, exit_len);
	}else{
		exit_len = sprintf(temp, "[JOB %d] Exited due to signal.\r\n", job->pid);
                write(client[client_index].socket_fd, temp, exit_len);

	}
	printf("%s", temp);
}
int splitWords(char *str, char *args[]){
	char delim[] = " ";
	char *ptr = strtok(str, delim);
	//char *arguments[10];
	int i = 0;
	while (ptr != NULL){
		args[i] = ptr;
		i += 1;
		ptr = strtok(NULL, delim);
	}
	/*for (int j = 0; j < i; j++){
		printf("%s\n", args[j]);
	//	if (j > 1){
	//		arguments[j-2] = args[j];
	//	}
	}
	printf("%d\n", i);
	*/return i;
}
int add_job (JobList * list, JobNode * node){
	//printf("%d\n", list->count);
	if (list->first == NULL){
		//list->first = malloc(sizeof(JobNode));
		list->first = node;}
	else{
		struct job_node *temp = list->first;
		while (temp->next != NULL){
			temp = temp->next;
		}
		//temp->next = malloc(sizeof(JobNode));
		temp->next = node;
	}
	list->count += 1;
	return 0;
}
int kill_job(JobList *list, int pid, int fd){
	if (list->first == NULL){
	}else{
		struct job_node *temp = list->first;
		if ((*temp).pid == pid){
			mark_job_dead(list, pid, 1);
			if (kill(pid, SIGKILL) == 0){
				return 0;
			}else{
				return -1;
			}
		}
		while (temp->next != NULL){
			if ((*temp->next).pid == pid){
				mark_job_dead(list, pid, 1);
				if (kill(pid, SIGKILL)==0){
					return 0;
				}
				else{
					return -1;
				}
			}
			temp = temp->next;
		
}}
		char Nill[BUFSIZE];
		sprintf(Nill, "[SERVER] Job %d not found\r\n", pid);
		write(fd, Nill, strlen(Nill));
		printf("%s", Nill);
		return 1;
	}

int remove_job(JobList *list, int pid){
	if (list->first == NULL){
		return -1;
	}
        struct job_node *temp = list->first;
        if ((*temp).pid == pid){
		if (temp->next != NULL){
			list->first = temp->next;
		}else{
			list->first = NULL;
		}
		free(temp);
                return 0;
         }
         while (temp->next != NULL){
                if ((*temp->next).pid == pid){
			struct job_node *rem = temp->next;
                        if (temp->next->next != NULL){
				temp->next = temp->next->next;
			}else{
				temp->next = NULL;
			}
			free(rem);
			return 0;
			
		}
		temp = temp->next;
	}
	return -1;
}

int mark_job_dead(JobList *list, int pid, int dead){
	if (list->first == NULL){
                return -1;
        }
	list->count -= 1;
        struct job_node *temp = list->first;
        if ((*temp).pid == pid){
                (*temp).dead = dead;
                return 0;
         }
         while (temp->next != NULL){
                if ((*temp->next).pid == pid){
                        (*temp->next).dead = dead;
                        return 0;
                }
		temp = temp->next;
        }
        return -1;

}

int empty_job_list(JobList *list){
	if (list->first == NULL){
                return -1;
        }
        struct job_node *temp = list->first;
        while (temp->next != NULL){
                remove_job(list, (*temp).pid);
		temp = temp->next;
        }
	list->first = NULL;
	return 0;

}
int delete_job_node(JobNode *node){
 	struct job_node *temp;
        while (node != NULL){
                temp = node;
		node = node->next;
                free(temp);
        }
	return 0;
}

int kill_all_jobs(JobList *list){
	if (list->first == NULL){
		return 0;}
 	struct job_node *temp = list->first;
        int counter = 0;
	while (temp->next != NULL){
                kill_job(list, (*temp).pid, 0);
                temp = temp->next;
		counter += 1;
        }
	return counter;
}

int kill_job_node(JobNode *node){
	if (node == NULL){
		return 1;
	}
	if (kill((*node).pid, SIGKILL) == 0){
                return 0;
        }else{
                return -1;
        }
	return 0;
}

int add_watcher(WatcherList *list, int watch){
	if (list->first == NULL){
                list->first = malloc(sizeof(struct watcher_node));
		(*list->first).client_fd = watch;
		(*list).count = 1;
	}
        else{
		(*list).count += 1;
                struct watcher_node *temp = list->first;
                while (temp->next != NULL){
                        temp = temp->next;
                }
                temp->next = malloc(sizeof(struct watcher_node));
		(*temp->next).client_fd = watch;
        }
        return 0;
	
}
int remove_watcher(WatcherList *list, int watch){
	struct watcher_node *temp = list->first;
	while (temp != NULL || (*temp->next).client_fd != watch) {
		temp = temp->next;
	}
	struct watcher_node *t = temp;
	t->next = t->next->next;
	free(temp->next);
	(*list).count -= 1;
	return 0;
}

int find_network_newline(const char *buf, int inbuf) {
        for (int i = 0; i < inbuf; i++){
                if (buf[i] == '\r'){
                        if (buf[i+1] == '\n'){
                                return i+2;
                        }
                }
        }
        return -1;
}

