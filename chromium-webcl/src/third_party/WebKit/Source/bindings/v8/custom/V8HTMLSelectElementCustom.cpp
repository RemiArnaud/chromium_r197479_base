/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "bindings/v8/custom/V8HTMLSelectElementCustom.h"

#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLOptionsCollection.h"
#include "core/html/HTMLSelectElement.h"

#include "V8HTMLOptionElement.h"
#include "V8HTMLSelectElement.h"
#include "V8Node.h"
#include "V8NodeList.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8Collection.h"
#include "bindings/v8/custom/V8NamedNodesCollection.h"

namespace WebCore {

v8::Handle<v8::Value> V8HTMLSelectElement::indexedPropertyGetter(uint32_t index, const v8::AccessorInfo& info)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(info.Holder()));
    HTMLSelectElement* select = V8HTMLSelectElement::toNative(info.Holder());
    RefPtr<Node> result = select->item(index);
    if (!result)
        return v8Undefined();

    return toV8Fast(result.release(), info, select);
}

v8::Handle<v8::Value> V8HTMLSelectElement::indexedPropertySetter(uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    HTMLSelectElement* select = V8HTMLSelectElement::toNative(info.Holder());
    return toOptionsCollectionSetter(index, value, select, info.GetIsolate());
}

v8::Handle<v8::Value> V8HTMLSelectElement::removeMethodCustom(const v8::Arguments& args)
{
    HTMLSelectElement* imp = V8HTMLSelectElement::toNative(args.Holder());
    return removeElement(imp, args);
}

v8::Handle<v8::Value> removeElement(HTMLSelectElement* imp, const v8::Arguments& args) 
{
    if (V8HTMLOptionElement::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate()))) {
        HTMLOptionElement* element = V8HTMLOptionElement::toNative(v8::Handle<v8::Object>::Cast(args[0]));
        imp->remove(element->index());
        return v8::Undefined();
    }

    imp->remove(toInt32(args[0]));
    return v8::Undefined();
}

} // namespace WebCore
