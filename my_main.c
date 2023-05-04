#include "codec.h"
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#define STR_SIZE 1025

int exit_flag = 0;

struct task {
	char data[STR_SIZE];
	void (*func)(char* data, int key);
	int key;
	int index;
};

struct task* create_task(char *data, void (*func)(char* data, int key), int key, int index) {
	struct task *t = (struct task*)malloc(sizeof(struct task));
	printf("malloc task %d\n", index);
	t->data[0] = '\0';
	strncat(t->data, data, STR_SIZE);
	t->func = func;
	t->key = key;
	t->index = index;
	return t;
}

struct task_node {
	struct task *task;
	struct task_node *next;
};

struct task_node* create_task_node(struct task *t) {
	struct task_node *tn = (struct task_node*)malloc(sizeof(struct task_node));
	printf("malloc task_node %d\n", t->index);
	if(tn == NULL)
		printf("tn is null\n");
	tn->task = t;
	tn->next = NULL;
	return tn;
}

struct task_queue {
	struct task_node *head;
	struct task_node *tail;
	int size;
};

void print_queue_index(struct task_queue *tq) {
	if(tq == NULL)
		printf("tq is null\n");
	struct task_node *tn = tq->head;
	printf("queue: ");
	while (tn != NULL) {
		printf("%d ", tn->task->index);
		tn = tn->next;
	}
	printf("\n");
}

struct task_queue* create_task_queue() {
	struct task_queue *tq = (struct task_queue*)malloc(sizeof(struct task_queue));
	printf("malloc task_queue\n");
	tq->head = NULL;
	tq->tail = NULL;
	tq->size = 0;
	return tq;
}

void enqueue_task(struct task_queue *tq, struct task *t) {
	struct task_node *tn = create_task_node(t);
	if (tq->size == 0) {
		tq->head = tn;
		tq->tail = tn;
	} else {
		tq->tail->next = tn;
		tq->tail = tn;
	}
	tq->size++;
}

struct task* dequeue_task(struct task_queue *tq) {
	// printf("dequeueing task\n");
	if (tq->size == 0) {
		return NULL;
	}
	struct task_node *tn = tq->head;
	struct task *t = tn->task;
	tq->head = tn->next;
	tq->size--;
	free(tn);
	printf("free task_node %d\n", t->index);
	// printf("successfully dequeued task %d from queue, size of queue is now %d\n", t->index, tq->size);
	return t;
}

struct task* remove_task(struct task_queue *tq, int index) {
	// printf("removing task %d\n", index);
	if (tq->size == 0) {
		return NULL;
	}
	struct task_node *tn = tq->head;
	struct task *t = tn->task;
	if (t->index == index) {
		tq->head = tn->next;
		tq->size--;
		free(tn);
		printf("free task_node %d\n", t->index);
		// printf("successfully removed task %d from queue, size of queue is now %d\n", t->index, tq->size);
		return t;
	}
	while (tn->next != NULL) {
		if (tn->next->task->index == index) {
			struct task_node *tn2 = tn->next;
			t = tn2->task;
			tn->next = tn2->next;
			if(tn2 == tq->tail)
				tq->tail = tn;
			tq->size--;
			free(tn2);
			printf("free task_node %d\n", t->index);
			// printf("successfully removed task %d from queue, size of queue is now %d\n", t->index, tq->size);
			return t;
		}
		tn = tn->next;
	}
	return NULL;
}

void destroy_task_queue(struct task_queue *tq) {
	while (tq->size > 0) {
		struct task* t = dequeue_task(tq);
		free(t);
		printf("free task %d\n", t->index);
	}
	free(tq);
	printf("free task_queue\n");
}

struct threadpool {
	int task_count;
	struct task_queue *todo;
	struct task_queue *done;
	pthread_mutex_t todo_lock;
	pthread_mutex_t done_lock;
	pthread_cond_t todo_cond;
	pthread_cond_t done_cond;
} *tp;

