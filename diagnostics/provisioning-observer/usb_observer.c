// Qualification-only observation of successful submissions, with a distinct composite USB identity.
#include "tusb.h"
#include <stdint.h>
#include <string.h>
uint32_t q_reports,q_moving,q_button_reports;
int32_t q_x,q_y;
uint8_t q_last_buttons;
static bool observe_report(uint8_t instance,uint8_t id,void const *data,uint16_t length) {
    bool result=tud_hid_n_report(instance,id,data,length);
    if(result&&length==3){const uint8_t *p=data;++q_reports;if(p[1]||p[2])++q_moving;if(p[0])++q_button_reports;q_x+=(int8_t)p[1];q_y+=(int8_t)p[2];q_last_buttons=p[0];}
    return result;
}
#define tud_hid_n_report observe_report
#define tud_descriptor_device_cb fnr_original_device_descriptor
#define tud_descriptor_configuration_cb fnr_original_configuration_descriptor
#define tud_descriptor_string_cb fnr_original_string_descriptor
#include FNR_PRODUCTION_USB
#undef tud_hid_n_report
#undef tud_descriptor_device_cb
#undef tud_descriptor_configuration_cb
#undef tud_descriptor_string_cb
static tusb_desc_device_t const device={
 .bLength=sizeof(tusb_desc_device_t),.bDescriptorType=TUSB_DESC_DEVICE,.bcdUSB=0x0200,
 .bDeviceClass=TUSB_CLASS_MISC,.bDeviceSubClass=MISC_SUBCLASS_COMMON,.bDeviceProtocol=MISC_PROTOCOL_IAD,
 .bMaxPacketSize0=64,.idVendor=0xcafe,.idProduct=0xf007,.bcdDevice=0x0100,
 .iManufacturer=1,.iProduct=2,.iSerialNumber=3,.bNumConfigurations=1};
static uint8_t const config[]={
 TUD_CONFIG_DESCRIPTOR(1,3,0,TUD_CONFIG_DESC_LEN+TUD_CDC_DESC_LEN+TUD_HID_DESC_LEN,0,100),
 TUD_CDC_DESCRIPTOR(0,0,0x82,8,0x03,0x83,64),
 TUD_HID_DESCRIPTOR(2,0,HID_ITF_PROTOCOL_MOUSE,50,0x81,8,1)};
uint8_t const *tud_descriptor_device_cb(void){return (uint8_t const*)&device;}
uint8_t const *tud_descriptor_configuration_cb(uint8_t index){return index==0?config:NULL;}
uint16_t const *tud_descriptor_string_cb(uint8_t index,uint16_t language){
 (void)language;static uint16_t out[64];static const char*strings[]={NULL,"FNR Project","FNR-007 Qualification Observer","FNR007A"};
 if(index==0){out[0]=0x0304;out[1]=0x0409;return out;}
 if(index>=4)return NULL;size_t n=strlen(strings[index]);if(n>63)n=63;
 for(size_t i=0;i<n;++i)out[i+1]=(uint8_t)strings[index][i];out[0]=(uint16_t)(0x0300|((n+1)*2));return out;
}
