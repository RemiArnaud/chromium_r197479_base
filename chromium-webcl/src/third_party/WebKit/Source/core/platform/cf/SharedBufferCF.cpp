/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "core/platform/SharedBuffer.h"

#include "core/platform/PurgeableBuffer.h"

namespace WebCore {

SharedBuffer::SharedBuffer(CFDataRef cfData)
    : m_size(0)
    , m_cfData(cfData)
{
}

CFDataRef SharedBuffer::createCFData()
{
    if (m_cfData) {
        CFRetain(m_cfData.get());
        return m_cfData.get();
    }

    // Internal data in SharedBuffer can be segmented. We need to get the contiguous buffer.
    const Vector<char>& contiguousBuffer = buffer();
    return CFDataCreate(0, reinterpret_cast<const UInt8*>(contiguousBuffer.data()), contiguousBuffer.size());
}

PassRefPtr<SharedBuffer> SharedBuffer::wrapCFData(CFDataRef data)
{
    return adoptRef(new SharedBuffer(data));
}

bool SharedBuffer::hasPlatformData() const
{
    return m_cfData;
}

const char* SharedBuffer::platformData() const
{
    return reinterpret_cast<const char*>(CFDataGetBytePtr(m_cfData.get()));
}

unsigned SharedBuffer::platformDataSize() const
{
    return CFDataGetLength(m_cfData.get());
}

void SharedBuffer::maybeTransferPlatformData()
{
    if (!m_cfData)
        return;
    
    ASSERT(!m_size);
    
    // Hang on to the m_cfData pointer in a local pointer as append() will re-enter maybeTransferPlatformData()
    // and we need to make sure to early return when it does.
    RetainPtr<CFDataRef> cfData(AdoptCF, m_cfData.leakRef());

    append(reinterpret_cast<const char*>(CFDataGetBytePtr(cfData.get())), CFDataGetLength(cfData.get()));
}

void SharedBuffer::clearPlatformData()
{
    m_cfData = 0;
}

void SharedBuffer::tryReplaceContentsWithPlatformBuffer(SharedBuffer* newContents)
{
    if (!newContents->m_cfData)
        return;

    clear();
    m_cfData = newContents->m_cfData;
}

}
