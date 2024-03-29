#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "uart0.h"
#include "tm4c123gh6pm.h"
/**
 * NEC IR RECEIVER FIRMWARE: ABIRIA PLACIDE, Dec, 2020
 */

/*
 * bit bat aliases
 */

#define INPUT_SIGNAL  (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 1*4))) //PORT E1 (input signal from IR receiver)
#define OUTPUT_SIGNAL (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 2*4))) //PORT E2 ( output signal to logic analyzer)

#define GREEN_LED     (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 3*4))) //PORT F3
#define BLUE_LED      (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 2*4))) //PORT F2

#define PORTE_INPUT     (1 << 1)
#define PORTE_OUTPUT    (1 << 2)

#define GREEN_LED_MASK  (1 << 3)
#define BLUE_LED_MASK   (1 << 2)

// Global shared variables between lab7_pwm_modulation.c file
uint8_t databitsCopy[32];
bool data_ready = 1; //if signal is found it will change to zero
bool ALERTBADON = 0;
bool ALERTGOODON = 0;
#define BLUE_BL_LED_MASK 16
//def. prototypes

void printInfo(uint8_t databits[32], uint8_t range1, uint8_t range0);

//start of lab6-receiver
void initLab6()
{
    putsUart0("\r\n\twaiting for IR signal...\r\n");
    data_ready = 1; //reinitialize for learn command in lab7.c file
    // Configure HW to work with 16 MHz XTAL, PLL enabled, system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Set GPIO ports to use APB (not needed since default configuration -- for clarity)
    // Note UART on port A must use APB
    //SYSCTL_GPIOHBCTL_R = 0;

    // Enable clocks
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R4| SYSCTL_RCGCGPIO_R5; //enable clocks for PORT E ,F
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R1; //enable clock for timer
    _delay_cycles(3);


    //configure GPIO registers
    GPIO_PORTE_DEN_R |= PORTE_INPUT | PORTE_OUTPUT; //PORT E1,E2
    GPIO_PORTF_DEN_R |= GREEN_LED_MASK | BLUE_LED_MASK;
    //PE1 input
    GPIO_PORTE_DIR_R &= ~(PORTE_INPUT  | PORTE_OUTPUT); //clear both ports

    //PE2 output direction reg.
    GPIO_PORTE_DIR_R |=  (PORTE_OUTPUT); //PORTE2 OUTPUT
    GPIO_PORTF_DIR_R |=  GREEN_LED_MASK | BLUE_LED_MASK;

    //configure falling endge interrupt for correspond input signal on PORTE1
    GPIO_PORTE_IS_R  &= ~(PORTE_INPUT);    // Interrupt sense = 0 = edge detect
    GPIO_PORTE_IBE_R &= ~(PORTE_INPUT);    // Interrupt both edges = 0
    GPIO_PORTE_IEV_R &= ~(PORTE_INPUT);    // Interrupt event = 0 = falling edge
    GPIO_PORTE_ICR_R |=  (PORTE_INPUT);    //clear interrupt
    GPIO_PORTE_IM_R  |=  (PORTE_INPUT);    //turn on interrupt
    NVIC_EN0_R |= 1  <<  (INT_GPIOE-16);   // Turn-on interrupt 20 (GPIOE)


    //configure timers
    // Configure Timer 1 as the time base

    /*
    TIMER1_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off timer before reconfiguring
    TIMER1_CFG_R = TIMER_CFG_32_BIT_TIMER;           // configure as 32-bit timer (A+B)
    TIMER1_TAMR_R = TIMER_TAMR_TAMR_PERIOD;          // configure for periodic mode (count down)
    TIMER1_TAILR_R = 90000; //2.25ms                       // set load value to 40e6 for 1 Hz interrupt rate
    TIMER1_IMR_R = TIMER_IMR_TATOIM;                 // turn-on interrupts
    NVIC_EN0_R |= 1 << (INT_TIMER1A-16);             // turn-on interrupt 37 (TIMER1A)
    TIMER1_CTL_R |= TIMER_CTL_TAEN;                  // turn-on timer

       startbits

       2.25 ms 0
       4.5  ms 0
       6.75 ms 0
       10.5 ms 1
       12   ms 1
       total: part1 time = 13.5 ms = 9ms low, 4.5 ms high
       sample at 2.25 ms low, 1.5 ms high using two timers
       running at 40 MHZ, every 2.25 ms = 90,000 HZ timer, 562.5 us = 22500 HZ

     */
}

