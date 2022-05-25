/* ******************************** SERVER ********************************
ARGUMENTS:
    -> path_config path_exec
        * path_config: relative path for configuration file
        * path_exec: relative path for transformations executables
*/

/* ******************************** COMMANDS ********************************
    ./sdstored ../configs/sdstored.conf ../bin/SDStore-transf/
*/

// ******************************** INCLUDES ********************************
#include "helper.h"

// ******************************** DEFINES ********************************
#define forkError "[ERROR] Fork unsuccessful.\n"
#define signalError "[ERROR] Signal handler not established.\n"
#define inputError "[ERROR] Desired transformations cannot be performed due to server configuration.\n"
#define fileError "[ERROR] Couldn't find source file.\n"
#define pendingStatus "Pending.\n"
#define concludedStatus "Concluded.\n"
#define executingStatus "Executing.\n"

// ******************************** STRUCTS ********************************
// Transformation information
typedef struct Transformation{
    char operation_name[SMALL_BUFF_SIZE];
    int max_operation_allowed;
    int currently_running;
    struct Trans * next;
} * Trans;

// Task information
typedef struct Task{
    pid_t pid_request;
    pid_t pid_executing;
    int fd_writter;
    int priority;
    char command[MID_BUFF_SIZE];
    char status[SMALL_BUFF_SIZE];
    struct Task * next;
} * Task;

// ******************************** GLOBAL VARIABLES ********************************
int channel;
char transformations_path[MAX_BUFF_SIZE];
Trans sc;
Task executing_tasks, pending_tasks;

// ******************************** FUNCTIONS ********************************
// Function to create a transformation
Trans makeTrans(char s[]){
    Trans t = malloc(sizeof(struct Transformation));
    char * token;
    token = strtok(s, " ");
    strcpy(t->operation_name, token);
    token = strtok(NULL, " ");
    t->max_operation_allowed = atoi(token);
    t->currently_running = 0;
    t->next = NULL;
    return t;
}

// Function to add a transformation
Trans addTransformation(Trans t, char s[]){
    Trans new = makeTrans(s);
    Trans * ptr = &t;
    while(*ptr && strcmp((*ptr)->operation_name, new->operation_name) < 0)
        ptr = & ((*ptr)->next);
    new->next = (*ptr);
    (*ptr) = new;
    return t;
}

// Function for server configurations
int loadServer(char * path[], Trans * tr){
    char buffer[MAX_BUFF_SIZE];
    Trans tmp_tr = NULL;

    int config_fd = open(path[1], O_RDONLY);

    if(config_fd < 0)
        return 0;
    
    while(readLine(config_fd, buffer))
        tmp_tr = addTransformation(tmp_tr,buffer);
    
    close(config_fd);
    * tr = tmp_tr;
    return 1;
}

// Function to create a Task
Task createTask(char s[], pid_t pid_r, int fd_wr){
    Task t = malloc(sizeof(struct Task));
    char * token;
    strcpy(t->command, s);
    token = strtok(s, " ");
    token = strtok(NULL, " ");
    if(strcmp(token, "0") == 0 || strcmp(token, "1") == 0 || 
        strcmp(token, "2") == 0 || strcmp(token, "3") == 0 || 
        strcmp(token, "4") == 0 || strcmp(token, "5") == 0)
        t->priority = atoi(token);
    else
        t->priority = 0;
    strcpy(t->status, "pending");
    t->pid_request = pid_r;
    t->pid_executing = -1;
    t->fd_writter = fd_wr;
    t->next = NULL;
    return t;
}

// Function to insert a task in the LL
Task taskJoiner(Task old, Task new){
    if(!old || old->priority < new->priority){
        new->next = old;
        return new;
    }
    else{
        old->next = taskJoiner(old->next,new);
        return old;
    }
}

// Function to delete a Task searching by request PID
void deleteTask_byRequestPID(Task * head_ref, pid_t key){
    Task temp = * head_ref, prev;
    if (temp != NULL && temp->pid_request == key) {
        *head_ref = temp->next;
        free(temp);
        return;
    }
    while (temp != NULL && temp->pid_request != key) {
        prev = temp;
        temp = temp->next;
    }
    if (temp == NULL)
        return;
    prev->next = temp->next;
    free(temp);
}

