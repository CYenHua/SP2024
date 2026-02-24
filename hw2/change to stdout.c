#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "hw2.h"

#define ERR_EXIT(s) perror(s), exit(errno);


/*
If you need help from TAs,
please remember :
0. Show your efforts
    0.1 Fully understand course materials
    0.2 Read the spec thoroughly, including frequently updated FAQ section
    0.3 Use online resources
    0.4 Ask your friends while avoiding plagiarism, they might be able to understand you better, since the TAs know the solution, 
        they might not understand what you're trying to do as quickly as someone who is also doing this homework.
1. be respectful
2. the quality of your question will directly impact the value of the response you get.
3. think about what you want from your question, what is the response you expect to get
4. what do you want the TA to help you with. 
    4.0 Unrelated to Homework (wsl, workstation, systems configuration)
    4.1 Debug
    4.2 Logic evaluation (we may answer doable yes or no, but not always correct or incorrect, as it might be giving out the solution)
    4.3 Spec details inquiry
    4.4 Testcase possibility
5. If the solution to answering your question requires the TA to look at your code, you probably shouldn't ask it.
6. We CANNOT tell you the answer, but we can tell you how your current effort may approach it.
7. If you come with nothing, we cannot help you with anything.
*/

// somethings I recommend leaving here, but you may delete as you please
static char root[MAX_FRIEND_INFO_LEN] = "Not_Tako";     // root of tree
static char friend_info[MAX_FRIEND_INFO_LEN];   // current process info
static char friend_name[MAX_FRIEND_NAME_LEN];   // current process name
static int friend_value;    // current process value
FILE* read_fp = NULL;
friend *head = NULL;

// Is Root of tree
static inline bool is_Not_Tako() {
    return (strcmp(friend_name, root) == 0);
}


// a bunch of prints for you
void print_direct_meet(char *friend_name) {
    fprintf(stdout, "Not_Tako has met %s by himself\n", friend_name);
}

void print_indirect_meet(char *parent_friend_name, char *child_friend_name) {
    fprintf(stdout, "Not_Tako has met %s through %s\n", child_friend_name, parent_friend_name);
}

void print_fail_meet(char *parent_friend_name, char *child_friend_name) {
    fprintf(stdout, "Not_Tako does not know %s to meet %s\n", parent_friend_name, child_friend_name);
}

void print_fail_check(char *parent_friend_name){
    fprintf(stdout, "Not_Tako has checked, he doesn't know %s\n", parent_friend_name);
}

void print_success_adopt(char *parent_friend_name, char *child_friend_name) {
    fprintf(stdout, "%s has adopted %s\n", parent_friend_name, child_friend_name);
}

void print_fail_adopt(char *parent_friend_name, char *child_friend_name) {
    fprintf(stdout, "%s is a descendant of %s\n", parent_friend_name, child_friend_name);
}

void print_compare_gtr(char *friend_name){
    fprintf(stdout, "Not_Tako is still friends with %s\n", friend_name);
}

void print_compare_leq(char *friend_name){
    fprintf(stdout, "%s is dead to Not_Tako\n", friend_name);
}

void print_final_graduate(){
    fprintf(stdout, "Congratulations! You've finished Not_Tako's annoying tasks!\n");
}



void fully_write(int write_fd, void *write_buf, int write_len){
    int total_len = 0;
    int bytes_written;
    while (total_len < write_len) {
        bytes_written = write(write_fd, (char*)write_buf + total_len, write_len - total_len);
        if (bytes_written == -1) {
            if (errno == EINTR) {
                continue;  
            }
        }
        
        total_len += bytes_written;
    }
}

void fully_read(int read_fd, void *read_buf, int read_len){
    int total_len = 0;
    int bytes_read;
    while (total_len < read_len) {
        bytes_read = read(read_fd, (char*)read_buf + total_len, read_len - total_len);
        
        if (bytes_read == -1) {
            if (errno == EINTR) {
                continue;  
            }  
        }
        if (bytes_read == 0) {
            break;  // 遇到 EOF
        }
        total_len += bytes_read;
    }
}

int max(int a, int b){
    return (a > b) ? a : b;
}

