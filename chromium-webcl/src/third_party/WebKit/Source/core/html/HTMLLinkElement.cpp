/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Rob Buis (rwlbuis@gmail.com)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 */

#include "config.h"
#include "core/html/HTMLLinkElement.h"

#include "HTMLNames.h"
#include "bindings/v8/ScriptEventListener.h"
#include "core/css/MediaList.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/StyleResolver.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentStyleSheetCollection.h"
#include "core/dom/Event.h"
#include "core/dom/EventSender.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/cache/CachedCSSStyleSheet.h"
#include "core/loader/cache/CachedResource.h"
#include "core/loader/cache/CachedResourceLoader.h"
#include "core/loader/cache/CachedResourceRequest.h"
#include "core/page/Frame.h"
#include "core/page/FrameTree.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/SecurityOrigin.h"
#include "core/page/Settings.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

static LinkEventSender& linkLoadEventSender()
{
    DEFINE_STATIC_LOCAL(LinkEventSender, sharedLoadEventSender, (eventNames().loadEvent));
    return sharedLoadEventSender;
}

inline HTMLLinkElement::HTMLLinkElement(const QualifiedName& tagName, Document* document, bool createdByParser)
    : HTMLElement(tagName, document)
    , m_linkLoader(this)
    , m_sizes(DOMSettableTokenList::create())
    , m_disabledState(Unset)
    , m_loading(false)
    , m_createdByParser(createdByParser)
    , m_isInShadowTree(false)
    , m_firedLoad(false)
    , m_loadedSheet(false)
    , m_pendingSheetType(None)
    , m_beforeLoadRecurseCount(0)
{
    ASSERT(hasTagName(linkTag));
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLLinkElement> HTMLLinkElement::create(const QualifiedName& tagName, Document* document, bool createdByParser)
{
    return adoptRef(new HTMLLinkElement(tagName, document, createdByParser));
}

HTMLLinkElement::~HTMLLinkElement()
{
    if (m_sheet)
        m_sheet->clearOwnerNode();

    if (m_cachedSheet)
        m_cachedSheet->removeClient(this);

    if (inDocument())
        document()->styleSheetCollection()->removeStyleSheetCandidateNode(this);

    linkLoadEventSender().cancelEvent(this);
}

void HTMLLinkElement::setDisabledState(bool disabled)
{
    DisabledState oldDisabledState = m_disabledState;
    m_disabledState = disabled ? Disabled : EnabledViaScript;
    if (oldDisabledState != m_disabledState) {
        // If we change the disabled state while the sheet is still loading, then we have to
        // perform three checks:
        if (styleSheetIsLoading()) {
            // Check #1: The sheet becomes disabled while loading.
            if (m_disabledState == Disabled)
                removePendingSheet();

            // Check #2: An alternate sheet becomes enabled while it is still loading.
            if (m_relAttribute.isAlternate() && m_disabledState == EnabledViaScript)
                addPendingSheet(Blocking);

            // Check #3: A main sheet becomes enabled while it was still loading and
            // after it was disabled via script. It takes really terrible code to make this
            // happen (a double toggle for no reason essentially). This happens on
            // virtualplastic.net, which manages to do about 12 enable/disables on only 3
            // sheets. :)
            if (!m_relAttribute.isAlternate() && m_disabledState == EnabledViaScript && oldDisabledState == Disabled)
                addPendingSheet(Blocking);

            // If the sheet is already loading just bail.
            return;
        }

        // Load the sheet, since it's never been loaded before.
        if (!m_sheet && m_disabledState == EnabledViaScript)
            process();
        else
            document()->styleResolverChanged(DeferRecalcStyle); // Update the style selector.
    }
}

void HTMLLinkElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == relAttr) {
        m_relAttribute = LinkRelAttribute(value);
        process();
    } else if (name == hrefAttr) {
        process();
    } else if (name == typeAttr) {
        m_type = value;
        process();
    } else if (name == sizesAttr) {
        setSizes(value);
        process();
    } else if (name == mediaAttr) {
        m_media = value.string().lower();
        process();
    } else if (name == disabledAttr)
        setDisabledState(!value.isNull());
    else if (name == onbeforeloadAttr)
        setAttributeEventListener(eventNames().beforeloadEvent, createAttributeEventListener(this, name, value));
    else {
        if (name == titleAttr && m_sheet)
            m_sheet->setTitle(value);
        HTMLElement::parseAttribute(name, value);
    }
}

bool HTMLLinkElement::shouldLoadLink()
{
    bool continueLoad = true;
    RefPtr<Document> originalDocument = document();
    int recursionRank = ++m_beforeLoadRecurseCount;
    if (!dispatchBeforeLoadEvent(getNonEmptyURLAttribute(hrefAttr)))
        continueLoad = false;

    // A beforeload handler might have removed us from the document or changed the document.
    if (continueLoad && (!inDocument() || document() != originalDocument))
        continueLoad = false;

    // If the beforeload handler recurses into the link element by mutating it, we should only
    // let the latest (innermost) mutation occur.
    if (recursionRank != m_beforeLoadRecurseCount)
        continueLoad = false;

    if (recursionRank == 1)
        m_beforeLoadRecurseCount = 0;

    return continueLoad;
}

