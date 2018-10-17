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

int buff[17]; //longest message + 1
int idx=0;
void uart_parse_input(void *pvParameters) {
    char ch;
    for(;;) {
        buff[idx++]=uart_getc(0);
        while (idx){
            if (!(buff[0]==0x8c || buff[0]==0x55)) {          shift_buff(1); continue;}
            if   (buff[0]==0x8c && buff[1]==0xfc)  {parse(2); shift_buff(2); continue;}
            if   (buff[0]==0x8c && idx>=2)         {          shift_buff(1); continue;}
            if (  buff[0]==0x55 && !(buff[1]==0xfe && buff[2]==0xfe) && idx>2 ) {
                                                              shift_buff(1); continue;} //now for sure 0x55fefe
            if (  buff[3]==0 || buff[3]>4 && idx>3){          shift_buff(1); continue;}
            if (  buff[4]==0 || buff[4]>9 && idx>4){          shift_buff(1); continue;}
            if (  buff[3]==3 && idx>=7 )  {
                if (crc(7))  {parse(7); shift_buff(7); continue;}
                if (crc(8))  {parse(8); shift_buff(8); continue;}
                if (idx>=8)  {          shift_buff(7); continue;} // failure for type 3 so flush it
            }
            if (  (buff[3]==1||buff[3]==2) && idx>=8 ) {
                if (crc(8))  {parse(8); shift_buff(8); continue;}
                if (crc(9))  {parse(9); shift_buff(9); continue;}
                if (idx>=9)  {          shift_buff(8); continue;} // failure for type 1 or 2 so flush it
            }
            if ( buff[3]==4 && idx==16)  {
                if (crc(16))  parse(16);
                shift_buff(16); continue;
            }
            if (idx==16) printf("something went wrong: idx=16!\n");
            break;
        }
    }
}

void uart_print_config(void *pvParameters){
    for(;;)
    {
        /* Get data */
        int baud = uart_get_baud(1);
        UART_StopBits stopbits = uart_get_stopbits(1);
        bool parity_enabled = uart_get_parity_enabled(1);
        UART_Parity parity = uart_get_parity(1);
        
        /* Print to UART0 */
        printf("Baud: %d ", baud);
        
        switch(stopbits){
        case UART_STOPBITS_0:
            printf("Stopbits: 0 ");
        break;
        case UART_STOPBITS_1:
            printf("Stopbits: 1 ");
        break;
        case UART_STOPBITS_1_5:
            printf("Stopbits: 1.5 ");
        break;
        case UART_STOPBITS_2:
            printf("Stopbits: 2");
        break;
        default:
            printf("Stopbits: Error");
        }
        
        printf("Parity bit enabled: %d ", parity_enabled);
        
        switch(parity){
        case UART_PARITY_EVEN:
            printf("Parity: Even");
        break;
        case UART_PARITY_ODD:
            printf("Parity: Odd");
        break;
        default:
            printf("Parity: Error");
        }
        
        printf("\n");
        
        vTaskDelay(1000.0 / portTICK_PERIOD_MS);
    }
}

void user_init(void){
    /* Activate UART for GPIO2 */
    gpio_set_iomux_function(2, IOMUX_GPIO2_FUNC_UART1_TXD);
    uart_set_baud(1, 9600);
    uart_set_baud(0, 9600);
    
    printf("SDK version:%s\n", sdk_system_get_sdk_version());
    
//    xTaskCreate(uart_send_data, "tsk1", 256, NULL, 2, NULL);

    xTaskCreate(uart_parse_input, "parse", 256, NULL, 1, NULL);
}