friend *CreateNode(pid_t pid, char *child_info){
    friend *new = (friend *)malloc(sizeof(friend));
    //if(new == NULL) fprintf(stderr, "mallocfail\n");
    new->pid = pid;
    strcpy(new->info, child_info);
    sscanf(child_info, "%[^_]_%d", new->name, &(new->value));
    new->next = NULL;
    return new;
}

bool send_command_to_child(friend *cur, const char *command, char *buf) {
    write(cur->write_fd, command, strlen(command));
    //fprintf(stderr, "debug143: %s, %s\n", friend_name, command);

    // 讀取子節點的回應，並將其寫入 buf
    int bytes_read = read(cur->read_fd, buf, MAX_CHECK_LEN - 1); // 預留一個空間給 '\0'
    if (bytes_read > 0) {
        buf[bytes_read] = '\0'; // 確保結束符號
        return strcmp(buf, "fail") != 0;
    }
    return false;
}


void meet(char *parent_name, char *child_info, bool print){
    char child_name[MAX_FRIEND_NAME_LEN];
    int child_value;
    sscanf(child_info, "%[^_]_%d", child_name, &child_value);
    if(strcmp(parent_name, friend_name) == 0){
        int parent_to_child[2], child_to_parent[2];
        pipe(parent_to_child);
        pipe(child_to_parent);
        //fcntl(parent_to_child[0], F_SETFD, fcntl(parent_to_child[0], F_GETFD) | FD_CLOEXEC);
        fcntl(parent_to_child[0], F_SETFD, fcntl(parent_to_child[1], F_GETFD) | FD_CLOEXEC);
        fcntl(child_to_parent[1], F_SETFD, fcntl(child_to_parent[0], F_GETFD) | FD_CLOEXEC);
        //fcntl(child_to_parent[1], F_SETFD, fcntl(child_to_parent[1], F_GETFD) | FD_CLOEXEC);
        pid_t pid = fork();
        if(pid == 0){
            close(parent_to_child[1]);
            close(child_to_parent[0]);
            dup2(parent_to_child[0], PARENT_READ_FD);  // 父到子通訊管道 -> 標準輸入
            dup2(child_to_parent[1], PARENT_WRITE_FD); // 子到父通訊管道 -> 標準輸出
            if(parent_to_child[0] != PARENT_READ_FD) close(parent_to_child[0]);
            if(child_to_parent[1] != PARENT_WRITE_FD) close(child_to_parent[1]);
            
            
            execl("./friend", "friend", child_info, (char *)NULL); 
        } 
        else{
            friend *child = CreateNode(pid, child_info);
            close(parent_to_child[0]);
            close(child_to_parent[1]);
            child->read_fd = child_to_parent[0];
            child->write_fd = parent_to_child[1];
            if(head == NULL){
                head = child;
            }
            else{
                friend *cur = head;     
                while(cur->next != NULL) cur = cur->next;
                cur->next = child;
            }
            if(is_Not_Tako() && print){
                print_direct_meet(child_name);
            }
            else if(!is_Not_Tako()){
                write(PARENT_WRITE_FD, "success", 7);
            }
        }
    }
    else{
        friend *cur = head;
        while(cur != NULL){
            char command[MAX_CMD_LEN] = {0};
            snprintf(command, sizeof(command), "Meet %s %s", parent_name, child_info);
            write(cur->write_fd, command, strlen(command));
            char buf[10] = {0};
            read(cur->read_fd, buf, 10);
            if(strcmp(buf, "success") == 0){
                if(is_Not_Tako() && print){
                    print_indirect_meet(parent_name, child_name);
                }
                else if(!is_Not_Tako()){
                    write(PARENT_WRITE_FD, "success", 7);
                }
                break;
            }
            cur = cur->next;
        }
        if(cur == NULL){
            if(is_Not_Tako() && print){
                print_fail_meet(parent_name, child_name);
            }
            else if(!is_Not_Tako()){
                write(PARENT_WRITE_FD, "fail", 4);
            }
        }
    }
}

