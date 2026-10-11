// Temporary FNR-002 UART integrity test; no peers, credentials or RF traffic.
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <link_diag.h>
static diag_parser parser;
static uint32_t accepted, pattern_errors, gaps, next_ping, stream_seq, overrun, rx_error;
static uint32_t next_send;
static bool running;
static void out(uint8_t b) { Serial.write(b); }
static void info() {
    const char text[]="FNR-002 ESP link diagnostic v1";
    diag_send(out,DIAG_INFO,ESP.getChipId(),(const uint8_t *)text,sizeof(text)-1);
}
static void stats() {
    uint8_t data[36];
    const uint32_t values[]={accepted,pattern_errors,gaps,stream_seq,overrun,rx_error,
                            parser.crc_errors,parser.length_errors,parser.overflows};
    for(unsigned i=0;i<9;i++) diag_put32(data+4*i,values[i]);
    diag_send(out,DIAG_STATS,0,data,sizeof(data));
}
static void frame(uint8_t type,uint32_t seq,const uint8_t *data,unsigned n) {
    if(type==DIAG_START && n==0) {
        accepted=pattern_errors=gaps=next_ping=stream_seq=overrun=rx_error=0;
        parser.crc_errors=parser.length_errors=parser.overflows=0;
        running=true; next_send=millis()+50; info();
    } else if(type==DIAG_PING && running) {
        if(seq!=next_ping) gaps++;
        next_ping=seq+1;
        if(!diag_pattern(data,n,seq,false)) pattern_errors++;
        accepted++; diag_send(out,DIAG_ECHO,seq,data,n);
    } else if(type==DIAG_STOP && n==0) {running=false; stats();}
    else if(type==DIAG_INFO && n==0) info();
    else if(type==DIAG_REBOOT && n==0) {Serial.flush(); ESP.restart();}
}
void setup() {
    WiFi.persistent(false); WiFi.mode(WIFI_OFF); WiFi.forceSleepBegin();
    Serial.setRxBufferSize(2048); Serial.begin(115200); Serial.setDebugOutput(false);
    delay(20); info();
}
void loop() {
    while(Serial.available()) diag_receive(&parser,(uint8_t)Serial.read(),frame);
    if(Serial.hasOverrun()) overrun++;
    if(Serial.hasRxError()) rx_error++;
    uint32_t now=millis();
    if(running && (int32_t)(now-next_send)>=0) {
        uint8_t data[250]; unsigned n=diag_length(stream_seq);
        diag_payload(data,n,stream_seq,true); diag_send(out,DIAG_STREAM,stream_seq++,data,n);
        next_send=now+50;
    }
    yield();
}
