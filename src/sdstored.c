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
#define ARR_SIZE 20

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
    char command[MED_BUFF_SIZE];
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
Task makeTask(char s[], pid_t pid_r, int fd_wr){
    Task t = malloc(sizeof(struct Task));
    char * token;
    strcpy(t->command, s);
    token = strtok(s, " ");
    if(strcmp(token, "0") == 0 || strcmp(token, "0") == 1 || 
        strcmp(token, "0") == 2 || strcmp(token, "0") == 3 || 
        strcmp(token, "0") == 4 || strcmp(token, "0") == 5)
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

// Function to add a task
Task addTask(Task t, char s[], pid_t pid_r, int fd_wr){
    Task new = makeTask(s, pid_r, fd_wr);
    Task * ptr = &t;
    while(* ptr && ((*ptr)->priority) > new->priority)
        ptr = & ((*ptr)->next);
    new->next = (*ptr);
    (*ptr) = new;
    return t;
}

// Function to load tasks
int loadTasks(Task * tr , char command[], pid_t pid_r, int fd_wr){
    Task tmp_tr = NULL;
    tmp_tr = addTask(tmp_tr, command, pid_r, fd_wr);
    * tr = tmp_tr;
    return 1;
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
void makePipes(int file_des[][2], int nrPipes){
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
void sendStatus(Trans * tr, int writer){
    Task * executing = &executing_tasks;
    Task * pending = &pending_tasks;
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

// STILL NEEDS TO BE DOUBLE CHECKED
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
        
        if((*tr)->max_operation_allowed < (resources_needed + (*tr)->currently_running))
            space_available = false;
        tr = & ((*tr)->next);
    }
    return space_available;
}

// ***** FOR TESTING
int dummyExecute(){
    pid_t pid;
    switch (pid = fork()){
        case -1:
            printMessage(forkError);
        case 0:
            printMessage("A EXECUTAR\n");
            sleep(5);
            _exit(pid);
        case 1:
            break;
    }
    return pid;
}

// Função para atualizar a lista de tarefas pendentes
void checkPendingTasks(){
    Task * tr = &pending_tasks;
    int num_transformations, executing_pid;
    char * transformationsList[SMALL_BUFF_SIZE];
    
    while(* tr){
        num_transformations = lineSplitter((*tr)->command, transformationsList);
        if(evaluateResourcesOcupation(&sc, transformationsList, num_transformations)){
            write((*tr)->fd_writter, executingStatus, strlen(executingStatus));
            occupyResources(&sc,transformationsList, num_transformations);
            executing_pid = dummyExecute();
            loadTasks(&executing_tasks, (*tr)->command, (*tr)->pid_request, (*tr)->fd_writter);
            updateStatusTaskByRequestPID(&executing_tasks, (*tr)->pid_request, "executing");
            updateExecPID(&executing_tasks, (*tr)->pid_request, executing_pid);
            deleteTask_byRequestPID(&pending_tasks, (*tr)->pid_request);
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
            write((*tmp)->fd_writter, concludedStatus, strlen(concludedStatus));
            close((*tmp)->fd_writter);
            num_transformations = lineSplitter((*tmp)->command, transformationsList);
            freeResources(&sc, transformationsList, num_transformations);
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
                if(!evaluateResourcesOcupation(&sc,transformationsList,num_transformations)){
                    write(fifo_writer, pendingStatus, strlen(pendingStatus));
                    loadTasks(&pending_tasks, tmp, pid, fifo_writer);
                    updateStatusTaskByRequestPID(&pending_tasks, pid, "pending");
                }
                else{
                    write(fifo_writer, executingStatus, strlen(executingStatus));
                    occupyResources(&sc, transformationsList, num_transformations);
                        // if (executeTaks(&sc,transformationsList,num_transformations) == 0)
                        //     write(fifo_writer, fileError, strlen(fileError));
                    executing_pid = dummyExecute();
                    loadTasks(&executing_tasks, tmp, pid, fifo_writer);
                    updateStatusTaskByRequestPID(&executing_tasks, pid, "executing");
                    updateExecPID(&executing_tasks, pid, executing_pid);
                    buffer[0] = '\0';
                    tmp[0] = '\0';
                }
                close(fifo_reader);
            } else{
                write(fifo_writer, inputError, strlen(inputError));
                close(fifo_reader);
                close(fifo_writer);
            }
        }
        else {
            switch(fork()){
                case -1:
                    printMessage(forkError);
                    return false;
                case 0:
                    sendStatus(&sc, fifo_writer);
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


//   /*
//     // **** FOR TESTING ****
//     Trans * tr = &sc;

//     while(* tr && (strcmp((*tr)->operation_name, "bcompress") != 0))
//         tr = & ((*tr)->next);
//     if(* tr){
//         // WRITE SMTHING HERE
//     }
//     */


// ********************* PAULA ******************

// // Function to get a transformation
// Trans getTrans(Trans transf, char name[]){
//     while(transf && strcmp(transf->operation_name, name)!=0)
//         transf = transf->next;
//     return transf;
// }

// // Function to execute a set of transformations
// int executeTaks(Trans *transf, char *transformationsList[], int num_transformations){
//     pid_t pid;
//     int input = open(transformationsList[2],O_RDONLY, 0777);
//     int output = open(transformationsList[3], O_RDWR | O_CREAT ,0777);

//     if(input  < 0 || output < 0)
//         return 0;
    
//     int fildes[num_transformations-4][2];

//     switch(pid = fork()){
//         case -1:
//             wait(NULL);
//             printMessage(forkError);
//             close(input);
//             close(output);
//         case 0:
//             if(num_transformations == 5){
//                 Trans *t = getTrans(&transf,transformationsList[4]);
//                 dup2(input,0);
//                 dup2(output,1);
//                 execl((*t)->operation_name,(*t)->operation_name,NULL);
//             } else {
//                 makePipes(fildes,num_transformations-4);

//                 for(int i = 4; i < num_transformations; i++){
//                     Trans *t = getTrans(&transf,transformationsList[i]);
//                     if((pid = fork()) == 0){

//                         if(i == num_transformations-1){
//                             dup2(input,0);
//                             dup2(fildes[i-5][1],1);
//                             closePipes(fildes, num_transformations-4);
//                             execl((*t)->operation_name, (*t)->operation_name, NULL);
//                         }
//                     }
//                     else
//                         if(i == 4) {
//                             dup2(output,1);
//                             dup2(fildes[i-4][0],0);
//                             closePipes(fildes,num_transformations-4);
//                             execl((*t)->operation_name, (*t)->operation_name, NULL);
//                         }
//                         else
//                             if(i != num_transformations-1){
//                                 dup2(fildes[i-4][0],0);
//                                 dup2(fildes[i-5][1],1);
//                                 closePipes(fildes,num_transformations-4);
//                                 execl((*t)->operation_name, (*t)->operation_name, NULL);
//                             }
//                         else{
//                             closePipes(fildes, num_transformations-1);
//                             _exit(0);
//                         }
//                 }
//             }
//         }
//     return 1;
// }