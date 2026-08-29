#include "page_backend.h"
#include "../../firmware/src/teletekst.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct buffer { char *data; size_t length; };
static size_t append(void *data,size_t size,size_t count,void *argument) {
    struct buffer *b=argument; size_t n=size*count;
    char *next=realloc(b->data,b->length+n+1); if(!next) return 0;
    b->data=next; memcpy(b->data+b->length,data,n); b->length+=n;
    b->data[b->length]=0; return n;
}
static int read_file(const char *path,struct buffer *b) {
    FILE *f=fopen(path,"rb"); if(!f) return -1;
    if(fseek(f,0,SEEK_END)|| (b->length=(size_t)ftell(f),fseek(f,0,SEEK_SET))) { fclose(f); return -1; }
    b->data=malloc(b->length+1); if(!b->data) { fclose(f); return -1; }
    int ok=fread(b->data,1,b->length,f)==b->length; fclose(f);
    b->data[b->length]=0; return ok?0:-1;
}
int page_backend_fetch(void *argument,unsigned char source,unsigned short page,
                       unsigned char subpage,unsigned char screen[960],
                       unsigned char *next,unsigned char clock[7]) {
    struct page_backend *backend=argument; struct buffer body={0}; int result=-1;
    if(backend->fixture) result=read_file(backend->fixture,&body);
    else if(backend->live) {
        char url[160]; const char *host=source?"teletekst.philips-p2000t.nl":"teletekst-data.nos.nl";
        snprintf(url,sizeof(url),subpage?"https://%s/json/%u-%u":"https://%s/json/%u",host,page,subpage);
        CURL *curl=curl_easy_init(); if(curl) {
            curl_easy_setopt(curl,CURLOPT_URL,url); curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,append);
            curl_easy_setopt(curl,CURLOPT_WRITEDATA,&body); curl_easy_setopt(curl,CURLOPT_TIMEOUT,20L);
            result=curl_easy_perform(curl)==CURLE_OK?0:-1; curl_easy_cleanup(curl);
        }
    }
    if(!result && !teletekst_decode_nos_json(body.data,body.length,page,screen,next)) result=-1;
    free(body.data);
    time_t now=time(NULL); struct tm local; localtime_r(&now,&local);
    clock[0]=(unsigned char)local.tm_hour; clock[1]=(unsigned char)local.tm_min;
    clock[2]=(unsigned char)local.tm_sec; clock[3]=(unsigned char)local.tm_mday;
    clock[4]=(unsigned char)(local.tm_mon+1); clock[5]=(unsigned char)(local.tm_year-100);
    clock[6]=(unsigned char)local.tm_wday;
    return result;
}
