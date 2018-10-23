/*  (c) 2018 HomeAccessoryKid
 *  This example drives a curtain motor as offered on e.g. alibaba
 *  with the brand of Aqara. It uses any ESP8266 with as little as 1MB flash. 
 */

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
//#include <espressif/esp_system.h> //for timestamp report only
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#include <string.h>
#include "lwip/api.h"

//use nc -kulnw0 8005 to collect this output
#define LOG(message, ...)  sprintf(string+strlen(string),message, ##__VA_ARGS__)
char string[1450]={0}; //in the end I do not know to prevent overflow, so I use the max size of 1 UDP packet
void log_send(void *pvParameters){
    struct netconn* conn;
    int i=0,len;
    
    while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) vTaskDelay(100);
    // Create UDP connection
    conn = netconn_new(NETCONN_UDP);
    if (netconn_bind(   conn, IP_ADDR_ANY,       8004) != ERR_OK) netconn_delete(conn);
    if (netconn_connect(conn, IP_ADDR_BROADCAST, 8005) != ERR_OK) netconn_delete(conn);
    
    while(1){
        len=strlen(string);
        if ((!i && len) || len>1000) {
            struct netbuf* buf = netbuf_new();
            void* data = netbuf_alloc(buf,len);
            memcpy (data,string,len);
            string[0]=0; //there is a risk of new LOG to add to string after we measured len
            if (netconn_send(conn, buf) == ERR_OK) netbuf_delete(buf);
            i=10;
        }
        if (!i) i=10;
        i--;
        vTaskDelay(1); //with len>1000 and delay=10ms, we can handle 800kbps input
    }
}

bool  hold=0,calibrate=0,reverse=0;
bool  changed=0;
int  target=0,current=0,state=2; //homekit values

// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section

#include "ota-api.h"
homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  "X");
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "1");
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         "Z");
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  "0.0.0");

// next use these two lines before calling homekit_server_init(&config);
//    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
//                                      &model.value.string_value,&revision.value.string_value);
//    config.accessories[0]->config_number=c_hash;
// end of OTA add-in instructions


homekit_value_t calibrate_get() {
    return HOMEKIT_BOOL(calibrate);
}
void calibrate_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid calibrate-value format: %d\n", value.format);
        return;
    }
    calibrate = value.bool_value;
    changed=1;
    printf("Calibrate: %d\n", calibrate);
    if (calibrate) {
        //homekit_characteristic_notify(&on, HOMEKIT_BOOL(calibrate)); //publish mode (not yet supported in accessory defintion)
    }
}

#define HOMEKIT_CHARACTERISTIC_CUSTOM_CALIBRATED HOMEKIT_CUSTOM_UUID("F0000004")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CALIBRATED(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_CALIBRATED, \
    .description = "Calibrate(d)", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
                 | homekit_permissions_paired_write \
                 | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

homekit_characteristic_t calibrated = HOMEKIT_CHARACTERISTIC_(CUSTOM_CALIBRATED, 0, .setter=calibrate_set, .getter=calibrate_get);


homekit_value_t reverse_get() {
    return HOMEKIT_BOOL(reverse);
}
void reverse_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid reverse-value format: %d\n", value.format);
        return;
    }
    reverse = value.bool_value;
    changed=1;
    printf("Reverse: %d\n", reverse);
    if (reverse) {
        //homekit_characteristic_notify(&on, HOMEKIT_BOOL(calibrate)); //publish mode (not yet supported in accessory defintion)
    }
}

#define HOMEKIT_CHARACTERISTIC_CUSTOM_REVERSED HOMEKIT_CUSTOM_UUID("F0000005")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_REVERSED(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_REVERSED, \
    .description = "Reversed", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
                 | homekit_permissions_paired_write \
                 | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__

homekit_characteristic_t reversed = HOMEKIT_CHARACTERISTIC_(CUSTOM_REVERSED, 0, .setter=reverse_set, .getter=reverse_get);



