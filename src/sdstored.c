/* **************** SERVER ****************
ARGUMENTS:
    -> path_config path_exec
        * path_config: relative path for configuration file
        * path_exec: relative path for transformations executables
*/

/* **************** COMMANDS **************** 
    ./sdstored ../configs/sdstored.conf ../bin/SDStore-transf/
*/

// **************** INCLUDES **************** 
#include "helper.h"

// **************** DEFINES **************** 
#define path_SDStore "SDStore-transf/"
#define forkError "[ERROR] Fork unsuccessful.\n"
#define signalError "[ERROR] Signal handler not established.\n"
#define inputError "[ERROR] Desired transformations cannot be performed due to server configuration.\n"
#define fileError "[ERROR] Couldn't find source file.\n"
#define pendingStatus "Pending.\n"
#define concludedStatus "Concluded.\n"
#define executingStatus "Executing.\n"

// **************** STRUCTS ****************
// Transformation information
typedef struct Transformation{
    char operation_name[64];
    int max_operation_allowed;
    int currently_running;
    struct Trans * next;
} * Trans;

// Task information
typedef struct Task{
    int id;
    char command[128];
    char status[16];
} Task;

// **************** GLOBAL VARIABLES ****************
int channel;

// **************** FUNCTIONS ****************
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
        for(int i = 2; i < nrTrans && space_available; i++){
            if(strcmp((*tr)->operation_name, transformations[i]) == 0)
                resources_needed++;
        }
        if((*tr)->max_operation_allowed < ((*tr)->currently_running + resources_needed))
            space_available = false;
        tr = & ((*tr)->next);
    }
    return space_available;
}

// Routine for handling sigterm
void sigterm_handler(int sig){
    close(channel);
}

// Function to show server status
void sendStatus(int writer, Trans * tr, Task * tasks, int nr_tasks){
    char buff[MAX_BUFF_SIZE]= "";
    int i, counter = 0, total_bytes = 0;

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

    for(i = 0; i < nr_tasks; i++){
        if(tasks[i].id != -1){
            counter++;
            total_bytes = sprintf(
            buff, "[Task #%d] %d: %s\n%s\n",
            counter,
            tasks[i].id,
            tasks[i].command,
            tasks[i].status);

            write(writer, buff, total_bytes);
            buff[0]= '\0';
        }
    }
}

// Function to realoc memory for tasks
bool updateTaskSize(Task ** tasks, int new_size){
    bool res = false;
    Task * temp = realloc(*tasks, (new_size * sizeof(Task)));
    if (temp != NULL){
        * tasks = temp;
        res = true;
    }
    return res;
}

// Function to create a task
void addTask(Task * tasks, int total_nr_tasks, int task_id, char command[]){
    tasks[total_nr_tasks].id = task_id;
    strcpy(tasks[total_nr_tasks].command,command);
    strcpy(tasks[total_nr_tasks].status,"pending");
}

// Function to update a tasks' status
void updateTask(Task * tasks, int total_nr_tasks, int task_id, char status[]){
    if(strcmp(status, "concluded") == 0){
        for(int i = 0; i < total_nr_tasks; i++){
            if (tasks[i].id == task_id){
                tasks[i].id = -1;
                strcpy(tasks[i].command,"");
                strcpy(tasks[i].status,"");
                break;
            }
        }
    }
    else{
        for(int i = 0; i < total_nr_tasks; i++){
            if (tasks[i].id == task_id){
                strcpy(tasks[i].status,status);
                break;
            }
        }
    }
}

// Function to check if some information is outdated and can be overwritten
int checkForSpot(Task * tasks, int max_nr_tasks){
    int res = -1;
    for(int i = 0; i < max_nr_tasks; i++){
        if (tasks[i].id == -1){
            res = i;
            break;
        }
    }
    return res;
}

// Function to update running resources
void updateResources(Trans * tr, char * transformations[], int nrTrans, char mode[]){
    if(strcmp(mode, "increase") == 0){
        while(* tr){
            for(int i = 2; i < nrTrans; i++){
                if(strcmp((*tr)->operation_name, transformations[i]) == 0)
                    (*tr)->currently_running++;
                }
            tr = & ((*tr)->next);

        }
    }
    else{
        while(* tr){
            for(int i = 2; i < nrTrans; i++){
                if(strcmp((*tr)->operation_name, transformations[i]) == 0)
                    (*tr)->currently_running--;
                }
        tr = & ((*tr)->next);
        }
    }
}

// Function to remove an elemnt from an array
bool removeElement(int arr[], int pos, int nr_elems){
    if (pos >= nr_elems + 1){
        for (int i = pos - 1; i < nr_elems -1; i++){  
                arr[i] = arr[i+1];  
            }
        return true;
    }
    return false;
}