void combine(char *buf1, char *buf2, char *result){
    memset(result, 0, MAX_CHECK_LEN);
    char *start_buf1 = buf1;
    char *start_buf2 = buf2;
    char *newline_buf1, *newline_buf2;
    char *result_ptr = result; 
    char *result_head = result; 
    if(*start_buf1 != '\0'){
        newline_buf1 = strchr(start_buf1, '\n');
        if (newline_buf1 == NULL) newline_buf1 = start_buf1 + strlen(start_buf1);
        int len_buf1 = newline_buf1 - start_buf1;
        strncpy(result_ptr, start_buf1, len_buf1);
        result_ptr += len_buf1;
        *result_ptr = '\n';
        result_ptr++;
        start_buf1 = newline_buf1 + 1;
    }
    while(*start_buf1 != '\0' && *start_buf2 != '\0'){
        newline_buf1 = strchr(start_buf1, '\n');
        newline_buf2 = strchr(start_buf2, '\n');
        if (newline_buf1 == NULL) newline_buf1 = start_buf1 + strlen(start_buf1);
        if (newline_buf2 == NULL) newline_buf2 = start_buf2 + strlen(start_buf2);
        int len_buf1 = newline_buf1 - start_buf1;
        int len_buf2 = newline_buf2 - start_buf2;
        strncpy(result_ptr, start_buf1, len_buf1);
        result_ptr += len_buf1;
        *result_ptr = ' ';
        result_ptr++;
        strncpy(result_ptr, start_buf2, len_buf2);
        result_ptr += len_buf2;
        *result_ptr = '\n';
        result_ptr++;
        start_buf1 = newline_buf1 + 1;
        start_buf2 = newline_buf2 + 1;
    }
 
    if (*start_buf1 != '\0') {
        strncpy(result_ptr, start_buf1, strlen(start_buf1));
        result_ptr += strlen(start_buf1);
    }

    if (*start_buf2 != '\0') {
        strncpy(result_ptr, start_buf2, strlen(start_buf2));
        result_ptr += strlen(start_buf2);
    }
    *result_ptr = '\0';
    //buf1 = result_head;
}
void collect(){
    friend *cur = head;
    /*char buf[MAX_CHECK_LEN] = {0}; //放最後要回傳的
    char from_child[MAX_CHECK_LEN];
    char result[MAX_CHECK_LEN];//放兩者結合的*/
    char *buf = (char *)malloc(MAX_CHECK_LEN);
    char *result = (char *)malloc(MAX_CHECK_LEN);
    char from_child[MAX_CHECK_LEN];
    buf[0] = '\0';
    strcat(buf, friend_info);
    //buf[strlen(buf)] = '\n';
    buf[strlen(buf)] = '\0';
    while (cur != NULL){
        write(cur->write_fd, "Collect", 7);
        //fully_write(cur->write_fd, "Collect", 7);
        memset(from_child, 0, sizeof(from_child));
        //fully_read(cur->read_fd, from_child, strlen(from_child) - 1);
        read(cur->read_fd, from_child, sizeof(from_child));

        if(strcmp(buf, friend_name) == 0){
            snprintf(buf + strlen(buf), MAX_CHECK_LEN - strlen(buf), "\n%s", from_child);
        }
        else{
            combine(buf, from_child, result);

            //buf = result;
            strncpy(buf, result, MAX_CHECK_LEN);

        }
        cur = cur->next;
    }
    if(strcmp(buf, friend_info) == 0) sprintf(buf + strlen(buf), "\n");

    if(is_Not_Tako()) write(STDOUT_FILENO, buf, strlen(buf));
    else write(PARENT_WRITE_FD, buf, strlen(buf));
}
void graduate(){
    char buf[10] = {0};
    if(is_Not_Tako()){
        print_final_graduate();
    }
    friend *cur = head;
    //if(cur == NULL) fprintf(stderr, "debug302\n");
    
    while (cur != NULL) {
        write(cur->write_fd, "graduate", 8); 
        
        int bytes_read = read(cur->read_fd, buf, sizeof(buf));
        if(bytes_read > 0){
            if(strncmp(buf, "Exited", 6) == 0){
                //fprintf(stderr, "debug312\n");
                //fprintf(stderr, "wait%s\n", cur->name);
                waitpid(cur->pid, NULL, 0);
                //fprintf(stderr, "wait%s\n", cur->name);
            }
        }
        close(cur->write_fd); 
        close(cur->read_fd);

        
        friend *temp = cur;
        cur = cur->next;
        free(temp);
    }
    head = NULL;
    if(!is_Not_Tako()){
        //fprintf(stderr, "%s ecited\n", friend_name);
        write(PARENT_WRITE_FD, "Exited", 6);
    }
    //fprintf(stderr, "%s exit\n", friend_name);ex
    _exit(0);
    //fprintf(stderr, "%s exit\n", friend_name);
}
bool checknode(char *check_friend_name, int task){
    if(strncmp(check_friend_name, friend_name, max(strlen(check_friend_name), strlen(friend_name))) == 0){
        switch(task){
            case 1:
                collect();
                break;
            case 2:
                graduate();
                break;
            default:
                break;
        }
    }
    else{
        friend *cur = head;
        friend *prev = NULL;
        bool find = false;
        char buf[MAX_CHECK_LEN] = {0};
        while(cur != NULL){
            char command[MAX_CMD_LEN] = {0};
            memset(buf, 0, sizeof(buf));
            if(task == 1) snprintf(command, sizeof(command), "Check %s", check_friend_name);  
            else if(task == 2){
                snprintf(command, sizeof(command), "checkForGraduate %s", check_friend_name);
            } 

            if (send_command_to_child(cur, command, buf)) {
                if (task == 1) {
                    if(is_Not_Tako()) write(STDOUT_FILENO, buf, strlen(buf));
                    else{
                        write(PARENT_WRITE_FD, buf, strlen(buf));
                    }
                     
                }
                else if (task == 2) {
                    if(strncmp(buf, "Exited", 6) == 0){
                        if (prev == NULL){
                            head = cur->next;
                        } 
                        else{
                            prev->next = cur->next;
                        }
                        waitpid(cur->pid, NULL, 0);
                        close(cur->write_fd);
                        close(cur->read_fd);
                        free(cur);
                    }
                    if(!is_Not_Tako()) write(PARENT_WRITE_FD, "success", 7);
                }
                find = true;
                break;
            }
            prev = cur;
            cur = cur->next;
        }
        if (!find) {
            if (!is_Not_Tako()) {
                write(PARENT_WRITE_FD, "fail", 4);
            }
            return false;
        }
    }      
    return true;
}



