#ifndef __CRAWLER_H
#define __CRAWLER_H

typedef struct Node{
	char* element;
	void* next;
}Node;

typedef struct __fetchS{ 
	char* (*_fetch_fn)(char* url) ; 
} fetchS; 

typedef struct __edgeS{ 
	char* (*_edge_fn)(char* to,char* from) ; 
} edgeS; 

Node* parse_page(char* fromLink, char* buf, Node* headPtr, void (*_edge_fn)(char *from, char *to));

int crawl(char *start_url,
	  int download_workers,
	  int parse_workers,
	  int queue_size,
	  char * (*fetch_fn)(char *url),
	  void (*edge_fn)(char *from, char *to));


#endif
