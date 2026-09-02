#include "P2000.h"
#include "p2wp_device.h"
#include "page_backend.h"

#include <SDL.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WIDTH=40*6*3, HEIGHT=24*10*3, FONT_BYTES=2240 };
static SDL_Window *window; static SDL_Renderer *renderer; static SDL_Texture *texture;
static uint32_t pixels[HEIGHT][WIDTH]; static uint8_t font_data[FONT_BYTES];
static const uint32_t palette[8]={0xff000000,0xffff0000,0xff00ff00,0xffffff00,
  0xff0000ff,0xffff00ff,0xff00ffff,0xffffffff};
static int running=1, headless=0, auto_keys=0, auto_source=1;
static const char *auto_custom_server;
static const char *auto_action;
static int auto_pause_frame=-1,auto_resume_frame=-1;
static int protocol_version=0;
static int status_length=0;
static const char *dump_path,*dump_frame_path,*dump_fetches_path;
static uint8_t queued_keys[16];static size_t queued_head,queued_tail;

void PicoCard_Out(byte port,byte value){p2wp_device_out(port,value);} byte PicoCard_In(byte port){return p2wp_device_in(port);}
void EmulatorControl_Out(byte port,byte value){(void)value;if(port==0xfc)queued_head=queued_tail=0;}
byte EmulatorControl_In(byte port){if(port==0xfd)return queued_head!=queued_tail;if(port==0xfe&&queued_head!=queued_tail)return queued_keys[queued_head++%sizeof(queued_keys)];return 0;}
int InitMachine(void){return 1;} void TrashMachine(void){} void Sound(int v){(void)v;} void FlushSound(void){}
void SyncEmulation(void){} void Pause(int ms){SDL_Delay((unsigned)ms);} char *GetResourcesPath(void){return ".";}
char *GetDocumentsPath(void){return ".";} void ShowErrorMessage(const char *f,...){fprintf(stderr,"%s\n",f);}
int LoadFont(const char *name){FILE *f=fopen(name,"rb"); if(!f)return 0; int ok=fread(font_data,1,sizeof(font_data),f)==sizeof(font_data);fclose(f);return ok;}
void PutChar(int x,int y,int c,int fg,int bg,int dh){
  if(x<0||x>=40||y<0||y>=24||c<0||c>=224)return;
  for(int oy=0;oy<10;oy++){int sy=dh==1?oy/2:dh==2?5+oy/2:oy;uint8_t bits=font_data[c*10+sy];
    for(int ox=0;ox<6;ox++){uint32_t color=palette[(bits&(0x20>>ox))?(fg&7):(bg&7)];
      for(int yy=0;yy<3;yy++)for(int xx=0;xx<3;xx++)pixels[y*30+oy*3+yy][x*18+ox*3+xx]=color;}}
}
void PutImage(void){if(headless)return;SDL_UpdateTexture(texture,NULL,pixels,WIDTH*4);SDL_RenderClear(renderer);SDL_RenderCopy(renderer,texture,NULL,NULL);SDL_RenderPresent(renderer);}

static int keycode(SDL_Keycode key){
  if(key>='a'&&key<='z'){static const uint8_t map[26]={34,29,28,12,36,15,13,9,70,14,62,65,30,25,49,53,3,39,11,37,38,31,35,27,33,10};return map[key-'a'];}
  if(key>='0'&&key<='9'){static const uint8_t map[10]={45,46,63,4,7,5,1,6,54,41};return map[key-'0'];}
  switch(key){case SDLK_SPACE:return 17;case SDLK_RETURN:return 52;case SDLK_BACKSPACE:return 44;
    case SDLK_SLASH:return 61;case SDLK_PERIOD:return 57;case SDLK_MINUS:return 47;
    case SDLK_SEMICOLON:case SDLK_COLON:return 71;
    case SDLK_LEFT:return 0;case SDLK_UP:return 2;case SDLK_DOWN:return 21;case SDLK_RIGHT:return 23;
    case SDLK_LSHIFT:return 72;case SDLK_RSHIFT:return 79;case SDLK_KP_ENTER:return 16;default:return -1;}
}
void Keyboard(void){if(headless)return;SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_QUIT)running=0;
  if(e.type==SDL_KEYDOWN&&!e.key.repeat&&e.key.keysym.sym==SDLK_F11){WarmReset();continue;}
  if(e.type==SDL_KEYDOWN&&!e.key.repeat&&e.key.keysym.sym==SDLK_F12){ColdReset();continue;}
  if(e.type==SDL_KEYDOWN||e.type==SDL_KEYUP){int code=keycode(e.key.keysym.sym);if(code>=0){int row=code/8,bit=code%8;
    if(e.type==SDL_KEYDOWN)KeyMap[row]&=(byte)~(1u<<bit);else KeyMap[row]|=(byte)(1u<<bit);}}}}