int find_parent_value(char *parent_name, char *child_name){
    friend *cur = head;
    char command[MAX_CMD_LEN] = {0};
    char buf[MAX_CHECK_LEN] = {0};
    bool find = false;
    //fprintf(stderr, "debug422: %s\n", friend_name);
    if(strcmp(friend_name, parent_name) == 0){
        sprintf(buf, "%d", friend_value);
        if(!is_Not_Tako()) write(PARENT_WRITE_FD, buf, strlen(buf));
        return friend_value;
        //fprintf(stderr, "debug426\n");
    }
    else{
        while (cur != NULL) {
            memset(command, 0, sizeof(command));
            //fprintf(stderr, "debug455: %s\n", cur->name);
            snprintf(command, sizeof(command), "findValue %s %s", parent_name, child_name);
            //fprintf(stderr, "debug448: %s\n", friend_name);
            if (send_command_to_child(cur, command, buf)) {
                //fprintf(stderr, "debug449: %s\n", friend_name);
                if((strcmp(friend_name, child_name) == 0)){
                    //fprintf(stderr, "debug464: %s\n", friend_name);
                    write(PARENT_WRITE_FD, "fail", 4);
                    return -1;
                }
                if(!is_Not_Tako()) write(PARENT_WRITE_FD, buf, strlen(buf));
                

                find = true;
                return atoi(buf);
            }
            cur = cur->next;
        }
        if (!find) {
            if (!is_Not_Tako()) {
                write(PARENT_WRITE_FD, "fail", 4);
            }
        }
        return -1;
    }

}

