/*
 * Header file for cache.c
 * Declares structs and functions used in cache.c
 */

#include <stdio.h>
#include <stdlib.h>

#define MAX_CACHE_SIZE 1049000

/* defines the structure for web object stored in cache */
typedef struct web_object
{
	char *content; 				/* actual web object content(value) */
	char *url;     				/* browser url(key) */
	size_t size;   				/* size of content */
	struct web_object *next;    /* pointer to next web_object */
	unsigned long access_time; 	/* access time for LRU judgement */
}web_object;

/* defines the structure for cache */
typedef struct cache
{
	web_object *head;  /* head pointer to first web_object */        
	size_t cache_size; /* current size of the cache */
}cache;

/* global variable */
cache *public_cache_pointer; 
unsigned long time_global;

/* function declaration */
void init_cache();
void evict_LRU();
web_object *create_web_object(char *content, char *url, size_t content_length);
void add_to_cache(web_object *wo);
web_object *find(char *url);
void evict_LRU();