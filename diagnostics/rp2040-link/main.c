/* Temporary SRAM test. GPIO0/1 only; no radio reset or boot GPIO. */
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/bootrom.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "link_diag.h"
#include <stdio.h>
#define RING_SIZE 8192u
static uint8_t ring[RING_SIZE];
static volatile uint32_t head,tail,overruns,framing,parity,breaks,drops;
static diag_parser parser;
static uint32_t sent,echoes,streams,echo_next,stream_next,gaps,pattern_errors,payload_bytes;
static uint32_t radio_stats[9],radio_id;
static uint64_t started,next_send,ends,stops,report_at;
static bool running,draining,have_stats,seen_radio;
static void rx_irq(void) {
    while(uart_is_readable(uart0)) {
        uint32_t v=uart_get_hw(uart0)->dr;
        if(v&UART_UARTDR_OE_BITS) overruns++;
        if(v&UART_UARTDR_FE_BITS) framing++;
        if(v&UART_UARTDR_PE_BITS) parity++;
        if(v&UART_UARTDR_BE_BITS) breaks++;
        uint32_t next=(head+1u)&(RING_SIZE-1u);
        if(next==tail) drops++;
        else {ring[head]=(uint8_t)v; __dmb(); head=next;}
    }
}
static void out(uint8_t b) {uart_putc_raw(uart0,b);}
static void frame(uint8_t type,uint32_t seq,const uint8_t *data,unsigned n) {
    if(type==DIAG_INFO) {seen_radio=true; radio_id=seq;}
    if(type==DIAG_STATS && n==36) {
        for(unsigned i=0;i<9;i++) radio_stats[i]=diag_get32(data+4*i);
        have_stats=true;
    }
    if(!running && !draining) return;
    if(type==DIAG_ECHO || type==DIAG_STREAM) {
        bool stream=type==DIAG_STREAM;
        uint32_t *expected=stream?&stream_next:&echo_next;
        if(seq!=*expected) gaps++;
        *expected=seq+1;
        if(!diag_pattern(data,n,seq,stream)) pattern_errors++;
        if(stream) streams++; else echoes++;
        payload_bytes+=n;
    }
}
static void report(const char *event) {
    printf("{\"event\":\"%s\",\"duration_us\":%llu,\"sent\":%lu,\"echoes\":%lu,\"streams\":%lu,"
           "\"payload_rx_bytes\":%lu,\"gaps\":%lu,\"pattern_errors\":%lu,\"crc_errors\":%lu,"
           "\"length_errors\":%lu,\"parser_overflows\":%lu,\"uart_overruns\":%lu,\"uart_framing\":%lu,"
           "\"uart_parity\":%lu,\"uart_breaks\":%lu,\"ring_drops\":%lu,\"radio_seen\":%u,\"radio_id\":%lu,"
           "\"radio_stats_received\":%u,\"radio_stats\":[%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu]}\n",
           event,(unsigned long long)(started?time_us_64()-started:0),
           (unsigned long)sent,(unsigned long)echoes,(unsigned long)streams,(unsigned long)payload_bytes,
           (unsigned long)gaps,(unsigned long)pattern_errors,(unsigned long)parser.crc_errors,
           (unsigned long)parser.length_errors,(unsigned long)parser.overflows,
           (unsigned long)overruns,(unsigned long)framing,(unsigned long)parity,(unsigned long)breaks,
           (unsigned long)drops,seen_radio,(unsigned long)radio_id,have_stats,
           (unsigned long)radio_stats[0],(unsigned long)radio_stats[1],(unsigned long)radio_stats[2],
           (unsigned long)radio_stats[3],(unsigned long)radio_stats[4],(unsigned long)radio_stats[5],
           (unsigned long)radio_stats[6],(unsigned long)radio_stats[7],(unsigned long)radio_stats[8]);
}
static void start(unsigned seconds) {
    if(running || draining) {puts("BUSY"); return;}
    sent=echoes=streams=echo_next=stream_next=gaps=pattern_errors=payload_bytes=0;
    memset(&parser,0,sizeof(parser)); memset(radio_stats,0,sizeof(radio_stats));
    have_stats=seen_radio=false;
    // Clear stale startup UART data before the bounded qualification interval.
    irq_set_enabled(UART0_IRQ,false); tail=head;
    overruns=framing=parity=breaks=drops=0; irq_set_enabled(UART0_IRQ,true);
    started=time_us_64(); ends=started+(uint64_t)seconds*1000000u;
    next_send=started+100000; report_at=started+30000000;
    running=true; diag_send(out,DIAG_START,0,NULL,0); report("start");
}
int main(void) {
    uart_init(uart0,115200); uart_set_format(uart0,8,1,UART_PARITY_NONE);
    uart_set_hw_flow(uart0,false,false);
    gpio_set_drive_strength(0,GPIO_DRIVE_STRENGTH_2MA);
    gpio_set_function(0,GPIO_FUNC_UART); gpio_set_function(1,GPIO_FUNC_UART);
    gpio_disable_pulls(0); gpio_disable_pulls(1);
    irq_set_exclusive_handler(UART0_IRQ,rx_irq); irq_set_priority(UART0_IRQ,0x40);
    irq_set_enabled(UART0_IRQ,true); uart_set_irq_enables(uart0,true,false);
    stdio_init_all(); stdio_set_translate_crlf(&stdio_usb,false);
    char command[24]; unsigned used=0;
    for(;;) {
        while(tail!=head) {uint8_t b=ring[tail]; __dmb(); tail=(tail+1u)&(RING_SIZE-1u); diag_receive(&parser,b,frame);}
        int ch=getchar_timeout_us(0);
        if(ch>=0) {
            if(ch=='\n') {
                command[used]=0;
                if(!strcmp(command,"RUN10")) start(10);
                else if(!strcmp(command,"RUN600")) start(600);
                else if(!strcmp(command,"INFO")) {diag_send(out,DIAG_INFO,0,NULL,0); report("info");}
                else if(!strcmp(command,"BOOTSEL")) reset_usb_boot(0,0);
                else if(!strcmp(command,"RADIORESET") && !running && !draining) diag_send(out,DIAG_REBOOT,0,NULL,0);
                else puts("COMMANDS RUN10 RUN600 INFO BOOTSEL RADIORESET");
                used=0;
            } else if(used+1<sizeof(command)) command[used++]=(char)ch;
            else used=0;
        }
        uint64_t now=time_us_64();
        if(running && now>=ends) {running=false; draining=true; stops=now+2000000;}
        if(running && now>=next_send) {
            uint8_t data[250]; unsigned n=diag_length(sent);
            diag_payload(data,n,sent,false); diag_send(out,DIAG_PING,sent++,data,n);
            next_send=now+50000;
        }
        if(running && now>=report_at) {if(stdio_usb_connected()) report("progress"); report_at=now+30000000;}
        if(draining && now>=stops) {
            diag_send(out,DIAG_STOP,0,NULL,0); draining=false; stops=now+1000000;
        }
        if(!running && !draining && stops && (have_stats || now>=stops)) {report("end"); stops=0;}
        tight_loop_contents();
    }
}