// Function to delete a Task searching by execute PID
void deleteTask_byExecPID(Task * head_ref, pid_t key){
    Task temp = * head_ref, prev;
    if (temp != NULL && temp->pid_executing == key) {
        *head_ref = temp->next;
        free(temp);
        return;
    }
    while (temp != NULL && temp->pid_executing != key) {
        prev = temp;
        temp = temp->next;
    }
    if (temp == NULL)
        return;
    prev->next = temp->next;
    free(temp);
}

// Function to update task status by requestPID
void updateExecPID(Task * ptr, pid_t task_id, pid_t exec_pid){
    while(* ptr && ((*ptr)->pid_request) != task_id)
        ptr = & ((*ptr)->next);
    if(* ptr){
        (*ptr)->pid_executing = exec_pid;
    }
}

// Function to update task status by requestPID
void updateStatusTaskByRequestPID(Task * ptr, pid_t task_id, char status[]){
    while(* ptr && ((*ptr)->pid_request) != task_id)
        ptr = & ((*ptr)->next);
    if(* ptr){
        strcpy((*ptr)->status, status);
    }
}

// Function to update task status by requestPID
void updateStatusTaskByExecPID(Task * ptr, pid_t pid_exec, char status[]){
    while(* ptr && ((*ptr)->pid_executing) != pid_exec)
        ptr = & ((*ptr)->next);
    if(* ptr){
        strcpy((*ptr)->status, status);
    }
}

// Function to create pipes
void createPipes(int file_des[][2], int nrPipes){
    int i;
    for(i = 0; i < nrPipes; i++)
        pipe(file_des[i]);
}

// Function to close pipes
void closePipes(int file_des[][2], int nrPipes){
    int i;
    for (i= 0; i < nrPipes; i++){
        close(file_des[i][0]);
        close(file_des[i][1]);
    }
}


// Function to show server status
void sendStatus(Trans * tr, Task * executing, Task * pending, int writer){
    char buff[MAX_BUFF_SIZE] = "";
    int counter = 1, total_bytes = 0;

    while(* tr){
        total_bytes = sprintf(
            buff, "[Transformation] %s: %d/%d running/max\n",
            (*tr)->operation_name, 
            (*tr)->currently_running,
            (*tr)->max_operation_allowed);
        write(writer, buff, total_bytes);
        buff[0]= '\0';
        tr = & ((*tr)->next);
    }
    buff[0]= '\0';
    while(* executing){
        total_bytes = sprintf(
                buff, "[Task #%d] %d: %s\nPriority: %d | Status: %s\n",
                counter++,
                (*executing)->pid_request, 
                (*executing)->command,
                (*executing)->priority,
                (*executing)->status);
        write(writer, buff, total_bytes);
        buff[0]= '\0';
        executing = & ((*executing)->next);
    }
    buff[0]= '\0';
    while(* pending){
        total_bytes = sprintf(
                buff, "[Task #%d] %d: %s\nPriority: %d | Status: %s\n",
                counter++,
                (*pending)->pid_request, 
                (*pending)->command,
                (*pending)->priority,
                (*pending)->status);
        write(writer, buff, total_bytes);
        buff[0]= '\0';
        pending = & ((*pending)->next);
    }
}

// Function to ocuppy resources
void occupyResources(Trans * tr, char * transformations[], int nrTrans){
    while(* tr){
        for(int i = 2; i < nrTrans; i++){
            if(strcmp((*tr)->operation_name, transformations[i]) == 0)
                (*tr)->currently_running++;
            }
        tr = & ((*tr)->next);
    }
}

// Function to free resources
void freeResources(Trans * tr, char * transformations[], int nrTrans){
    while(* tr){
            for(int i = 2; i < nrTrans; i++){
                if(strcmp((*tr)->operation_name, transformations[i]) == 0)
                    (*tr)->currently_running--;
                }
        tr = & ((*tr)->next);
    }
}


// Function for validating requet
bool validateInput(Trans * tr, char * transformations[], int nrTrans){
    int resources_needed;
    int space_available = true;
    while(* tr  && space_available){
        resources_needed = 0;
        for(int i = 2; i < nrTrans && space_available; i++){
            if(strcmp((*tr)->operation_name, transformations[i]) == 0)
                resources_needed++;
        }
        if((*tr)->max_operation_allowed < resources_needed)
            space_available = false;
        tr = & ((*tr)->next);
    }
    return space_available;
}

// Function for cheking if resources are free
bool evaluateResourcesOcupation(Trans * tr, char * transformations[], int nrTrans){
    int resources_needed;
    bool space_available = true;
    while(* tr  && space_available){
        resources_needed = 0;
        for(int i = 2; i < nrTrans; i++){
            if(strcmp((*tr)->operation_name, transformations[i]) == 0)
                resources_needed++;
        }
        
        if((*tr)->max_operation_allowed < ((*tr)->currently_running + resources_needed))
            return false;
            
        tr = & ((*tr)->next);
    }
    return space_available;
}

