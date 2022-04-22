#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>

int *action = NULL;
int *elves_waiting = NULL;
int *elves_helped = NULL;
int *rainds_ready = NULL;
int *workshop_closed = NULL;
int *process_count = NULL;
sem_t *santa, *get_help, *mutex, *got_help, *start_waiting, *hitch, *end;
FILE *file;

int action_id = 0;
int elves_waiting_id = 0;
int elves_helped_id = 0;
int rainds_ready_id = 0;
int workshop_id = 0;
int process_count_id = 0;

int elf_count = 0;
int raind_count = 0;
int max_elf_time = 0;
int max_raind_time = 0;

void load_params(int argc, char **argv){
    if(argc != 5){
        fprintf(stderr, "Byly zadané nevalidní parametry!\n");
        exit(1);
    }
    char *tmp;
    elf_count = strtol(argv[1], &tmp, 10);
    if(strcmp(tmp, "")){
        fprintf(stderr, "Byly zadané nevalidní parametry!\n");
        exit(1);
    }
    raind_count = strtol(argv[2], &tmp, 10);
    if(strcmp(tmp, "")){
        fprintf(stderr, "Byly zadané nevalidní parametry!\n");
        exit(1);
    }
    max_elf_time = strtol(argv[3], &tmp, 10);
    if(strcmp(tmp, "")){
        fprintf(stderr, "Byly zadané nevalidní parametry!\n");
        exit(1);
    }
    max_raind_time = strtol(argv[4], &tmp, 10);
    if(strcmp(tmp, "")){
        fprintf(stderr, "Byly zadané nevalidní parametry!\n");
        exit(1);
    }

    if(elf_count <= 0 || elf_count >= 1000 || raind_count <= 0 || raind_count >= 20 || max_elf_time < 0 || max_elf_time > 1000 || max_raind_time < 0 || max_raind_time > 1000){
        fprintf(stderr, "Byly zadané nevalidní parametry!\n");
        exit(1);
    }
}

void semaphore_init(){
    santa = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    sem_init(santa, 1, 0);
    get_help = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    sem_init(get_help, 1, 0);
    got_help = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    sem_init(got_help, 1, 0);
    mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    sem_init(mutex, 1, 1);
    start_waiting = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    sem_init(start_waiting, 1, 1);
    hitch = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    sem_init(hitch, 1, 0);
    end = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    sem_init(end, 1, 0);
}

void shared_memory_init(){
    action_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | 0666);
    action = (int *) shmat(action_id, NULL, 0);
    *action = 0;
    
    elves_waiting_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | 0666);
    elves_waiting = (int *) shmat(elves_waiting_id, NULL, 0);
    *elves_waiting = 0;

    elves_helped_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | 0666);
    elves_helped = (int *) shmat(elves_helped_id, NULL, 0);
    *elves_helped = 0;

    rainds_ready_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | 0666);
    rainds_ready = (int *) shmat(rainds_ready_id, NULL, 0);
    *rainds_ready = 0;

    workshop_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | 0666);
    workshop_closed = (int *) shmat(workshop_id, NULL, 0);
    *workshop_closed = 0;

    process_count_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | 0666);
    process_count = (int *) shmat(process_count_id, NULL, 0);
    *process_count = 0;
}

void semaphore_delete(){
    munmap(santa, sizeof(sem_t));
    munmap(get_help, sizeof(sem_t));
    munmap(got_help, sizeof(sem_t));
    munmap(mutex, sizeof(sem_t));
    munmap(start_waiting, sizeof(sem_t));
    munmap(hitch, sizeof(sem_t));
    munmap(end, sizeof(sem_t));
}

void shared_memory_delete(){
    shmdt(action);
    shmdt(elves_waiting);
    shmdt(elves_helped);
    shmdt(rainds_ready);
    shmdt(workshop_closed);
    shmdt(process_count);

    shmctl(action_id, IPC_RMID, NULL);
    shmctl(elves_waiting_id, IPC_RMID, NULL);
    shmctl(elves_helped_id, IPC_RMID, NULL);
    shmctl(rainds_ready_id, IPC_RMID, NULL);
    shmctl(workshop_id, IPC_RMID, NULL);
    shmctl(process_count_id, IPC_RMID, NULL);
}

void cleanup(){
    semaphore_delete();
    shared_memory_delete();
    fclose(file);
}

void santa_process(){
    sem_wait(mutex);
    (*action)++;
    fprintf(file, "%d: Santa: going to sleep\n", (*action));
    sem_post(mutex);

    while(1){
        sem_wait(santa);
        sem_wait(mutex);
        if((*elves_waiting == 3) && (*workshop_closed == 0)){ //Elves are waiting for help
            (*action)++;
            fprintf(file, "%d: Santa: helping elves\n", (*action));
            sem_post(get_help);
            sem_post(mutex);
            sem_wait(got_help);
            sem_wait(mutex);
            (*action)++;
            fprintf(file, "%d: Santa: going to sleep\n", (*action));
        }
        else if(*rainds_ready == 0){ //All the raindeers are hitched and christmass can start
            (*action)++;
            fprintf(file, "%d: Santa: Christmas started\n", (*action));
            (*process_count)--;
            if(*process_count == 0){
                sem_post(end);
            }
            sem_post(mutex);
            exit(0);
        }
        else{ //If neither of conditions before are met, it must mean than santa needs to close workshop
            (*action)++;
            fprintf(file, "%d: Santa: closing workshop\n", *action);
            *workshop_closed = 1;
            sem_post(hitch);
            sem_post(get_help);
        }
        sem_post(mutex);
    }

}

