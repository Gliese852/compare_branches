#include <QDebug>
#include <QVector>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslConfiguration>
#if __has_include(<rpm/rpmver.h>)
	#include <rpm/rpmver.h>
#else
	#include <rpm/rpmvercmp.h>
#endif

#include "getrest.h"

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
			qInfo() << i_name << " -- " << j_name;
			return false;
		}
	}
	return true;
}

int main(int argc, char *argv[]) {

	QTextStream out{stdout};
	QString base_url = "https://rdb.altlinux.org/api/export/branch_binary_packages/";
	

	if (argc != 3) {
		qInfo() << "Usage: compare_branches <branch1> <branch2>";
		return 0;
	}

	QString branch1(argv[1]);
	QString branch2(argv[2]);

	QSslConfiguration sslConfig;
	sslConfig.setDefaultConfiguration(QSslConfiguration::defaultConfiguration());
	sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
	sslConfig.setProtocol(QSsl::TlsV1_2);

	QJsonObject j1, j2;

	qInfo() << "Get branch" << branch1;
	if(!sendRequest(createRequest(base_url + branch1, sslConfig), j1)) {
		qFatal("Branch %s failed to load", branch1.toLocal8Bit().data());
	}
	qInfo() << "Get branch" << branch2;
	if(!sendRequest(createRequest(base_url + branch2, sslConfig), j2)) {
		qFatal("Branch %s failed to load", branch2.toLocal8Bit().data());
	}

	QJsonArray ja1 = j1["packages"].toArray();
	QJsonArray ja2 = j2["packages"].toArray();

	auto arches1 = getArches(ja1);
	auto arches2 = getArches(ja2);

	// leave only common architectures
	arches1.erase(std::remove_if(arches1.begin(), arches1.end(), [&arches2](QString &arch1) { return !arches2.contains(arch1); }), arches1.end());

	QJsonObject result;
	QJsonArray query;
	for(auto arch : arches1) {
		qInfo() << "Processing architecture" << arch;
		QJsonObject jo;
		jo["arch"] = arch;
		JsonArrayRange r1 = selectArch(ja1, arch);
		JsonArrayRange r2 = selectArch(ja2, arch);
		if(!checkSortingByName(r1) || !checkSortingByName(r2)) qFatal("Packages is supposed to be sorted by name");
		jo["only1"] = subtraction(r1, r2);
		jo["only2"] = subtraction(r2, r1);
		jo["greater1"] = versionGreater(r1, r2);
		query.push_back(std::move(jo));
	}
	result["query"] = query;

	out << QJsonDocument(result).toJson();

	return 0;
}