// Function to get a transformation
Trans getTrans(Trans transf, char name[]){
    while(transf && strcmp(transf->operation_name, name) != 0)
        transf = transf->next;
    return transf;
}

// Function to execute a task
int executeTask(char * args[], int size){
    int index, nr_transformations, start_index;
    pid_t pid;
    
    if(strcmp(args[0], "0") == 0 || strcmp(args[0], "1") == 0 || 
        strcmp(args[0], "2") == 0 || strcmp(args[0], "3") == 0 || 
        strcmp(args[0], "4") == 0 || strcmp(args[0], "5") == 0)
        index = 1;
    else
        index = 0;
    
    nr_transformations = size - 2 - index;
    start_index = 2 + index; 
    
    int input = open(args[index],O_RDONLY, 0666);
    int output = open(args[index+1], O_CREAT| O_APPEND | O_WRONLY ,0666);

    int file_des[nr_transformations][2];
    char full_path[MAX_BUFF_SIZE];
    
    switch (pid = fork()){
        case -1:
            printMessage(forkError);
        case 0:
            if(nr_transformations == 1){
                strcpy(full_path, transformations_path);
                strcat(full_path, args[start_index]);
                dup2(input, STDIN_FILENO); 
                dup2(output, STDOUT_FILENO);
                execl(full_path ,args[start_index], NULL);
            }
            else{
                createPipes(file_des, nr_transformations);
                strcpy(full_path, transformations_path);
                for(int i = start_index; i < nr_transformations; i++){
                    strcat(full_path, args[i]);
                    switch (fork()){
                        case -1:
                            printMessage(forkError);
                        case 0:
                            if(i == nr_transformations - 1){
                                dup2(input, STDIN_FILENO);
                                dup2(file_des[i-3][1], STDOUT_FILENO);
                                closePipes(file_des, nr_transformations);
                                execl(full_path, args[i], NULL);
                            }
                            else{
                                if(i == 2){
                                    dup2(output, STDOUT_FILENO);
                                    dup2(file_des[i-2][0], STDIN_FILENO);
                                    closePipes(file_des, nr_transformations);
                                    execl(full_path, args[i], NULL);
                                }
                                else if(i != nr_transformations){
                                    dup2(file_des[i-2][0], STDIN_FILENO);
                                    dup2(file_des[i-3][1], STDOUT_FILENO);
                                    closePipes(file_des, nr_transformations);
                                    execl(full_path, args[i], NULL);
                                }
                                else{
                                    closePipes(file_des, nr_transformations);
                                    _exit(0);
                                }
                            }
                    }
                }
            }
            _exit(pid);
        default:
            close(input);
            close(output);
            break;
    }
    return pid;
}

// Função para atualizar a lista de tarefas pendentes
void checkPendingTasks(){
    Task * tr = &pending_tasks;
    Task tmp_t = NULL;
    char tmp[MID_BUFF_SIZE];
    int num_transformations, executing_pid;
    char * transformationsList[SMALL_BUFF_SIZE];
    while(* tr){
        strcpy(tmp, (*tr)->command);
        num_transformations = lineSplitter(tmp, transformationsList);
        if(evaluateResourcesOcupation(&sc, transformationsList, num_transformations)){
            write((*tr)->fd_writter, executingStatus, strlen(executingStatus));
            occupyResources(&sc,transformationsList, num_transformations);
            executing_pid = executeTask(transformationsList + 1, num_transformations - 1);
            tmp_t = createTask((*tr)->command, (*tr)->pid_request, (*tr)->fd_writter);
            executing_tasks = taskJoiner(executing_tasks, tmp_t);
            deleteTask_byRequestPID(&pending_tasks, tmp_t->pid_request);
            updateStatusTaskByRequestPID(&executing_tasks, tmp_t->pid_request, "executing");
            updateExecPID(&executing_tasks, tmp_t->pid_request, executing_pid);
            return;
        }
        tr = & ((*tr)->next);
    }
}

