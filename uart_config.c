/* A basic example that demonstrates how UART can be configured.
   Outputs some test data with 100baud, 1.5 stopbits, even parity bit to UART1
   (GPIO2; D4 for NodeMCU boards)

   This sample code is in the public domain.
 */

#include <espressif/esp_common.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <esp/uart.h>
#include <esp/uart_regs.h>

void uart_send_data(void *pvParameters){
    /* Activate UART for GPIO2 */
    gpio_set_iomux_function(2, IOMUX_GPIO2_FUNC_UART1_TXD);
    
    /* Set baud rate of UART1 to 100 (so it's easier to measure) */
    uart_set_baud(1, 9600);
    
    /* Set to 1.5 stopbits */
    uart_set_stopbits(1, UART_STOPBITS_1);
    
    /* Enable parity bit */
    uart_set_parity_enabled(1, false);
    
    /* Set parity bit to even */
    //uart_set_parity(1, UART_PARITY_EVEN);
    
    /* Repeatedly send some example packets */
    for(;;)
    {
        uart_putc(1, 0B00000000);
        uart_putc(1, 0B00000001);
        uart_putc(1, 0B10101010);
        uart_flush_txfifo(1);
        vTaskDelay(4000/portTICK_PERIOD_MS);
    }
}

int buff[16]={0x55,0xfe,0xfe,0x03,0x01,0xb9,0x24,0,0,0,0,0,0,0,0,0}; //longest message
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
    if (positions<4) printf("%02x%02x\n",buff[0],buff[1]);
    else {
        for (i=3;i<positions-2;i++) printf("%02x",buff[i]);
        printf("\n");
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
    for(;;) {
        buff[idx++]=uart_getc(0);
        while (idx){
            if (!(buff[0]==0x8c || buff[0]==0x55))  {          shift_buff(1); continue;}
            if   (buff[0]==0x8c && buff[1]==0xfc)   {parse(2); shift_buff(2); continue;}
            if   (buff[0]==0x8c && idx>=2)          {          shift_buff(1); continue;}
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
            if (idx==16) printf("something went wrong: idx=16!\n");
            break;
        }
    }
}

void user_init(void){
    /* Activate UART for GPIO2 */
    gpio_set_iomux_function(2, IOMUX_GPIO2_FUNC_UART1_TXD);
    uart_set_baud(1, 9600);
    uart_set_baud(0, 9600);
    
    printf("SDK version:%s\n", sdk_system_get_sdk_version());
    
//    xTaskCreate(uart_send_data, "tsk1", 256, NULL, 2, NULL);
printf("%04x\n",crc16(5));
    //xTaskCreate(uart_parse_input, "parse", 256, NULL, 1, NULL);
}
