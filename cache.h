#include <stdio.h>
#include <stdlib.h>

typedef struct web_object
{
	char *content; // pointer to actual web service content(value)
	char *url;     // pointer to relevant browser url(key)
	size_t size;   // size of content
	struct web_object *next; // pointer to next web_objecy
	unsigned long access_time; // access time for LRU judgement
}web_object;

typedef struct cache
{
	web_object *head;
	size_t cache_size;
}cache;

/* global variable */
cache *public_cache_pointer; // public cache pointer
unsigned long time_global;

/* function declaration */
void init_cache();
void evict_LRU();
web_object *create_web_object(char *content, char *url, size_t content_length);
void add_to_cache(web_object *wo);
web_object *find(char *url);
void evict_LRU();
void print_cache();