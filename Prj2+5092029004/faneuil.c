/**********************************************************************************************
* Project2:
* 1. In this project, my program implement the concurrency of three kinds of threads: immigrant, 
* spectator and judge with the use of semaphores to meet the requirement of constraints.
* 2. The program run in an infinite loop without starvation.
* 3. The program can exit gracefully.
***********************************************************************************************/
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>

#define MAX_THREAD 500

// define a queue
typedef struct {
	int start;
	int end;
	int max_size;
	int *data;
} queue_int;
	
void queue_init(queue_int *q, int max) {
	q->max_size = max;
	q->start = 0;
	q->end = 1;
	q->data = malloc(q->max_size * sizeof(int));
}

void doublespace(queue_int *q) {
	int *tmp = q->data;
	q->data = malloc(q->max_size * 2 * sizeof(int));
	int i;
	for (i = 0; i < q->max_size; ++i) q->data[i] = tmp[i];
	q->max_size *= 2;
	free(tmp);
}
void push(queue_int *q, int item) {
	q->data[q->end] = item;
	if ((q->end + 1) % q->max_size == q->start) doublespace(q);
	q->end = (q->end + 1) % q->max_size;
}
int pop(queue_int *q) {
	if ((q->start + 1) % q->max_size == q->end) {
		printf("Pop Error!\n");
		exit(EXIT_FAILURE);
	}
	q->start = (q->start + 1) % q->max_size;
	return q->data[q->start];
}
void free_vector(queue_int *q) {
	free(q->data);
}


// record the number of the immigrants, spectators and judges
int imm_num = 0, spec_num = 0, jud_num = 0;
int current_imm_uncheck = 0;

// semaphores
sem_t jud_in, imm_check, has_sitdown, not_sitdown, imm_confirmed[MAX_THREAD];

// To avoid the starvation
sem_t imm_enter;
	
// To record the immigrants who have sitdown
queue_int queue_imm_sitdown; 
int sitdown_num = 0, unconfirm_num = 0;

// suitable random delays between immigrants’, judges’, and spectators’
void delay() {
	sleep(rand() % 3 + 1);
}
// suitalbe random dealys between any two successive actions of immigrants, judges, and spectators
void shortdelay() {
	sleep(0.5 + (double)rand() / RAND_MAX);
}

// functions for immigrants
void *imm_() {
	sem_wait(&jud_in);
	sem_post(&jud_in);
	int imm_no = imm_num;
	++imm_num;
	++current_imm_uncheck;
	++unconfirm_num;
	shortdelay();
	printf("Immigrant #%d enter\n", imm_no);
	sem_post(&imm_enter);
	shortdelay();
	printf("Immigrant #%d checkIn\n", imm_no);
	sem_post(&imm_check);
	--current_imm_uncheck;	
	sem_wait(&not_sitdown);
	shortdelay();
	printf("Immigrant #%d sitDown\n", imm_no);
	push(&queue_imm_sitdown, imm_no);
	sem_post(&has_sitdown);
	sem_wait(&imm_confirmed[imm_no]);
	shortdelay();
	printf("Immigrant #%d getCertificate\n", imm_no);
	sem_wait(&jud_in);
	shortdelay();
	printf("Immigrant #%d leave\n", imm_no);
	sem_post(&jud_in);
	sem_wait(&imm_enter);
	pthread_exit(NULL);
}
// functions for spectators
void *spec_() {
	sem_wait(&jud_in);
	sem_post(&jud_in);
	int spec_no = spec_num;
	++spec_num;	
	shortdelay();
	printf("Spectator #%d enter\n", spec_no);
	shortdelay();
	printf("Spectator #%d spectate\n", spec_no);
	shortdelay();
	printf("Spectator #%d leave\n", spec_no);
	pthread_exit(NULL);
}
// functions for judges
void *jud_() {
	sem_wait(&imm_enter);
	sem_post(&imm_enter);
	// Judge can not enter when there's already a judge in the room
	sem_wait(&jud_in);
	int jud_no = jud_num;
	++jud_num;
	shortdelay();
	printf("Judge #%d enter\n", jud_no);
	// The judge cannot confirm until all immigrants have invoked checkIn
	int i;
	for (i = 0; i < current_imm_uncheck; ++i) sem_wait(&imm_check);
	if (unconfirm_num != 0) {
		do {
			sem_wait(&has_sitdown);
			int imm_no = pop(&queue_imm_sitdown);
			shortdelay();
			printf("Judge #%d confirm the immigrant #%d \n", jud_no, imm_no);
			--unconfirm_num;
			sem_post(&imm_confirmed[imm_no]);
			sem_post(&not_sitdown);
		} while (unconfirm_num > 0);
	}
	shortdelay();
	printf("Judge #%d leave\n", jud_no);
	sem_post(&jud_in);
	pthread_exit(NULL);
}

// multi-threads
int Nthread = 0;
pthread_t *threads;

// pick a random function as the new thread
void new_thread(int tid) {
	switch (rand() % 3) {
	case 0: pthread_create(&threads[tid], NULL, imm_, NULL); break;
	case 1: pthread_create(&threads[tid], NULL, spec_, NULL); break;
	case 2: pthread_create(&threads[tid], NULL, jud_, NULL); break;
	}
}


int main() {
	srand(time(NULL));
	threads = malloc(MAX_THREAD * sizeof(pthread_t));
	sem_init(&jud_in, 0, 1);
	queue_init(&queue_imm_sitdown, 500);
	sem_init(&imm_enter, 0, 0);
	sem_init(&imm_check, 0, 0);
	sem_init(&has_sitdown, 0, 0);
	sem_init(&not_sitdown, 0, 500);
	int i;
	for (i = 0; i < MAX_THREAD; ++i) sem_init(&imm_confirmed[i], 0, 0);
	while (1) {
		new_thread(Nthread);
		++Nthread;
		delay();
	}
	free_vector(&queue_imm_sitdown);
	return 0;
}
