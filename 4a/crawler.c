#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <pthread.h>
#include "crawler.h"
#include "cs537.c"
#include <errno.h>
#include <fcntl.h>

#define numOfPages 5

int work;
int linkQueueLimit, linkQueueSize;
pthread_mutex_t lock_linkQ, lock_pageQ;
pthread_cond_t cv_linkQ_fill, cv_linkQ_empty, cv_pageQ;
Node* pageQueueHead, *linkQueueHead, *fromURLs;
int visited[numOfPages];

//taken from http://www.cse.yorku.ca/~oz/hash.html
unsigned long
hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash%5;
}


/**** Queue Functions ****/
Node* enqueue(Node* headPtr, char* element)
{
	if(headPtr == NULL)
	{
		headPtr = (Node*)malloc(sizeof(Node));
		headPtr->element = element;
		headPtr-> next = NULL;
	}

	else
	{
		Node* ptr;
		ptr = (Node*)malloc(sizeof(Node));
		ptr->element = element;
		ptr-> next = headPtr;
		headPtr = ptr;
	}
	//printf("Head ptr: %x\n", headPtr);
	return headPtr;
}

Node* dequeue(Node* headPtr)
{
	if(headPtr == NULL)
	{
		app_error("Remove operation on empty queue");
		return NULL;
	}
	headPtr = headPtr->next;
	return headPtr;
}

char* front(Node* headPtr)
{
	if(headPtr==NULL)
	{
		app_error("Front operation on empty queue");
		return NULL;
	}
	return headPtr->element;
}

/*char* frontAndDequeue(Node* headPtr)
{
	char* element = headPtr->element;
	if(headPtr==NULL)
	{
		app_error("Front operation on empty queue");
		return NULL;
	}

	dequeue(headPtr);
	return element;
}*/
/**** End of Queue Functions ****/



Node* parse_page(char* fromLink, char* buf, Node* headPtr, void (*_edge_fn)(char *from, char *to))
{
	char* result, *savePtr;
	char** links;
	const char delim[2] = "\n ";
	result = strtok_r(buf, delim, &savePtr);
	//printf("In parse page\n");
	//printf("%s\n", result);

	while(result != NULL)
	{
		//printf("In parse page while loop\n");
		//printf("in:%s\n", result);

		if((strncmp(result,"link:",5))==0)
		{
			const char del[5] = "link:";
			char* res;
			unsigned long int h;
			res = strtok(result, del);
			//printf("%s\n", res);
			_edge_fn(fromLink, res);

			h = hash(res);	
			//printf("%lu     %d  \n",h, visited[h]);
			//add new link to queue
			//pthread_mutex_lock(&lock_linkQ);
			while(linkQueueSize>=linkQueueLimit)
				pthread_cond_wait(&cv_linkQ_empty,&lock_linkQ);
			if(visited[h] <0)
			{
				headPtr = enqueue(headPtr,res);
				linkQueueSize++;
				visited[h]++;
				work++;
			}
			pthread_cond_signal(&cv_linkQ_fill);
			//pthread_mutex_unlock(&lock_linkQ);
		}

		result = strtok_r(NULL, delim, &savePtr);
	} //while

	return headPtr;

}

void* parse(void* fn)
{
	//1 mutex and 2 CVs
	//wait for page queue to not be empty
	//parse page
	//wait for link queue to not be full
	//signal for link queue
	while(1)
	{
		/*if(work == 0)
			return NULL;*/		
		//printf("In parse  %d\n", work);

		edgeS* func = (edgeS*)fn;

		pthread_mutex_lock(&lock_pageQ);
		while(pageQueueHead == NULL)
		{	
			//printf("Here\n");
			pthread_cond_wait(&cv_pageQ,&lock_pageQ);
		}
		//printf("here2\n");
		char* pg = front(pageQueueHead);
		pageQueueHead = dequeue(pageQueueHead);
		work--;
		char* from = front(fromURLs);
		fromURLs = dequeue(fromURLs);
		//if(pg != NULL)
		//pthread_cond_signal(&cv_pageQ);
		pthread_mutex_unlock(&lock_pageQ);

		//printf("here3\n");
		pthread_mutex_lock(&lock_linkQ);
		while(linkQueueSize >= linkQueueLimit)
			pthread_cond_wait(&cv_linkQ_empty,&lock_linkQ);
		linkQueueHead = parse_page(from, pg, linkQueueHead,func->_edge_fn);
		pthread_cond_signal(&cv_linkQ_fill);
		pthread_mutex_unlock(&lock_linkQ);


	}
}