void elf_process(int id){
    srand(time(0));

    sem_wait(mutex);
    (*action)++;
    fprintf(file, "%d: Elf %d: started\n", (*action), id);
    sem_post(mutex);
    
    while(1){
        //wait random time on the interval  <0,TE>
        if(max_elf_time != 0){
            usleep((rand()%max_elf_time)*1000);
        }
        //say need help
        sem_wait(mutex);
        (*action)++;
        fprintf(file, "%d: Elf %d: need help\n", *action, id);
        sem_post(mutex);

        sem_wait(mutex);
            //check if workshop is closed and if it is take holidays and open
            //all the elf related semaphores(start_waiting and get_help), 
            //so that all of the elves waiting at at those semaphores
            //can proceed further and take holidays
        if(*workshop_closed){
            (*action)++;
            fprintf(file, "%d: Elf %d: taking holidays\n", *action, id);
            sem_post(start_waiting);
            sem_post(get_help);
            (*process_count)--;
            if(*process_count == 0){
                sem_post(end);
            }
            sem_post(mutex);
            exit(0);
        }
        sem_post(mutex);

        //let elfs into the "waiting for help" queue, this is closed only if santa
        //is helping other elfs at the moment
        sem_wait(start_waiting);
        sem_wait(mutex);
        (*elves_waiting)++;
        if(*elves_waiting == 3){
            //check if workshop is closed and if it is take holidays and open
            //all the elf related semaphores(start_waiting and get_help), 
            //so that all of the elves waiting at at those semaphores
            //can proceed further and take holidays
            if(*workshop_closed){
                (*action)++;
                fprintf(file, "%d: Elf %d: taking holidays\n", *action, id);
                sem_post(start_waiting);
                sem_post(get_help);
                (*process_count)--;
                if(*process_count == 0){
                    sem_post(end);
                }
                sem_post(mutex);
                exit(0);
            }
            //tell santa, that 3 elfs are waiting for help
            sem_post(santa);
        }
        else{
            sem_post(start_waiting);
        }
        sem_post(mutex);

        //this gets opened after santa helps the elves, so they can also
        //say that they got helped
        sem_wait(get_help);

        sem_wait(mutex);

        //check if workshop is closed and if it is take holidays and open
        //all the elf related semaphores(start_waiting and get_help), 
        //so that all of the elves waiting at at those semaphores
        //can proceed further and take holidays
        if(*workshop_closed){
            (*action)++;
            fprintf(file, "%d: Elf %d: taking holidays\n", *action, id);
            sem_post(start_waiting);
            sem_post(get_help);
            (*process_count)--;
            if(*process_count == 0){
                sem_post(end);
            }
            sem_post(mutex);
            exit(0);
        }

        (*action)++;
        fprintf(file, "%d: Elf %d: get help\n", *action, id);
        (*elves_helped)++;
        if(*elves_helped == 3){ 
            sem_post(got_help); //tell santa he can go back to sleep
            *elves_helped = 0;
            *elves_waiting = 0;
            sem_post(start_waiting); //allow new set of elves to start waiting for help
        }
        else{
            sem_post(get_help); //allow other elves, who got helped from santa to say they got helped
        }
        sem_post(mutex);
    }
}

void raind_process(int id){
    sem_wait(mutex);
    (*action)++;
    fprintf(file, "%d: RD %d: rstarted\n", (*action), id);
    sem_post(mutex);

    srand(time(0));

    if(max_raind_time > 1){
        usleep((rand()%(max_raind_time/2)+max_raind_time/2)*1000); //sleep random time in the interval <TR/2,TR>
    }

    sem_wait(mutex);
        (*action)++;
        fprintf(file, "%d: RD %d: return home\n", (*action), id);
        (*rainds_ready)++;
        if(*rainds_ready == raind_count){
            sem_post(santa); //tell santa, all raindeers have returned
        }
    sem_post(mutex);

    sem_wait(hitch);
    sem_wait(mutex);
    (*action)++;
    fprintf(file, "%d: RD %d: get hitched\n", (*action), id);
    
    (*rainds_ready)--;
    if(*rainds_ready == 0){
        sem_post(santa);
    }
    sem_post(hitch);
    (*process_count)--;
    if(*process_count == 0){
        sem_post(end);
    }
    sem_post(mutex);
    exit(0);
}



int main(int argc, char **argv){

    load_params(argc, argv);

    file = fopen("proj2.out", "w");

    setbuf(file, NULL);

    semaphore_init();

    shared_memory_init();

    *process_count = 1 + elf_count + raind_count;

    for(int i = 0; i <= elf_count+raind_count; i++){
        pid_t new_proc = fork();
        if(new_proc == 0){
            
            if(i == 0){
                santa_process();
            }
            else if(i > 0 && i <= elf_count){
                elf_process(i);
            }
            else{
                raind_process(i-elf_count);
            }
            break;
        }
        else if(new_proc == -1){
            fprintf(stderr, "Program failed to create new process!\n");
            cleanup();
            exit(1);       
        }
    }

    sem_wait(end);

    cleanup();
    
    return 0;
}