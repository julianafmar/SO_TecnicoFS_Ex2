#include "tecnicofs_client_api.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define SIZE 40

int session_id;
char const *server;
int pipe_server;
char const *client;
int fclient;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    char buf[sizeof(char) + 40];
    memset(buf, '\0', 41);

    if((pipe_server = open(server_pipe_path, O_WRONLY)) < 0){
        return -1;
    }
    char pipe_path[40];
    memset(pipe_path, '\0', 40);
    memcpy(pipe_path, client_pipe_path, strlen(client_pipe_path));
    char c = 1;
    memcpy(buf, &c, sizeof(char));
    memcpy(buf + sizeof(char), client_pipe_path, strlen(client_pipe_path));
    unlink(pipe_path);
    if(mkfifo(pipe_path, 0777) < 0){
        exit(1);
    }
    if(write(pipe_server, &buf, sizeof(char)+40) <= 0){
        return -1;
    }
    if((fclient = open(pipe_path, O_RDONLY)) < 0){
        exit(1);
    }
    if(read(fclient, &session_id, sizeof(int)) <= 0){
        return -1;
    }
    
    client = pipe_path;
    server = server_pipe_path;
    return 0;
}

int tfs_unmount() {
    char buf[SIZE];
    char c = 2;

    memcpy(buf, &c, sizeof(char));
    memcpy(buf + sizeof(char), &session_id, sizeof(int));
    if(write(pipe_server, buf, sizeof(int) + sizeof(char)) <= 0){
        return -1;
    }
    
    if(close(pipe_server) == -1){
        return -1;
    }
    if(close(fclient) == -1){
        return -1;
    }

    unlink(client);
    return 0;
}

int tfs_open(char const *name, int flags) {
    char buf[sizeof(char) + sizeof(int) + sizeof(int) + SIZE];
    int n;

    char path_name[40];
    memset(path_name, '\0', 40);
    memcpy(path_name, name, strlen(name));
    char c = 3;
    memcpy(buf, &c, sizeof(char));
    memcpy(buf + sizeof(char), &session_id, sizeof(int));
    memcpy(buf + sizeof(char) + sizeof(int), path_name, 40);
    memcpy(buf + sizeof(char) + sizeof(int) + 40, &flags, sizeof(int));

    if(write(pipe_server, buf, sizeof(char) + sizeof(int) + sizeof(int) + 40) <= 0){
        return -1;
    }
    if(read(fclient, &n, sizeof(int)) <= 0){
        return -1;
    }
    return n;
}

int tfs_close(int fhandle) {
    char buf[sizeof(char) + sizeof(int) + sizeof(int)];
    int n;
    char c = 4;

    memcpy(buf, &c, sizeof(char));
    memcpy(buf + sizeof(char), &session_id, sizeof(int));
    memcpy(buf + sizeof(char) + sizeof(int), &fhandle, sizeof(int));
    if(write(pipe_server, buf, sizeof(char) + sizeof(int) + sizeof(int)) <= 0){
        return -1;
    }
    if(read(fclient, &n, sizeof(int)) <= 0){
        return -1;
    }
    return n;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    char buf[sizeof(char) + sizeof(int) + sizeof(int) + sizeof(size_t) + strlen(buffer)];
    ssize_t n;

    char c = 5;
    memcpy(buf, &c, sizeof(char));
    memcpy(buf + sizeof(char), &session_id, sizeof(int));
    memcpy(buf + sizeof(char) + sizeof(int), &fhandle, sizeof(int));
    memcpy(buf + sizeof(char) + sizeof(int) + sizeof(int), &len, sizeof(size_t));
    memcpy(buf + sizeof(char) + sizeof(int) + sizeof(int) + sizeof(size_t), buffer, len);
    if(write(pipe_server, buf, sizeof(char) + sizeof(int) + sizeof(int) + sizeof(size_t) + len) <= 0){
        return -1;
    }

    if(read(fclient, &n, sizeof(ssize_t)) <= 0){
        return -1;
    }
    return n;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    char buf[sizeof(char) + sizeof(int) + sizeof(int) + sizeof(size_t)];
    ssize_t size;

    char c = 6;
    memcpy(buf, &c, sizeof(char));
    memcpy(buf + sizeof(char), &session_id, sizeof(int));
    memcpy(buf + sizeof(char) + sizeof(int), &fhandle, sizeof(int));
    memcpy(buf + sizeof(char) + sizeof(int) + sizeof(int), &len, sizeof(size_t));
    if(write(pipe_server, buf, sizeof(char) + sizeof(int) + sizeof(int) + sizeof(size_t)) <= 0){
        return -1;
    }

    if(read(fclient, &size, sizeof(ssize_t)) <= 0){
        return -1;
    }
    if(read(fclient, buffer, sizeof(buffer)) <= 0){
        return -1;
    }
    return (ssize_t) size;
}

int tfs_shutdown_after_all_closed() {
    char buf[sizeof(char) + sizeof(int)];
    char c = 7;

    memcpy(buf, &c, sizeof(char));
    memcpy(buf + sizeof(char), &session_id, sizeof(int));
    if(write(pipe_server, buf, sizeof(char) + sizeof(int)) <= 0){
        return -1;
    }
    return 0;
}