void* download(void* fn)
{
	//1 CV and 1 mutex
	//wait for link queue to not be empty
	//download page from link
	//signal page queue
	while(1)
	{	

		/*if(work==0)
			return NULL;*/
		//printf("In download   %d\n",work);
		fetchS* func = (fetchS*)fn;

		pthread_mutex_lock(&lock_linkQ);
		while(linkQueueHead == NULL)
			pthread_cond_wait(&cv_linkQ_fill, &lock_linkQ);
		char* pg = NULL;
		char* link = front(linkQueueHead);
		linkQueueHead = dequeue(linkQueueHead);
		linkQueueSize--;
		pthread_cond_signal(&cv_linkQ_empty);
		pthread_mutex_unlock(&lock_linkQ);

		pg = func->_fetch_fn(link);

		pthread_mutex_lock(&lock_pageQ);
		pageQueueHead = enqueue(pageQueueHead,pg);
		work++;
		fromURLs = enqueue(fromURLs,link);
		pthread_cond_signal(&cv_pageQ);
		pthread_mutex_unlock(&lock_pageQ);

	}
}

int crawl(char *start_url,
	  int download_workers,
	  int parse_workers,
	  int queue_size,
	  char * (*_fetch_fn)(char *url),
	  void (*_edge_fn)(char *from, char *to)) {

	char* pg;
	int error = 0;
	int i;

	linkQueueLimit = queue_size;
	linkQueueSize = 0;

	//pthread_t parser, downloader;
	pthread_t parser[parse_workers];
	pthread_t downloader[download_workers];

	//struct to pass to threads
	fetchS f_fn;
	edgeS e_fn;

	f_fn._fetch_fn = _fetch_fn;
	e_fn._edge_fn = _edge_fn;

	error = pthread_mutex_init(&lock_pageQ, NULL);
	assert(error == 0);
	error = pthread_mutex_init(&lock_linkQ, NULL);
	assert(error == 0);
	error = pthread_cond_init(&cv_pageQ, NULL);
	assert(error==0);
	error = pthread_cond_init(&cv_linkQ_empty, NULL);
	assert(error==0);
	error = pthread_cond_init(&cv_linkQ_fill, NULL);
	assert(error==0);

	

	//initialize variables
	work = 0;
	pageQueueHead = NULL;
	linkQueueHead = NULL;
	fromURLs = NULL;

	for(i= 0; i<numOfPages; i++)
	{
		visited[i] = -1;
	}

	visited[hash(start_url)]++;

	//retrieve first page
	pg = _fetch_fn(start_url);
	//printf("In crawl\n");
	//printf("%s\n", pg);

	if(pg == NULL)
	{
		app_error("fetch error");
		return -1;
	}
	//store first page in queue
	pthread_mutex_lock(&lock_pageQ);
	pageQueueHead = enqueue(pageQueueHead,pg);
	work++;
	fromURLs = enqueue(fromURLs,start_url);
	pthread_mutex_unlock(&lock_pageQ);

	//create threads and make sure main function waits for them to finish


	for(i = 0; i < download_workers; i++)
	{
		pthread_create(&downloader[i], NULL, download, &f_fn);
		//pthread_join(downloader[i],NULL);
	}

	for(i = 0; i < parse_workers; i++)
	{
		pthread_create(&parser[i], NULL, parse, &e_fn);
		//pthread_join(parser[i],NULL);
	}

	for(i = 0; i < download_workers; i++)
	{
		//pthread_create(&downloader[i], NULL, download, &f_fn);
		pthread_join(downloader[i],NULL);
	}

	for(i = 0; i < parse_workers; i++)
	{
		//pthread_create(&parser[i], NULL, parse, &e_fn);
		pthread_join(parser[i],NULL);
	}



	pthread_cond_destroy(&cv_linkQ_empty);
	pthread_cond_destroy(&cv_linkQ_fill);
	pthread_cond_destroy(&cv_pageQ);
	pthread_mutex_destroy(&lock_pageQ);
	pthread_mutex_destroy(&lock_linkQ);
  	return 0;
}
