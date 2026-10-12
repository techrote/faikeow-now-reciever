// Bounded FNR-007 synthetic sender. Secrets are supplied only by private_pair.h.
#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
#include <user_interface.h>
#include <espnow.h>
}
#include "private_pair.h"
#include "fnr/radio_auth.h"
#include "radio_auth_crypto.h"
static uint8_t original_mac[6];
static bool radio_active, sending, finishing;
static volatile bool outstanding;
static volatile uint32_t tx_ok, tx_fail;
static uint32_t attempted, queued, enqueue_fail, remaining, sequence, interval, due, finish_at;
static uint64_t session;
static unsigned channel, identity_mode, key_mode, profile, buttons;
static int dx, dy;
static void mac_print(const uint8_t *m) {
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x",m[0],m[1],m[2],m[3],m[4],m[5]);
}
static void tx_done(uint8_t *,uint8_t result) {
    if (result==0) ++tx_ok; else ++tx_fail;
    outstanding=false;
}
static void status(const char *event) {
    uint8_t mac[6];WiFi.macAddress(mac);
    Serial.printf("{\"event\":\"%s\",\"identity\":\"FNR-007 bounded sender v2\",\"mac\":\"",event);
    mac_print(mac);Serial.print("\",\"receiver_mac\":\"");mac_print(receiver_mac);
    Serial.printf("\",\"channel\":%u,\"identity_mode\":%u,\"key_mode\":%u,\"profile\":%u,"
                  "\"session\":\"%08lx%08lx\",\"next_sequence\":%lu,\"attempted\":%lu,"
                  "\"queued\":%lu,\"enqueue_fail\":%lu,\"tx_ok\":%lu,\"tx_fail\":%lu,"
                  "\"outstanding\":%u,\"sending\":%u,\"key_present\":true}\n",
                  unsigned(wifi_get_channel()),identity_mode,key_mode,profile,
                  (unsigned long)(session>>32),(unsigned long)session,(unsigned long)sequence,
                  (unsigned long)attempted,(unsigned long)queued,(unsigned long)enqueue_fail,
                  (unsigned long)tx_ok,(unsigned long)tx_fail,unsigned(outstanding),unsigned(sending));
}
static void stop() {
    sending=finishing=false;remaining=0;
    if(radio_active){esp_now_unregister_send_cb();esp_now_deinit();}
    radio_active=outstanding=false;WiFi.mode(WIFI_OFF);
}
static void execute(const char *line) {
    if(!strcmp(line,"INFO")||!strcmp(line,"STATS")){status("status");return;}
    if(!strcmp(line,"OFF")){stop();status("off");return;}
    unsigned ch,id,km,pr,seq,count,spacing,bt;int x,y,end=0;
    if(sscanf(line,"TX %u %u %u %u %u %u %u %u %d %d%n",&ch,&id,&km,&pr,&seq,&count,&spacing,&bt,&x,&y,&end)!=10||line[end]||
       ch<1||ch>14||id>1||km>7||pr<1||pr>255||count<1||count>500||spacing<20||spacing>1000||
       count*spacing>30000||bt>3||x< -127||x>127||y< -127||y>127||sending||outstanding) {
        Serial.println("{\"event\":\"error\",\"stage\":\"command\"}");return;
    }
    stop();channel=ch;identity_mode=id;key_mode=km;profile=pr;sequence=seq;remaining=count;interval=spacing;buttons=bt;dx=x;dy=y;
    attempted=queued=enqueue_fail=tx_ok=tx_fail=0;
    WiFi.persistent(false);WiFi.setAutoReconnect(false);WiFi.mode(WIFI_STA);
    wifi_station_set_auto_connect(0);wifi_station_disconnect();
    uint8_t source[6];memcpy(source,original_mac,6);
    if(id){source[0]|=2;source[5]^=0x80;}
    if(!wifi_set_macaddr(STATION_IF,source)||!wifi_set_channel((uint8_t)ch)||wifi_get_channel()!=ch||esp_now_init()!=0){
        stop();Serial.println("{\"event\":\"error\",\"stage\":\"radio_init\"}");return;
    }
    radio_active=true;uint8_t key[16];memcpy(key,peer_key,16);if(km==1)key[0]^=0x80;
    int result=esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    result|=esp_now_register_send_cb(tx_done);
    const bool plaintext = km==2 || km==4 || km==7;
    result|=esp_now_add_peer(receiver_mac,ESP_NOW_ROLE_SLAVE,(uint8_t)ch,plaintext?nullptr:key,plaintext?0:16);
    volatile uint8_t *wipe=key;for(unsigned i=0;i<16;++i)wipe[i]=0;
    if(result){stop();Serial.println("{\"event\":\"error\",\"stage\":\"peer_init\"}");return;}
    system_phy_set_max_tpw(82);sending=true;due=millis()+100;status("tx_started");
}
void setup() {
    Serial.setRxBufferSize(256);Serial.begin(115200);Serial.setDebugOutput(false);
    WiFi.persistent(false);WiFi.setAutoReconnect(false);WiFi.mode(WIFI_STA);WiFi.macAddress(original_mac);WiFi.mode(WIFI_OFF);
    session=((uint64_t)os_random()<<32)|os_random();if(!session)session=1;
    status("boot");
}
void loop() {
    static char line[128];static unsigned used;static bool discard;
    for(unsigned i=0;i<32&&Serial.available();++i){
        int ch=Serial.read();
        if(ch=='\n'){if(!discard){line[used]=0;execute(line);}else Serial.println("{\"event\":\"error\",\"stage\":\"overflow\"}");used=0;discard=false;}
        else if(ch!='\r'){if(used+1<sizeof(line))line[used++]=(char)ch;else discard=true;}
    }
    uint32_t now=millis();
    if(sending&&outstanding&&(int32_t)(now-due)>2000){stop();Serial.println("{\"event\":\"error\",\"stage\":\"callback_timeout\"}");}
    if(sending&&!outstanding&&(int32_t)(now-due)>=0){
        uint8_t packet[22]={1,(uint8_t)profile,1,0};
        for(unsigned i=0;i<8;++i)packet[4+i]=(uint8_t)(session>>(8*i));
        for(unsigned i=0;i<4;++i)packet[12+i]=(uint8_t)(sequence>>(8*i));
        packet[16]=4;packet[18]=1;packet[19]=(uint8_t)buttons;packet[20]=(uint8_t)dx;packet[21]=(uint8_t)dy;
        uint8_t wire[44]; size_t wire_size=0;
        fnr_auth_context context={config_id,config_generation,{0},{0}};
        WiFi.macAddress(context.sender);memcpy(context.receiver,receiver_mac,6);
        if(key_mode==6)++context.generation;
        uint8_t auth_key[16];memcpy(auth_key,peer_key,16);
        if(key_mode==1||key_mode==4)auth_key[0]^=0x80;
        bool wrapped=fnr_auth_wrap(&context,fnr_auth_crypto_hmac,auth_key,
                                   packet,sizeof(packet),wire,sizeof(wire),&wire_size);
        volatile uint8_t *wipe=auth_key;for(unsigned i=0;i<16;++i)wipe[i]=0;
        if(!wrapped){stop();Serial.println("{\"event\":\"error\",\"stage\":\"authentication\"}");return;}
        if(key_mode==5)wire[wire_size-1]^=1;
        const bool bare=key_mode==2||key_mode==3;
        ++attempted;outstanding=true;
        if(esp_now_send(receiver_mac,bare?packet:wire,(uint8_t)(bare?sizeof(packet):wire_size))==0)++queued;else{++enqueue_fail;outstanding=false;}
        ++sequence;due=now+interval;
        if(--remaining==0){sending=false;finishing=true;finish_at=now+2000;}
    }
    if(finishing&&(!outstanding||(int32_t)(now-finish_at)>=0)){finishing=false;status("tx_done");}
    delay(1);
}
