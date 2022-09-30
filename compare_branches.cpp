#include <QObject>
#include <QFile>
#include <QCoreApplication>
#include <QTextStream>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <rpm/rpmver.h>

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

struct JsonArrayRange {
	QJsonArray::iterator begin;
	QJsonArray::iterator end;
};

JsonArrayRange selectArch(QJsonArray &array, const QString &arch)
{
	JsonArrayRange result { array.end(), array.end() };
	bool capture = false;
	for(auto i = array.begin(); i != array.end(); ++i) {
		QString test_arch = i->toObject()["arch"].toString();
		if(!capture && test_arch == arch) {
			result.begin = i;
			capture = true;
		} else if(capture && test_arch != arch) {
			result.end = i;
			break;
		}
	}
	return result;
}

//using a vector to keep the sequence
QVector<QString> getArches(const QJsonArray &array)
{
	QVector<QString> result;
	for(auto i : array) {
		QString arch = i.toObject()["arch"].toString();
		if (!result.contains(arch)) {
			result.push_back(std::move(arch));
		}
	}
	return result;
}

// all packages that are in r1 but not in r2
QJsonArray subtraction(JsonArrayRange r1, JsonArrayRange r2)
{
	QJsonArray result;
	for(auto i = r1.begin, j = r2.begin; i != r1.end; ++i) {
		QString name = i->toObject()["name"].toString();
		while (j != r2.end) {
			QString test_name = j->toObject()["name"].toString();
			if (test_name < name) {
				++j;
				continue;
			}
			if (test_name > name) {
				result.push_back(*i);
			}
			break;
		}
		if (j == r2.end) {
			result.push_back(*i);
			continue;
		}
	}
	return result;
}

bool versionStringGreater(QString v1, QString v2)
{
	int cmp_result = rpmvercmp(v1.toLocal8Bit().data(), v2.toLocal8Bit().data());
	return cmp_result > 0;
}

// all packages whose version is greater in the r1 than in the r2
QJsonArray versionGreater(JsonArrayRange r1, JsonArrayRange r2)
{
	QJsonArray result;
	for(auto i = r1.begin; i != r1.end; ++i) {
		QString name = i->toObject()["name"].toString();
		for(auto j = r2.begin; j != r2.end; ++j) {
			QString test_name = j->toObject()["name"].toString();
			if (test_name < name) {
				continue;
			} else if (test_name > name) {
				r2.begin = j;
				break;
			} else {
				// identical names
				QString v1 = i->toObject()["version"].toString();
				QString v2 = j->toObject()["version"].toString();
				if(versionStringGreater(v1, v2)) {
					qDebug() << name << ":" << v1 << " is greater than " << v2;
					result.push_back(*i);
				}
				r2.begin = j;
				break;
			}
		}
	}
	return result;
}

// true if sorted by name
bool checkSortingByName(JsonArrayRange r)
{
	if (r.begin == r.end) return true; // ?
	for(auto i = r.begin, j = r.begin + 1; j != r.end; ++i, ++j) {
		QString i_name = i->toObject()["name"].toString();
		QString j_name = j->toObject()["name"].toString();
		if (j_name <= i_name) {
			qDebug() << i_name << " -- " << j_name;

			return false;
		}
	}
	return true;
}

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);
	QTextStream out{stdout};

	//QFile f1{"p10.json"};
	//QFile f2{"sisyphus.json"};

	QFile f1{"test1.json"};
	QFile f2{"test2.json"};

	if (!f1.open(QIODevice::ReadOnly)) {
		qWarning("Cannot open file for reading");
		return 1;
	}

	if (!f2.open(QIODevice::ReadOnly)) {
		qWarning("Cannot open file for reading");
		return 1;
	}

	QJsonObject j1 = parseJson(QTextStream(&f1).readAll().toLocal8Bit());
	QJsonObject j2 = parseJson(QTextStream(&f2).readAll().toLocal8Bit());

	f1.close();
	f2.close();

	QJsonArray ja1 = j1["packages"].toArray();
	QJsonArray ja2 = j2["packages"].toArray();

	auto arches1 = getArches(ja1);
	auto arches2 = getArches(ja2);

	// leave only common architectures
	arches1.erase(std::remove_if(arches1.begin(), arches1.end(), [&arches2](QString &arch1) { return !arches2.contains(arch1); }), arches1.end());

	QJsonObject result;
	QJsonArray query;
	for(auto arch : arches1) {
		qDebug() << "Doing arch: " << arch;
		QJsonObject jo;
		jo["arch"] = arch;
		JsonArrayRange r1 = selectArch(ja1, arch);
		JsonArrayRange r2 = selectArch(ja2, arch);
		//qDebug("check sort");
		if(!checkSortingByName(r1) || !checkSortingByName(r2)) qFatal("packages is supposed to be sorted by name");
		//qDebug("sub 1-2");
		jo["only1"] = subtraction(r1, r2);
		//qDebug("sub 2-1");
		jo["only2"] = subtraction(r2, r1);
		//qDebug("greater");
		jo["greater1"] = versionGreater(r1, r2);
		query.push_back(std::move(jo));
	}
	result["query"] = query;

	out << QJsonDocument(result).toJson();

	if(true) return 0;

	auto arr1 = j1["packages"].toArray();

	QString prevarch;
	QSet<QString> prevarches;

	for(auto i1 : arr1) {
		QString arch = i1.toObject()["arch"].toString();
		if(!prevarches.contains(arch)) {
			qDebug() << "New arch: " << arch;
			prevarches.insert(arch);
		} else if (prevarch != arch) {
			qDebug() << "not new arch: " << arch;
		}
		prevarch = arch;
	}

	if(true) return 0;

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

	return 0;
}
