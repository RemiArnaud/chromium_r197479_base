/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
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
#include "core/html/HTMLCollection.h"

#include "HTMLNames.h"
#include "core/dom/ClassNodeList.h"
#include "core/dom/NodeList.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/NodeTraversal.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLObjectElement.h"
#include "core/html/HTMLOptionElement.h"

#include <utility>

namespace WebCore {

using namespace HTMLNames;

static bool shouldOnlyIncludeDirectChildren(CollectionType type)
{
    switch (type) {
    case DocAll:
    case DocAnchors:
    case DocApplets:
    case DocEmbeds:
    case DocForms:
    case DocImages:
    case DocLinks:
    case DocScripts:
    case DocumentNamedItems:
    case MapAreas:
    case TableRows:
    case SelectOptions:
    case SelectedOptions:
    case DataListOptions:
    case WindowNamedItems:
    case FormControls:
        return false;
    case NodeChildren:
    case TRCells:
    case TSectionRows:
    case TableTBodies:
        return true;
    case ChildNodeListType:
    case ClassNodeListType:
    case NameNodeListType:
    case TagNodeListType:
    case HTMLTagNodeListType:
    case RadioNodeListType:
    case LabelsNodeListType:
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static NodeListRootType rootTypeFromCollectionType(CollectionType type)
{
    switch (type) {
    case DocImages:
    case DocApplets:
    case DocEmbeds:
    case DocForms:
    case DocLinks:
    case DocAnchors:
    case DocScripts:
    case DocAll:
    case WindowNamedItems:
    case DocumentNamedItems:
    case FormControls:
        return NodeListIsRootedAtDocument;
    case NodeChildren:
    case TableTBodies:
    case TSectionRows:
    case TableRows:
    case TRCells:
    case SelectOptions:
    case SelectedOptions:
    case DataListOptions:
    case MapAreas:
        return NodeListIsRootedAtNode;
    case ChildNodeListType:
    case ClassNodeListType:
    case NameNodeListType:
    case TagNodeListType:
    case HTMLTagNodeListType:
    case RadioNodeListType:
    case LabelsNodeListType:
        break;
    }
    ASSERT_NOT_REACHED();
    return NodeListIsRootedAtNode;
}

static NodeListInvalidationType invalidationTypeExcludingIdAndNameAttributes(CollectionType type)
{
    switch (type) {
    case DocImages:
    case DocEmbeds:
    case DocForms:
    case DocScripts:
    case DocAll:
    case NodeChildren:
    case TableTBodies:
    case TSectionRows:
    case TableRows:
    case TRCells:
    case SelectOptions:
    case MapAreas:
        return DoNotInvalidateOnAttributeChanges;
    case DocApplets:
    case SelectedOptions:
    case DataListOptions:
        // FIXME: We can do better some day.
        return InvalidateOnAnyAttrChange;
    case DocAnchors:
        return InvalidateOnNameAttrChange;
    case DocLinks:
        return InvalidateOnHRefAttrChange;
    case WindowNamedItems:
        return InvalidateOnIdNameAttrChange;
    case DocumentNamedItems:
        return InvalidateOnIdNameAttrChange;
    case FormControls:
        return InvalidateForFormControls;
    case ChildNodeListType:
    case ClassNodeListType:
    case NameNodeListType:
    case TagNodeListType:
    case HTMLTagNodeListType:
    case RadioNodeListType:
    case LabelsNodeListType:
        break;
    }
    ASSERT_NOT_REACHED();
    return DoNotInvalidateOnAttributeChanges;
}

HTMLCollection::HTMLCollection(Node* ownerNode, CollectionType type, ItemAfterOverrideType itemAfterOverrideType)
    : LiveNodeListBase(ownerNode, rootTypeFromCollectionType(type), invalidationTypeExcludingIdAndNameAttributes(type),
        WebCore::shouldOnlyIncludeDirectChildren(type), type, itemAfterOverrideType)
{
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLCollection> HTMLCollection::create(Node* base, CollectionType type)
{
    return adoptRef(new HTMLCollection(base, type, DoesNotOverrideItemAfter));
}

HTMLCollection::~HTMLCollection()
{
    // HTMLNameCollection removes cache by itself.
    if (type() != WindowNamedItems && type() != DocumentNamedItems)
        ownerNode()->nodeLists()->removeCacheWithAtomicName(this, type());
}

template <class NodeListType>
inline bool isMatchingElement(const NodeListType*, Element*);

template <> inline bool isMatchingElement(const HTMLCollection* htmlCollection, Element* element)
{
    CollectionType type = htmlCollection->type();
    if (!element->isHTMLElement() && !(type == DocAll || type == NodeChildren))
        return false;

    switch (type) {
    case DocImages:
        return element->hasLocalName(imgTag);
    case DocScripts:
        return element->hasLocalName(scriptTag);
    case DocForms:
        return element->hasLocalName(formTag);
    case TableTBodies:
        return element->hasLocalName(tbodyTag);
    case TRCells:
        return element->hasLocalName(tdTag) || element->hasLocalName(thTag);
    case TSectionRows:
        return element->hasLocalName(trTag);
    case SelectOptions:
        return element->hasLocalName(optionTag);
    case SelectedOptions:
        return element->hasLocalName(optionTag) && toHTMLOptionElement(element)->selected();
    case DataListOptions:
        if (element->hasLocalName(optionTag)) {
            HTMLOptionElement* option = static_cast<HTMLOptionElement*>(element);
            if (!option->isDisabledFormControl() && !option->value().isEmpty())
                return true;
        }
        return false;
    case MapAreas:
        return element->hasLocalName(areaTag);
    case DocApplets:
        return element->hasLocalName(appletTag) || (element->hasLocalName(objectTag) && static_cast<HTMLObjectElement*>(element)->containsJavaApplet());
    case DocEmbeds:
        return element->hasLocalName(embedTag);
    case DocLinks:
        return (element->hasLocalName(aTag) || element->hasLocalName(areaTag)) && element->fastHasAttribute(hrefAttr);
    case DocAnchors:
        return element->hasLocalName(aTag) && element->fastHasAttribute(nameAttr);
    case DocAll:
    case NodeChildren:
        return true;
    case FormControls:
    case DocumentNamedItems:
    case TableRows:
    case WindowNamedItems:
    case ChildNodeListType:
    case ClassNodeListType:
    case NameNodeListType:
    case TagNodeListType:
    case HTMLTagNodeListType:
    case RadioNodeListType:
    case LabelsNodeListType:
        ASSERT_NOT_REACHED();
    }
    return false;
}

template <> inline bool isMatchingElement(const LiveNodeList* nodeList, Element* element)
{
    return nodeList->nodeMatches(element);
}

template <> inline bool isMatchingElement(const HTMLTagNodeList* nodeList, Element* element)
{
    return nodeList->nodeMatchesInlined(element);
}

template <> inline bool isMatchingElement(const ClassNodeList* nodeList, Element* element)
{
    return nodeList->nodeMatchesInlined(element);
}

static Node* previousNode(Node* base, Node* previous, bool onlyIncludeDirectChildren)
{
    return onlyIncludeDirectChildren ? previous->previousSibling() : NodeTraversal::previous(previous, base);
}

static inline Node* lastDescendent(Node* node)
{
    node = node->lastChild();
    for (Node* current = node; current; current = current->lastChild())
        node = current;
    return node;
}

static Node* lastNode(Node* rootNode, bool onlyIncludeDirectChildren)
{
    return onlyIncludeDirectChildren ? rootNode->lastChild() : lastDescendent(rootNode);
}

ALWAYS_INLINE Node* LiveNodeListBase::iterateForPreviousNode(Node* current) const
{
    bool onlyIncludeDirectChildren = shouldOnlyIncludeDirectChildren();
    CollectionType collectionType = type();
    Node* rootNode = this->rootNode();
    for (; current; current = previousNode(rootNode, current, onlyIncludeDirectChildren)) {
        if (isNodeList(collectionType)) {
            if (current->isElementNode() && isMatchingElement(static_cast<const LiveNodeList*>(this), toElement(current)))
                return toElement(current);
        } else {
            if (current->isElementNode() && isMatchingElement(static_cast<const HTMLCollection*>(this), toElement(current)))
                return toElement(current);
        }
    }
    return 0;
}

ALWAYS_INLINE Node* LiveNodeListBase::itemBefore(Node* previous) const
{
    Node* current;
    if (LIKELY(!!previous)) // Without this LIKELY, length() and item() can be 10% slower.
        current = previousNode(rootNode(), previous, shouldOnlyIncludeDirectChildren());
    else
        current = lastNode(rootNode(), shouldOnlyIncludeDirectChildren());

    if (type() == ChildNodeListType)
        return current;
    return iterateForPreviousNode(current);
}

template <class NodeListType>
inline Element* firstMatchingElement(const NodeListType* nodeList, ContainerNode* root)
{
    Element* element = ElementTraversal::firstWithin(root);
    while (element && !isMatchingElement(nodeList, element))
        element = ElementTraversal::next(element, root);
    return element;
}

template <class NodeListType>
inline Element* nextMatchingElement(const NodeListType* nodeList, Element* current, ContainerNode* root)
{
    do {
        current = ElementTraversal::next(current, root);
    } while (current && !isMatchingElement(nodeList, current));
    return current;
}

template <class NodeListType>
inline Element* traverseMatchingElementsForwardToOffset(const NodeListType* nodeList, unsigned offset, Element* currentElement, unsigned& currentOffset, ContainerNode* root)
{
    ASSERT(currentOffset < offset);
    while ((currentElement = nextMatchingElement(nodeList, currentElement, root))) {
        if (++currentOffset == offset)
            return currentElement;
    }
    return 0;
}

// FIXME: This should be in ChildNodeList
inline Node* LiveNodeListBase::traverseChildNodeListForwardToOffset(unsigned offset, Node* currentNode, unsigned& currentOffset) const
{
    ASSERT(type() == ChildNodeListType);
    ASSERT(currentOffset < offset);
    while ((currentNode = currentNode->nextSibling())) {
        if (++currentOffset == offset)
            return currentNode;
    }
    return 0;
}

// FIXME: This should be in LiveNodeList
inline Element* LiveNodeListBase::traverseLiveNodeListFirstElement(ContainerNode* root) const
{
    ASSERT(isNodeList(type()));
    ASSERT(type() != ChildNodeListType);
    if (type() == HTMLTagNodeListType)
        return firstMatchingElement(static_cast<const HTMLTagNodeList*>(this), root);
    if (type() == ClassNodeListType)
        return firstMatchingElement(static_cast<const ClassNodeList*>(this), root);
    return firstMatchingElement(static_cast<const LiveNodeList*>(this), root);
}

// FIXME: This should be in LiveNodeList
inline Element* LiveNodeListBase::traverseLiveNodeListForwardToOffset(unsigned offset, Element* currentElement, unsigned& currentOffset, ContainerNode* root) const
{
    ASSERT(isNodeList(type()));
    ASSERT(type() != ChildNodeListType);
    if (type() == HTMLTagNodeListType)
        return traverseMatchingElementsForwardToOffset(static_cast<const HTMLTagNodeList*>(this), offset, currentElement, currentOffset, root);
    if (type() == ClassNodeListType)
        return traverseMatchingElementsForwardToOffset(static_cast<const ClassNodeList*>(this), offset, currentElement, currentOffset, root);
    return traverseMatchingElementsForwardToOffset(static_cast<const LiveNodeList*>(this), offset, currentElement, currentOffset, root);
}

bool ALWAYS_INLINE LiveNodeListBase::isLastItemCloserThanLastOrCachedItem(unsigned offset) const
{
    ASSERT(isLengthCacheValid());
    unsigned distanceFromLastItem = cachedLength() - offset;
    if (!isItemCacheValid())
        return distanceFromLastItem < offset;

    return cachedItemOffset() < offset && distanceFromLastItem < offset - cachedItemOffset();
}
    
bool ALWAYS_INLINE LiveNodeListBase::isFirstItemCloserThanCachedItem(unsigned offset) const
{
    ASSERT(isItemCacheValid());
    if (cachedItemOffset() < offset)
        return false;

    unsigned distanceFromCachedItem = cachedItemOffset() - offset;
    return offset < distanceFromCachedItem;
}

ALWAYS_INLINE void LiveNodeListBase::setItemCache(Node* item, unsigned offset, unsigned elementsArrayOffset) const
{
    setItemCache(item, offset);
    if (overridesItemAfter()) {
        ASSERT_WITH_SECURITY_IMPLICATION(item->isElementNode());
        static_cast<const HTMLCollection*>(this)->m_cachedElementsArrayOffset = elementsArrayOffset;
    } else
        ASSERT(!elementsArrayOffset);
}

unsigned LiveNodeListBase::length() const
{
    if (isLengthCacheValid())
        return cachedLength();

    item(UINT_MAX);
    ASSERT(isLengthCacheValid());
    
    return cachedLength();
}

// FIXME: It is silly that these functions are in HTMLCollection.cpp.
Node* LiveNodeListBase::item(unsigned offset) const
{
    if (isItemCacheValid() && cachedItemOffset() == offset)
        return cachedItem();

    if (isLengthCacheValid() && cachedLength() <= offset)
        return 0;

    ContainerNode* root = rootContainerNode();
    if (!root) {
        // FIMXE: In someTextNode.childNodes case the root is Text. We shouldn't even make a LiveNodeList for that.
        setLengthCache(0);
        return 0;
    }

    if (isLengthCacheValid() && !overridesItemAfter() && isLastItemCloserThanLastOrCachedItem(offset)) {
        Node* lastItem = itemBefore(0);
        ASSERT(lastItem);
        setItemCache(lastItem, cachedLength() - 1, 0);
    } else if (!isItemCacheValid() || isFirstItemCloserThanCachedItem(offset) || (overridesItemAfter() && offset < cachedItemOffset())) {
        unsigned offsetInArray = 0;
        Node* firstItem;
        if (type() == ChildNodeListType)
            firstItem = root->firstChild();
        else if (isNodeList(type()))
            firstItem = traverseLiveNodeListFirstElement(root);
        else
            firstItem = static_cast<const HTMLCollection*>(this)->traverseFirstElement(offsetInArray, root);

        if (!firstItem) {
            setLengthCache(0);
            return 0;
        }
        setItemCache(firstItem, 0, offsetInArray);
        ASSERT(!cachedItemOffset());
    }

    if (cachedItemOffset() == offset)
        return cachedItem();

    return itemBeforeOrAfterCachedItem(offset, root);
}

inline Node* LiveNodeListBase::itemBeforeOrAfterCachedItem(unsigned offset, ContainerNode* root) const
{
    unsigned currentOffset = cachedItemOffset();
    Node* currentItem = cachedItem();
    ASSERT(currentItem);
    ASSERT(currentOffset != offset);

    if (offset < cachedItemOffset()) {
        ASSERT(!overridesItemAfter());
        while ((currentItem = itemBefore(currentItem))) {
            ASSERT(currentOffset);
            currentOffset--;
            if (currentOffset == offset) {
                setItemCache(currentItem, currentOffset, 0);
                return currentItem;
            }
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    unsigned offsetInArray = 0;
    if (type() == ChildNodeListType)
        currentItem = traverseChildNodeListForwardToOffset(offset, currentItem, currentOffset);
    else if (isNodeList(type()))
        currentItem = traverseLiveNodeListForwardToOffset(offset, toElement(currentItem), currentOffset, root);
    else
        currentItem = static_cast<const HTMLCollection*>(this)->traverseForwardToOffset(offset, toElement(currentItem), currentOffset, offsetInArray, root);

    if (!currentItem) {
        // Did not find the item. On plus side, we now know the length.
        setLengthCache(currentOffset + 1);
        return 0;
    }
    setItemCache(currentItem, currentOffset, offsetInArray);
    return currentItem;
}

Element* HTMLCollection::virtualItemAfter(unsigned&, Element*) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

static inline bool nameShouldBeVisibleInDocumentAll(HTMLElement* element)
{
    // The document.all collection returns only certain types of elements by name,
    // although it returns any type of element by id.
    return element->hasLocalName(appletTag)
        || element->hasLocalName(embedTag)
        || element->hasLocalName(formTag)
        || element->hasLocalName(imgTag)
        || element->hasLocalName(inputTag)
        || element->hasLocalName(objectTag)
        || element->hasLocalName(selectTag);
}

bool HTMLCollection::checkForNameMatch(Element* element, bool checkName, const AtomicString& name) const
{
    if (!element->isHTMLElement())
        return false;
    
    HTMLElement* e = toHTMLElement(element);
    if (!checkName)
        return e->getIdAttribute() == name;

    if (type() == DocAll && !nameShouldBeVisibleInDocumentAll(e))
        return false;

    return e->getNameAttribute() == name && e->getIdAttribute() != name;
}

inline Element* firstMatchingChildElement(const HTMLCollection* nodeList, ContainerNode* root)
{
    Element* element = ElementTraversal::firstWithin(root);
    while (element && !isMatchingElement(nodeList, element))
        element = ElementTraversal::nextSkippingChildren(element, root);
    return element;
}

inline Element* nextMatchingChildElement(const HTMLCollection* nodeList, Element* current, ContainerNode* root)
{
    do {
        current = ElementTraversal::nextSkippingChildren(current, root);
    } while (current && !isMatchingElement(nodeList, current));
    return current;
}

inline Element* HTMLCollection::traverseFirstElement(unsigned& offsetInArray, ContainerNode* root) const
{
    if (overridesItemAfter())
        return virtualItemAfter(offsetInArray, 0);
    ASSERT(!offsetInArray);
    if (shouldOnlyIncludeDirectChildren())
        return firstMatchingChildElement(static_cast<const HTMLCollection*>(this), root);
    return firstMatchingElement(static_cast<const HTMLCollection*>(this), root);
}

inline Element* HTMLCollection::traverseNextElement(unsigned& offsetInArray, Element* previous, ContainerNode* root) const
{
    if (overridesItemAfter())
        return virtualItemAfter(offsetInArray, previous);
    ASSERT(!offsetInArray);
    if (shouldOnlyIncludeDirectChildren())
        return nextMatchingChildElement(this, previous, root);
    return nextMatchingElement(this, previous, root);
}

inline Element* HTMLCollection::traverseForwardToOffset(unsigned offset, Element* currentElement, unsigned& currentOffset, unsigned& offsetInArray, ContainerNode* root) const
{
    ASSERT(currentOffset < offset);
    if (overridesItemAfter()) {
        offsetInArray = m_cachedElementsArrayOffset;
        while ((currentElement = virtualItemAfter(offsetInArray, currentElement))) {
            if (++currentOffset == offset)
                return currentElement;
        }
        return 0;
    }
    if (shouldOnlyIncludeDirectChildren()) {
        while ((currentElement = nextMatchingChildElement(this, currentElement, root))) {
            if (++currentOffset == offset)
                return currentElement;
        }
        return 0;
    }
    return traverseMatchingElementsForwardToOffset(this, offset, currentElement, currentOffset, root);
}

Node* HTMLCollection::namedItem(const AtomicString& name) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.

    ContainerNode* root = rootContainerNode();
    if (!root)
        return 0;

    unsigned arrayOffset = 0;
    unsigned i = 0;
    for (Element* element = traverseFirstElement(arrayOffset, root); element; element = traverseNextElement(arrayOffset, element, root)) {
        if (checkForNameMatch(element, /* checkName */ false, name)) {
            setItemCache(element, i, arrayOffset);
            return element;
        }
        i++;
    }

    i = 0;
    for (Element* element = traverseFirstElement(arrayOffset, root); element; element = traverseNextElement(arrayOffset, element, root)) {
        if (checkForNameMatch(element, /* checkName */ true, name)) {
            setItemCache(element, i, arrayOffset);
            return element;
        }
        i++;
    }

    return 0;
}

void HTMLCollection::updateNameCache() const
{
    if (hasNameCache())
        return;

    ContainerNode* root = rootContainerNode();
    if (!root)
        return;

    unsigned arrayOffset = 0;
    for (Element* element = traverseFirstElement(arrayOffset, root); element; element = traverseNextElement(arrayOffset, element, root)) {
        if (!element->isHTMLElement())
            continue;
        HTMLElement* htmlElement = toHTMLElement(element);
        const AtomicString& idAttrVal = htmlElement->getIdAttribute();
        const AtomicString& nameAttrVal = htmlElement->getNameAttribute();
        if (!idAttrVal.isEmpty())
            appendIdCache(idAttrVal, htmlElement);
        if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal && (type() != DocAll || nameShouldBeVisibleInDocumentAll(htmlElement)))
            appendNameCache(nameAttrVal, htmlElement);
    }

    setHasNameCache();
}

bool HTMLCollection::hasNamedItem(const AtomicString& name) const
{
    if (name.isEmpty())
        return false;

    updateNameCache();

    if (Vector<Element*>* cache = idCache(name)) {
        if (!cache->isEmpty())
            return true;
    }

    if (Vector<Element*>* cache = nameCache(name)) {
        if (!cache->isEmpty())
            return true;
    }

    return false;
}

void HTMLCollection::namedItems(const AtomicString& name, Vector<RefPtr<Node> >& result) const
{
    ASSERT(result.isEmpty());
    if (name.isEmpty())
        return;

    updateNameCache();

    Vector<Element*>* idResults = idCache(name);
    Vector<Element*>* nameResults = nameCache(name);

    for (unsigned i = 0; idResults && i < idResults->size(); ++i)
        result.append(idResults->at(i));

    for (unsigned i = 0; nameResults && i < nameResults->size(); ++i)
        result.append(nameResults->at(i));
}

PassRefPtr<NodeList> HTMLCollection::tags(const String& name)
{
    return ownerNode()->getElementsByTagName(name);
}

void HTMLCollection::append(NodeCacheMap& map, const AtomicString& key, Element* element)
{
    OwnPtr<Vector<Element*> >& vector = map.add(key.impl(), nullptr).iterator->value;
    if (!vector)
        vector = adoptPtr(new Vector<Element*>);
    vector->append(element);
}

} // namespace WebCore