void destroy_threadpool() {
	printf("destroying threadpool\n");
	destroy_task_queue(tp->todo);
	tp->todo = NULL;
	destroy_task_queue(tp->done);
	tp->done = NULL;
	pthread_mutex_destroy(&tp->todo_lock);
	pthread_mutex_destroy(&tp->done_lock);
	pthread_cond_destroy(&tp->todo_cond);
	pthread_cond_destroy(&tp->done_cond);
	free(tp);
	printf("free threadpool\n");
	tp = NULL;
}

struct tid_node {
	pthread_t tid;
	struct tid_node *next;
};

void* run_task_thread(void *arg) {
	struct task *t = (struct task*)arg;
	// printf("running task %d\n", t->index);
	t->func(t->data, t->key);
	// printf("finished task %d\n", t->index);
	// printf("asking for lock run task thread INDEX = %d\n", t->index);
	pthread_mutex_lock(&tp->done_lock);
	// printf("got lock run task thread INDEX = %d\n", t->index);
	enqueue_task(tp->done, t);
	// printf("added task %d to done queue\n", t->index);
	// printf("tail of done queue is %d\n", tp->done->tail->task->index);
	// print_queue_index(tp->done);
	// printf("signaling cond run task thread\n");
	pthread_mutex_unlock(&tp->done_lock);
	pthread_cond_signal(&tp->done_cond);
	// printf("released lock run task thread INDEX = %d\n", t->index);
	printf("finished task %d\n", t->index);
	pthread_exit(NULL);
}

void* run_todo_thread(void *arg) {
	struct tid_node *tid_head = NULL;
	int exit_flag2 = 0;
	while (!exit_flag || tp->todo->size > 0) {
		// printf("asking for lock run todo thread\n");
		pthread_mutex_lock(&tp->todo_lock);
		// printf("got lock run todo thread\n");
		while (tp->todo->size == 0) {
			if (exit_flag) {
				exit_flag2 = 1;
				break;
			}
			// printf("waiting for cond signal run todo thread\n");
			pthread_cond_wait(&tp->todo_cond, &tp->todo_lock);
			// printf("got cond signal run todo thread\n");
		}
		if (exit_flag2) {
			break;
		}
		struct task *t = dequeue_task(tp->todo);
		// pthread_mutex_unlock(&tp->todo_lock);
		struct tid_node *new_tid_node = (struct tid_node*)malloc(sizeof(struct tid_node));
		printf("malloc tid node\n");
		pthread_create(&new_tid_node->tid, NULL, run_task_thread, t);
		new_tid_node->next = tid_head;
		tid_head = new_tid_node;
		pthread_mutex_unlock(&tp->todo_lock);
		// printf("released lock run todo thread\n");
	}
	while (tid_head != NULL) {
		printf("joining task thead\n");
		pthread_join(tid_head->tid, NULL);
		printf("joined task thread\n");
		struct tid_node *tn = tid_head;
		tid_head = tid_head->next;
		free(tn);
		printf("free tid node\n");
	}
	printf("exiting run todo thread\n");
	pthread_exit(NULL);
}

void add_task_to_threadpool(struct task *t) {
	// printf("asking for lock add task to the threadpool\n");
	pthread_mutex_lock(&tp->todo_lock);
	// printf("got lock add task to the threadpool\n");
	enqueue_task(tp->todo, t);
	tp->task_count++;
	// printf("sending cond signal add task to threadpool\n");
	pthread_mutex_unlock(&tp->todo_lock);
	pthread_cond_signal(&tp->todo_cond);
	// printf("released lock add task to the threadpool\n");
	// printf("\nadded task %d\n", t->index);
}

int is_next_index_done(int index) {
	// printf("checking if index %d is done\n", index);
	// print_queue_index(tp->done);
	struct task_node *tn = tp->done->head;
	while (tn != NULL) {
		// printf("checking index %d\n", tn->task->index);
		if (tn->task->index == index) {
			// printf("found index %d in done queue\n", index);
			return 1;
		}
		tn = tn->next;
	}
	// printf("did not find index %d in done queue\n", index);
	return 0;
}

