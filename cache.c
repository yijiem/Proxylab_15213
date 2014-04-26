#include "cache.h"
//#define MAX_CACHE_SIZE 45 // 15 * 3

/*
int main() {
	init_cache();

    find("http://www.cmu.edu");
    web_object *cmu = create_web_object("<HTTP>cmu<HTTP>", "http://www.cmu.edu", 15);
    add_to_cache(cmu);

    find("http://www.ucb.edu");
    web_object *ucb = create_web_object("<HTTP>ucb<HTTP>", "http://www.ucb.edu", 15);
    add_to_cache(ucb);

    find("http://www.mit.edu");
	web_object *mit = create_web_object("<HTTP>mit<HTTP>", "http://www.mit.edu", 15);
	add_to_cache(mit);

    print_cache();

	find("http://www.cmu.edu");

    find("http://www.stanford.edu");
    web_object *stanford = create_web_object("<HTTP>stanford<HTTP>", "http://www.stanford.edu", 20);
    add_to_cache(stanford);
    print_cache();
}
*/

/*
 * for debugging
 */
void print_cache()
{
    web_object *iter;
	for (iter = public_cache_pointer->head; iter != NULL; iter = iter->next) {
		printf("web content = %s\n", iter->content);
		printf("url = %s\n", iter->url);
		printf("access time = %d\n", iter->access_time);
	}
}


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
 * create a web object based on content from web service, url from brower, and content_length
 */
web_object *create_web_object(char *content, char *url, size_t content_length)
{
	web_object *wo = (web_object *) malloc(sizeof(web_object) * 1);
	wo->content = (char *) malloc(sizeof(char) * content_length);
	wo->url = (char *) malloc(sizeof(char) * strlen(url) + 1);
	memcpy(wo->content, content, content_length);
	strcpy(wo->url, url);
    wo->size = content_length;
    wo->next = NULL;
    wo->access_time = time_global;
    return wo;
}

/*
 * add web_object to the first element in cache linked list
 * after adding, decide whether cache size is greated than MAX_CACHE_SIZE
 *               if it is greater, then call evict_LRU()
 */
void add_to_cache(web_object *wo)
{
    if (public_cache_pointer->head == NULL) { // no web object in current cache
        public_cache_pointer->head = wo;
        public_cache_pointer->cache_size = wo->size;
        return;
    } else { // current cache has web object
    	web_object* oldFirst = public_cache_pointer->head;
    	public_cache_pointer->head = wo;
    	wo->next = oldFirst;
    	public_cache_pointer->cache_size += wo->size;
    	// if size after adding exceeds MAX_CACHE_SIZE, then needs to evict LRU until range is appropriate
    	while (public_cache_pointer->cache_size > MAX_CACHE_SIZE) { // use while instead of if
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
	time_global++;
    if (public_cache_pointer->head == NULL) { // no web object in current cache
        return NULL;
    }
    web_object *iter;
    for (iter = public_cache_pointer->head; iter != NULL; iter = iter->next) {
        if(!strcmp(iter->url, url)) { // url has found
        	iter->access_time = time_global;
        	return iter;
        }
    }
    return NULL;
}

/*
 * search cache to find the least-recently-used web-object, and evict it from the cache
 */
void evict_LRU()
{
    web_object *prevLRU; // the pointer to the previous object of the least-recently-used object
    web_object *prev;
    unsigned long LRU; // time of the least recently used object
    web_object *iter;
    // search for LRU object
    for (iter = public_cache_pointer->head; iter != NULL; iter = iter->next) {
        if (iter == public_cache_pointer->head) { // first iteration, set LRU, prev, prevLRU
        	LRU = iter->access_time;
        	prevLRU = NULL;
        	prev = iter;
        	continue;
        } else {
            if (iter->access_time < LRU) { // new potential LRU
                LRU = iter->access_time;
                prevLRU = prev;
            }
            prev = iter;
        }
    }
    // search complete, start eviction
    if (prevLRU == NULL) {
    // LRU is the first block of cache
        web_object *secondObject = public_cache_pointer->head->next;
        public_cache_pointer->cache_size -= public_cache_pointer->head->size; // substract content size
        free(public_cache_pointer->head->content); // free content inside web content struct
        free(public_cache_pointer->head->url);
        free(public_cache_pointer->head); // free web content struct
        public_cache_pointer->head = secondObject;
    } else {
    	web_object *nextObject = prevLRU->next->next; // block next to the LRU block
    	public_cache_pointer->cache_size -= prevLRU->next->size;
    	free(prevLRU->next->content);
    	free(prevLRU->next->url);
    	free(prevLRU->next);
    	prevLRU->next = nextObject;
    }
}

