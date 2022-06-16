#include "operations.h"
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define S 20
#define SIZE 40

char pipe_clients[S][SIZE];
int clients[S];
int number_of_clients = 0;

pthread_mutex_t mutex;
pthread_t tid[S];
pthread_cond_t cond[S];

typedef struct args{
    char op_code;
    int session_id;
    char file_name[SIZE];
    int flags;
    int fhandle;
    size_t len;
    char buffer[1024];
    char client_pipe[1024];
} args;

args operation;

void *task_manager();

int main(int argc, char **argv) {
    int fserv, fclient = 0, fhandle;
    size_t len;
    operation.op_code = 0;

    if(pthread_mutex_init(&mutex, NULL) == -1){
        return -1;
    }

    for(int i = 0; i < S; i++){
        clients[i] = -1;
    }

    for(int i = 0; i < S; i++){
        if (pthread_cond_init(&cond[i], NULL) != 0){
            return -1;
        }
        if(pthread_create(&tid[i], 0, task_manager, NULL) != 0){
            return -1;
        }
    }

    tfs_init();

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }
    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    unlink(pipename);
    if(mkfifo(pipename, 0777) < 0){
        exit(1);
    }
    if((fserv = open(pipename, O_RDONLY)) < 0){
        exit(1);
    }

    while(1){
        ssize_t in;
        char input;
        char client_name[SIZE];
        memset(client_name, '\0', SIZE);
        int session_id, num = -1;
        if((in = read(fserv, &input, sizeof(char))) < 0){
            return -1;
        }
        else if(in == 0){
            if((fserv = open(pipename, O_RDONLY)) < 0){
                exit(1);
            }
            if((in = read(fserv, &input, sizeof(char))) < 0){
                return -1;
            }
        }
        if(input == TFS_OP_CODE_MOUNT){
            operation.op_code = TFS_OP_CODE_MOUNT;
            if(number_of_clients >= S){
                return -1;
            }
            if(read(fserv, client_name, SIZE) < 0){
                return -1;
            }
            strcpy(operation.client_pipe, client_name);
            for(int i = 0; i < S; i++){
                if(clients[i] == -1){
                    clients[i] = fclient;
                    strcpy(pipe_clients[i], client_name);
                    num = i;
                    break;
                }
            }
            if(num == -1){
                return -1;
            }
            operation.session_id = num;

            pthread_cond_signal(&cond[operation.session_id]);
        }
        if(input == TFS_OP_CODE_UNMOUNT){
            operation.op_code = TFS_OP_CODE_UNMOUNT;
            if(read(fserv, &session_id, sizeof(int)) == -1){
                return -1;
            }
            operation.session_id = session_id;
            
            pthread_cond_signal(&cond[operation.session_id]);
        }
        if(input == TFS_OP_CODE_OPEN){
            operation.op_code = TFS_OP_CODE_OPEN;
            int flags;
            char cn[SIZE];

            memset(cn, '\0', SIZE);
            if(read(fserv, &session_id, sizeof(int)) == -1){
                return -1;
            }
            if(read(fserv, &cn, SIZE) == -1){
                return -1;
            }
            if(read(fserv, &flags, sizeof(int)) == -1){
                return -1;
            }
            operation.session_id = session_id;
            strcpy(operation.file_name, cn);
            operation.flags = flags;

            pthread_cond_signal(&cond[operation.session_id]);
        }
        if(input == TFS_OP_CODE_CLOSE){
            operation.op_code = TFS_OP_CODE_CLOSE;
            if(read(fserv, &session_id, sizeof(int)) == -1){
                return -1;
            }

            if(read(fserv, &fhandle, sizeof(int)) == -1){
                return -1;
            }
            operation.session_id = session_id;
            operation.fhandle = fhandle;
            pthread_cond_signal(&cond[operation.session_id]);
            
        }
        if(input == TFS_OP_CODE_WRITE){
            operation.op_code = TFS_OP_CODE_WRITE;
            if(read(fserv, &session_id, sizeof(int)) == -1){
                return -1;
            }
    
            if(read(fserv, &fhandle, sizeof(int)) == -1){
                return -1;
            }
            if(read(fserv, &len, sizeof(size_t)) == -1){
                return -1;
            }
            char buf[len];
            if(read(fserv, buf, sizeof(buf)) == -1){
                return -1;
            }
            operation.session_id = session_id;
            operation.fhandle = fhandle;
            operation.len = len;
            strcpy(operation.buffer, buf);
            pthread_cond_signal(&cond[operation.session_id]);
            
        }
        if(input == TFS_OP_CODE_READ){
            operation.op_code = TFS_OP_CODE_READ;
            if(read(fserv, &session_id, sizeof(int)) == -1){
                return -1;
            }
            if(read(fserv, &fhandle, sizeof(int)) == -1){
                return -1;
            }
            if(read(fserv, &len, sizeof(size_t)) == -1){
                return -1;
            }
            operation.session_id = session_id;
            operation.fhandle = fhandle;
            operation.len = len;
            pthread_cond_signal(&cond[operation.session_id]);
            
        }
        if(input == TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED){
            operation.op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
            if(read(fserv, &session_id, sizeof(int)) == -1){
                return -1;
            }
            operation.session_id = session_id;
            pthread_cond_signal(&cond[session_id]);            
        }
    }
    for(int i = 0; i < S; i++){
        pthread_join(tid[i], NULL);
    }
    return 0;
}

