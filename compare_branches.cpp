#include <QObject>
#include <QCoreApplication>
#include <QTextStream>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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
	qDebug() << url;
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

bool sendRequest(
		QNetworkRequest request,
		QJsonObject &resp)
{
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

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	QTextStream out{stdout};

	if (argc < 2) {
		qDebug() << "need branch";
		return 0;
	}

	QString branch(argv[1]);

	QSslConfiguration sslConfig;
	sslConfig.setDefaultConfiguration(QSslConfiguration::defaultConfiguration());
	sslConfig.setProtocol(QSsl::TlsV1_2);

	QJsonObject json;

	//sendRequest(createRequest("https://rdb.altlinux.org/api/license", sslConfig), json);
	sendRequest(createRequest("https://rdb.altlinux.org/api/export/branch_binary_packages/" + branch, sslConfig), json);
	//sendRequest(createRequest("https://rdb.altlinux.org/api/export/sitemap_packages/p10", sslConfig), json);

	out << "--- BEGIN JSON ---\n";
	out << QJsonDocument(json).toJson();
	out << "--- END JSON ---\n";
	return 0;
}