// Function to get a transformation
Trans getTrans(Trans transf, char name[]){
    while(transf && strcmp(transf->operation_name, name)!=0)
        transf = transf->next;
    return transf;
}

// Function to execute a set of transformations
int executeTaks(Trans *transf, char *transformationsList[], int num_transformations){
    int pid, status;
    int input = open(transformationsList[2],O_RDONLY, 0777);
    int output = open(transformationsList[3], O_RDWR | O_CREAT ,0777);

    if(input  < 0 || output < 0)
        return 0;
    
    int fildes[num_transformations-4][2];

    switch(pid = fork()){
        case -1:
            wait(NULL);
            printMessage(forkError);
            close(input);
            close(output);
        case 0:
            if(num_transformations == 5){
                Trans *t = getTrans(&transf,transformationsList[4]);
                dup2(input,0);
                dup2(output,1);
                execl((*t)->operation_name,(*t)->operation_name,NULL);
            } else {
                makePipes(fildes,num_transformations-4);

                for(int i = 4; i < num_transformations; i++){
                    Trans *t = getTrans(&transf,transformationsList[i]);
                    if((pid = fork()) == 0){

                        if(i == num_transformations-1){
                            dup2(input,0);
                            dup2(fildes[i-5][1],1);
                            closePipes(fildes, num_transformations-4);
                            execl((*t)->operation_name, (*t)->operation_name, NULL);
                        }
                    }
                    else
                        if(i == 4) {
                            dup2(output,1);
                            dup2(fildes[i-4][0],0);
                            closePipes(fildes,num_transformations-4);
                            execl((*t)->operation_name, (*t)->operation_name, NULL);
                        }
                        else
                            if(i != num_transformations-1){
                                dup2(fildes[i-4][0],0);
                                dup2(fildes[i-5][1],1);
                                closePipes(fildes,num_transformations-4);
                                execl((*t)->operation_name, (*t)->operation_name, NULL);
                            }
                        else{
                            closePipes(fildes, num_transformations-1);
                            _exit(0);
                        }
                }
            }
        }
    return 1;
}

// Function to check if a pending task can be done
int executePending(int pendingList[], int pendingFifoList[], int nr_pending, Trans * sc, Task * tasks, int nr_tasks){
    char * transformationsList[MAX_BUFF_SIZE], buffer[MAX_BUFF_SIZE];
    int num_transformations, i, j, pid;
    int ret = -1; 
    
    for(i = 0; i < nr_pending; i++){
        pid = pendingList[i];
        j = 0;
        while(tasks[j].id != pid && j < nr_tasks)
            j++;
        if (j < nr_tasks){
            strcpy(buffer, tasks[j].command);
            num_transformations = lineSplitter(buffer, transformationsList);
            if (evaluateResourcesOcupation(&sc,transformationsList,num_transformations)){
                ret = i;
                break;
            }
        }
    }

    if(ret != -1){
        removeElement(pendingList, ret, nr_pending);
        write(pendingFifoList[i], executingStatus, strlen(executingStatus));
        updateTask(tasks, nr_tasks, pid, "executing");
        updateResources(&sc, transformationsList, num_transformations, "increase");
        if (executeTaks(&sc,transformationsList,num_transformations) == 0)
            printMessage(fileError);
        close(pendingFifoList[ret]);
        removeElement(pendingFifoList, ret, nr_pending);
        updateTask(tasks, nr_tasks, pid, "concluded");
        updateResources(&sc, transformationsList, num_transformations, "decrease");
    }

    return ret;
}

// ********* FOR TESTING
bool son_finished = true;
int size = 10;
int pending_tasks[10], pending_fifos[10], executing_tasks[10], executing_fifos[10], finished_tasks[10], finished_fifos[10];
int total_executing = 0, total_pending = 0, total_finished = 0;

void sigchild_handler(int signum){
    int i = 0;
    while(finished_tasks[i] != -1 && i < size) i++;
        if(i < size)
            finished_tasks[i] = pid;
    son_finished = true;
}

void dummyExecute(int pid){
    switch (fork()){
    case -1:
        printMessage(forkError);
    case 0:
        sleep(10);
        exit(pid);
    }
}