void be_adopted(int parent_value){
    //fprintf(stderr, "%s call be_adopted\n", friend_name);
    int fifo = open("./Adopt.fifo", O_RDWR);
    friend *cur = head;
    bool success = true;
    char command[MAX_CMD_LEN];
    friend_value %= parent_value;
    snprintf(friend_info, sizeof(friend_info), "%s_%02d", friend_name, friend_value);
    while (cur != NULL) {
        //fprintf(stderr, "%s's child %s\n", friend_name, cur->name);
        //if(cur->next == NULL) fprintf(stderr, "%s end\n", friend_name);
        memset(command, 0, sizeof(command));
        snprintf(command, sizeof(command), "beAdopt %d", parent_value);
        write(cur->write_fd, command, strlen(command));
       

        char meet_command[MAX_CMD_LEN] = {0};
        cur->value %= parent_value;
        snprintf(cur->info, sizeof(cur->info), "%s_%02d", cur->name, cur->value);
        snprintf(meet_command, sizeof(meet_command), "Meet %s %s\n", friend_name, cur->info);
        //fprintf(stderr, "meet_command: %s : %s", friend_name, meet_command);
        //if(cur->next == NULL) fprintf(stderr, "%s wrong\n", friend_name);
        //fprintf(stderr, "meet_command:%s\n", meet_command);
        write(fifo, meet_command, strlen(meet_command));

        char buf[10] = {0};
        if(!send_command_to_child(cur, command, buf)){
            if(!is_Not_Tako()){
                write(PARENT_WRITE_FD, "fail", 4);
            }
            success = false;
        }

        cur = cur->next;
    }
    if(success) write(PARENT_WRITE_FD, "success", 7);

    close(fifo); // 關閉 FIFO 檔案

    

}




void adopt(char *parent_name, char *child_name, int parent_value){
    char *fifo_path = "./Adopt.fifo";
    bool find = false;

    if(strcmp(child_name, friend_name) == 0){
        be_adopted(parent_value);
    }
    else{
        friend *cur = head;
        char buf[20] = {0};
        while(cur != NULL){
            char command[MAX_CMD_LEN] = {0};
            memset(buf, 0, sizeof(buf));
            bool find = false;
            snprintf(command, sizeof(command), "Adopt %s %s %d", parent_name, child_name, parent_value);
            if(strcmp(cur->name, child_name) == 0){
                int fifo = open("./Adopt.fifo", O_RDWR);
                char meet_command[MAX_CMD_LEN] = {0};
                cur->value %= parent_value;
                snprintf(cur->info, sizeof(cur->info), "%s_%02d", cur->name, cur->value);
                snprintf(meet_command, sizeof(meet_command), "Meet %s %s\n", parent_name, cur->info);
                //fprintf(stderr, "meet_command:%s\n", meet_command);
                write(fifo, meet_command, strlen(meet_command));
                
                close(fifo);
            }
            if (send_command_to_child(cur, command, buf)){
                if(!is_Not_Tako()){
                    write(PARENT_WRITE_FD, "success", 7);
                }
                
                find = true;
                break;
            }
            cur = cur->next;
        }
        if(!find){
            if(!is_Not_Tako()){
                write(PARENT_WRITE_FD, "fail", 4);
            }
        }   
    }
    //fprintf(stderr, "debug490\n");

}




/* terminate child pseudo code
void clean_child(){
    close(child read_fd);
    close(child write_fd);
    call wait() or waitpid() to reap child; // this is blocking
}

*/

//remember read and write may not be fully transmitted in HW1?




//please do above 2 functions to save some time