//this function will print out info given a range and array
void printInfo(uint8_t databits[32], uint8_t range0, uint8_t range1) // range0-range1 has to be lsb-msb eg. 16-23
{
    putsUart0("\r\n\t");
    uint8_t i;
    for (i=range0; i <=range1; i++)
    {
        putcUart0(databits[i]+'0');
        databitsCopy[i] = databits[i]; //copy data bits found and store them in global array shared between files
    }
    putsUart0("\r\n");
}


void configure_timer1(uint32_t freq, bool period) //freq is # of clock cyles converted from time eg. 9ms => f = sys_clock / (1/9ms)
{
    TIMER1_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off timer before reconfiguring
    TIMER1_CFG_R = TIMER_CFG_32_BIT_TIMER;           // configure as 32-bit timer (A+B)
    if (period == true)
    {
        TIMER1_TAMR_R = TIMER_TAMR_TAMR_PERIOD;          // configure for periodic mode (count down)
    }
    else
    {
        TIMER1_TAMR_R = TIMER_TAMR_TAMR_1_SHOT;          // configure for 1-shot mode(count down)
    }

    TIMER1_TAILR_R = freq;                           // set load value to 40e6 for 1 Hz interrupt rate
    TIMER1_IMR_R = TIMER_IMR_TATOIM;                 // turn-on interrupts
    NVIC_EN0_R |= 1 << (INT_TIMER1A-16);             // turn-on interrupt 37 (TIMER1A)
    TIMER1_CTL_R |= TIMER_CTL_TAEN;                  // turn-on timer
}

void IRQ_Falling_Edge_ISR() //called only once at start of signal then turned off
{
    //turn off falling edge interrupt
    GPIO_PORTE_IM_R  &= ~(PORTE_INPUT);
    //set timer to 2.25ms
    configure_timer1(90000,0); //timer has to be periodic freq = 90000 = 2.25 ms @ 40 Mhz clock
    //toggle GPIO output
    BLUE_LED = 1;
    OUTPUT_SIGNAL ^= 1;
    //clear interrupt
    GPIO_PORTE_ICR_R |= PORTE_INPUT;
}

void reset_IR_Receiver()
{
    //turn on DEN after clearing databits
    GPIO_PORTE_DEN_R |= PORTE_INPUT | PORTE_OUTPUT;
    TIMER1_CTL_R &= ~TIMER_CTL_TAEN; //turn off timer
    TIMER1_ICR_R = TIMER_ICR_TATOCINT; //clear interrupt
    BLUE_LED = 0; //turn off blue led

    //configure falling endge interrupt for correspond input signal on PORTE1 for next signal
    GPIO_PORTE_IS_R  &= ~(PORTE_INPUT);   // Interrupt sense = 0 = edge detect
    GPIO_PORTE_IBE_R &= ~(PORTE_INPUT);   // Interrupt both edges = 0
    GPIO_PORTE_IEV_R &= ~(PORTE_INPUT);   // Interrupt event = 0 = falling edge
    GPIO_PORTE_ICR_R |=  (PORTE_INPUT);   // clear interrupt
    GPIO_PORTE_IM_R  |= (PORTE_INPUT);    // turn on interrupt
    NVIC_EN0_R |= 1  << (INT_GPIOE-16);   // Turn-on interrupt 20 (GPIOE)
}


