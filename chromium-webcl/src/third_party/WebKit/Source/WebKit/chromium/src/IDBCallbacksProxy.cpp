/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "IDBCallbacksProxy.h"

#include "IDBDatabaseBackendProxy.h"
#include "IDBDatabaseCallbacksProxy.h"
#include "WebIDBCallbacks.h"
#include "WebIDBCursorImpl.h"
#include "WebIDBDatabaseCallbacks.h"
#include "WebIDBDatabaseError.h"
#include "WebIDBDatabaseException.h"
#include "WebIDBDatabaseImpl.h"
#include "WebIDBKey.h"
#include "WebIDBMetadata.h"
#include "modules/indexeddb/IDBCursorBackendInterface.h"
#include "modules/indexeddb/IDBDatabaseBackendInterface.h"
#include "modules/indexeddb/IDBDatabaseError.h"
#include "modules/indexeddb/IDBMetadata.h"
#include <public/WebData.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<IDBCallbacksProxy> IDBCallbacksProxy::create(PassOwnPtr<WebIDBCallbacks> callbacks)
{
    return adoptRef(new IDBCallbacksProxy(callbacks));
}

IDBCallbacksProxy::IDBCallbacksProxy(PassOwnPtr<WebIDBCallbacks> callbacks)
    : m_callbacks(callbacks)
    , m_didComplete(false)
    , m_didCreateProxy(false)
{
}

IDBCallbacksProxy::~IDBCallbacksProxy()
{
    // This cleans up the request's IPC id.
    if (!m_didComplete)
        m_callbacks->onError(WebIDBDatabaseError(WebIDBDatabaseExceptionAbortError, WebString()));
}

void IDBCallbacksProxy::onError(PassRefPtr<IDBDatabaseError> idbDatabaseError)
{
    m_didComplete = true;
    m_callbacks->onError(WebIDBDatabaseError(idbDatabaseError));
}

void IDBCallbacksProxy::onSuccess(PassRefPtr<IDBCursorBackendInterface> idbCursorBackend, PassRefPtr<IDBKey> key, PassRefPtr<IDBKey> primaryKey, PassRefPtr<SharedBuffer> value)
{
    m_didComplete = true;
    m_callbacks->onSuccess(new WebIDBCursorImpl(idbCursorBackend), key, primaryKey, WebData(value));
}

void IDBCallbacksProxy::onSuccess(PassRefPtr<IDBDatabaseBackendInterface> backend, const IDBDatabaseMetadata& metadata)
{
    ASSERT(m_databaseCallbacks.get());
    m_didComplete = true;
    WebIDBDatabaseImpl* impl = m_didCreateProxy ? 0 : new WebIDBDatabaseImpl(backend, m_databaseCallbacks.release());
    m_callbacks->onSuccess(impl, metadata);
}

void IDBCallbacksProxy::onSuccess(PassRefPtr<IDBKey> idbKey)
{
    m_didComplete = true;
    m_callbacks->onSuccess(WebIDBKey(idbKey));
}

void IDBCallbacksProxy::onSuccess(const Vector<String>& stringList)
{
    m_didComplete = true;
    m_callbacks->onSuccess(stringList);
}

void IDBCallbacksProxy::onSuccess(PassRefPtr<SharedBuffer> value)
{
    m_didComplete = true;
    m_callbacks->onSuccess(WebData(value));
}

void IDBCallbacksProxy::onSuccess(PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey> key, const IDBKeyPath& keyPath)
{
    m_callbacks->onSuccess(WebData(value), key, keyPath);
    m_didComplete = true;
}

void IDBCallbacksProxy::onSuccess(int64_t value)
{
    m_didComplete = true;
    m_callbacks->onSuccess(value);
}

void IDBCallbacksProxy::onSuccess()
{
    m_didComplete = true;
    m_callbacks->onSuccess();
}

void IDBCallbacksProxy::onSuccess(PassRefPtr<IDBKey> key, PassRefPtr<IDBKey> primaryKey, PassRefPtr<SharedBuffer> value)
{
    m_didComplete = true;
    m_callbacks->onSuccess(key, primaryKey, WebData(value));
}

void IDBCallbacksProxy::onSuccessWithPrefetch(const Vector<RefPtr<IDBKey> >& keys, const Vector<RefPtr<IDBKey> >& primaryKeys, const Vector<RefPtr<SharedBuffer> >& values)
{
    m_didComplete = true;
    const size_t n = keys.size();

    WebVector<WebIDBKey> webKeys(n);
    WebVector<WebIDBKey> webPrimaryKeys(n);
    WebVector<WebData> webValues(n);

    for (size_t i = 0; i < n; ++i) {
        webKeys[i] = WebIDBKey(keys[i]);
        webPrimaryKeys[i] = WebIDBKey(primaryKeys[i]);
        webValues[i] = WebData(values[i]);
    }

    m_callbacks->onSuccessWithPrefetch(webKeys, webPrimaryKeys, webValues);
}

void IDBCallbacksProxy::onBlocked(int64_t existingVersion)
{
    m_callbacks->onBlocked(existingVersion);
}

void IDBCallbacksProxy::onUpgradeNeeded(int64_t oldVersion, PassRefPtr<IDBDatabaseBackendInterface> database, const IDBDatabaseMetadata& metadata)
{
    ASSERT(m_databaseCallbacks);
    m_didCreateProxy = true;
    m_callbacks->onUpgradeNeeded(oldVersion, new WebIDBDatabaseImpl(database, m_databaseCallbacks), metadata);
}

void IDBCallbacksProxy::setDatabaseCallbacks(PassRefPtr<IDBDatabaseCallbacksProxy> databaseCallbacks)
{
    ASSERT(!m_databaseCallbacks);
    m_databaseCallbacks = databaseCallbacks;
}

} // namespace WebKit
