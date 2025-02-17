/*
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "core/css/CSSValue.h"

#include "core/css/CSSAspectRatioValue.h"
#include "core/css/CSSBorderImageSliceValue.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSCanvasValue.h"
#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSImageGeneratorValue.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSInheritedValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSLineBoxContainValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSUnicodeRangeValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSVariableValue.h"
#include "core/css/FontFeatureValue.h"
#include "core/css/FontValue.h"
#include "core/css/ShadowValue.h"
#include "core/css/WebKitCSSArrayFunctionValue.h"
#include "core/css/WebKitCSSFilterValue.h"
#include "core/css/WebKitCSSMixFunctionValue.h"
#include "core/css/WebKitCSSShaderValue.h"
#include "core/css/WebKitCSSTransformValue.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include "core/svg/SVGColor.h"
#include "core/svg/SVGPaint.h"

#if ENABLE(SVG)
#include "core/css/WebKitCSSSVGDocumentValue.h"
#endif

namespace WebCore {

struct SameSizeAsCSSValue : public RefCounted<SameSizeAsCSSValue> {
    uint32_t bitfields;
};

COMPILE_ASSERT(sizeof(CSSValue) == sizeof(SameSizeAsCSSValue), CSS_value_should_stay_small);

class TextCloneCSSValue : public CSSValue {
public:
    static PassRefPtr<TextCloneCSSValue> create(ClassType classType, const String& text) { return adoptRef(new TextCloneCSSValue(classType, text)); }

    String cssText() const { return m_cssText; }

    void reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
        info.addMember(m_cssText, "cssText");
    }

private:
    TextCloneCSSValue(ClassType classType, const String& text) 
        : CSSValue(classType, /*isCSSOMSafe*/ true)
        , m_cssText(text)
    {
        m_isTextClone = true;
    }

    String m_cssText;
};

bool CSSValue::isImplicitInitialValue() const
{
    return m_classType == InitialClass && static_cast<const CSSInitialValue*>(this)->isImplicit();
}

CSSValue::Type CSSValue::cssValueType() const
{
    if (isInheritedValue())
        return CSS_INHERIT;
    if (isPrimitiveValue())
        return CSS_PRIMITIVE_VALUE;
    if (isValueList())
        return CSS_VALUE_LIST;
    if (isInitialValue())
        return CSS_INITIAL;
    return CSS_CUSTOM;
}

