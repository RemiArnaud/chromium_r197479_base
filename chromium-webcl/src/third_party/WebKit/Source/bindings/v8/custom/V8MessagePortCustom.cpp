/*
 * Copyright (C) 2009, 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "V8MessagePort.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8Utilities.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/MessagePort.h"
#include "wtf/ArrayBuffer.h"

namespace WebCore {

v8::Handle<v8::Value> V8MessagePort::postMessageMethodCustom(const v8::Arguments& args)
{
    MessagePort* messagePort = V8MessagePort::toNative(args.Holder());
    MessagePortArray portArray;
    ArrayBufferArray arrayBufferArray;
    if (args.Length() > 1) {
        if (!extractTransferables(args[1], portArray, arrayBufferArray, args.GetIsolate()))
            return v8::Undefined();
    }
    bool didThrow = false;
    RefPtr<SerializedScriptValue> message =
        SerializedScriptValue::create(args[0],
                                      &portArray,
                                      &arrayBufferArray,
                                      didThrow,
                                      args.GetIsolate());
    if (didThrow)
        return v8::Undefined();
    ExceptionCode ec = 0;
    messagePort->postMessage(message.release(), &portArray, ec);
    return setDOMException(ec, args.GetIsolate());
}

} // namespace WebCore
