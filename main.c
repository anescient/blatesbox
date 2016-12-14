
// fn                   port        16F688 pin
// pull-down button     RA2         11
// beep                 RC3         7
// dot data             RC4         6
// dot clock            RC1         9
// dot latch            RC2         8

#pragma config FCMEN = OFF      // failsafe clock monitor disabled
#pragma config IESO = OFF       // internal external switchover disabled
#pragma config BOREN = OFF      // brown-out reset disabled
#pragma config CPD = OFF        // data code protection off
#pragma config CP = OFF         // code protection off
#pragma config MCLRE = OFF      // MCLR/Vpp pin is digital input
#pragma config PWRTE = ON       // power-up timer enabled
#pragma config WDTE = OFF       // watchdog timer disabled
#pragma config FOSC = INTOSCIO  // internal clock, no clock output

#include <xc.h>
#include "xabcrand.h"

// numerical die values (1-6, or 0 for none/no display)
unsigned char die_a;
unsigned char die_b;

// current (last-set) display state
unsigned char dotstate;

// countdown to blink
unsigned char flashtime;

// countdown to blank
unsigned char holdtime;

// roll velocity
// 0 : not rolling (hold)
// >0 : rolling, higher value => faster roll (lower delay)
unsigned char velocity;
#define INITVELOCITY 3
#define MAXVELOCITY 16

// while rolling, countdown to next roll
unsigned int rolldelay;
#define MINROLLDELAY 350
#define MAXROLLDELAY 1200

// if set, perform roll when able then clear
bit doroll;

// beep countdown (delay to off)
unsigned char beeptime;

void beep(unsigned char t)
{
    RC3 = 1;
    beeptime = t;
}

// 0 in => random 1-6 out
// 1-6 in => random neighboring 1-6 out
unsigned char rolldie(unsigned char d)
{
    const static unsigned char nextface[6][4] =
    { { 2, 3, 5, 4 },
      { 1, 4, 6, 3 },
      { 1, 2, 6, 5 },
      { 1, 5, 6, 2 },
      { 1, 3, 6, 4 },
      { 2, 4, 5, 3 } };

    if(d == 0)
    {
        unsigned char r;
        do
        {
            r = rand();
        } while(r > 251); // random multiple of 6
        return (r % 6) + 1;
    }
    else if(d > 6) // defensive
        return 0;
    else
        return nextface[d - 1][rand_four()];
}

/* translate numerical value to dot pattern
 * return four-bit pattern 0b0000ABCD
 *   A   C
 *   B D B
 *   C   A
 * e.g. 4 => 1010 (A and C on)
 * special value 0 => all dark, 0000
 * >6 undefined
 */
unsigned char mapdots(unsigned char d)
{
    if(d == 6)
        return 0b1110;
    else
    {
        unsigned char dots = 0;
        if(d & 1) // 1, 3, 5
            dots |= 0b0001;
        if(d & 2) // 2, 3
            dots |= rand_bit() ? 0b1000 : 0b0010;
        else if(d & 4) // 4, 5
            dots |= 0b1010;
        return dots;
    }
}

/* set dot display states
 * bits in argument are ABCDABCD, four dot groups * 2 dice
 */
void setdots(unsigned char dd)
{
    dotstate = dd;
    RC4 = dd & 0b00000001 ? 1 : 0; RC1 = 1; RC1 = 0;
    RC4 = dd & 0b00000010 ? 1 : 0; RC1 = 1; RC1 = 0;
    RC4 = dd & 0b00000100 ? 1 : 0; RC1 = 1; RC1 = 0;
    RC4 = dd & 0b00001000 ? 1 : 0; RC1 = 1; RC1 = 0;
    RC4 = dd & 0b00010000 ? 1 : 0; RC1 = 1; RC1 = 0;
    RC4 = dd & 0b00100000 ? 1 : 0; RC1 = 1; RC1 = 0;
    RC4 = dd & 0b01000000 ? 1 : 0; RC1 = 1; RC1 = 0;
    RC4 = dd & 0b10000000 ? 1 : 0; RC1 = 1; RC1 = 0;
    RC2 = 1;
    RC2 = 0;
    RC4 = 0;
}