void HTMLLinkElement::process()
{
    if (!inDocument() || m_isInShadowTree) {
        ASSERT(!m_sheet);
        return;
    }

    String type = m_type.lower();
    KURL url = getNonEmptyURLAttribute(hrefAttr);

    if (!m_linkLoader.loadLink(m_relAttribute, type, m_sizes->toString(), url, document()))
        return;

    if ((m_disabledState != Disabled) && m_relAttribute.isStyleSheet()
        && document()->frame() && url.isValid()) {
        
        String charset = getAttribute(charsetAttr);
        if (charset.isEmpty() && document()->frame())
            charset = document()->charset();
        
        if (m_cachedSheet) {
            removePendingSheet();
            m_cachedSheet->removeClient(this);
            m_cachedSheet = 0;
        }

        if (!shouldLoadLink())
            return;

        m_loading = true;

        bool mediaQueryMatches = true;
        if (!m_media.isEmpty()) {
            RefPtr<RenderStyle> documentStyle = StyleResolver::styleForDocument(document());
            RefPtr<MediaQuerySet> media = MediaQuerySet::createAllowingDescriptionSyntax(m_media);
            MediaQueryEvaluator evaluator(document()->frame()->view()->mediaType(), document()->frame(), documentStyle.get());
            mediaQueryMatches = evaluator.eval(media.get());
        }

        // Don't hold up render tree construction and script execution on stylesheets
        // that are not needed for the rendering at the moment.
        bool blocking = mediaQueryMatches && !isAlternate();
        addPendingSheet(blocking ? Blocking : NonBlocking);

        // Load stylesheets that are not needed for the rendering immediately with low priority.
        ResourceLoadPriority priority = blocking ? ResourceLoadPriorityUnresolved : ResourceLoadPriorityVeryLow;
        CachedResourceRequest request(ResourceRequest(document()->completeURL(url)), charset, priority);
        request.setInitiator(this);
        m_cachedSheet = document()->cachedResourceLoader()->requestCSSStyleSheet(request);
        
        if (m_cachedSheet)
            m_cachedSheet->addClient(this);
        else {
            // The request may have been denied if (for example) the stylesheet is local and the document is remote.
            m_loading = false;
            removePendingSheet();
        }
    } else if (m_sheet) {
        // we no longer contain a stylesheet, e.g. perhaps rel or type was changed
        clearSheet();
        document()->styleResolverChanged(DeferRecalcStyle);
    }
}

void HTMLLinkElement::clearSheet()
{
    ASSERT(m_sheet);
    ASSERT(m_sheet->ownerNode() == this);
    m_sheet->clearOwnerNode();
    m_sheet = 0;
}

Node::InsertionNotificationRequest HTMLLinkElement::insertedInto(ContainerNode* insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    if (!insertionPoint->inDocument())
        return InsertionDone;

    m_isInShadowTree = isInShadowTree();
    if (m_isInShadowTree)
        return InsertionDone;

    document()->styleSheetCollection()->addStyleSheetCandidateNode(this, m_createdByParser);

    process();
    return InsertionDone;
}

void HTMLLinkElement::removedFrom(ContainerNode* insertionPoint)
{
    HTMLElement::removedFrom(insertionPoint);
    if (!insertionPoint->inDocument())
        return;

    m_linkLoader.released();

    if (m_isInShadowTree) {
        ASSERT(!m_sheet);
        return;
    }
    document()->styleSheetCollection()->removeStyleSheetCandidateNode(this);

    if (m_sheet)
        clearSheet();

    if (styleSheetIsLoading())
        removePendingSheet(RemovePendingSheetNotifyLater);

    if (document()->renderer())
        document()->styleResolverChanged(DeferRecalcStyle);
}

void HTMLLinkElement::finishParsingChildren()
{
    m_createdByParser = false;
    HTMLElement::finishParsingChildren();
}

void HTMLLinkElement::setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CachedCSSStyleSheet* cachedStyleSheet)
{
    if (!inDocument()) {
        ASSERT(!m_sheet);
        return;
    }
    // Completing the sheet load may cause scripts to execute.
    RefPtr<Node> protector(this);

    CSSParserContext parserContext(document(), baseURL, charset);

    if (RefPtr<StyleSheetContents> restoredSheet = const_cast<CachedCSSStyleSheet*>(cachedStyleSheet)->restoreParsedStyleSheet(parserContext)) {
        ASSERT(restoredSheet->isCacheable());
        ASSERT(!restoredSheet->isLoading());

        m_sheet = CSSStyleSheet::create(restoredSheet, this);
        m_sheet->setMediaQueries(MediaQuerySet::createAllowingDescriptionSyntax(m_media));
        m_sheet->setTitle(title());

        m_loading = false;
        sheetLoaded();
        notifyLoadedSheetAndAllCriticalSubresources(false);
        return;
    }

    RefPtr<StyleSheetContents> styleSheet = StyleSheetContents::create(href, parserContext);

    m_sheet = CSSStyleSheet::create(styleSheet, this);
    m_sheet->setMediaQueries(MediaQuerySet::createAllowingDescriptionSyntax(m_media));
    m_sheet->setTitle(title());

    styleSheet->parseAuthorStyleSheet(cachedStyleSheet, document()->securityOrigin());

    m_loading = false;
    styleSheet->notifyLoadedSheet(cachedStyleSheet);
    styleSheet->checkLoaded();

    if (styleSheet->isCacheable())
        const_cast<CachedCSSStyleSheet*>(cachedStyleSheet)->saveParsedStyleSheet(styleSheet);
}