// **************** MAIN ****************
int main(int argc, char *argv[]){
    // Checking for argc
    if(argc < 3 || argc > 3){
        printMessage(argCountError);
        return 0;
    }

    Trans sc = NULL;
    Task * tasks = NULL;

    // Loading server configuration
    if(!loadServer(argv, &sc)){
        printMessage(serverError);
        return 0;
    }

    if (signal(SIGINT, sigterm_handler) == SIG_ERR || signal(SIGCHLD, sigchild_handler) == SIG_ERR)
        printMessage(signalError);

    // Creating communication channel
    if(mkfifo(fifo, 0666) == -1){
        printMessage(fifoError);
        return 0;
    }

    int pid, fifo_reader, fifo_writer, num_transformations;
    int freeSpot = -1, read_bytes = 0, total_nr_tasks = 0, max_nr_tasks = 10;
    char pid_reading[32], pid_writing[32], buffer[MAX_BUFF_SIZE], tmp[MAX_BUFF_SIZE];
    char * transformationsList[MAX_BUFF_SIZE];
    channel = open(fifo, O_RDWR);
    tasks = malloc(sizeof(struct Task) * max_nr_tasks);

    for(int i = 0; i < max_nr_tasks; i++)
        tasks[i].id = -1;

    while(read(channel, &pid, sizeof(pid)) > 0){
        if(son_finished){
            printMessage("A SON HAS FINISHED WORKING\n");
            //close(fifo_reader);
            son_finished = false;
        }

        if(total_nr_tasks == max_nr_tasks){
            freeSpot = checkForSpot(tasks, max_nr_tasks);
            if(freeSpot == -1){
                max_nr_tasks += 5;
                updateTaskSize(&tasks, max_nr_tasks);
            }
        }
        
        sprintf(pid_writing, "../tmp/%d_writer", pid);
        sprintf(pid_reading, "../tmp/%d_reader", pid);

        fifo_reader = open(pid_writing, O_RDONLY);
        fifo_writer = open(pid_reading, O_WRONLY);
        read_bytes = read(fifo_reader, &buffer, MAX_BUFF_SIZE);
        buffer[read_bytes] = '\0';
        
        if(strcmp(buffer, "status")!= 0){
            strcpy(tmp,buffer);
            num_transformations = lineSplitter(buffer, transformationsList);
            if(validateInput(&sc,transformationsList,num_transformations)){
                if(freeSpot == -1)
                    addTask(tasks, total_nr_tasks, pid, tmp);
                else{
                    addTask(tasks, freeSpot, pid, tmp);
                    freeSpot = -1;
                }
                total_nr_tasks++;
                if(!evaluateResourcesOcupation(&sc,transformationsList,num_transformations)){
                    write(fifo_writer, pendingStatus, strlen(pendingStatus));
                    updateTask(tasks, total_nr_tasks, pid, "pending");
                    pending_tasks[total_pending] = pid;
                    pending_fifos[total_nr_tasks] = fifo_writer;
                    total_pending++;
                    close(fifo_reader);
                }
                else{
                    write(fifo_writer, executingStatus, strlen(executingStatus));
                    updateTask(tasks, total_nr_tasks, pid, "executing");
                    updateResources(&sc, transformationsList, num_transformations, "increase");
                    executing_tasks[total_executing] = pid;
                    executing_fifos[total_executing] = fifo_writer;
                    total_executing++;
                    close(fifo_reader);
                    // if (executeTaks(&sc,transformationsList,num_transformations) == 0)
                    //     write(fifo_reader, fileError, strlen(fileError));
                    //updateTask(tasks, total_nr_tasks, pid, "concluded");
                    //updateResources(&sc, transformationsList, num_transformations, "decrease");
                    
                    // switch(fork()){
                    //     case -1:
                    //         printMessage(forkError);
                    //         return false;
                    //     case 0:
                    //         //[SON]
                    //         sleep(10);
                    //         /*
                    //         executar transformações
                    //         */
                    //         _exit(pid);
                    //     default:
                    //         buffer[0] = '\0';
                    //         tmp[0] = '\0';
                    //         wait(NULL);
                    //         //updateResources(&sc, transformationsList, num_transformations, "decrease");
                    //         //updateTask(tasks, total_nr_tasks, pid, "concluded");
                    //         close(fifo_reader);
                    //         close(fifo_writer);
                    // }
                }
            }
            else{
                write(fifo_writer, inputError, strlen(inputError));
                close(fifo_reader);
                close(fifo_writer);
            } 
        }
        else{
            switch(fork()){
                case -1:
                    printMessage(forkError);
                    return false;
                case 0:
                    sendStatus(fifo_writer, &sc, tasks, max_nr_tasks);
                    _exit(pid);
                default:
                    buffer[0] = '\0';                
                    close(fifo_reader);
                    close(fifo_writer);
            }
        }
        // this doesn't seem to work :(
        // if (executePending(pending_tasks, pending_fifos, total_pending, &sc, tasks, total_nr_tasks) != -1){
        //        total_pending--;
        // }
    }

    unlink(fifo);
    return 0;
}

  /*
    // **** FOR TESTING ****
    Trans * tr = &sc;

    while(* tr && (strcmp((*tr)->operation_name, "bcompress") != 0))
        tr = & ((*tr)->next);
    if(* tr){
        // WRITE SMTHING HERE
    }
    */