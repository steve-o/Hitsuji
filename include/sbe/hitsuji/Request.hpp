/* Generated SBE (Simple Binary Encoding) message codec */
#ifndef _REQUEST_HPP_
#define _REQUEST_HPP_

/* math.h needed for NAN */
#include <math.h>
#include "sbe/sbe.hpp"

#include "hitsuji/VarDataEncoding.hpp"
#include "hitsuji/BooleanType.hpp"

using namespace sbe;

namespace hitsuji {

class Request
{
private:
    char *buffer_;
    int bufferLength_;
    int *positionPtr_;
    int offset_;
    int position_;
    int actingBlockLength_;
    int actingVersion_;

public:

    static sbe_uint16_t sbeBlockLength(void)
    {
        return (sbe_uint16_t)9;
    }

    static sbe_uint16_t sbeTemplateId(void)
    {
        return (sbe_uint16_t)1;
    }

    static sbe_uint16_t sbeSchemaId(void)
    {
        return (sbe_uint16_t)1;
    }

    static sbe_uint16_t sbeSchemaVersion(void)
    {
        return (sbe_uint16_t)0;
    }

    static const char *sbeSemanticType(void)
    {
        return "";
    }

    sbe_uint64_t offset(void) const
    {
        return offset_;
    }

    Request &wrapForEncode(char *buffer, const int offset, const int bufferLength)
    {
        buffer_ = buffer;
        offset_ = offset;
        bufferLength_ = bufferLength;
        actingBlockLength_ = sbeBlockLength();
        actingVersion_ = sbeSchemaVersion();
        position(offset + actingBlockLength_);
        positionPtr_ = &position_;
        return *this;
    }

    Request &wrapForDecode(char *buffer, const int offset, const int actingBlockLength, const int actingVersion,                         const int bufferLength)
    {
        buffer_ = buffer;
        offset_ = offset;
        bufferLength_ = bufferLength;
        actingBlockLength_ = actingBlockLength;
        actingVersion_ = actingVersion;
        positionPtr_ = &position_;
        position(offset + actingBlockLength_);
        return *this;
    }

    sbe_uint64_t position(void) const
    {
        return position_;
    }

    void position(const sbe_uint64_t position)
    {
        if (SBE_BOUNDS_CHECK_EXPECT((position > bufferLength_), 0))
        {
            throw "buffer too short";
        }
        position_ = position;
    }

    int size(void) const
    {
        return position() - offset_;
    }

    char *buffer(void)
    {
        return buffer_;
    }

    int actingVersion(void) const
    {
        return actingVersion_;
    }

    static int rwfVersionId(void)
    {
        return 1;
    }

    static int rwfVersionSinceVersion(void)
    {
         return 0;
    }

    bool rwfVersionInActingVersion(void)
    {
        return (actingVersion_ >= 0) ? true : false;
    }


    static const char *rwfVersionMetaAttribute(const MetaAttribute::Attribute metaAttribute)
    {
        switch (metaAttribute)
        {
            case MetaAttribute::EPOCH: return "unix";
            case MetaAttribute::TIME_UNIT: return "nanosecond";
            case MetaAttribute::SEMANTIC_TYPE: return "";
        }

        return "";
    }

    static sbe_uint16_t rwfVersionNullValue()
    {
        return (sbe_uint16_t)65535;
    }

    static sbe_uint16_t rwfVersionMinValue()
    {
        return (sbe_uint16_t)0;
    }

    static sbe_uint16_t rwfVersionMaxValue()
    {
        return (sbe_uint16_t)65534;
    }

    sbe_uint16_t rwfVersion(void) const
    {
        return SBE_LITTLE_ENDIAN_ENCODE_16(*((sbe_uint16_t *)(buffer_ + offset_ + 0)));
    }

    Request &rwfVersion(const sbe_uint16_t value)
    {
        *((sbe_uint16_t *)(buffer_ + offset_ + 0)) = SBE_LITTLE_ENDIAN_ENCODE_16(value);
        return *this;
    }

    static int tokenId(void)
    {
        return 2;
    }

    static int tokenSinceVersion(void)
    {
         return 0;
    }

    bool tokenInActingVersion(void)
    {
        return (actingVersion_ >= 0) ? true : false;
    }


    static const char *tokenMetaAttribute(const MetaAttribute::Attribute metaAttribute)
    {
        switch (metaAttribute)
        {
            case MetaAttribute::EPOCH: return "unix";
            case MetaAttribute::TIME_UNIT: return "nanosecond";
            case MetaAttribute::SEMANTIC_TYPE: return "";
        }

        return "";
    }

    static sbe_int32_t tokenNullValue()
    {
        return -2147483648;
    }

    static sbe_int32_t tokenMinValue()
    {
        return -2147483647;
    }

    static sbe_int32_t tokenMaxValue()
    {
        return 2147483647;
    }

    sbe_int32_t token(void) const
    {
        return SBE_LITTLE_ENDIAN_ENCODE_32(*((sbe_int32_t *)(buffer_ + offset_ + 2)));
    }