void* run_done_thread(void *arg) {
	int index = 0;
	int exit_flag2 = 0;
	while (!exit_flag || index < tp->task_count) {
		// printf("asking for lock run done thread\n");
		pthread_mutex_lock(&tp->done_lock);
		// printf("got lock run done thread\n");
		while (is_next_index_done(index) == 0) {
			if(exit_flag) {
				exit_flag2 = 1;
				break;
			}
			// printf("waiting for cond run done thread\n");
			pthread_cond_wait(&tp->done_cond, &tp->done_lock);
			// printf("cond signal run done thread, NEXT TO PRINT = %d\n", index);
		}
		if (exit_flag2) {
			break;
		}
		struct task *t = remove_task(tp->done, index);
		printf("\nPRINTING task %d\n", t->index);
		printf("%s", t->data);
		free(t);
		printf("free task\n");
		// printf("\nfinished task %d\n", t->index);
		index++;
		pthread_mutex_unlock(&tp->done_lock);
		// printf("released lock run done thread\n");
		// index++;
	}
	printf("exiting run done thread\n");
	pthread_exit(NULL);
}

void create_threadpool(pthread_t *todo_tid, pthread_t *done_tid) {
	tp = (struct threadpool*)malloc(sizeof(struct threadpool));
	printf("malloc threadpool\n");
	tp->task_count = 0;
	tp->todo = create_task_queue();
	tp->done = create_task_queue();
	pthread_mutex_init(&tp->todo_lock, NULL);
	pthread_mutex_init(&tp->done_lock, NULL);
	pthread_cond_init(&tp->todo_cond, NULL);
	pthread_cond_init(&tp->done_cond, NULL);
	pthread_create(todo_tid, NULL, run_todo_thread, NULL);
	pthread_create(done_tid, NULL, run_done_thread, NULL);
}


int main(int argc, char *argv[])
{
	printf("size of threadpool is %ld\n", sizeof(struct threadpool));
	printf("size of task is %ld\n", sizeof(struct task));
	printf("size of task_node is %ld\n", sizeof(struct task_node));
	printf("size of queue is %ld\n", sizeof(struct task_queue));

	if (argc != 3) {
	    printf("usage: key flag < file \n");
	    printf("!! data more than 1024 char will be ignored !!\n");
	    return 0;
	}

	int key = atoi(argv[1]);
	// printf("key is %i \n",key);

	int encrypt_flag;
	if(strcmp(argv[2], "-e") == 0) {
		encrypt_flag = 1;
	}
	else if(strcmp(argv[2], "-d") == 0) {
		encrypt_flag = 0;
	}
	else {
		printf("invalid flag\n");
		return 0;
	}

	char c;
	int counter = 0;
	char data[STR_SIZE]; 
	pthread_t todo_tid, done_tid;
	create_threadpool(&todo_tid, &done_tid);
	struct task *t;
	int index = 0;

	while ((c = getchar()) != EOF)
	{
	//   printf("%c", c);
	  data[counter] = c;
	  counter++;

	  if (counter == 1024){
		// printf("\nFULL\n");
		data[1024] = '\0';
		if(encrypt_flag) {
			t = create_task(data, encrypt, key, index++);
		}
		else {
			t = create_task(data, decrypt, key, index++);
		}
		add_task_to_threadpool(t);
		counter = 0;
	  }
	}
	
	if (counter > 0)
	{
		// printf("\nLAST size %d\n", counter);
		// char lastData[counter];
		// lastData[0] = '\0';
		// strncat(lastData, data, counter);
		data[counter] = '\0';
		if(encrypt_flag) {
			t = create_task(data, encrypt, key, index++);
		}
		else {
			t = create_task(data, decrypt, key, index++);
		}
		add_task_to_threadpool(t);
	}

	exit_flag = 1;
	printf("joinging todo thread\n");
	pthread_join(todo_tid, NULL);
	printf("joinging done thread\n");
	pthread_join(done_tid, NULL);
	printf("all joined\n");
	destroy_threadpool();

	return 0;
}