// Função para tratar uma tarefa já executada
void cleanFinishedTasks(pid_t pid_ex, int status){
    int num_transformations;
    char * transformationsList[SMALL_BUFF_SIZE];
    Task * tmp = &executing_tasks;
    if(WIFEXITED(status)){
        while(* tmp && ((*tmp)->pid_executing != pid_ex))
            tmp = & ((*tmp)->next);
        if(* tmp){
            num_transformations = lineSplitter((*tmp)->command, transformationsList);
            freeResources(&sc, transformationsList, num_transformations);
            write((*tmp)->fd_writter, concludedStatus, strlen(concludedStatus));
            close((*tmp)->fd_writter);
            deleteTask_byRequestPID(&executing_tasks, (*tmp)->pid_request);        
        }
    }
}

// **************** SIGNAL HANDLING ****************
// Handler para sinal SIGCHLD
void sigchild_handler(int signum){
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0){
        cleanFinishedTasks(pid, status);
    }
}

// Handler para sinal SIGALARM
void sigalarm_handler(int signum){
    checkPendingTasks();
    alarm(1);
}

// Routine for handling SIGTERM
void sigterm_handler(int sig){
    close(channel);
}

// ******************************** MAIN ********************************
int main(int argc, char *argv[]){
    // Checking for argc
    if(argc < 3 || argc > 3){
        printMessage(argCountError);
        return 0;
    }

    sc = NULL;
    executing_tasks = NULL;
    pending_tasks = NULL;
    Task tmp_t = NULL;

    // Loading server configuration
    if(!loadServer(argv, &sc)){
        printMessage(serverError);
        return 0;
    }

    // Setting up signal handlers
    if (signal(SIGINT, sigterm_handler) == SIG_ERR || signal(SIGCHLD, sigchild_handler) == SIG_ERR
            || signal(SIGALRM, sigalarm_handler) == SIG_ERR)
        printMessage(signalError);

    // Creating communication channel
    if(mkfifo(fifo, 0666) == -1){
        printMessage(fifoError);
        return 0;
    }

    strcpy(transformations_path, argv[2]);

    pid_t pid, executing_pid;
    int fifo_reader, fifo_writer, num_transformations;
    int read_bytes = 0;
    char pid_reading[32], pid_writing[32], buffer[MAX_BUFF_SIZE], tmp[MAX_BUFF_SIZE];
    char * transformationsList[SMALL_BUFF_SIZE];
    
    channel = open(fifo, O_RDWR);

    alarm(1);
    
    while(read(channel, &pid, sizeof(pid)) > 0){
        sprintf(pid_writing, "../tmp/%d_writer", pid);
        sprintf(pid_reading, "../tmp/%d_reader", pid);

        fifo_reader = open(pid_writing, O_RDONLY);
        fifo_writer = open(pid_reading, O_WRONLY);
        read_bytes = read(fifo_reader, &buffer, MAX_BUFF_SIZE);
        buffer[read_bytes] = '\0';
        
        if(strcmp(buffer, "status")!= 0){
            strcpy(tmp, buffer);
            num_transformations = lineSplitter(buffer, transformationsList);
            if(validateInput(&sc, transformationsList, num_transformations)){
                if(!evaluateResourcesOcupation(&sc, transformationsList, num_transformations)){
                    write(fifo_writer, pendingStatus, strlen(pendingStatus));
                    tmp_t = createTask(tmp, pid, fifo_writer);
                    pending_tasks = taskJoiner(pending_tasks, tmp_t);
                    updateStatusTaskByRequestPID(&pending_tasks, pid, "pending");
                }
                else{
                    write(fifo_writer, executingStatus, strlen(executingStatus));
                    occupyResources(&sc, transformationsList, num_transformations);
                    tmp_t = createTask(tmp, pid, fifo_writer);
                    executing_tasks = taskJoiner(executing_tasks, tmp_t);
                    executing_pid = executeTask(transformationsList + 1, num_transformations - 1);
                    updateStatusTaskByRequestPID(&executing_tasks, pid, "executing");
                    updateExecPID(&executing_tasks, pid, executing_pid);
                }
                close(fifo_reader);
                buffer[0] = '\0';
                tmp[0] = '\0';
            }
            else{
                write(fifo_writer, inputError, strlen(inputError));
                close(fifo_reader);
                close(fifo_writer);
            }
        }
        else{
            switch(pid = fork()){
                case -1:
                    printMessage(forkError);
                    return false;
                case 0:
                    sendStatus(&sc, &executing_tasks, &pending_tasks, fifo_writer);
                    _exit(pid);
                default:
                    buffer[0] = '\0';                
                    close(fifo_reader);
                    close(fifo_writer);
            }
        }   
    }
    unlink(fifo);
    return 0;
}