#pragma once

#include <QNetworkRequest>

#if defined(LIBGETREST_LIBRARY)
#  define LIBGETREST_EXPORT Q_DECL_EXPORT
#else
#  define LIBGETREST_EXPORT Q_DECL_IMPORT
#endif

LIBGETREST_EXPORT QNetworkRequest createRequest(const QString &url, const QSslConfiguration &sslConfig);
LIBGETREST_EXPORT bool sendRequest(QNetworkRequest request, QJsonObject &resp);