void *task_manager(){
    while(1){
        pthread_mutex_lock(&mutex);
        while(operation.op_code == 0){
            pthread_cond_wait(&cond[operation.session_id], &mutex);
        }
        int n, fclient;
        ssize_t m;
        if(operation.op_code == TFS_OP_CODE_MOUNT){
            if((fclient = open(operation.client_pipe, O_WRONLY)) < 0){
                exit(1);
            }
            if(write(fclient, &operation.session_id, sizeof(int)) == -1){
                operation.op_code = TFS_OP_CODE_UNMOUNT;
            }
            number_of_clients++;
            clients[operation.session_id] = fclient;
        }
        if(operation.op_code == TFS_OP_CODE_UNMOUNT){
            if(close(clients[operation.session_id]) == -1){
                exit(1);
            }
            
            clients[operation.session_id] = -1;
            strcpy(pipe_clients[operation.session_id], "");
            number_of_clients--;
        }
        if(operation.op_code == TFS_OP_CODE_OPEN){
            if((n = tfs_open(operation.file_name, operation.flags)) != -1){
                if(write(clients[operation.session_id], &n, sizeof(int)) == -1){
                    operation.op_code = TFS_OP_CODE_UNMOUNT;;
                }
            }
        }
        if(operation.op_code == TFS_OP_CODE_CLOSE){
            if((n = tfs_close(operation.fhandle)) != -1){
                if(write(clients[operation.session_id], &n, sizeof(int)) == -1){
                    operation.op_code = TFS_OP_CODE_UNMOUNT;
                }
            }
        }
        if(operation.op_code == TFS_OP_CODE_WRITE){
            if((m = tfs_write(operation.fhandle, &operation.buffer, operation.len)) == operation.len){
                if(write(clients[operation.session_id], &m, sizeof(int)) == -1){
                    operation.op_code = TFS_OP_CODE_UNMOUNT;
                }
            }
        }
        if(operation.op_code == TFS_OP_CODE_READ){
            char buffer[operation.len];
            if((m = tfs_read(operation.fhandle, buffer, operation.len)) != -1){
                if(write(clients[operation.session_id], &m, sizeof(ssize_t)) == -1){
                    operation.op_code = TFS_OP_CODE_UNMOUNT;
                }
                if(write(clients[operation.session_id], &buffer, sizeof(ssize_t)) == -1){
                    operation.op_code = TFS_OP_CODE_UNMOUNT;
                }
            }
        }
        if(operation.op_code == TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED){
            if(tfs_destroy_after_all_closed() == -1){
                exit(1);
            }
            exit(0);
        }
        operation.op_code = 0;
        pthread_mutex_unlock(&mutex);
    }
    return 0;   
}