#ifndef TR2_INPUT_H
#define TR2_INPUT_H
#include <stdint.h>
/* Tomb2.exe 4472f0 reads 256 DirectInput scan-code states; 44d9b0
   indexes those states directly. Translate Windows messages by physical
   keypad position, before binding capture, action lookup and key release. */
static int tr2_message_key(int key,uint32_t info){
 unsigned scan=(info>>16)&255,extended=(info>>24)&1;
 if(extended){if(scan==0x1c)return 0x6c; /* numpad Enter */
  if(scan==0x35)return 0x6f; /* numpad Divide */
 }else{
  /* Tomb2.exe 44dce2 checks the physical top row, independently of layout. */
  if(scan>=0x02 && scan<=0x08)return '1'+(int)scan-0x02;
  switch(scan){
  case 0x47:return 0x67;case 0x48:return 0x68;case 0x49:return 0x69;
  case 0x4b:return 0x64;case 0x4c:return 0x65;case 0x4d:return 0x66;
  case 0x4f:return 0x61;case 0x50:return 0x62;case 0x51:return 0x63;
  case 0x52:return 0x60;case 0x53:return 0x6e;
  case 0x37:return 0x6a;case 0x4a:return 0x6d;case 0x4e:return 0x6b;
  }
 }
 /* Also support synthetic messages without a scan code. */
 if(key==0x0d && extended)return 0x6c;
 return key;
}
#endif
