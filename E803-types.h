#pragma once
/* types for 803 39 bit words */ 
#include <stdint.h>


// Removed union with a byte array
// Store 803 39bit word in a 64 bit value.

typedef uint64_t  E803word;



#if 0
/** \typedef E803word
 * Structure to hold an 803 word.
 * Note it actually holds 64 bits !  
 */ 
typedef struct e803word
{
    union 
    {
	uint8_t bytes[8];
	uint64_t word;
    };

} E803word;




/** \typedef E803Mant
 *  Structure to hold the mantissa from an 803 floating point number.
 */ 
typedef struct e803mant
{
    char Mbytes[6];
} E803Mant;

/** \typedef E803Dword
 *  Structure to hold a doublelength 803 number.
 */ 
typedef struct e803dword
{
    E803word AR;
    E803word ACC;
} E803Dword;
#endif
