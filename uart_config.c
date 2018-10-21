#include <espressif/esp_common.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <esp/uart.h>
#include <esp/uart_regs.h>

//#include <espressif/esp_wifi.h>
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

void uart_send_output(void *pvParameters){
    int i;
    char  open[]={0x55,0xfe,0xfe,0x03,0x01,0xb9,0x24};
    char close[]={0x55,0xfe,0xfe,0x03,0x02,0xf9,0x25};
    char pause[]={0x55,0xfe,0xfe,0x03,0x03,0x38,0xe5};
    vTaskDelay(1000); //wait 10 seconds
    while(1) {
        LOG(" open\n");for (i=0;i<7;i++) uart_putc(1, open[i]);uart_flush_txfifo(1);vTaskDelay(300);
        LOG("close\n");for (i=0;i<7;i++) uart_putc(1,close[i]);uart_flush_txfifo(1);vTaskDelay(300);
        LOG("pause\n");for (i=0;i<7;i++) uart_putc(1,pause[i]);uart_flush_txfifo(1);vTaskDelay(300);
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

//uint CRC16_2(QByteArray buf, int len) {}
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

void user_init(void){
    /* Activate UART for GPIO2 */
    gpio_set_iomux_function(2, IOMUX_GPIO2_FUNC_UART1_TXD);
    uart_set_baud(1, 9600);
    uart_set_baud(0, 9600);
    
/*    struct sdk_station_config config = {
        .ssid = "removed",
        .password = "removed",
    };
    // Required to call wifi_set_opmode before station_set_config.
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);/**/
    
    xTaskCreate(log_send, "logsend", 256, NULL, 4, NULL); //is prio4 a good idea??
    LOG("SDK version:%s\n", sdk_system_get_sdk_version());

    xTaskCreate(uart_parse_input, "parse", 256, NULL, 1, NULL);
    xTaskCreate(uart_send_output, "send",  256, NULL, 1, NULL);
}
