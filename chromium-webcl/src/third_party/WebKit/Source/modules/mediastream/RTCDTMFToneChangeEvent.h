/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTCDTMFToneChangeEvent_h
#define RTCDTMFToneChangeEvent_h

#if ENABLE(MEDIA_STREAM)

#include "core/dom/Event.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

struct RTCDTMFToneChangeEventInit : public EventInit {
    String tone;
};

class RTCDTMFToneChangeEvent : public Event {
public:
    virtual ~RTCDTMFToneChangeEvent();

    static PassRefPtr<RTCDTMFToneChangeEvent> create();
    static PassRefPtr<RTCDTMFToneChangeEvent> create(const String& tone);
    static PassRefPtr<RTCDTMFToneChangeEvent> create(const AtomicString& type, const RTCDTMFToneChangeEventInit& initializer);

    const String& tone() const;

    virtual const AtomicString& interfaceName() const;

private:
    RTCDTMFToneChangeEvent();
    explicit RTCDTMFToneChangeEvent(const String& tone);
    explicit RTCDTMFToneChangeEvent(const RTCDTMFToneChangeEventInit&);

    String m_tone;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RTCDTMFToneChangeEvent_h
