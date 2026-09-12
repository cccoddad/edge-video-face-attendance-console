#ifndef DATABASEMIGRATION_H
#define DATABASEMIGRATION_H

#include <QSqlDatabase>

namespace DatabaseMigration {
    bool migrate(QSqlDatabase database, QString *errorMessage = nullptr);
    int currentVersion(QSqlDatabase database);
}

#endif // DATABASEMIGRATION_H
