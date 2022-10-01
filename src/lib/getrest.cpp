#include "getrest.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>

QJsonObject parseJson(const QByteArray &text)
{
	QJsonObject jsonObj;
	QJsonDocument jsonDoc;
	QJsonParseError parseError;
	jsonDoc = QJsonDocument::fromJson(text, &parseError);
	if(parseError.error != QJsonParseError::NoError){
		qDebug() << text;
		qWarning() << "Json parse error: " << parseError.errorString();
	} else {
		if(jsonDoc.isObject())
			jsonObj  = jsonDoc.object();
		else if (jsonDoc.isArray())
			jsonObj["non_field_errors"] = jsonDoc.array();
	}
	return jsonObj;
}

QNetworkRequest createRequest(const QString &url, const QSslConfiguration &sslConfig)
{
	QNetworkRequest request;
	request.setUrl(QUrl(url));
	request.setHeader(QNetworkRequest::ServerHeader, "application/json");
	request.setSslConfiguration(sslConfig);
	return request;
}

bool onFinishRequest(QNetworkReply *reply)
{
	auto replyError = reply->error();
	if (replyError == QNetworkReply::NoError ) {
		int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if ((code >=200) && (code < 300)) {
			return true;
		}
	}
	return false;
}

bool sendRequest(QNetworkRequest request, QJsonObject &resp)
{
	int argc = 1;
	QCoreApplication app(argc, 0);

	QNetworkAccessManager manager;
	QNetworkReply *reply;
	reply = manager.get(request);
	QEventLoop loop;
	bool success = true;

	loop.connect(reply, &QNetworkReply::finished, &loop, [reply, &resp, &loop, &success]() {

		 resp = parseJson(reply->readAll());

		 if (!onFinishRequest(reply)) {
		     qDebug() << reply->error();
			 success = false;
		 }
		 reply->close();
		 reply->deleteLater();
		 loop.quit();
	});

	loop.exec();

	return success;

}