/* clear display and immediately restore state */
void blinkdots()
{
    //RC4 = 0; // should be clear already
    RC1 = 1; RC1 = 0;
    RC1 = 1; RC1 = 0;
    RC1 = 1; RC1 = 0;
    RC1 = 1; RC1 = 0;
    RC1 = 1; RC1 = 0;
    RC1 = 1; RC1 = 0;
    RC1 = 1; RC1 = 0;
    RC1 = 1; RC1 = 0;
    RC2 = 1;
    RC2 = 0;
    setdots(dotstate);
}

void interrupt isr(void)
{
    if(RAIF) // port A change
    {
        if(RA2 == 0) // button down
        {
            rand_push(TMR0);
            if(velocity == 0)
                velocity = INITVELOCITY;
            else
            {
                velocity += 3;
                if(velocity > MAXVELOCITY)
                    velocity = MAXVELOCITY;
            }
        }
        else // button up
            rand_push(~TMR0);

        RAIF = 0;
    }

    if(T0IF) // Timer0
    {
        /** blink step **/
        if(flashtime > 0)
            flashtime--;
        else // flashtime == 0
        {
            if(velocity == 0 && die_a != die_b)
                blinkdots();
            flashtime = 255;
        }

        /** hold step **/
        if(holdtime > 0)
        {
            holdtime--;
            if(holdtime == 0)
                setdots(0x00);
        }

        /** beep step **/
        if(beeptime > 0)
        {
            beeptime--;
            if(beeptime == 0)
                RC3 = 0;
        }

        /** roll step **/
        if(velocity > 0)
        {
            if(rolldelay > 0)
                rolldelay--;
            else // rolldelay == 0
            {
                if(RA2 == 1) // button up
                    velocity--;
                else // button down
                    if(velocity < MAXVELOCITY)
                        velocity++;

                doroll = 1;

                if(velocity > 0)
                {
                    rolldelay = (MAXVELOCITY + 1 - velocity) * 100;
                    if(rolldelay < MINROLLDELAY) rolldelay = MINROLLDELAY;
                    if(rolldelay > MAXROLLDELAY) rolldelay = MAXROLLDELAY;
                }
            }
        }

        T0IF = 0;
    }
}

void main(void)
{
    OSCCON = 0b01110001;        // 8MHz, internal osc
    while(!HTS);                // wait for osc stable

    // init port C (beep and display outputs)
    TRISC = 0b00100001;         // RC4, RC3, RC2, RC1 output
    PORTC = 0x00;

    die_a = 0;
    die_b = 0;
    setdots(0x00);

    // init port A (button)
    ANSEL = 0x00;               // all digital I/O
    //TRISA2 = 1;               // RA2 input (default on reset)
    WPUA = 0b00000100;          // pull-up on RA2
    nRAPU = 0;                  // pull-ups enabled

    // init button interrupt
    while(RA2 == 0);            // wait for button up
    IOCA2 = 1;                  // interrupt-on-change RA2
    RAIF = 0;
    RAIE = 1;                   // enable port A interrupt-on-change

    flashtime = 0;
    holdtime = 0;
    velocity = 0;
    rolldelay = 0;
    doroll = 0;
    beeptime = 0;
    rand_init(0x44, 0x53, 0x11);

    // init Timer0
    TMR0 = 0;
    PSA = 1;                    // prescaler to WDT, no prescale on Timer0
    T0CS = 0;                   // Timer0 on instruction cycle
    T0IF = 0;
    T0IE = 1;                   // enable Timer0 interrupt

    GIE = 1;                    // enable interrupts

    while(1)
    {
        if(doroll)
        {
            beep(velocity > 0 ? 70 : 255);
            die_a = rolldie(die_a);
            die_b = rolldie(die_b);
            setdots(mapdots(die_a) | (mapdots(die_b) << 4));
            if(velocity > 0)
                holdtime = 200;
            doroll = 0;
        }
    }
}
