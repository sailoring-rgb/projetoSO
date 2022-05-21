/* **** SERVER **** 
ARGUMENTS:
    -> path_config path_exec
        * path_config: relative path for configuration file
        * path_exec: relative path for transformations executables
*/

/* **** COMMANDS **** 
    ./sdstored ../configs/sdstored.conf ../bin/SDStore-transf/
*/

#include "helper.h"
#define path_SDStore "SDStore-transf/"
#define forkError "[ERROR] Fork unsuccessful.\n"
#define signalError "[ERROR] Signal handler not established.\n"


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

// Global variables
int channel;

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

// Aply a given transformation
bool transformFile(char * file_path, char * transList[], int nrTrans){
    // OPEN File, apply transformations, close
    return true;
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

// Function for cheking if resources are free
bool checkResources(Trans * tr, char * name, int nrTrans){
    while(* tr && (strcmp((*tr)->operation_name, name) != 0))
        tr = & ((*tr)->next);
    
    if(* tr)
        if (((*tr)->currently_running + nrTrans) < (*tr)->max_operation_allowed)
            return true;

    return false;
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

// Realoc memory for tasks if needed
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

// Function to check if some information is not needed

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

// ***** MAIN *****
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

    if (signal(SIGINT, sigterm_handler) == SIG_ERR)
        printMessage(signalError);

    // Creating communication channel
    if(mkfifo(fifo, 0666) == -1){
        printMessage(fifoError);
        return 0;
    }

    int pid, fifo_reader, fifo_writer, freeSpot = -1, read_bytes = 0, total_nr_tasks = 0, max_nr_tasks = 10;
    char pid_reading[32], pid_writing[32], buffer[MAX_BUFF_SIZE];
    channel = open(fifo, O_RDWR);
    tasks = malloc(sizeof(struct Task) * max_nr_tasks);

    while(read(channel, &pid, sizeof(pid)) > 0){
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
            if(freeSpot == -1)
                addTask(tasks, total_nr_tasks, pid, buffer);
            else
                addTask(tasks, freeSpot, pid, buffer);
            total_nr_tasks++;
        }

        /*
            Verificar existência de recursos aqui;
            Se não estiverem disponíveis, adicionar uma lista (com o PID do processo) para ser executado depois
            Arranjar forma de dar trigger para ser executado
            Quando passar a execução, informar cliente
        */
        
        switch(fork()){
            case -1:
                printMessage(forkError);
                return false;
            case 0:
                //[SON]
                if(strcmp(buffer, "status") == 0){
                    sendStatus(fifo_writer, &sc, tasks, total_nr_tasks);
                }
                else{
                    sleep(10);
                    /*
                    executar transformações
                    */
                }
                _exit(pid);
            default:
                buffer[0] = '\0';
                //updateTask(tasks, total_nr_tasks, pid, "concluded");
                //total_nr_tasks--;
                close(fifo_reader);
                close(fifo_writer);
        }
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