int main(int argc, char *argv[]) {
    pid_t process_pid = getpid(); // you might need this when using fork()
    if (argc != 2) {
        fprintf(stderr, "Usage: ./friend [friend_info]\n");
        return 0;
    }
    //setvbuf(stdin, NULL, _IONBF, 0);
    //setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0); // prevent buffered I/O, equivalent to fflush() after each stdout, study this as you may need to do it for other friends against their parents
    
    // put argument one into friend_info
    memset(friend_info, 0, sizeof(friend_info));
   // fprintf(stderr, "debug366: %s\n", argv[1]);
    strncpy(friend_info, argv[1], MAX_FRIEND_INFO_LEN);
   // fprintf(stderr, "debug365: %s\n", friend_info);


    
    if(strcmp(argv[1], root) == 0){
        // is Not_Tako
        strncpy(friend_name, friend_info,MAX_FRIEND_NAME_LEN);      // put name into friend_nae
        friend_name[MAX_FRIEND_NAME_LEN - 1] = '\0';        // in case strcmp messes with you
        read_fp = stdin;        // takes commands from stdin
        friend_value = 100;     // Not_Tako adopting nodes will not mod their values
    }
    else{
        // is other friends
        // extract name and value from info
        memset(friend_name, 0, sizeof(friend_name));
        sscanf(friend_info, "%[^_]_%d", friend_name, &friend_value);
        
       
        read_fp = stdin;
    }

    char command[MAX_CMD_LEN] = {0};
    if(is_Not_Tako()){
        while(fgets(command, sizeof(command), stdin) != NULL){
            //fprintf(stderr, "debug479: %s's %s\n", friend_name, command);
            if(strncmp(command, "Meet", 4) == 0){
                char parent_name[MAX_FRIEND_NAME_LEN], child_info[MAX_FRIEND_INFO_LEN];
                sscanf(command + 5, "%s %s", parent_name, child_info);
                meet(parent_name, child_info, true);
            }
            else if(strncmp(command, "Check", 5) == 0){
                char Check_friend_name[MAX_FRIEND_NAME_LEN];
                sscanf(command + 6, "%s", Check_friend_name);
                bool find = checknode(Check_friend_name, 1); //1代表collect
                if(!find) print_fail_check(Check_friend_name);
            }
            
            else if(strncmp(command, "Graduate", 8) == 0){
                char Graduate_friend_name[MAX_FRIEND_NAME_LEN] = {0};
                sscanf(command + 9, "%s", Graduate_friend_name);
                bool find = checknode(Graduate_friend_name, 1);
                if(!find) print_fail_check(Graduate_friend_name);
                if(find){
                    checknode(Graduate_friend_name, 2);
                }
            }
            else if(strncmp(command, "Adopt", 5) == 0){
                //建一個fifo檔案
                char parent_name[MAX_FRIEND_NAME_LEN], child_name[MAX_FRIEND_NAME_LEN];
                sscanf(command + 6, "%s %s", parent_name, child_name);
                if(strcmp(child_name, "Not_Tako") == 0){
                    memset(command, 0, sizeof(command));
                    print_fail_adopt(parent_name, child_name);
                    continue;
                }
                char *fifo_path = "./Adopt.fifo";
                mkfifo(fifo_path, 0666);
                //fprintf(stderr, "730\n");
                int parent_value = find_parent_value(parent_name, child_name);
                //fprintf(stderr, "732\n");
                if(parent_value == -1){
                    print_fail_adopt(parent_name, child_name);
                }
                else{
                    int fifo = open(fifo_path, O_RDWR);
                    //fprintf(stderr, "debug737\n");
                    adopt(parent_name, child_name, parent_value);
                    //fprintf(stderr, "debug739\n");
                    print_success_adopt(parent_name, child_name);
                    checknode(child_name, 2); //graduate
                    
                    char fifo_buf[MAX_FIFO_CMD] = {0};
                    //fprintf(stderr, "open_fifo\n");
                    read(fifo, fifo_buf, sizeof(fifo_buf) - 1);
                    //fprintf(stderr, "fifo_read\n");
                    // write(STDOUT_FILENO, fifo_buf, strlen(fifo_buf));
                    char *token = strtok(fifo_buf, "\n");
                    while(token != NULL){
                        //fprintf(stderr, "token:%s\n", token);
                        char parent_name[MAX_FRIEND_NAME_LEN], child_info[MAX_FRIEND_INFO_LEN];
                        sscanf(token + 5, "%s %s", parent_name, child_info);
                        meet(parent_name, child_info, false);

                        token = strtok(NULL, "\n");
                    }
                    close(fifo);

                }

                

                

                unlink(fifo_path);
            }
            
            memset(command, 0, sizeof(command));
        }
    }
    else{
        memset(command, 0, sizeof(command));
        while(read(PARENT_READ_FD, command, sizeof(command) - 1) > 0){
            if(strncmp(command, "Meet", 4) == 0){
                char parent_name[MAX_FRIEND_NAME_LEN] = {0}, child_info[MAX_FRIEND_INFO_LEN] = {0};
                sscanf(command + 5, "%s %s", parent_name, child_info);
                meet(parent_name, child_info, true);        
            }
            else if(strncmp(command, "Check", 5) == 0){
                char Check_friend_name[MAX_FRIEND_NAME_LEN] = {0};
                sscanf(command + 6, "%s", Check_friend_name);
                checknode(Check_friend_name, 1);
            }
            else if(strncmp(command, "Collect", 7) == 0){
                collect();
            }
            else if(strncmp(command, "Adopt", 5) == 0){
                char parent_name[MAX_FRIEND_NAME_LEN] = {0}, child_name[MAX_FRIEND_NAME_LEN] = {0}, parent_value[4] = {0};
                sscanf(command + 6, "%s %s %s", parent_name, child_name, parent_value);
                adopt(parent_name, child_name, atoi(parent_value));
            }
            else if(strncmp(command, "graduate", 8) == 0){
                graduate();
                
            }
            else if(strncmp(command, "checkForGraduate", strlen("checkForGraduate")) == 0){
                char Graduate_friend_name[MAX_FRIEND_NAME_LEN] = {0};
                sscanf(command + 17, "%s", Graduate_friend_name);
                checknode(Graduate_friend_name, 2);
            }
            else if(strncmp(command, "findValue", strlen("findValue")) == 0){
                char parent_name[MAX_FRIEND_NAME_LEN], child_name[MAX_FRIEND_NAME_LEN];
                sscanf(command + 10, "%s %s", parent_name, child_name);
                find_parent_value(parent_name, child_name);
            }
            else if(strncmp(command, "beAdopt", 7) == 0){
                //fprintf(stderr, "debug834: %s beAdopt\n", friend_name);
                char parent_value[4];
                sscanf(command + 8, "%s", parent_value);
                int value = atoi(parent_value);
                be_adopted(value);
            }

            memset(command, 0, sizeof(command));
        }

        
    }
   

    //TODO:
    /* you may follow SOP if you wish, but it is not guaranteed to consider every possible outcome

    1. read from parent/stdin
    2. determine what the command is (Meet, Check, Adopt, Graduate, Compare(bonus)), I recommend using strcmp() and/or char check
    3. find out who should execute the command (extract information received)
    4. execute the command or tell the requested friend to execute the command
        4.1 command passing may be required here
    5. after previous command is done, repeat step 1.
    */

    // Hint: do not return before receiving the command "Graduate"
    // please keep in mind that every process runs this exact same program, so think of all the possible cases and implement them

    /* pseudo code
    if(Meet){
        create array[2]
        make pipe
        use fork.
            Hint: remember to fully understand how fork works, what it copies or doesn't
        check if you are parent or child
        as parent or child, think about what you do next.
            Hint: child needs to run this program again
    }
    else if(Check){
        obtain the info of this subtree, what are their info?
        distribute the info into levels 1 to 7 (refer to Additional Specifications: subtree level <= 7)
        use above distribution to print out level by level
            Q: why do above? can you make each process print itself?
            Hint: we can only print line by line, is DFS or BFS better in this case?
    }
    else if(Graduate){
        perform Check
        terminate the entire subtree
            Q1: what resources have to be cleaned up and why?
            Hint: Check pseudo code above
            Q2: what commands needs to be executed? what are their orders to avoid deadlock or infinite blocking?
            A: (tell child to die, reap child, tell parent you're dead, return (die))
    }
    else if(Adopt){
        remember to make fifo
        obtain the info of child node subtree, what are their info?
            Q: look at the info you got, how do you know where they are in the subtree?
            Hint: Think about how to recreate the subtree to design your info format
        A. terminate the entire child node subtree
        B. send the info through FIFO to parent node
            Q: why FIFO? will usin pipe here work? why of why not?
            Hint: Think about time efficiency, and message length
        C. parent node recreate the child node subtree with the obtained info
            Q: which of A, B and C should be done first? does parent child position in the tree matter?
            Hint: when does blocking occur when using FIFO?(mkfifo, open, read, write, unlink)
        please remember to mod the values of the subtree, you may use bruteforce methods to do this part (I did)
        also remember to print the output
    }
    else if(full_cmd[1] == 'o'){
        Bonus has no hints :D
    }
    else{
        there's an error, we only have valid commmands in the test cases
        fprintf(stderr, "%s received error input : %s\n", friend_name, full_cmd); // use this to print out what you received
    }
    */

   // final print, please leave this in, it may bepart of the test case output
    /*if(is_Not_Tako()){
        print_final_graduate();
    }*/
    return 0;
}