void uart_send_output(void *pvParameters){
    int i;
    char  open[]={0x55,0xfe,0xfe,0x03,0x01,0xb9,0x24};
    char close[]={0x55,0xfe,0xfe,0x03,0x02,0xf9,0x25};
    char pause[]={0x55,0xfe,0xfe,0x03,0x03,0x38,0xe5};
    vTaskDelay(1000); //wait 10 seconds
    while(1) {
        LOG(" open\n");for (i=0;i<7;i++) uart_putc(1, open[i]);uart_flush_txfifo(1);vTaskDelay(500);
        LOG("close\n");for (i=0;i<7;i++) uart_putc(1,close[i]);uart_flush_txfifo(1);vTaskDelay(500);
        LOG("pause\n");for (i=0;i<7;i++) uart_putc(1,pause[i]);uart_flush_txfifo(1);vTaskDelay(500);
    }
}

int buff[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //longest message
int idx=0;

void shift_buff(int positions) {
    int i;
    for (i=positions;i<16;i++) {
        buff[i-positions]=buff[i];
    }
    for (i=16-positions;i<16;i++) buff[i]=0;
    idx-=positions;
}

void parse(int positions) {
    int i=0;
    if (positions<4) LOG("%02x%02x\n",buff[0],buff[1]);
    else {
        for (i=3;i<positions-2;i++) LOG("%02x",buff[i]);
        LOG("\n");
    }
}

uint  crc16(int len) {   
  uint crc = 0xFFFF;

  for (int pos = 0; pos < len; pos++)
  {
    crc ^= (uint)buff[pos];          // XOR byte into least sig. byte of crc

    for (int i = 8; i != 0; i--) {    // Loop over each bit
      if ((crc & 0x0001) != 0) {      // If the LSB is set
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      }
      else                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
    }
  }
  // Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)
  return crc;
}

void uart_parse_input(void *pvParameters) {
    //LOG("%04x\n",crc16(5));

    //int i;
    for(;;) {
        buff[idx++]=uart_getc(0);
        //for (i=1;i<idx;i++) LOG("   ");
        //LOG("v%d\n",idx);
        while (idx){
            //for (i=0;i<16;i++) LOG("%02x.",buff[i]);
            //LOG("   %d\n",idx);
            if (!(buff[0]==0x88 || buff[0]==0x55))  {          shift_buff(1); continue;}
            if   (buff[0]==0x88 && buff[1]==0xf8)   {parse(2); shift_buff(2); continue;}
            if   (buff[0]==0x88 && idx>=2)          {          shift_buff(1); continue;}
            if (  buff[0]==0x55 && !(buff[1]==0xfe && buff[2]==0xfe) && idx>2 ) {
                                                               shift_buff(1); continue;} //now for sure 0x55fefe
            if ( (buff[3]==0 || buff[3]>4) && idx>3){          shift_buff(1); continue;}
            if ( (buff[4]==0 || buff[4]>9) && idx>4){          shift_buff(1); continue;}
            if (  buff[3]==3 && idx>=7 )  {
                if (!crc16(7)) {parse(7); shift_buff(7); continue;}
                if (!crc16(8)) {parse(8); shift_buff(8); continue;}
                if (idx>=8)    {          shift_buff(7); continue;} // failure for type 3 so flush it
            }
            if (  (buff[3]==1||buff[3]==2) && idx>=8 ) {
                if (!crc16(8)) {parse(8); shift_buff(8); continue;}
                if (!crc16(9)) {parse(9); shift_buff(9); continue;}
                if (idx>=9)    {          shift_buff(8); continue;} // failure for type 1 or 2 so flush it
            }
            if ( buff[3]==4 && idx==16)  {
                if (!crc16(16)) parse(16);
                shift_buff(16); continue;
            }
            if (idx==16) LOG("something went wrong: idx=16!\n");
            break;
        }
    }
}

void motor_init() {
    /* Activate UART for GPIO2 */
    gpio_set_iomux_function(2, IOMUX_GPIO2_FUNC_UART1_TXD);
    uart_set_baud(1, 9600);
    uart_set_baud(0, 9600);

    xTaskCreate(uart_parse_input, "parse", 256, NULL, 1, NULL);
    xTaskCreate(uart_send_output, "send",  256, NULL, 1, NULL);
    //xTaskCreate(motor_loop_task, "loop", 512, NULL, 1, NULL);
}


homekit_value_t hold_get() {
    return HOMEKIT_BOOL(on);
}
void hold_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid hold-value format: %d\n", value.format);
        return;
    }
    //printf("H:%3d @ %d\n",value.bool_value,sdk_system_get_time());
    printf("H:%3d\n",value.bool_value);
    hold = value.bool_value;
    changed=1;
}

