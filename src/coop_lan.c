/* Caves LAN prototype. Peers exchange one bounded input frame per simulation
   tick. Only inputs cross the wire; addresses and engine structs never do. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <string.h>
#include "coop_lan.h"

static SOCKET listener=INVALID_SOCKET,peer=INVALID_SOCKET;
static int connecting,started,queued,sent,received;
static uint32_t sequence,tx[5],rx[5];
static const char *failure="Connection closed.";

const char *coop_lan_error(void){return failure;}
void coop_lan_close(void){
    if(peer!=INVALID_SOCKET)closesocket(peer);
    if(listener!=INVALID_SOCKET)closesocket(listener);
    if(started)WSACleanup();
    peer=listener=INVALID_SOCKET;
    started=connecting=queued=sent=received=0;sequence=0;
}
int coop_lan_open(const char *address){
    WSADATA w;
    failure="Could not initialise LAN networking.";
    if(WSAStartup(MAKEWORD(2,2),&w))return 0;
    started=1;
    SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(s==INVALID_SOCKET)return 0;
    peer=s;
    u_long nonblocking=1;
    if(ioctlsocket(s,FIONBIO,&nonblocking))return 0;
    struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_port=htons(27971);
    if(!address){
        int exclusive=1;
        setsockopt(s,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(const char *)&exclusive,sizeof exclusive);
        failure="Could not listen on TCP port 27971. Is another host running?";
        if(bind(s,(const struct sockaddr *)&a,sizeof a) || listen(s,1))return 0;
        listener=s;peer=INVALID_SOCKET;
    }else{
        failure="Enter the host PC's IPv4 address.";
        if(inet_pton(AF_INET,address,&a.sin_addr)!=1)return 0;
        connecting=connect(s,(const struct sockaddr *)&a,sizeof a)!=0;
        failure="Could not reach the host. Start the host first and check its LAN address.";
        if(connecting && WSAGetLastError()!=WSAEWOULDBLOCK)return 0;
    }
    return 1;
}
static int ready(void){
    if(listener!=INVALID_SOCKET){
        peer=accept(listener,NULL,NULL);
        if(peer==INVALID_SOCKET)return WSAGetLastError()==WSAEWOULDBLOCK?0:-1;
        closesocket(listener);listener=INVALID_SOCKET;
    }else if(connecting){
        fd_set write_set,errors;FD_ZERO(&write_set);FD_SET(peer,&write_set);errors=write_set;
        struct timeval timeout={0};
        int result=select(0,NULL,&write_set,&errors,&timeout);
        if(result<=0)return result;
        if(FD_ISSET(peer,&errors))return -1;
        connecting=0;
    }
    if(!sequence && !queued){
        int yes=1;setsockopt(peer,IPPROTO_TCP,TCP_NODELAY,(const char *)&yes,sizeof yes);
    }
    return 1;
}
static int exchange(uint32_t signature,uint32_t state,uint32_t input,uint32_t *local,uint32_t *remote){
    int result=ready();if(result!=1)return result;
    failure="The other player disconnected.";
    if(!queued){
        tx[0]=htonl(0x54524202);tx[1]=htonl(signature);tx[2]=htonl(sequence);
        tx[3]=htonl(state);tx[4]=htonl(input);queued=1;
    }
    if(sent<(int)sizeof tx){
        int n=send(peer,(const char *)tx+sent,(int)sizeof tx-sent,0);
        if(n<=0)return n<0 && WSAGetLastError()==WSAEWOULDBLOCK?0:-1;
        sent+=n;
    }
    if(received<(int)sizeof rx){
        int n=recv(peer,(char *)rx+received,(int)sizeof rx-received,0);
        if(n<=0)return n<0 && WSAGetLastError()==WSAEWOULDBLOCK?0:-1;
        received+=n;
    }
    if(sent!=(int)sizeof tx || received!=(int)sizeof rx)return 0;
    failure="Both players need the same build and level data.";
    if(rx[0]!=tx[0] || rx[1]!=tx[1])return -1;
    failure="The two games lost sync. Start a new session.";
    if(rx[2]!=tx[2] || rx[3]!=tx[3])return -1;
    *local=ntohl(tx[4]);*remote=ntohl(rx[4]);

    queued=sent=received=0;++sequence;return 1;
}

int coop_lan_frame(uint32_t signature,uint32_t state,uint32_t input,uint32_t *local,uint32_t *remote){
    int r=exchange(signature,state,input,local,remote);
    if(r==1 && (*remote & ~(COOP_MOVEMENT_MASK|COOP_SMALL_PACK|COOP_LARGE_PACK)))return -1;
    return r;
}
int coop_lan_setup(uint32_t build,unsigned stage,uint32_t value,uint32_t *remote){
    uint32_t local;
    return exchange(build,0x434f0000u+stage,value,&local,remote);
}