static void set_matrix_key(int code,int down){int row=code/8,bit=code%8;if(down)KeyMap[row]&=(byte)~(1u<<bit);else KeyMap[row]|=(byte)(1u<<bit);}
static int screen_has(const char *text){size_t n=strlen(text);for(int row=0;row<24;row++)for(int col=0;col+ (int)n<=40;col++)if(!memcmp(VRAM+row*80+col,text,n))return 1;return 0;}
static void inject_key(int code){if(code<80)set_matrix_key(code,1);queued_keys[queued_tail++%sizeof(queued_keys)]=(uint8_t)code;}
static int ascii_keycode(char value){
  if(value>='A'&&value<='Z')value=(char)(value-'A'+'a');
  if(value>='a'&&value<='z'){static const uint8_t map[26]={34,29,28,12,36,15,13,9,70,14,62,65,30,25,49,53,3,39,11,37,38,31,35,27,33,10};return map[value-'a'];}
  if(value>='0'&&value<='9'){static const uint8_t map[10]={45,46,63,4,7,5,1,6,54,41};return map[value-'0'];}
  switch(value){case ':':return 71;case '/':return 61;case '.':return 57;case '-':return 47;case '?':return 133;default:return -1;}
}
static void automatic_keyboard(int frame){static int stage,pressed=-1,release_frame,legacy_warning_seen;
  static size_t custom_position;
  if(pressed>=0&&frame>=release_frame){set_matrix_key(pressed,0);pressed=-1;}
  int code=-1;
  if(stage==0&&frame>=5){code=17;stage++;}
  else if(stage==1&&!legacy_warning_seen&&frame>=30&&screen_has("COMPATIBILITEITSMODUS")){code=17;legacy_warning_seen=1;}
  else if(stage==1&&screen_has("Emulated WiFi")){code=46;stage++;}
  else if(stage==2&&screen_has("WIFI-PROFIEL BEWAREN")){code=25;stage++;}
  else if(stage==3&&frame>=150&&screen_has("KIES BRON (0-2)")){code=auto_source==2?63:auto_source==1?46:45;stage=auto_source==0?4:5;}
  else if(stage==4&&pressed<0&&auto_source==0&&screen_has("ADRES (MAX. 96 TEKENS)")){
    if(auto_custom_server&&auto_custom_server[custom_position])code=ascii_keycode(auto_custom_server[custom_position++]);
    else{code=52;stage=5;}}
  else if(stage==5&&auto_action&&RAM&&RAM[0x18c7]){ /* cartridge page-valid byte at 0x78c7 */
    code=!strcmp(auto_action,"START")?128:ascii_keycode(auto_action[0]);stage=6;}
  else if(stage>=5&&(frame==auto_pause_frame||frame==auto_resume_frame)){code=34;}
  if(code>=0){fprintf(stderr,"auto: frame %d key %d stage %d\n",frame,code,stage);inject_key(code);if(code<80){pressed=code;release_frame=frame+4;}}
}
static int dump_screen(void){
  if(!dump_path)return 1;
  FILE *f=fopen(dump_path,"wb");if(!f)return 0;
  for(int row=0;row<24;row++){if(fwrite(VRAM+row*80,1,40,f)!=40){fclose(f);return 0;}}
  fclose(f);return 1;
}
static int dump_frame(void){if(!dump_frame_path)return 1;FILE *f=fopen(dump_frame_path,"wb");if(!f)return 0;
  int ok=fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels);fclose(f);return ok;}
static int dump_fetches(const struct page_backend *backend){
  if(!dump_fetches_path)return 1;
  FILE *f=fopen(dump_fetches_path,"wb");if(!f)return 0;
  size_t count=backend->request_count<sizeof(backend->requested_subpages)?backend->request_count:sizeof(backend->requested_subpages);
  int ok=fwrite(backend->requested_subpages,1,count,f)==count;fclose(f);return ok;
}

