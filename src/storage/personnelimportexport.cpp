#include "personnelimportexport.h"

#include <QDate>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QVariant>

namespace {
QString unquoteCsvField(const QString &field)
{
    QString trimmed = field.trimmed();
    if (trimmed.size() >= 2 && trimmed.startsWith(QLatin1Char('"'))
            && trimmed.endsWith(QLatin1Char('"'))) {
        trimmed = trimmed.mid(1, trimmed.size() - 2);
        trimmed.replace("\"\"", "\"");
    }
    return trimmed;
}

QString csvEscape(const QString &value)
{
    QString escaped = value;
    escaped.replace('"', "\"\"");
    return QString("\"%1\"").arg(escaped);
}

bool isHeaderRow(const QString &number, const QString &name)
{
    const QString n = number.trimmed().toLower();
    const QString m = name.trimmed().toLower();
    return n == "工号" || n == "number" || n == "编号"
            || m == "姓名" || m == "name";
}
}

PersonnelImportResult PersonnelImportExport::importCsv(const QSqlDatabase &database,
                                                       const QString &csvFilePath,
                                                       const QString &delimiter)
{
    PersonnelImportResult result;
    if (!database.isOpen()) {
        result.errorMessage = QStringLiteral("数据库未打开");
        result.failed = -1;
        return result;
    }

    QFile file(csvFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = file.errorString();
        result.failed = -1;
        return result;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    if (!stream.atEnd()) {
        QString firstLine = stream.readLine();
        if (firstLine.startsWith('\xEF')) {
            firstLine = firstLine.mid(3);
        }
        const QStringList header = firstLine.split(delimiter);
        if (header.size() >= 2 && isHeaderRow(header[0], header[1])) {
            // header row consumed, skip
        } else {
            stream.seek(0);
        }
    }

    QSqlQuery query(database);
    query.prepare("INSERT INTO user(number, name, partment, faceid, facepictrue, entertime) "
                  "VALUES(?, ?, ?, 0, '', ?)");
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");

    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QStringList fields = line.split(delimiter);
        if (fields.size() < 2) {
            ++result.failed;
            continue;
        }
        const QString number = unquoteCsvField(fields[0]);
        const QString name = unquoteCsvField(fields[1]);
        const QString department = fields.size() > 2 ? unquoteCsvField(fields[2]) : QString();

        if (number.isEmpty() || name.isEmpty()) {
            ++result.failed;
            continue;
        }

        QSqlQuery checkQuery(database);
        checkQuery.prepare("SELECT 1 FROM user WHERE number = ? LIMIT 1");
        checkQuery.addBindValue(number);
        if (checkQuery.exec() && checkQuery.next()) {
            ++result.skipped;
            continue;
        }

        query.addBindValue(number);
        query.addBindValue(name);
        query.addBindValue(department);
        query.addBindValue(today);
        if (query.exec()) {
            ++result.imported;
        } else {
            ++result.failed;
        }
        query.finish();
    }
    return result;
}

bool PersonnelImportExport::exportCsv(const QSqlDatabase &database,
                                       const QString &csvFilePath,
                                       QString *errorMessage)
{
    if (!database.isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("数据库未打开");
        }
        return false;
    }

    QFile file(csvFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream.setGenerateByteOrderMark(true);

    QStringList header;
    header << csvEscape("工号") << csvEscape("姓名") << csvEscape("部门") << csvEscape("入职日期");
    stream << header.join(',') << "\r\n";

    QSqlQuery query(database);
    if (!query.exec("SELECT number, name, partment, entertime FROM user ORDER BY number")) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    while (query.next()) {
        QStringList row;
        row << csvEscape(query.value(0).toString())
            << csvEscape(query.value(1).toString())
            << csvEscape(query.value(2).toString())
            << csvEscape(query.value(3).toString());
        stream << row.join(',') << "\r\n";
    }
    return true;
}
