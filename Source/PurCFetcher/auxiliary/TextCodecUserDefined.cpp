/*
 * Copyright (C) 2007-2017 Apple, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "TextCodecUserDefined.h"

#include <array>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace PurcFetcher {

void TextCodecUserDefined::registerEncodingNames(EncodingNameRegistrar registrar)
{
    registrar("x-user-defined", "x-user-defined");
}

void TextCodecUserDefined::registerCodecs(TextCodecRegistrar registrar)
{
    registrar("x-user-defined", [] {
        return makeUnique<TextCodecUserDefined>();
    });
}

String TextCodecUserDefined::decode(const char* bytes, size_t length, bool, bool, bool&)
{
    StringBuilder result;
    result.reserveCapacity(length);
    for (size_t i = 0; i < length; ++i) {
        signed char c = bytes[i];
        result.append(static_cast<UChar>(c & 0xF7FF));
    }
    return result.toString();
}

static Vector<uint8_t> encodeComplexUserDefined(StringView string, UnencodableHandling handling)
{
    Vector<uint8_t> result;

    for (auto character : string.codePoints()) {
        int8_t signedByte = character;
        if ((signedByte & 0xF7FF) == character)
            result.append(signedByte);
        else {
            // No way to encode this character with x-user-defined.
            UnencodableReplacementArray replacement;
            int replacementLength = TextCodec::getUnencodableReplacement(character, handling, replacement);
            result.append(replacement.data(), replacementLength);
        }
    }

    return result;
}

Vector<uint8_t> TextCodecUserDefined::encode(StringView string, UnencodableHandling handling)
{
    {
        Vector<uint8_t> result(string.length());
        auto* bytes = result.data();

        // Convert and simultaneously do a check to see if it's all ASCII.
        UChar ored = 0;
        for (auto character : string.codeUnits()) {
            *bytes++ = character;
            ored |= character;
        }

        if (!(ored & 0xFF80))
            return result;
    }

    // If it wasn't all ASCII, call the function that handles more-complex cases.
    return encodeComplexUserDefined(string, handling);
}

} // namespace PurcFetcher