static int read_exact(const char *path,uint8_t *dst,size_t size){FILE *f=fopen(path,"rb");if(!f){fprintf(stderr,"%s: %s\n",path,strerror(errno));return 0;}int ok=fread(dst,1,size,f)==size&&fgetc(f)==EOF;fclose(f);return ok;}
int main(int argc,char **argv){const char *monitor=NULL,*cart=NULL,*font="emulator/assets/Default.fnt",*fixture=NULL;int live=0,frames=0;
  for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--monitor")&&++i<argc)monitor=argv[i];else if(!strcmp(argv[i],"--cartridge")&&++i<argc)cart=argv[i];
    else if(!strcmp(argv[i],"--font")&&++i<argc)font=argv[i];else if(!strcmp(argv[i],"--fixture")&&++i<argc)fixture=argv[i];
    else if(!strcmp(argv[i],"--live"))live=1;else if(!strcmp(argv[i],"--headless"))headless=1;else if(!strcmp(argv[i],"--frames")&&++i<argc)frames=atoi(argv[i]);
    else if(!strcmp(argv[i],"--auto"))auto_keys=1;else if(!strcmp(argv[i],"--auto-source")&&++i<argc)auto_source=atoi(argv[i]);
    else if(!strcmp(argv[i],"--custom-server")&&++i<argc)auto_custom_server=argv[i];
    else if(!strcmp(argv[i],"--auto-key")&&++i<argc)auto_action=argv[i];
    else if(!strcmp(argv[i],"--p2wp-version")&&++i<argc)protocol_version=atoi(argv[i]);
    else if(!strcmp(argv[i],"--p2wp-status-length")&&++i<argc)status_length=atoi(argv[i]);
    else if(!strcmp(argv[i],"--auto-pause-frame")&&++i<argc)auto_pause_frame=atoi(argv[i]);
    else if(!strcmp(argv[i],"--auto-resume-frame")&&++i<argc)auto_resume_frame=atoi(argv[i]);
    else if(!strcmp(argv[i],"--dump-screen")&&++i<argc)dump_path=argv[i];
    else if(!strcmp(argv[i],"--dump-frame")&&++i<argc)dump_frame_path=argv[i];
    else if(!strcmp(argv[i],"--dump-fetches")&&++i<argc)dump_fetches_path=argv[i];else{fprintf(stderr,"bad argument: %s\n",argv[i]);return 2;}}
  if(!monitor||!cart||auto_source<0||auto_source>2||(auto_source==0&&auto_keys&&!auto_custom_server)||protocol_version<0||protocol_version>255||(status_length!=0&&status_length!=5&&status_length!=9&&status_length!=13&&status_length!=17)){fprintf(stderr,"Usage: p2000t-emulator --monitor ROM --cartridge ROM [--live|--fixture JSON] [--auto-source 0|1|2 --custom-server URL] [--p2wp-version N] [--p2wp-status-length 5|9|13|17]\n");return 2;}
  uint8_t m[4096],c[16384];if(!read_exact(monitor,m,sizeof(m))||!read_exact(cart,c,sizeof(c)))return 1;
  if(SDL_Init(headless?SDL_INIT_TIMER:SDL_INIT_VIDEO))return 1;
  if(!headless){window=SDL_CreateWindow("P2000T Teletekst",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,WIDTH,HEIGHT,0);renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);if(!renderer)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);texture=renderer?SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,WIDTH,HEIGHT):NULL;if(!window||!renderer||!texture)return 1;}
  FontName=font;EightyColumnsCard=0;Z80_IPeriod=2500000/50;UPeriod=1;if(!InitP2000(m,c))return 1;
  struct page_backend backend={.fixture=fixture,.live=live};p2wp_device_init(page_backend_fetch,&backend);if(protocol_version)p2wp_device_set_protocol_range((uint8_t)protocol_version,(uint8_t)protocol_version);if(status_length)p2wp_device_set_status_length((uint8_t)status_length);OutputReg|=0x40;
  Z80_Regs regs;Z80_GetRegs(&regs);regs.PC.D=0x1010;Z80_SetRegs(&regs);
  for(int frame=0;running&&(!frames||frame<frames);frame++){if(auto_keys)automatic_keyboard(frame);Z80_Execute();if(!headless)SDL_Delay(20);}
  RefreshScreen();if(auto_keys)fprintf(stderr,"auto: final PC=0x%04x queue=%zu\n",Z80_GetPC(),queued_tail-queued_head);
  int ok=dump_screen()&&dump_frame()&&dump_fetches(&backend);TrashP2000();SDL_Quit();return ok?0:1;
}
