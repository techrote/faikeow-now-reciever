// Qualification-only command/observation loop. Actual production MCU adapters are included unchanged.
#define main fnr_unused_production_main
#include FNR_PRODUCTION_MAIN
#undef main
#include FNR_PRODUCTION_TRANSPORT
#include "tusb.h"
#include "pico/bootrom.h"
#include "pico/unique_id.h"
#include <stdio.h>
#include <string.h>
extern uint32_t q_reports,q_moving,q_button_reports;
extern int32_t q_x,q_y;
extern uint8_t q_last_buttons;
static char output[3072];static size_t output_size,output_offset;
static uint32_t irq_resume_at,acks,revokes;
static bool boot_staged,boot_ack,boot_peer,boot_profile,prior_ack;
static bool snapshot(const char *event){
 if(output_offset<output_size)return false;
 fnr_rp_provisioning_status p;fnr_rp_provisioning_snapshot(&p);
 fnr_receiver_core c;fnr_receiver_adapter_snapshot(&c);
 fnr_rp_transport_diagnostics t;fnr_transport_snapshot_foreground(&t);
 fnr_transport_radio_status r={0};bool have_radio=fnr_transport_unpack_status(&receiver.status_slot,&r)==FNR_TRANSPORT_OK;
 char uid[17];pico_get_unique_board_id_string(uid,sizeof(uid));
 int n=snprintf(output,sizeof(output),
 "{\"event\":\"%s\",\"identity\":\"FNR-007 RP observer v1\",\"source\":\"%s\",\"uid\":\"%s\",\"time_ms\":%lu,"
 "\"record_result\":%u,\"config_id\":\"0x%08lx%08lx\",\"generation\":%lu,\"channel\":%u,\"profile_id\":%u,\"profile_schema_version\":%u,\"platform_version\":%u,\"freshness_timeout_ms\":%lu,"
 "\"boot_staged\":%u,\"boot_ack\":%u,\"boot_peer\":%u,\"boot_profile\":%u,\"staged\":%u,\"ack\":%u,\"peer_enabled\":%u,\"profile_enabled\":%u,"
 "\"acks\":%lu,\"revokes\":%lu,\"usb_state\":%u,\"fresh\":%u,\"have_session\":%u,\"session\":\"%08lx%08lx\",\"last_sequence\":%lu,"
 "\"seen\":%lu,\"accepted\":%lu,\"unauthorized_peer\":%lu,\"wrong_profile\":%lu,\"duplicates\":%lu,\"stale\":%lu,\"timeouts\":%lu,\"transport_invalidations\":%lu,"
 "\"neutral_queued\":%lu,\"neutral_blocked\":%lu,\"mouse_inputs\":%lu,\"mouse_buttons\":%u,\"mouse_x\":%d,\"mouse_y\":%d,"
 "\"hid_reports\":%lu,\"hid_moving\":%lu,\"hid_button_reports\":%lu,\"hid_x\":%ld,\"hid_y\":%ld,\"hid_last_buttons\":%u,"
 "\"have_radio_status\":%u,\"radio_state\":%u,\"radio_channel\":%u,\"radio_received\":%lu,\"radio_accepted\":%lu,\"radio_rejected_source\":%lu,\"radio_dropped\":%lu,"
 "\"transport_frames\":%lu,\"status_frames\":%lu,\"datagram_frames\":%lu,\"link_timeouts\":%lu,\"bad_crc\":%lu,\"bad_cobs\":%lu,\"uart_overruns\":%lu}\n",
 event,FNR_SOURCE_ID,uid,(unsigned long)to_ms_since_boot(get_absolute_time()),
 (unsigned)p.record_result,(unsigned long)(p.config.config_id>>32),(unsigned long)p.config.config_id,(unsigned long)p.config.generation,p.config.channel,p.config.profile_id,p.config.profile_schema_version,p.config.platform_version,(unsigned long)p.config.freshness_timeout_ms,
 boot_staged,boot_ack,boot_peer,boot_profile,p.staged,p.radio_acknowledged,c.config.peer_enabled,c.config.profile_enabled,
 (unsigned long)acks,(unsigned long)revokes,c.usb_state,c.fresh,c.have_session,(unsigned long)(c.session>>32),(unsigned long)c.session,(unsigned long)c.last_sequence,
 (unsigned long)c.counters.datagrams_seen,(unsigned long)c.counters.accepted,(unsigned long)c.counters.unauthorized_peer,(unsigned long)c.counters.wrong_profile,(unsigned long)c.counters.duplicates,(unsigned long)c.counters.stale,(unsigned long)c.counters.freshness_timeouts,(unsigned long)c.counters.transport_invalidations,
 (unsigned long)fnr_usb_device.counters.neutral_reports_queued,(unsigned long)fnr_usb_device.counters.reports_blocked_by_neutral,(unsigned long)fnr_mouse_state.accepted_inputs,fnr_mouse_state.buttons,fnr_mouse_state.x,fnr_mouse_state.y,
 (unsigned long)q_reports,(unsigned long)q_moving,(unsigned long)q_button_reports,(long)q_x,(long)q_y,q_last_buttons,
 have_radio,r.state,r.channel,(unsigned long)r.radio.received,(unsigned long)r.radio.accepted,(unsigned long)r.radio.rejected_source,(unsigned long)r.radio.dropped,
 (unsigned long)t.parser.frames_valid,(unsigned long)t.parser.status_received,(unsigned long)t.parser.datagrams_received,(unsigned long)t.parser.link_timeouts,(unsigned long)t.parser.bad_crc,(unsigned long)t.parser.bad_cobs,(unsigned long)t.overruns);
 if(n<0||(size_t)n>=sizeof(output)){output_size=output_offset=0;return false;}
 output_size=(size_t)n;output_offset=0;return true;
}
static void command(const char *line){
 if(!strcmp(line,"INFO")||!strcmp(line,"STATS")){(void)snapshot("snapshot");return;}
 if(!strcmp(line,"BOOTSEL")){reset_usb_boot(0,0);return;}
 unsigned ms;int end=0;
 if(sscanf(line,"LINKOFF %u%n",&ms,&end)==1&&!line[end]&&ms>=3500&&ms<=6000&&!irq_resume_at){uart_set_irq_enables(uart0,false,false);irq_resume_at=to_ms_since_boot(get_absolute_time())+ms;(void)snapshot("linkoff");}
}
int main(void){
 fnr_rp_transport_init();
 if(!fnr_usb_profile_init()||!fnr_receiver_adapter_init(&fnr_usb_device,&fnr_mouse_state)||!fnr_rp_provisioning_init())for(;;)tight_loop_contents();
 fnr_rp_provisioning_status p;fnr_rp_provisioning_snapshot(&p);fnr_receiver_core c;fnr_receiver_adapter_snapshot(&c);
 boot_staged=p.staged;boot_ack=p.radio_acknowledged;boot_peer=c.config.peer_enabled;boot_profile=c.config.profile_enabled;
 static char line[32];static size_t used;static bool discard;
 for(;;){
  fnr_rp_transport_task();fnr_usb_adapter_task();
  fnr_receiver_provisioning_status a;fnr_receiver_adapter_provisioning_snapshot(&a);
  if(a.radio_acknowledged!=prior_ack){if(a.radio_acknowledged)++acks;else ++revokes;prior_ack=a.radio_acknowledged;}
  uint32_t now=to_ms_since_boot(get_absolute_time());
  if(irq_resume_at&&(int32_t)(now-irq_resume_at)>=0){uart_set_irq_enables(uart0,true,true);irq_resume_at=0;}
  if(output_offset<output_size){uint32_t room=tud_cdc_write_available();size_t left=output_size-output_offset;uint32_t count=(uint32_t)(left<room?left:room);if(count>128)count=128;if(count){output_offset+=tud_cdc_write(output+output_offset,count);tud_cdc_write_flush();}}
  for(unsigned i=0;i<16&&tud_cdc_available();++i){uint8_t ch;if(tud_cdc_read(&ch,1)!=1)break;if(ch=='\n'){if(!discard){line[used]=0;command(line);}used=0;discard=false;}else if(ch!='\r'){if(used+1<sizeof(line))line[used++]=(char)ch;else discard=true;}}
  tight_loop_contents();
 }
}
