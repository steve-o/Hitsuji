/* Generated SBE (Simple Binary Encoding) message codec */
#ifndef _BOOLEANTYPE_HPP_
#define _BOOLEANTYPE_HPP_

/* math.h needed for NAN */
#include <math.h>
#include "sbe/sbe.hpp"

using namespace sbe;

namespace hitsuji {

class BooleanType
{
public:

    enum Value 
    {
        NO = (sbe_uint8_t)0,
        YES = (sbe_uint8_t)1,
        NULL_VALUE = (sbe_uint8_t)255
    };

    static BooleanType::Value get(const sbe_uint8_t value)
    {
        switch (value)
        {
            case 0: return NO;
            case 1: return YES;
            case 255: return NULL_VALUE;
        }

        throw "unknown value for enum BooleanType";
    }
};
}
#endif
