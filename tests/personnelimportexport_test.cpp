#include "../src/storage/personnelimportexport.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVariant>
#include <cstdio>

static int s_passCount = 0;
static int s_failCount = 0;

static void expect(bool cond, const char *label)
{
    if (cond) {
        ++s_passCount;
    } else {
        ++s_failCount;
        std::fprintf(stderr, "FAIL: %s\n", label);
    }
}

static QSqlDatabase openTestDb(const QString &name)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
    db.setDatabaseName(":memory:");
    db.open();
    QSqlQuery q(db);
    q.exec("CREATE TABLE user(number varchar(32) primary key, name text, "
           "partment text, faceid int, facepictrue text, entertime text)");
    return db;
}

static void writeCsvFile(const QString &path, const QString &content)
{
    QFile f(path);
    f.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream s(&f);
    s.setCodec("UTF-8");
    s << content;
}

static int testImportBasic()
{
    std::fprintf(stdout, "Test: basic CSV import\n");
    QTemporaryDir dir;
    const QString csvPath = dir.filePath("import.csv");
    writeCsvFile(csvPath,
        "工号,姓名,部门\n"
        "E001,张三,研发部\n"
        "E002,李四,市场部\n");

    QSqlDatabase db = openTestDb("import-basic");
    PersonnelImportResult result = PersonnelImportExport::importCsv(db, csvPath);
    expect(result.imported == 2, "imported 2 rows");
    expect(result.skipped == 0, "skipped 0");
    expect(result.failed == 0, "failed 0");

    QSqlQuery q(db);
    q.exec("SELECT number, name, partment FROM user ORDER BY number");
    expect(q.next() && q.value(0).toString() == "E001", "E001 exists");
    expect(q.next() && q.value(0).toString() == "E002", "E002 exists");

    db.close();
    return 0;
}

static int testImportSkipsDuplicates()
{
    std::fprintf(stdout, "Test: import skips duplicate employee numbers\n");
    QTemporaryDir dir;
    const QString csvPath = dir.filePath("dup.csv");
    writeCsvFile(csvPath,
        "工号,姓名,部门\n"
        "E001,张三,研发部\n"
        "E002,李四,市场部\n");

    QSqlDatabase db = openTestDb("import-dup");
    PersonnelImportExport::importCsv(db, csvPath);

    const QString csvPath2 = dir.filePath("dup2.csv");
    writeCsvFile(csvPath2,
        "工号,姓名,部门\n"
        "E001,张三改,研发二部\n"
        "E003,王五,财务部\n");

    PersonnelImportResult result2 = PersonnelImportExport::importCsv(db, csvPath2);
    expect(result2.imported == 1, "imported 1 new row");
    expect(result2.skipped == 1, "skipped 1 duplicate");

    QSqlQuery q(db);
    q.exec("SELECT COUNT(*) FROM user");
    q.next();
    expect(q.value(0).toInt() == 3, "total 3 users in database");

    db.close();
    return 0;
}

static int testImportEmptyNameFails()
{
    std::fprintf(stdout, "Test: import with empty name fails that row\n");
    QTemporaryDir dir;
    const QString csvPath = dir.filePath("empty.csv");
    writeCsvFile(csvPath,
        "工号,姓名,部门\n"
        "E001,,研发部\n"
        "E002,李四,市场部\n");

    QSqlDatabase db = openTestDb("import-empty");
    PersonnelImportResult result = PersonnelImportExport::importCsv(db, csvPath);
    expect(result.imported == 1, "imported 1 valid row");
    expect(result.failed == 1, "failed 1 empty-name row");

    db.close();
    return 0;
}

static int testExportBasic()
{
    std::fprintf(stdout, "Test: basic CSV export\n");
    QTemporaryDir dir;
    const QString exportPath = dir.filePath("export.csv");

    QSqlDatabase db = openTestDb("export-basic");
    QSqlQuery q(db);
    q.exec("INSERT INTO user(number, name, partment, faceid, facepictrue, entertime) "
           "VALUES('E001', '张三', '研发部', 0, '', '2026-09-12')");
    q.exec("INSERT INTO user(number, name, partment, faceid, facepictrue, entertime) "
           "VALUES('E002', '李四', '市场部', 0, '', '2026-09-12')");

    QString error;
    bool ok = PersonnelImportExport::exportCsv(db, exportPath, &error);
    expect(ok, "export succeeds");
    expect(error.isEmpty(), "no error");

    QFile f(exportPath);
    f.open(QIODevice::ReadOnly | QIODevice::Text);
    QTextStream s(&f);
    s.setCodec("UTF-8");
    const QString content = s.readAll();
    expect(content.contains("E001"), "contains E001");
    expect(content.contains("张三"), "contains name");
    expect(content.contains("工号"), "contains header");
    expect(content.contains("\r\n") || content.contains("\n"), "has line endings");
    const int lineCount = content.split("\n", QString::SkipEmptyParts).size();
    expect(lineCount >= 3, "header + 2 data rows");

    db.close();
    return 0;
}

static int testRoundTrip()
{
    std::fprintf(stdout, "Test: import then export round-trip\n");
    QTemporaryDir dir;
    const QString importPath = dir.filePath("roundtrip_in.csv");
    writeCsvFile(importPath,
        "工号,姓名,部门\n"
        "E001,张三,研发部\n"
        "E002,李四,市场部\n");

    QSqlDatabase db = openTestDb("roundtrip");
    PersonnelImportExport::importCsv(db, importPath);

    const QString exportPath = dir.filePath("roundtrip_out.csv");
    PersonnelImportExport::exportCsv(db, exportPath);

    QFile f(exportPath);
    f.open(QIODevice::ReadOnly | QIODevice::Text);
    QTextStream s(&f);
    s.setCodec("UTF-8");
    const QString content = s.readAll();
    expect(content.contains("E001"), "round-trip contains E001");
    expect(content.contains("E002"), "round-trip contains E002");
    expect(content.contains("研发部"), "round-trip contains department");

    db.close();
    return 0;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testImportBasic();
    testImportSkipsDuplicates();
    testImportEmptyNameFails();
    testExportBasic();
    testRoundTrip();

    std::fprintf(stdout, "\nPersonnelImportExportTest: %d passed, %d failed\n",
                 s_passCount, s_failCount);
    return s_failCount > 0 ? 1 : 0;
}
