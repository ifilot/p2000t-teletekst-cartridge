#include "p2wp_device.h"
#include "../../firmware/src/p2wp.h"

#include <stdbool.h>
#include <string.h>

enum { TX_PORT=0x40, RX_PORT=0x41, STATUS_PORT=0x42, SCREEN_SIZE=960,
       CHUNK_SIZE=240 };

static p2wp_parser_t parser;
static p2wp_fetch_fn fetch_page;
static void *fetch_context;
static uint8_t encoded[P2WP_MAX_ENCODED];
static size_t encoded_length, encoded_position;
static uint8_t screen[SCREEN_SIZE], next_subpage, local_clock[7];
static uint8_t fetch_state, fetch_error;
static uint8_t profile_state;
static uint8_t protocol_minimum=P2WP_MIN_VERSION;
static uint8_t protocol_maximum=P2WP_MAX_VERSION;
static uint8_t session_version=P2WP_BOOTSTRAP_VERSION;
static uint8_t status_length_override;

static void respond(const p2wp_frame_t *request, const uint8_t *payload,
                    size_t length) {
    p2wp_frame_t reply = { .version=request->type==P2WP_TYPE_HELLO
            ? P2WP_BOOTSTRAP_VERSION : session_version,
        .flags=P2WP_FLAG_RESPONSE, .type=request->type,
        .sequence=request->sequence, .payload_length=(uint16_t)length };
    if (length) memcpy(reply.payload, payload, length);
    encoded_length = p2wp_encode(&reply, encoded, sizeof(encoded));
    encoded_position = 0;
}

static void respond_error(const p2wp_frame_t *request,uint8_t error) {
    p2wp_frame_t reply = { .version=request->version,
        .flags=P2WP_FLAG_RESPONSE|P2WP_FLAG_ERROR, .type=request->type,
        .sequence=request->sequence, .payload_length=1, .payload={error} };
    encoded_length=p2wp_encode(&reply,encoded,sizeof(encoded));encoded_position=0;
}

static void dispatch(const p2wp_frame_t *request) {
    uint8_t payload[P2WP_MAX_PAYLOAD] = {0};
    switch (request->type) {
    case P2WP_TYPE_HELLO: {
        if(request->payload_length!=8||memcmp(request->payload,"P2WP",4)||
           request->payload[4]>request->payload[5]){
            respond_error(request,P2WP_ERROR_INVALID_PAYLOAD);break;
        }
        uint8_t selected=p2wp_select_version(request->payload[4],request->payload[5],
                                             protocol_minimum,protocol_maximum);
        if(!selected){respond_error(request,P2WP_ERROR_UNSUPPORTED_VERSION);break;}
        session_version=selected;
        const uint8_t hello[]={'P','2','W','P',selected,15,240,0};
        respond(request, hello, sizeof(hello)); break;
    }
    case P2WP_TYPE_WIFI_PROFILE_STATUS:
        payload[0]=profile_state; payload[1]=0; respond(request,payload,2); break;
    case P2WP_TYPE_ECHO:
        respond(request,request->payload,request->payload_length); break;
    case P2WP_TYPE_WIFI_SCAN_START:
    case P2WP_TYPE_WIFI_CONNECT:
        respond(request,NULL,0); break;
    case P2WP_TYPE_WIFI_PROFILE_SAVE:
        profile_state=1; respond(request,NULL,0); break;
    case P2WP_TYPE_WIFI_PROFILE_DELETE:
        profile_state=0; respond(request,NULL,0); break;
    case P2WP_TYPE_WIFI_PROFILE_CONNECT:
        respond(request,NULL,0); break;
    case P2WP_TYPE_WIFI_SCAN_STATUS:
        payload[0]=2; payload[1]=1; payload[2]=1; respond(request,payload,3); break;
    case P2WP_TYPE_WIFI_SCAN_RESULT: {
        const char ssid[]="Emulated WiFi";
        payload[0]=request->payload[0]; payload[1]=(uint8_t)-35;
        payload[2]=0; payload[3]=sizeof(ssid)-1;
        memcpy(payload+4,ssid,sizeof(ssid)-1);
        respond(request,payload,4+sizeof(ssid)-1); break;
    }
    case P2WP_TYPE_WIFI_STATUS:
        payload[0]=2; respond(request,payload,1); break;
    case P2WP_TYPE_TELETEKST_FETCH_START: {
        uint16_t page=request->payload[0]|((uint16_t)request->payload[1]<<8);
        fetch_error = fetch_page && fetch_page(fetch_context,request->payload[3],
            page,request->payload[2],screen,&next_subpage,local_clock)==0 ? 0:7;
        fetch_state=fetch_error?4:3; respond(request,NULL,0); break;
    }
    case P2WP_TYPE_TELETEKST_FETCH_STATUS:
        payload[0]=fetch_state; payload[1]=fetch_error; payload[4]=next_subpage;
        if(session_version>=3||status_length_override>=9){memcpy(payload+5,local_clock,3);
          payload[8]=fetch_error?0:1;memcpy(payload+9,local_clock+3,4);
          respond(request,payload,status_length_override?status_length_override:13);
        }else respond(request,payload,status_length_override?status_length_override:5);break;
    case P2WP_TYPE_TELETEKST_FETCH_ROWS:
        respond(request,screen+(size_t)request->payload[0]*CHUNK_SIZE,CHUNK_SIZE); break;
    default: break;
    }
}

void p2wp_device_init(p2wp_fetch_fn fetch, void *context) {
    fetch_page=fetch; fetch_context=context; p2wp_device_reset();
}
void p2wp_device_set_protocol_range(uint8_t minimum,uint8_t maximum) {
    protocol_minimum=minimum;protocol_maximum=maximum;p2wp_device_reset();
}
void p2wp_device_set_status_length(uint8_t length) { status_length_override=length; }
void p2wp_device_reset(void) {
    p2wp_parser_init(&parser); encoded_length=encoded_position=0;
    session_version=P2WP_BOOTSTRAP_VERSION;
    fetch_state=fetch_error=next_subpage=profile_state=0; memset(screen,0,sizeof(screen));
}
void p2wp_device_out(uint8_t port, uint8_t value) {
    if (port!=TX_PORT) return;
    p2wp_frame_t request;
    if (p2wp_parser_feed(&parser,value,&request)==P2WP_PARSE_FRAME) dispatch(&request);
}
uint8_t p2wp_device_in(uint8_t port) {
    if (port==STATUS_PORT) return 2u | (encoded_position<encoded_length?1u:0u);
    if (port==RX_PORT && encoded_position<encoded_length) return encoded[encoded_position++];
    return 0xff;
}
