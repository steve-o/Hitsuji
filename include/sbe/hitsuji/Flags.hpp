/* Generated SBE (Simple Binary Encoding) message codec */
#ifndef _FLAGS_HPP_
#define _FLAGS_HPP_

/* math.h needed for NAN */
#include <math.h>
#include "sbe/sbe.hpp"

using namespace sbe;

namespace hitsuji {

class Flags
{
private:
    char *buffer_;
    int offset_;
    int actingVersion_;

public:
    Flags &wrap(char *buffer, const int offset, const int actingVersion, const int bufferLength)
    {
        if (SBE_BOUNDS_CHECK_EXPECT((offset > (bufferLength - 1)), 0))
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
        return 1;
    }


    Flags &clear(void)
    {
        *((sbe_uint8_t *)(buffer_ + offset_)) = 0;
        return *this;
    }


    bool abort(void) const
    {
        return ((*((sbe_uint8_t *)(buffer_ + offset_))) & (0x1L << 0)) ? true : false;
    }

    Flags &abort(const bool value)
    {
        sbe_uint8_t bits = (*((sbe_uint8_t *)(buffer_ + offset_)));
        bits = value ? (bits | (0x1L << 0)) : (bits & ~(0x1L << 0));
        *((sbe_uint8_t *)(buffer_ + offset_)) = (bits);
        return *this;
    }

    bool useAttribInfoInUpdates(void) const
    {
        return ((*((sbe_uint8_t *)(buffer_ + offset_))) & (0x1L << 1)) ? true : false;
    }

    Flags &useAttribInfoInUpdates(const bool value)
    {
        sbe_uint8_t bits = (*((sbe_uint8_t *)(buffer_ + offset_)));
        bits = value ? (bits | (0x1L << 1)) : (bits & ~(0x1L << 1));
        *((sbe_uint8_t *)(buffer_ + offset_)) = (bits);
        return *this;
    }
};
}
#endif