void IRQ_Timer1_ISR()
{
    static uint8_t startbitsindex = 0;
    static uint8_t zerobits = 0;
    static uint8_t onebits = 0;
    static uint8_t databitsindex =0;
    static uint8_t databits[32];
    uint8_t startbits[] = {0,0,0,1,1,1};

        if (startbitsindex <=5 )
        {
            if(INPUT_SIGNAL == startbits[startbitsindex])
            {

                if(startbitsindex < 2)
                {
                    OUTPUT_SIGNAL ^=1;
                    configure_timer1(90000,0); //2.25ms
                    /*
                    putsUart0("2.25ms == < 2");
                    putsUart0("\r\n");
                    */
                }
                else if(startbitsindex == 2)
                {
                    OUTPUT_SIGNAL ^=1;
                    configure_timer1(150000,0); //3.75ms
                    /*
                    putsUart0("3.75ms == 2");
                    putsUart0("\r\n");
                    */

                }

                else if(startbitsindex == 3 || startbitsindex == 4)
                {
                  OUTPUT_SIGNAL ^=1;
                  configure_timer1(60000,0); //1.5ms = 60000 1.6ms =64000 1.8ms = 72000
                  /*
                  putsUart0("1.5ms == 3 | 4");
                  putsUart0("\r\n");
                  */

                }

                //TIMER1_ICR_R = TIMER_ICR_TATOCINT;
                else if(startbitsindex == 5)
                {

                    configure_timer1(22500,1); //562.5 us == 22500 570 us = 22800
                    OUTPUT_SIGNAL ^=1;
                    /*
                    putsUart0("562.5 us == 5");
                    putsUart0("\r\n\t");
                    */
                }
                startbitsindex+=1; //increment index
            }

            else
            { //RESET_ON_BUTTON_PRESS
                GPIO_PORTE_DIR_R &= ~(PORTE_INPUT | PORTE_OUTPUT);
                GPIO_PORTE_DEN_R &= ~(PORTE_INPUT | PORTE_OUTPUT);
                //_delay_cycles(25000000);
                _delay_cycles(2500000);
                OUTPUT_SIGNAL =0;
                startbitsindex =0; //reset startbits index
                zerobits = 0;
                onebits = 0;
                databitsindex =0; //databits resets
                uint32_t i;
                for(i =0 ; i < 32; i++)//clear databits array
                {
                    databits[i] = 0;
                }
                reset_IR_Receiver(); //reset controller
            }
        }

        else if(startbitsindex >5 && databitsindex < 32)
        {
            OUTPUT_SIGNAL ^=1;
            if (databitsindex < 32)
            {
                if(INPUT_SIGNAL == 0)
                {
                    if(zerobits == 1 && onebits == 1)
                    {
                        databits[databitsindex] = 0;
                        databitsindex++;
                        /*
                        putsUart0("0");
                        putsUart0("\r\n\t");
                        */
                    }
                    zerobits = 1;
                    onebits = 0;
                }

                else if (INPUT_SIGNAL == 1)
                {
                    if (zerobits == 1 && onebits == 2)
                    {
                        databits[databitsindex] = 1;
                        databitsindex++;
                        zerobits = 0;
                        onebits = 0;
                        /*
                        putsUart0("1");
                        putsUart0("\r\n\t");
                        */

                    }
                    else
                    {
                        onebits++;
                    }

                }
            }

        }

        else if(databitsindex >31) //fix later RESET_ON_BUTTON_PRESS
        {
            //decode signal
            //enum Controller = { }
            GPIO_PORTE_DIR_R &= ~(PORTE_INPUT | PORTE_OUTPUT); //CLEAR
            GPIO_PORTE_DEN_R &= ~(PORTE_INPUT | PORTE_OUTPUT); //turn off data

            uint8_t i;
            uint8_t sum = 0;
            uint8_t exponential = 0;

            for (i=23; i >=16; i--)
            {
                /*
                putcUart0(databits[i]+'0');
                putsUart0("\r\n\t");
                */
                //convert from binary array to decimal
                if(databits[i] == 1)
                {
                    sum += (1 << exponential);
                }
                exponential+=1;
            }

            GREEN_LED = 1;
            if(sum == 162)
            {
                //CH-0
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);

                data_ready =0;
            }
            else if(sum == 98)
            {
                //CH-1
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);
                data_ready =0;

            }
            else if(sum == 226)
            {
                //CH+ - 2
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);
                data_ready =0;
            }
            else if(sum == 34)
            {
                //PREV - 3
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);
                data_ready =0;
            }
            else if(sum == 2)
            {
                //NEXT - 4
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);
                data_ready =0;
            }
            else if(sum == 194)
            {
                //PLAY-PAUSE - 5
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);
            }
            else if(sum == 224)
            {
                //VOL- 6
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);

                data_ready =0;
            }
            else if(sum == 168)
            {
                //VOL+ - 7
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);

                data_ready =0;
            }
            else if(sum == 144)
            {
                //EQ -8
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);

                data_ready =0;
            }
            else if(sum == 104)
            {
                //0 --9
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);
                data_ready =0;
            }
            else if(sum == 152)
            {
                //100+ -- 10
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);

                data_ready =0;
            }
            else if(sum == 176)
            {
                //200+ --11
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);

                data_ready =0;
            }
            else if(sum == 48 )
            {
                //1 -- 12
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);

                data_ready =0;
            }
            else if(sum == 24)
            {
                //2 --13
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);

                data_ready =0;
            }
            else if(sum == 122)
            {
                //3 --14
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);

                data_ready =0;
            }
            else if(sum == 16)
            {
                //4 --15
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);
                data_ready =0;
            }
            else if(sum == 56)
            {
                //5 --16
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);

                data_ready =0;
            }
            else if(sum == 90)
            {
                //6 -- 17
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);

                data_ready =0;
            }
            else if(sum == 66)
            {
                //7 --18
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);

                data_ready =0;
            }
            else if(sum == 74)
            {
                //8 --19
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);

                data_ready =0;
            }
            else if(sum == 82)
            {
                //9 --20
                putsUart0("Address:");
                printInfo(databits,0,7); //databits[8-15] = addr.

                putsUart0("Data:");
                printInfo(databits,16,23);

                data_ready =0;
            }

            else
            {
                if(ALERTBADON)
                {
                    //beep for bad command
                    //change cmp value in pwm
                    // enable pwm
                    PWM0_2_CMPA_R = 100;
                    GPIO_PORTE_DEN_R   |= (BLUE_BL_LED_MASK); //enable PWM
                    _delay_cycles(20000000); //wait one second
                    GPIO_PORTE_DEN_R   &= ~(BLUE_BL_LED_MASK);//disable
                    //delay half a second
                    //PWM0_2_CMPA_R = 3000;   //pe 4
                }
            }

            if(data_ready == 0)
            {
                if(ALERTGOODON)
                {
                    //change cmp value in pwm
                    PWM0_2_CMPA_R = 3000;
                    GPIO_PORTE_DEN_R   |= (BLUE_BL_LED_MASK);
                    _delay_cycles(20000000);
                    GPIO_PORTE_DEN_R   &= ~(BLUE_BL_LED_MASK);
                    //enable pwm
                    //delay half a second
                }
            }


            startbitsindex =0; //reset startbits index
            zerobits = 0;
            onebits = 0;
            databitsindex =0; //databits resets

            for(i =0 ; i < 32; i++)//clear databits array
            {
                databits[i] = 0;
            }

            reset_IR_Receiver(); //reset controller

            //_delay_cycles(25000000); //delay new commands for 1 second
            _delay_cycles(2500000);
        }

        TIMER1_ICR_R = TIMER_ICR_TATOCINT; //clear  timer interrupt interrupt
}