    Request &token(const sbe_int32_t value)
    {
        *((sbe_int32_t *)(buffer_ + offset_ + 2)) = SBE_LITTLE_ENDIAN_ENCODE_32(value);
        return *this;
    }

    static int serviceIdId(void)
    {
        return 3;
    }

    static int serviceIdSinceVersion(void)
    {
         return 0;
    }

    bool serviceIdInActingVersion(void)
    {
        return (actingVersion_ >= 0) ? true : false;
    }


    static const char *serviceIdMetaAttribute(const MetaAttribute::Attribute metaAttribute)
    {
        switch (metaAttribute)
        {
            case MetaAttribute::EPOCH: return "unix";
            case MetaAttribute::TIME_UNIT: return "nanosecond";
            case MetaAttribute::SEMANTIC_TYPE: return "";
        }

        return "";
    }

    static sbe_uint16_t serviceIdNullValue()
    {
        return (sbe_uint16_t)65535;
    }

    static sbe_uint16_t serviceIdMinValue()
    {
        return (sbe_uint16_t)0;
    }

    static sbe_uint16_t serviceIdMaxValue()
    {
        return (sbe_uint16_t)65534;
    }

    sbe_uint16_t serviceId(void) const
    {
        return SBE_LITTLE_ENDIAN_ENCODE_16(*((sbe_uint16_t *)(buffer_ + offset_ + 6)));
    }

    Request &serviceId(const sbe_uint16_t value)
    {
        *((sbe_uint16_t *)(buffer_ + offset_ + 6)) = SBE_LITTLE_ENDIAN_ENCODE_16(value);
        return *this;
    }

    static int useAttribInfoInUpdatesId(void)
    {
        return 4;
    }

    static int useAttribInfoInUpdatesSinceVersion(void)
    {
         return 0;
    }

    bool useAttribInfoInUpdatesInActingVersion(void)
    {
        return (actingVersion_ >= 0) ? true : false;
    }


    static const char *useAttribInfoInUpdatesMetaAttribute(const MetaAttribute::Attribute metaAttribute)
    {
        switch (metaAttribute)
        {
            case MetaAttribute::EPOCH: return "unix";
            case MetaAttribute::TIME_UNIT: return "nanosecond";
            case MetaAttribute::SEMANTIC_TYPE: return "";
        }

        return "";
    }

    BooleanType::Value useAttribInfoInUpdates(void) const
    {
        return BooleanType::get((*((sbe_uint8_t *)(buffer_ + offset_ + 8))));
    }

    Request &useAttribInfoInUpdates(const BooleanType::Value value)
    {
        *((sbe_uint8_t *)(buffer_ + offset_ + 8)) = (value);
        return *this;
    }

    static const char *itemNameMetaAttribute(const MetaAttribute::Attribute metaAttribute)
    {
        switch (metaAttribute)
        {
            case MetaAttribute::EPOCH: return "unix";
            case MetaAttribute::TIME_UNIT: return "nanosecond";
            case MetaAttribute::SEMANTIC_TYPE: return "";
        }

        return "";
    }

    static const char *itemNameCharacterEncoding()
    {
        return "UTF-8";
    }

    static int itemNameSinceVersion(void)
    {
         return 0;
    }

    bool itemNameInActingVersion(void)
    {
        return (actingVersion_ >= 0) ? true : false;
    }

    static int itemNameId(void)
    {
        return 5;
    }


    static int itemNameHeaderSize()
    {
        return 1;
    }

    sbe_int64_t itemNameLength(void) const
    {
        return (*((sbe_uint8_t *)(buffer_ + position())));
    }

    const char *itemName(void)
    {
         const char *fieldPtr = (buffer_ + position() + 1);
         position(position() + 1 + *((sbe_uint8_t *)(buffer_ + position())));
         return fieldPtr;
    }

    int getItemName(char *dst, const int length)
    {
        sbe_uint64_t sizeOfLengthField = 1;
        sbe_uint64_t lengthPosition = position();
        position(lengthPosition + sizeOfLengthField);
        sbe_int64_t dataLength = (*((sbe_uint8_t *)(buffer_ + lengthPosition)));
        int bytesToCopy = (length < dataLength) ? length : dataLength;
        sbe_uint64_t pos = position();
        position(position() + (sbe_uint64_t)dataLength);
        ::memcpy(dst, buffer_ + pos, bytesToCopy);
        return bytesToCopy;
    }

    int putItemName(const char *src, const int length)
    {
        sbe_uint64_t sizeOfLengthField = 1;
        sbe_uint64_t lengthPosition = position();
        *((sbe_uint8_t *)(buffer_ + lengthPosition)) = ((sbe_uint8_t)length);
        position(lengthPosition + sizeOfLengthField);
        sbe_uint64_t pos = position();
        position(position() + (sbe_uint64_t)length);
        ::memcpy(buffer_ + pos, src, length);
        return length;
    }
};
}
#endif
