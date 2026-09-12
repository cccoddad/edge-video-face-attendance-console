#ifndef PERSONNELIMPORTEXPORT_H
#define PERSONNELIMPORTEXPORT_H

#include <QSqlDatabase>
#include <QString>

struct PersonnelImportResult
{
    int imported = 0;
    int skipped = 0;
    int failed = 0;
    QString errorMessage;
};

class PersonnelImportExport
{
public:
    static PersonnelImportResult importCsv(const QSqlDatabase &database,
                                           const QString &csvFilePath,
                                           const QString &delimiter = QStringLiteral(","));
    static bool exportCsv(const QSqlDatabase &database,
                          const QString &csvFilePath,
                          QString *errorMessage = nullptr);
};

#endif // PERSONNELIMPORTEXPORT_H
