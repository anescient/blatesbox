#ifndef XABCRAND_H
#define	XABCRAND_H

typedef struct
{
    unsigned char a;
    unsigned char b;
    unsigned char c;
    unsigned char x;
    unsigned char bits; // queue of random bits
    unsigned char bitsleft; // bits remaining in queue (0-8)
} xabcstate_t;

// reset/seed
void rand_init(unsigned char s1, unsigned char s2, unsigned char s3);

// add entropy
void rand_push(unsigned char s);

// random 0-255
unsigned char rand();

// random 0-3
unsigned char rand_four();

// random 0-1
unsigned char rand_bit();

#endif	/* XABCRAND_H */
