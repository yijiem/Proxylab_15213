/*
 * Implementation of the caching functionality of proxy.c
 */

#include "cache.h"
#include "csapp.h"

/*
 * initialize cache
 */
void init_cache()
{
    public_cache_pointer = (cache *) malloc(sizeof(cache) * 1);
    public_cache_pointer->head = NULL;
    public_cache_pointer->cache_size = 0;
    time_global = 0;
}

/*
 * create a web object based on content, url from brower, and content_length
 */
web_object *create_web_object(char *content, char *url, size_t content_length)
{
	web_object *wo = (web_object *) malloc(sizeof(web_object) * 1);
	wo->content = (char *) malloc(sizeof(char) * content_length);
    /* count for terminating charactor '\0'*/
	wo->url = (char *) malloc(sizeof(char) * strlen(url) + 1); 
	memcpy(wo->content, content, content_length);
	strcpy(wo->url, url);
    wo->size = content_length;
    wo->next = NULL;
    wo->access_time = time_global;
    return wo;
}

/*
 * add web_object to the front of the linked list
 * after adding, decide whether cache size is greater than MAX_CACHE_SIZE
 * if it is greater, then call evict_LRU()
 */
void add_to_cache(web_object *wo)
{
    if (public_cache_pointer->head == NULL) {
        /* no web object in current cache */
        public_cache_pointer->head = wo;
        public_cache_pointer->cache_size = wo->size;
        return;
    } else { 
    	web_object* oldFirst = public_cache_pointer->head;
    	public_cache_pointer->head = wo;
    	wo->next = oldFirst;
    	public_cache_pointer->cache_size += wo->size;
    	/* eviction until satisfies cache size requirement */
    	while (public_cache_pointer->cache_size > MAX_CACHE_SIZE) { 
    		evict_LRU();
    	}
    	return;
    }
}

/*
 * find certain web_object in cache based on url
 * return NULL, if no web object in current cache
 *                            or
 *              if certain web_object do not exist
 */
web_object *find(char *url)
{
    web_object *iter;
    /* increment global timer every time we search cache */
	time_global++;
    if (public_cache_pointer->head == NULL) { 
        return NULL;
    }
    for (iter = public_cache_pointer->head; 
                    iter != NULL; iter = iter->next) {
        if(!strcmp(iter->url, url)) { 
            /* found in cache */
        	iter->access_time = time_global;
        	return iter;
        }
    }
    /* not found */
    return NULL;
}

/*
 * search cache to find the least-recently-used web-object
 * and evict it from cache
 */
void evict_LRU()
{
    web_object *prevLRU; 
    web_object *prev;
    unsigned long LRU; /* access time of least recently used object */
    web_object *iter;
    web_object *secondObject;
    web_object *nextObject;
    
    /* search for LRU object */
    for (iter = public_cache_pointer->head; 
                            iter != NULL; iter = iter->next) {
        if (iter == public_cache_pointer->head) {
            /* first iteration, set LRU, prev, prevLRU */
        	LRU = iter->access_time;
        	prevLRU = NULL;
        	prev = iter;
        	continue;
        } else {
            if (iter->access_time < LRU) { 
                /* new potential LRU */
                LRU = iter->access_time;
                prevLRU = prev;
            }
            prev = iter;
        }
    }
    /* search complete, start eviction */
    if (prevLRU == NULL) {
    /* LRU is the first block of cache */
        secondObject = public_cache_pointer->head->next;
        /* update cache size */
        public_cache_pointer->cache_size -= public_cache_pointer->head->size;
        /* free memory */ 
        free(public_cache_pointer->head->content); 
        free(public_cache_pointer->head->url);
        free(public_cache_pointer->head);
        /* manage linked list */ 
        public_cache_pointer->head = secondObject;
    } else {
    	nextObject = prevLRU->next->next; 
        /* update cache size */
    	public_cache_pointer->cache_size -= prevLRU->next->size;
        /* free memory */
    	free(prevLRU->next->content);
    	free(prevLRU->next->url);
    	free(prevLRU->next);
        /* manage linked list */
    	prevLRU->next = nextObject;
    }
}