homekit_value_t target_get() {
    return HOMEKIT_UINT8(target);
}
void target_set(homekit_value_t value) {
    if (value.format != homekit_format_uint8) {
        printf("Invalid target-value format: %d\n", value.format);
        return;
    }
    //printf("T:%3d @ %d\n",value.int_value,sdk_system_get_time());
    printf("T:%3d\n",value.int_value);
    target = value.int_value;
    changed=1;
}

homekit_value_t current_get() {
    return HOMEKIT_UINT8(current);
}
void current_set(homekit_value_t value) {
    if (value.format != homekit_format_uint8) {
        printf("Invalid current-value format: %d\n", value.format);
        return;
    }
    //printf("C:%3d @ %d\n",value.int_value,sdk_system_get_time());
    printf("C:%3d\n",value.int_value);
    current = value.int_value;
    changed=1;
}

homekit_value_t state_get() {
    return HOMEKIT_UINT8(state);
}
void state_set(homekit_value_t value) {
    if (value.format != homekit_format_uint8) {
        printf("Invalid state-value format: %d\n", value.format);
        return;
    }
    //printf("S:%3d @ %d\n",value.int_value,sdk_system_get_time());
    printf("S:%3d\n",value.int_value);
    state = value.int_value;
    changed=1;
}

void identify_task(void *_args) {
    int oldmode;
    oldmode=mode;
    mode=10; changed=1;
    vTaskDelay(5000 / portTICK_PERIOD_MS); //5 sec
    mode=oldmode; changed=1;
    vTaskDelete(NULL);
}

void identify(homekit_value_t _value) {
    printf("Identify\n");
    xTaskCreate(identify_task, "identify", 256, NULL, 2, NULL);
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1,
        .category=homekit_accessory_category_window_covering,
        .services=(homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "AQARA-curtain-motor-ZNCLDJ11M"),
                    &manufacturer,
                    &serial,
                    &model,
                    &revision,
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
                    NULL
                }),
            HOMEKIT_SERVICE(WINDOW_COVERING, .primary=true,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "Curtain"),
                    HOMEKIT_CHARACTERISTIC(
                        CURRENT_POSITION, 0,
                        .getter=current_get,
                        .setter=current_set
                    ),
                    HOMEKIT_CHARACTERISTIC(
                        TARGET_POSITION, 0,
                        .getter=target_get,
                        .setter=target_set
                    ),
                    HOMEKIT_CHARACTERISTIC(
                        POSITION_STATE, 2,
                        .getter=state_get,
                        .setter=state_set
                    ),
                    HOMEKIT_CHARACTERISTIC(
                        HOLD_POSITION, 0,
                        .getter=hold_get,
                        .setter=hold_set
                    ),
                    &ota_trigger,
                    &calibrated,
                    &reversed,
                    NULL
                }),
            NULL
        }),
    NULL
};


homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void user_init(void) {
    xTaskCreate(log_send, "logsend", 256, NULL, 4, NULL); //is prio4 a good idea??
    LOG("Aqara Curtain Motor SDK version:%s\n", sdk_system_get_sdk_version());

    motor_init();

    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
                                      &model.value.string_value,&revision.value.string_value);
    //c_hash=1013; revision.value.string_value="0.1.13"; //cheat line
    config.accessories[0]->config_number=c_hash;
    
    homekit_server_init(&config);
}
