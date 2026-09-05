#include "page_backend.h"
#include "../../firmware/src/custom_endpoint.h"
#include "../../firmware/src/p2wp.h"
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
unsigned char page_backend_fetch(void *argument,unsigned char source,
                       const char *custom_url,unsigned short page,
                       unsigned char subpage,unsigned char screen[960],
                       unsigned char *next,unsigned short *previous,
                       unsigned short *following,unsigned char clock[7]) {
    struct page_backend *backend=argument; struct buffer body={0};
    unsigned char error=P2WP_TELETEKST_ERROR_INVALID_DATA;
    char url[224];
    if(source==P2WP_TELETEKST_SOURCE_CUSTOM){
        custom_endpoint_t endpoint;char path[CUSTOM_ENDPOINT_REQUEST_PATH_MAX];
        if(!custom_url||!custom_endpoint_parse(custom_url,strlen(custom_url),&endpoint)||
           !custom_endpoint_page_path(&endpoint,page,subpage,path,sizeof(path)))
          return P2WP_TELETEKST_ERROR_INVALID_DATA;
        const char *scheme=endpoint.tls?"https":"http";
        unsigned default_port=endpoint.tls?443u:80u;
        int length=endpoint.port==default_port
          ?snprintf(url,sizeof(url),"%s://%s%s",scheme,endpoint.host,path)
          :snprintf(url,sizeof(url),"%s://%s:%u%s",scheme,endpoint.host,
                    endpoint.port,path);
        if(length<0||(size_t)length>=sizeof(url))
          return P2WP_TELETEKST_ERROR_REQUEST_START;
    }else{
        const char *base=source==P2WP_TELETEKST_SOURCE_P2000T
          ?"https://teletekst.philips-p2000t.nl"
          :source==P2WP_TELETEKST_SOURCE_ARCHIVE
            ?"https://teletekstarchief.nl":"https://teletekst-data.nos.nl";
        int length=snprintf(url,sizeof(url),subpage?"%s/json/%u-%u":"%s/json/%u",
                            base,page,subpage);
        if(length<0||(size_t)length>=sizeof(url))
          return P2WP_TELETEKST_ERROR_REQUEST_START;
    }
    if(backend->request_count<sizeof(backend->requested_subpages)) {
        backend->requested_subpages[backend->request_count]=subpage;
        backend->requested_pages[backend->request_count]=page;
        backend->requested_sources[backend->request_count]=source;
    }
    backend->request_count++;
    if(backend->fail_page==page)return backend->fail_error;
    if(backend->fixture) error=read_file(backend->fixture,&body)==0
      ?P2WP_TELETEKST_ERROR_NONE:P2WP_TELETEKST_ERROR_INVALID_DATA;
    else if(backend->live) {
        CURL *curl=curl_easy_init(); if(curl) {
            curl_easy_setopt(curl,CURLOPT_URL,url); curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,append);
            curl_easy_setopt(curl,CURLOPT_WRITEDATA,&body); curl_easy_setopt(curl,CURLOPT_TIMEOUT,20L);
            if(source==P2WP_TELETEKST_SOURCE_CUSTOM){curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0L);
              curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);}
            CURLcode curl_result=curl_easy_perform(curl);long status=0;
            curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
            if(curl_result==CURLE_OK)
              error=status==404?P2WP_TELETEKST_ERROR_PAGE_NOT_FOUND:
                status==200?P2WP_TELETEKST_ERROR_NONE:
                P2WP_TELETEKST_ERROR_HTTP_STATUS;
            else if(curl_result==CURLE_COULDNT_RESOLVE_HOST)
              error=P2WP_TELETEKST_ERROR_DNS;
            else if(curl_result==CURLE_OPERATION_TIMEDOUT)
              error=P2WP_TELETEKST_ERROR_TIMEOUT;
            else if(curl_result==CURLE_OUT_OF_MEMORY)
              error=P2WP_TELETEKST_ERROR_OUT_OF_MEMORY;
            else if(curl_result==CURLE_PARTIAL_FILE)
              error=P2WP_TELETEKST_ERROR_CONTENT_LENGTH;
            else if(curl_result==CURLE_RECV_ERROR||curl_result==CURLE_GOT_NOTHING)
              error=P2WP_TELETEKST_ERROR_CONNECTION_CLOSED;
            else error=P2WP_TELETEKST_ERROR_CONNECT;
            curl_easy_cleanup(curl);
        }else error=P2WP_TELETEKST_ERROR_OUT_OF_MEMORY;
    }
    if(!error){teletekst_metadata_t metadata;
      if(!teletekst_decode_json(body.data,body.length,page,screen,&metadata))
        error=P2WP_TELETEKST_ERROR_INVALID_DATA;
      else{*next=metadata.next_subpage;*previous=metadata.previous_page;
        *following=metadata.next_page;}}
    if(!error && backend->fixture && subpage!=0) *next=0;
    free(body.data);
    time_t now=time(NULL); struct tm local; localtime_r(&now,&local);
    clock[0]=(unsigned char)local.tm_hour; clock[1]=(unsigned char)local.tm_min;
    clock[2]=(unsigned char)local.tm_sec; clock[3]=(unsigned char)local.tm_mday;
    clock[4]=(unsigned char)(local.tm_mon+1); clock[5]=(unsigned char)(local.tm_year-100);
    clock[6]=(unsigned char)local.tm_wday;
    return error;
}
