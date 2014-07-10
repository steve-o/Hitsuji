/* Generated SBE (Simple Binary Encoding) message codec */
#ifndef _GROUPSIZEENCODING_HPP_
#define _GROUPSIZEENCODING_HPP_

/* math.h needed for NAN */
#include <math.h>
#include "sbe/sbe.hpp"

using namespace sbe;

namespace hitsuji {

class GroupSizeEncoding
{
private:
    char *buffer_;
    int offset_;
    int actingVersion_;

public:
    GroupSizeEncoding &wrap(char *buffer, const int offset, const int actingVersion, const int bufferLength)
    {
        if (SBE_BOUNDS_CHECK_EXPECT((offset > (bufferLength - 3)), 0))
        {
            throw "buffer too short for flyweight";
        }
        buffer_ = buffer;
        offset_ = offset;
        actingVersion_ = actingVersion;
        return *this;
    }

    static int size(void)
    {
        return 3;
    }


    static sbe_uint16_t blockLengthNullValue()
    {
        return (sbe_uint16_t)65535;
    }

    static sbe_uint16_t blockLengthMinValue()
    {
        return (sbe_uint16_t)0;
    }

    static sbe_uint16_t blockLengthMaxValue()
    {
        return (sbe_uint16_t)65534;
    }

    sbe_uint16_t blockLength(void) const
    {
        return SBE_LITTLE_ENDIAN_ENCODE_16(*((sbe_uint16_t *)(buffer_ + offset_ + 0)));
    }

    GroupSizeEncoding &blockLength(const sbe_uint16_t value)
    {
        *((sbe_uint16_t *)(buffer_ + offset_ + 0)) = SBE_LITTLE_ENDIAN_ENCODE_16(value);
        return *this;
    }

    static sbe_uint8_t numInGroupNullValue()
    {
        return (sbe_uint8_t)255;
    }

    static sbe_uint8_t numInGroupMinValue()
    {
        return (sbe_uint8_t)0;
    }

    static sbe_uint8_t numInGroupMaxValue()
    {
        return (sbe_uint8_t)254;
    }

    sbe_uint8_t numInGroup(void) const
    {
        return (*((sbe_uint8_t *)(buffer_ + offset_ + 2)));
    }

    GroupSizeEncoding &numInGroup(const sbe_uint8_t value)
    {
        *((sbe_uint8_t *)(buffer_ + offset_ + 2)) = (value);
        return *this;
    }
};
}
#endif