bool HTMLLinkElement::styleSheetIsLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return m_sheet->contents()->isLoading();
}

void HTMLLinkElement::linkLoaded()
{
    dispatchEvent(Event::create(eventNames().loadEvent, false, false));
}

void HTMLLinkElement::linkLoadingErrored()
{
    dispatchEvent(Event::create(eventNames().errorEvent, false, false));
}

void HTMLLinkElement::didStartLinkPrerender()
{
    dispatchEvent(Event::create(eventNames().webkitprerenderstartEvent, false, false));
}

void HTMLLinkElement::didStopLinkPrerender()
{
    dispatchEvent(Event::create(eventNames().webkitprerenderstopEvent, false, false));
}

void HTMLLinkElement::didSendLoadForLinkPrerender()
{
    dispatchEvent(Event::create(eventNames().webkitprerenderloadEvent, false, false));
}

void HTMLLinkElement::didSendDOMContentLoadedForLinkPrerender()
{
    dispatchEvent(Event::create(eventNames().webkitprerenderdomcontentloadedEvent, false, false));
}

bool HTMLLinkElement::sheetLoaded()
{
    if (!styleSheetIsLoading()) {
        removePendingSheet();
        return true;
    }
    return false;
}

void HTMLLinkElement::dispatchPendingLoadEvents()
{
    linkLoadEventSender().dispatchPendingEvents();
}

void HTMLLinkElement::dispatchPendingEvent(LinkEventSender* eventSender)
{
    ASSERT_UNUSED(eventSender, eventSender == &linkLoadEventSender());
    if (m_loadedSheet)
        linkLoaded();
    else
        linkLoadingErrored();
}

void HTMLLinkElement::notifyLoadedSheetAndAllCriticalSubresources(bool errorOccurred)
{
    if (m_firedLoad)
        return;
    m_loadedSheet = !errorOccurred;
    linkLoadEventSender().dispatchEventSoon(this);
    m_firedLoad = true;
}

void HTMLLinkElement::startLoadingDynamicSheet()
{
    // We don't support multiple blocking sheets.
    ASSERT(m_pendingSheetType < Blocking);
    addPendingSheet(Blocking);
}

bool HTMLLinkElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name().localName() == hrefAttr || HTMLElement::isURLAttribute(attribute);
}

KURL HTMLLinkElement::href() const
{
    return document()->completeURL(getAttribute(hrefAttr));
}

String HTMLLinkElement::rel() const
{
    return getAttribute(relAttr);
}

String HTMLLinkElement::target() const
{
    return getAttribute(targetAttr);
}

String HTMLLinkElement::type() const
{
    return getAttribute(typeAttr);
}

IconType HTMLLinkElement::iconType() const
{
    return m_relAttribute.iconType();
}

String HTMLLinkElement::iconSizes() const
{
    return m_sizes->toString();
}

void HTMLLinkElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    // Favicons are handled by a special case in LegacyWebArchive::create()
    if (m_relAttribute.iconType() != InvalidIcon)
        return;

    if (!m_relAttribute.isStyleSheet())
        return;
    
    // Append the URL of this link element.
    addSubresourceURL(urls, href());
    
    // Walk the URLs linked by the linked-to stylesheet.
    if (CSSStyleSheet* styleSheet = const_cast<HTMLLinkElement*>(this)->sheet())
        styleSheet->contents()->addSubresourceStyleURLs(urls);
}

void HTMLLinkElement::addPendingSheet(PendingSheetType type)
{
    if (type <= m_pendingSheetType)
        return;
    m_pendingSheetType = type;

    if (m_pendingSheetType == NonBlocking)
        return;
    document()->styleSheetCollection()->addPendingSheet();
}

void HTMLLinkElement::removePendingSheet(RemovePendingSheetNotificationType notification)
{
    PendingSheetType type = m_pendingSheetType;
    m_pendingSheetType = None;

    if (type == None)
        return;
    if (type == NonBlocking) {
        // Document::removePendingSheet() triggers the style selector recalc for blocking sheets.
        document()->styleResolverChanged(RecalcStyleImmediately);
        return;
    }

    document()->styleSheetCollection()->removePendingSheet(
        notification == RemovePendingSheetNotifyImmediately
        ? DocumentStyleSheetCollection::RemovePendingSheetNotifyImmediately
        : DocumentStyleSheetCollection::RemovePendingSheetNotifyLater);
}

DOMSettableTokenList* HTMLLinkElement::sizes() const
{
    return m_sizes.get();
}

void HTMLLinkElement::setSizes(const String& value)
{
    m_sizes->setValue(value);
}

} // namespace WebCore
