#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>

class AppConfig
{
public:
    static QString dataDirectory();
    static QString photoDirectory();
    static QString databasePath();
    static QString faceDatabasePath();
    static QString modelDirectory();
    static QString modelPath(const QString &fileName);
    static float recognitionThreshold();
    static int attendanceCooldownSeconds();
    static bool hasRequiredModels(QString *errorMessage = nullptr);
};

#endif // APPCONFIG_H
