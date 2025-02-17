/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/rendering/RenderSlider.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "core/css/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/Event.h"
#include "core/dom/EventNames.h"
#include "core/dom/MouseEvent.h"
#include "core/dom/Node.h"
#include "core/dom/ShadowRoot.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/StepRange.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/MediaControlElements.h"
#include "core/html/shadow/SliderThumbElement.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include <wtf/MathExtras.h>

using std::min;

namespace WebCore {

const int RenderSlider::defaultTrackLength = 129;

RenderSlider::RenderSlider(HTMLInputElement* element)
    : RenderFlexibleBox(element)
{
    // We assume RenderSlider works only with <input type=range>.
    ASSERT(element->isRangeControl());
}

RenderSlider::~RenderSlider()
{
}

bool RenderSlider::canBeReplacedWithInlineRunIn() const
{
    return false;
}

int RenderSlider::baselinePosition(FontBaseline, bool /*firstLine*/, LineDirectionMode, LinePositionMode) const
{
    // FIXME: Patch this function for writing-mode.
    return height() + marginTop();
}

void RenderSlider::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    maxLogicalWidth = defaultTrackLength * style()->effectiveZoom();
    if (!style()->width().isPercent())
        minLogicalWidth = maxLogicalWidth;
}

void RenderSlider::computePreferredLogicalWidths()
{
    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(style()->width().value());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxPreferredLogicalWidth = max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(style()->minWidth().value()));
        m_minPreferredLogicalWidth = max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(style()->minWidth().value()));
    }

    if (style()->maxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(style()->maxWidth().value()));
        m_minPreferredLogicalWidth = min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(style()->maxWidth().value()));
    }

    LayoutUnit toAdd = borderAndPaddingWidth();
    m_minPreferredLogicalWidth += toAdd;
    m_maxPreferredLogicalWidth += toAdd;

    setPreferredLogicalWidthsDirty(false); 
}

void RenderSlider::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    // FIXME: Find a way to cascade appearance.
    // http://webkit.org/b/62535
    RenderBox* thumbBox = sliderThumbElementOf(node())->renderBox();
    if (thumbBox && thumbBox->isSliderThumb())
        static_cast<RenderSliderThumb*>(thumbBox)->updateAppearance(style());

    RenderFlexibleBox::layout();
}

bool RenderSlider::inDragMode() const
{
    return sliderThumbElementOf(node())->active();
}

} // namespace WebCore
