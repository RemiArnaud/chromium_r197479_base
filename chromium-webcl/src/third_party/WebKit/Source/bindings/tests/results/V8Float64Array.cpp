/*
    This file is part of the WebKit open source project.
    This file has been generated by generate-bindings.pl. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#include "V8Float64Array.h"

#include "V8ArrayBufferView.h"
#include "V8Float32Array.h"
#include "V8Int32Array.h"
#include "bindings/v8/BindingState.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMWrapper.h"
#include "bindings/v8/custom/V8ArrayBufferViewCustom.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/ExceptionCode.h"
#include "core/page/Frame.h"
#include "RuntimeEnabledFeatures.h"
#include <wtf/Float32Array.h>
#include <wtf/Float64Array.h>
#include <wtf/GetPtr.h>
#include <wtf/Int32Array.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/UnusedParam.h>

#if ENABLE(BINDING_INTEGRITY)
#if defined(OS_WIN)
#pragma warning(disable: 4483)
extern "C" { extern void (*const __identifier("??_7Float64Array@WebCore@@6B@")[])(); }
#else
extern "C" { extern void* _ZTVN7WebCore12Float64ArrayE[]; }
#endif
#endif // ENABLE(BINDING_INTEGRITY)

namespace WebCore {

#if ENABLE(BINDING_INTEGRITY)
// This checks if a DOM object that is about to be wrapped is valid.
// Specifically, it checks that a vtable of the DOM object is equal to
// a vtable of an expected class.
// Due to a dangling pointer, the DOM object you are wrapping might be
// already freed or realloced. If freed, the check will fail because
// a free list pointer should be stored at the head of the DOM object.
// If realloced, the check will fail because the vtable of the DOM object
// differs from the expected vtable (unless the same class of DOM object
// is realloced on the slot).
inline void checkTypeOrDieTrying(Float64Array* object)
{
    void* actualVTablePointer = *(reinterpret_cast<void**>(object));
#if defined(OS_WIN)
    void* expectedVTablePointer = reinterpret_cast<void*>(__identifier("??_7Float64Array@WebCore@@6B@"));
#else
    void* expectedVTablePointer = &_ZTVN7WebCore12Float64ArrayE[2];
#endif
    if (actualVTablePointer != expectedVTablePointer)
        CRASH();
}
#endif // ENABLE(BINDING_INTEGRITY)

#if defined(OS_WIN)
// In ScriptWrappable, the use of extern function prototypes inside templated static methods has an issue on windows.
// These prototypes do not pick up the surrounding namespace, so drop out of WebCore as a workaround.
} // namespace WebCore
using WebCore::ScriptWrappable;
using WebCore::V8Float64Array;
using WebCore::Float64Array;
#endif
void initializeScriptWrappableForInterface(Float64Array* object)
{
    if (ScriptWrappable::wrapperCanBeStoredInObject(object))
        ScriptWrappable::setTypeInfoInObject(object, &V8Float64Array::info);
}
#if defined(OS_WIN)
namespace WebCore {
#endif
WrapperTypeInfo V8Float64Array::info = { V8Float64Array::GetTemplate, V8Float64Array::derefObject, 0, 0, 0, V8Float64Array::installPerContextPrototypeProperties, &V8ArrayBufferView::info, WrapperTypeObjectPrototype };

namespace Float64ArrayV8Internal {

template <typename T> void V8_USE(T) { }

static v8::Handle<v8::Value> fooMethod(const v8::Arguments& args)
{
    if (args.Length() < 1)
        return throwNotEnoughArgumentsError(args.GetIsolate());
    Float64Array* imp = V8Float64Array::toNative(args.Holder());
    V8TRYCATCH(Float32Array*, array, V8Float32Array::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate())) ? V8Float32Array::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0);
    return toV8(imp->foo(array), args.Holder(), args.GetIsolate());
}

static v8::Handle<v8::Value> fooMethodCallback(const v8::Arguments& args)
{
    return Float64ArrayV8Internal::fooMethod(args);
}

static v8::Handle<v8::Value> setMethod(const v8::Arguments& args)
{
    return setWebGLArrayHelper<Float64Array, V8Float64Array>(args);
}

static v8::Handle<v8::Value> setMethodCallback(const v8::Arguments& args)
{
    return Float64ArrayV8Internal::setMethod(args);
}

static v8::Handle<v8::Value> constructor(const v8::Arguments& args)
{
    return constructWebGLArray<Float64Array, V8Float64Array, double>(args, &V8Float64Array::info, v8::kExternalDoubleArray);
}

} // namespace Float64ArrayV8Internal

v8::Handle<v8::Object> wrap(Float64Array* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    v8::Handle<v8::Object> wrapper = V8Float64Array::createWrapper(impl, creationContext, isolate);
    if (!wrapper.IsEmpty())
        wrapper->SetIndexedPropertiesToExternalArrayData(impl->baseAddress(), v8::kExternalDoubleArray, impl->length());
    return wrapper;
}

static const V8DOMConfiguration::BatchedMethod V8Float64ArrayMethods[] = {
    {"set", Float64ArrayV8Internal::setMethodCallback, 0, 0},
};

v8::Handle<v8::Value> V8Float64Array::constructorCallback(const v8::Arguments& args)
{
    if (!args.IsConstructCall())
        return throwTypeError("DOM object constructor cannot be called as a function.", args.GetIsolate());

    if (ConstructorMode::current() == ConstructorMode::WrapExistingObject)
        return args.Holder();

    return Float64ArrayV8Internal::constructor(args);
}

static v8::Persistent<v8::FunctionTemplate> ConfigureV8Float64ArrayTemplate(v8::Persistent<v8::FunctionTemplate> desc, v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    desc->ReadOnlyPrototype();

    v8::Local<v8::Signature> defaultSignature;
    defaultSignature = V8DOMConfiguration::configureTemplate(desc, "Float64Array", V8ArrayBufferView::GetTemplate(isolate, currentWorldType), V8Float64Array::internalFieldCount,
        0, 0,
        V8Float64ArrayMethods, WTF_ARRAY_LENGTH(V8Float64ArrayMethods), isolate, currentWorldType);
    UNUSED_PARAM(defaultSignature); // In some cases, it will not be used.
    desc->SetCallHandler(V8Float64Array::constructorCallback);
    desc->SetLength(1);
    v8::Local<v8::ObjectTemplate> instance = desc->InstanceTemplate();
    v8::Local<v8::ObjectTemplate> proto = desc->PrototypeTemplate();
    UNUSED_PARAM(instance); // In some cases, it will not be used.
    UNUSED_PARAM(proto); // In some cases, it will not be used.

    // Custom Signature 'foo'
    const int fooArgc = 1;
    v8::Handle<v8::FunctionTemplate> fooArgv[fooArgc] = { V8PerIsolateData::from(isolate)->rawTemplate(&V8Float32Array::info, currentWorldType) };
    v8::Handle<v8::Signature> fooSignature = v8::Signature::New(desc, fooArgc, fooArgv);
    proto->Set(v8::String::NewSymbol("foo"), v8::FunctionTemplate::New(Float64ArrayV8Internal::fooMethodCallback, v8Undefined(), fooSignature, 1));

    // Custom toString template
    desc->Set(v8::String::NewSymbol("toString"), V8PerIsolateData::current()->toStringTemplate());
    return desc;
}

v8::Persistent<v8::FunctionTemplate> V8Float64Array::GetTemplate(v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    V8PerIsolateData::TemplateMap::iterator result = data->templateMap(currentWorldType).find(&info);
    if (result != data->templateMap(currentWorldType).end())
        return result->value;

    v8::HandleScope handleScope;
    v8::Persistent<v8::FunctionTemplate> templ =
        ConfigureV8Float64ArrayTemplate(data->rawTemplate(&info, currentWorldType), isolate, currentWorldType);
    data->templateMap(currentWorldType).add(&info, templ);
    return templ;
}

bool V8Float64Array::HasInstance(v8::Handle<v8::Value> value, v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    return V8PerIsolateData::from(isolate)->hasInstance(&info, value, currentWorldType);
}

bool V8Float64Array::HasInstanceInAnyWorld(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    return V8PerIsolateData::from(isolate)->hasInstance(&info, value, MainWorld)
        || V8PerIsolateData::from(isolate)->hasInstance(&info, value, IsolatedWorld)
        || V8PerIsolateData::from(isolate)->hasInstance(&info, value, WorkerWorld);
}


v8::Handle<v8::Object> V8Float64Array::createWrapper(PassRefPtr<Float64Array> impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl.get());
    ASSERT(DOMDataStore::getWrapper(impl.get(), isolate).IsEmpty());

#if ENABLE(BINDING_INTEGRITY)
    checkTypeOrDieTrying(impl.get());
#endif
    ASSERT(static_cast<void*>(static_cast<ArrayBufferView*>(impl.get())) == static_cast<void*>(impl.get()));

    v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, &info, impl.get(), isolate);
    if (UNLIKELY(wrapper.IsEmpty()))
        return wrapper;

    installPerContextProperties(wrapper, impl.get(), isolate);
    V8DOMWrapper::associateObjectWithWrapper(impl, &info, wrapper, isolate, hasDependentLifetime ? WrapperConfiguration::Dependent : WrapperConfiguration::Independent);
    return wrapper;
}
void V8Float64Array::derefObject(void* object)
{
    static_cast<Float64Array*>(object)->deref();
}

} // namespace WebCore