void CSSValue::addSubresourceStyleURLs(ListHashSet<KURL>& urls, const StyleSheetContents* styleSheet) const
{
    // This should get called for internal instances only.
    ASSERT(!isCSSOMSafe());

    if (isPrimitiveValue())
        static_cast<const CSSPrimitiveValue*>(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (isValueList())
        static_cast<const CSSValueList*>(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (classType() == FontFaceSrcClass)
        static_cast<const CSSFontFaceSrcValue*>(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (classType() == ReflectClass)
        static_cast<const CSSReflectValue*>(this)->addSubresourceStyleURLs(urls, styleSheet);
}

bool CSSValue::hasFailedOrCanceledSubresources() const
{
    // This should get called for internal instances only.
    ASSERT(!isCSSOMSafe());

    if (isValueList())
        return static_cast<const CSSValueList*>(this)->hasFailedOrCanceledSubresources();
    if (classType() == FontFaceSrcClass)
        return static_cast<const CSSFontFaceSrcValue*>(this)->hasFailedOrCanceledSubresources();
    if (classType() == ImageClass)
        return static_cast<const CSSImageValue*>(this)->hasFailedOrCanceledSubresources();
    if (classType() == CrossfadeClass)
        return static_cast<const CSSCrossfadeValue*>(this)->hasFailedOrCanceledSubresources();
#if ENABLE(CSS_IMAGE_SET)
    if (classType() == ImageSetClass)
        return static_cast<const CSSImageSetValue*>(this)->hasFailedOrCanceledSubresources();
#endif
    return false;
}

void CSSValue::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    if (m_isTextClone) {
        ASSERT(isCSSOMSafe());
        static_cast<const TextCloneCSSValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    }

    ASSERT(!isCSSOMSafe() || isSubtypeExposedToCSSOM());
    switch (classType()) {
    case PrimitiveClass:
        static_cast<const CSSPrimitiveValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case ImageClass:
        static_cast<const CSSImageValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case CursorImageClass:
        static_cast<const CSSCursorImageValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case CanvasClass:
        static_cast<const CSSCanvasValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case CrossfadeClass:
        static_cast<const CSSCrossfadeValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case LinearGradientClass:
        static_cast<const CSSLinearGradientValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case RadialGradientClass:
        static_cast<const CSSRadialGradientValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case CubicBezierTimingFunctionClass:
        static_cast<const CSSCubicBezierTimingFunctionValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case LinearTimingFunctionClass:
        static_cast<const CSSLinearTimingFunctionValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case StepsTimingFunctionClass:
        static_cast<const CSSStepsTimingFunctionValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case AspectRatioClass:
        static_cast<const CSSAspectRatioValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case BorderImageSliceClass:
        static_cast<const CSSBorderImageSliceValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case FontFeatureClass:
        static_cast<const FontFeatureValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case FontClass:
        static_cast<const FontValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case FontFaceSrcClass:
        static_cast<const CSSFontFaceSrcValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case FunctionClass:
        static_cast<const CSSFunctionValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case InheritedClass:
        static_cast<const CSSInheritedValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case InitialClass:
        static_cast<const CSSInitialValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case ReflectClass:
        static_cast<const CSSReflectValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case ShadowClass:
        static_cast<const ShadowValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case UnicodeRangeClass:
        static_cast<const CSSUnicodeRangeValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case LineBoxContainClass:
        static_cast<const CSSLineBoxContainValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case CalculationClass:
        static_cast<const CSSCalcValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case WebKitCSSArrayFunctionValueClass:
        static_cast<const WebKitCSSArrayFunctionValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case WebKitCSSMixFunctionValueClass:
        static_cast<const WebKitCSSMixFunctionValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case WebKitCSSShaderClass:
        static_cast<const WebKitCSSShaderValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case VariableClass:
        static_cast<const CSSVariableValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
#if ENABLE(SVG)
    case SVGColorClass:
        static_cast<const SVGColor*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case SVGPaintClass:
        static_cast<const SVGPaint*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case WebKitCSSSVGDocumentClass:
        static_cast<const WebKitCSSSVGDocumentValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
#endif
    case ValueListClass:
        static_cast<const CSSValueList*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
#if ENABLE(CSS_IMAGE_SET)
    case ImageSetClass:
        static_cast<const CSSImageSetValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
#endif
    case WebKitCSSFilterClass:
        static_cast<const WebKitCSSFilterValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case WebKitCSSTransformClass:
        static_cast<const WebKitCSSTransformValue*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    }
    ASSERT_NOT_REACHED();
}

template<class ChildClassType>
inline static bool compareCSSValues(const CSSValue& first, const CSSValue& second)
{
    return static_cast<const ChildClassType&>(first).equals(static_cast<const ChildClassType&>(second));
}

bool CSSValue::equals(const CSSValue& other) const
{
    if (m_isTextClone) {
        ASSERT(isCSSOMSafe());
        return static_cast<const TextCloneCSSValue*>(this)->cssText() == other.cssText();
    }

    if (m_classType == other.m_classType) {
        switch (m_classType) {
        case AspectRatioClass:
            return compareCSSValues<CSSAspectRatioValue>(*this, other);
        case BorderImageSliceClass:
            return compareCSSValues<CSSBorderImageSliceValue>(*this, other);
        case CanvasClass:
            return compareCSSValues<CSSCanvasValue>(*this, other);
        case CursorImageClass:
            return compareCSSValues<CSSCursorImageValue>(*this, other);
        case FontClass:
            return compareCSSValues<FontValue>(*this, other);
        case FontFaceSrcClass:
            return compareCSSValues<CSSFontFaceSrcValue>(*this, other);
        case FontFeatureClass:
            return compareCSSValues<FontFeatureValue>(*this, other);
        case FunctionClass:
            return compareCSSValues<CSSFunctionValue>(*this, other);
        case LinearGradientClass:
            return compareCSSValues<CSSLinearGradientValue>(*this, other);
        case RadialGradientClass:
            return compareCSSValues<CSSRadialGradientValue>(*this, other);
        case CrossfadeClass:
            return compareCSSValues<CSSCrossfadeValue>(*this, other);
        case ImageClass:
            return compareCSSValues<CSSImageValue>(*this, other);
        case InheritedClass:
            return compareCSSValues<CSSInheritedValue>(*this, other);
        case InitialClass:
            return compareCSSValues<CSSInitialValue>(*this, other);
        case PrimitiveClass:
            return compareCSSValues<CSSPrimitiveValue>(*this, other);
        case ReflectClass:
            return compareCSSValues<CSSReflectValue>(*this, other);
        case ShadowClass:
            return compareCSSValues<ShadowValue>(*this, other);
        case LinearTimingFunctionClass:
            return compareCSSValues<CSSLinearTimingFunctionValue>(*this, other);
        case CubicBezierTimingFunctionClass:
            return compareCSSValues<CSSCubicBezierTimingFunctionValue>(*this, other);
        case StepsTimingFunctionClass:
            return compareCSSValues<CSSStepsTimingFunctionValue>(*this, other);
        case UnicodeRangeClass:
            return compareCSSValues<CSSUnicodeRangeValue>(*this, other);
        case ValueListClass:
            return compareCSSValues<CSSValueList>(*this, other);
        case WebKitCSSTransformClass:
            return compareCSSValues<WebKitCSSTransformValue>(*this, other);
        case LineBoxContainClass:
            return compareCSSValues<CSSLineBoxContainValue>(*this, other);
        case CalculationClass:
            return compareCSSValues<CSSCalcValue>(*this, other);
#if ENABLE(CSS_IMAGE_SET)
        case ImageSetClass:
            return compareCSSValues<CSSImageSetValue>(*this, other);
#endif
        case WebKitCSSFilterClass:
            return compareCSSValues<WebKitCSSFilterValue>(*this, other);
        case WebKitCSSArrayFunctionValueClass:
            return compareCSSValues<WebKitCSSArrayFunctionValue>(*this, other);
        case WebKitCSSMixFunctionValueClass:
            return compareCSSValues<WebKitCSSMixFunctionValue>(*this, other);
        case WebKitCSSShaderClass:
            return compareCSSValues<WebKitCSSShaderValue>(*this, other);
        case VariableClass:
            return compareCSSValues<CSSVariableValue>(*this, other);
#if ENABLE(SVG)
        case SVGColorClass:
            return compareCSSValues<SVGColor>(*this, other);
        case SVGPaintClass:
            return compareCSSValues<SVGPaint>(*this, other);
        case WebKitCSSSVGDocumentClass:
            return compareCSSValues<WebKitCSSSVGDocumentValue>(*this, other);
#endif
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    } else if (m_classType == ValueListClass && other.m_classType != ValueListClass)
        return static_cast<const CSSValueList*>(this)->equals(other);
    else if (m_classType != ValueListClass && other.m_classType == ValueListClass)
        return static_cast<const CSSValueList&>(other).equals(*this);
    return false;
}

String CSSValue::cssText() const
{
    if (m_isTextClone) {
         ASSERT(isCSSOMSafe());
        return static_cast<const TextCloneCSSValue*>(this)->cssText();
    }
    ASSERT(!isCSSOMSafe() || isSubtypeExposedToCSSOM());

    switch (classType()) {
    case AspectRatioClass:
        return static_cast<const CSSAspectRatioValue*>(this)->customCssText();
    case BorderImageSliceClass:
        return static_cast<const CSSBorderImageSliceValue*>(this)->customCssText();
    case CanvasClass:
        return static_cast<const CSSCanvasValue*>(this)->customCssText();
    case CursorImageClass:
        return static_cast<const CSSCursorImageValue*>(this)->customCssText();
    case FontClass:
        return static_cast<const FontValue*>(this)->customCssText();
    case FontFaceSrcClass:
        return static_cast<const CSSFontFaceSrcValue*>(this)->customCssText();
    case FontFeatureClass:
        return static_cast<const FontFeatureValue*>(this)->customCssText();
    case FunctionClass:
        return static_cast<const CSSFunctionValue*>(this)->customCssText();
    case LinearGradientClass:
        return static_cast<const CSSLinearGradientValue*>(this)->customCssText();
    case RadialGradientClass:
        return static_cast<const CSSRadialGradientValue*>(this)->customCssText();
    case CrossfadeClass:
        return static_cast<const CSSCrossfadeValue*>(this)->customCssText();
    case ImageClass:
        return static_cast<const CSSImageValue*>(this)->customCssText();
    case InheritedClass:
        return static_cast<const CSSInheritedValue*>(this)->customCssText();
    case InitialClass:
        return static_cast<const CSSInitialValue*>(this)->customCssText();
    case PrimitiveClass:
        return static_cast<const CSSPrimitiveValue*>(this)->customCssText();
    case ReflectClass:
        return static_cast<const CSSReflectValue*>(this)->customCssText();
    case ShadowClass:
        return static_cast<const ShadowValue*>(this)->customCssText();
    case LinearTimingFunctionClass:
        return static_cast<const CSSLinearTimingFunctionValue*>(this)->customCssText();
    case CubicBezierTimingFunctionClass:
        return static_cast<const CSSCubicBezierTimingFunctionValue*>(this)->customCssText();
    case StepsTimingFunctionClass:
        return static_cast<const CSSStepsTimingFunctionValue*>(this)->customCssText();
    case UnicodeRangeClass:
        return static_cast<const CSSUnicodeRangeValue*>(this)->customCssText();
    case ValueListClass:
        return static_cast<const CSSValueList*>(this)->customCssText();
    case WebKitCSSTransformClass:
        return static_cast<const WebKitCSSTransformValue*>(this)->customCssText();
    case LineBoxContainClass:
        return static_cast<const CSSLineBoxContainValue*>(this)->customCssText();
    case CalculationClass:
        return static_cast<const CSSCalcValue*>(this)->customCssText();
#if ENABLE(CSS_IMAGE_SET)
    case ImageSetClass:
        return static_cast<const CSSImageSetValue*>(this)->customCssText();
#endif
    case WebKitCSSFilterClass:
        return static_cast<const WebKitCSSFilterValue*>(this)->customCssText();
    case WebKitCSSArrayFunctionValueClass:
        return static_cast<const WebKitCSSArrayFunctionValue*>(this)->customCssText();
    case WebKitCSSMixFunctionValueClass:
        return static_cast<const WebKitCSSMixFunctionValue*>(this)->customCssText();
    case WebKitCSSShaderClass:
        return static_cast<const WebKitCSSShaderValue*>(this)->customCssText();
    case VariableClass:
        return static_cast<const CSSVariableValue*>(this)->value();
#if ENABLE(SVG)
    case SVGColorClass:
        return static_cast<const SVGColor*>(this)->customCssText();
    case SVGPaintClass:
        return static_cast<const SVGPaint*>(this)->customCssText();
    case WebKitCSSSVGDocumentClass:
        return static_cast<const WebKitCSSSVGDocumentValue*>(this)->customCssText();
#endif
    }
    ASSERT_NOT_REACHED();
    return String();
}

String CSSValue::serializeResolvingVariables(const HashMap<AtomicString, String>& variables) const
{
    switch (classType()) {
    case PrimitiveClass:
        return static_cast<const CSSPrimitiveValue*>(this)->customSerializeResolvingVariables(variables);
    case ReflectClass:
        return static_cast<const CSSReflectValue*>(this)->customSerializeResolvingVariables(variables);
    case ValueListClass:
        return static_cast<const CSSValueList*>(this)->customSerializeResolvingVariables(variables);
    case WebKitCSSTransformClass:
        return static_cast<const WebKitCSSTransformValue*>(this)->customSerializeResolvingVariables(variables);
    default:
        return cssText();
    }
}

void CSSValue::destroy()
{
    if (m_isTextClone) {
        ASSERT(isCSSOMSafe());
        delete static_cast<TextCloneCSSValue*>(this);
        return;
    }
    ASSERT(!isCSSOMSafe() || isSubtypeExposedToCSSOM());

    switch (classType()) {
    case AspectRatioClass:
        delete static_cast<CSSAspectRatioValue*>(this);
        return;
    case BorderImageSliceClass:
        delete static_cast<CSSBorderImageSliceValue*>(this);
        return;
    case CanvasClass:
        delete static_cast<CSSCanvasValue*>(this);
        return;
    case CursorImageClass:
        delete static_cast<CSSCursorImageValue*>(this);
        return;
    case FontClass:
        delete static_cast<FontValue*>(this);
        return;
    case FontFaceSrcClass:
        delete static_cast<CSSFontFaceSrcValue*>(this);
        return;
    case FontFeatureClass:
        delete static_cast<FontFeatureValue*>(this);
        return;
    case FunctionClass:
        delete static_cast<CSSFunctionValue*>(this);
        return;
    case LinearGradientClass:
        delete static_cast<CSSLinearGradientValue*>(this);
        return;
    case RadialGradientClass:
        delete static_cast<CSSRadialGradientValue*>(this);
        return;
    case CrossfadeClass:
        delete static_cast<CSSCrossfadeValue*>(this);
        return;
    case ImageClass:
        delete static_cast<CSSImageValue*>(this);
        return;
    case InheritedClass:
        delete static_cast<CSSInheritedValue*>(this);
        return;
    case InitialClass:
        delete static_cast<CSSInitialValue*>(this);
        return;
    case PrimitiveClass:
        delete static_cast<CSSPrimitiveValue*>(this);
        return;
    case ReflectClass:
        delete static_cast<CSSReflectValue*>(this);
        return;
    case ShadowClass:
        delete static_cast<ShadowValue*>(this);
        return;
    case LinearTimingFunctionClass:
        delete static_cast<CSSLinearTimingFunctionValue*>(this);
        return;
    case CubicBezierTimingFunctionClass:
        delete static_cast<CSSCubicBezierTimingFunctionValue*>(this);
        return;
    case StepsTimingFunctionClass:
        delete static_cast<CSSStepsTimingFunctionValue*>(this);
        return;
    case UnicodeRangeClass:
        delete static_cast<CSSUnicodeRangeValue*>(this);
        return;
    case ValueListClass:
        delete static_cast<CSSValueList*>(this);
        return;
    case WebKitCSSTransformClass:
        delete static_cast<WebKitCSSTransformValue*>(this);
        return;
    case LineBoxContainClass:
        delete static_cast<CSSLineBoxContainValue*>(this);
        return;
    case CalculationClass:
        delete static_cast<CSSCalcValue*>(this);
        return;
#if ENABLE(CSS_IMAGE_SET)
    case ImageSetClass:
        delete static_cast<CSSImageSetValue*>(this);
        return;
#endif
    case WebKitCSSFilterClass:
        delete static_cast<WebKitCSSFilterValue*>(this);
        return;
    case WebKitCSSArrayFunctionValueClass:
        delete static_cast<WebKitCSSArrayFunctionValue*>(this);
        return;
    case WebKitCSSMixFunctionValueClass:
        delete static_cast<WebKitCSSMixFunctionValue*>(this);
        return;
    case WebKitCSSShaderClass:
        delete static_cast<WebKitCSSShaderValue*>(this);
        return;
    case VariableClass:
        delete static_cast<CSSVariableValue*>(this);
        return;
#if ENABLE(SVG)
    case SVGColorClass:
        delete static_cast<SVGColor*>(this);
        return;
    case SVGPaintClass:
        delete static_cast<SVGPaint*>(this);
        return;
    case WebKitCSSSVGDocumentClass:
        delete static_cast<WebKitCSSSVGDocumentValue*>(this);
        return;
#endif
    }
    ASSERT_NOT_REACHED();
}

PassRefPtr<CSSValue> CSSValue::cloneForCSSOM() const
{
    switch (classType()) {
    case PrimitiveClass:
        return static_cast<const CSSPrimitiveValue*>(this)->cloneForCSSOM();
    case ValueListClass:
        return static_cast<const CSSValueList*>(this)->cloneForCSSOM();
    case ImageClass:
    case CursorImageClass:
        return static_cast<const CSSImageValue*>(this)->cloneForCSSOM();
    case WebKitCSSFilterClass:
        return static_cast<const WebKitCSSFilterValue*>(this)->cloneForCSSOM();
    case WebKitCSSArrayFunctionValueClass:
        return static_cast<const WebKitCSSArrayFunctionValue*>(this)->cloneForCSSOM();
    case WebKitCSSMixFunctionValueClass:
        return static_cast<const WebKitCSSMixFunctionValue*>(this)->cloneForCSSOM();
    case WebKitCSSTransformClass:
        return static_cast<const WebKitCSSTransformValue*>(this)->cloneForCSSOM();
#if ENABLE(CSS_IMAGE_SET)
    case ImageSetClass:
        return static_cast<const CSSImageSetValue*>(this)->cloneForCSSOM();
#endif
#if ENABLE(SVG)
    case SVGColorClass:
        return static_cast<const SVGColor*>(this)->cloneForCSSOM();
    case SVGPaintClass:
        return static_cast<const SVGPaint*>(this)->cloneForCSSOM();
#endif
    default:
        ASSERT(!isSubtypeExposedToCSSOM());
        return TextCloneCSSValue::create(classType(), cssText());
    }
}

}
