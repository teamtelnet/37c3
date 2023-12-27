#include <avr/pgmspace.h>

#define HINT_LENGTH  5
#define HINT_NUMS    3

const char hint_1[] PROGMEM = "abcde";
const char hint_2[] PROGMEM = "ABCDE";
const char hint_3[] PROGMEM = "01234";

PGM_P const hints[] PROGMEM = 
{
    hint_1,
    hint_2,
    hint_3
};
