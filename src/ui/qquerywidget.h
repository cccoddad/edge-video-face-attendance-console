#ifndef QQUERYWIDGET_H
#define QQUERYWIDGET_H

#include <QWidget>
#include <QSqlQueryModel>
#include <QSqlTableModel>
#include "attendancereport.h"

namespace Ui {
class QqueryWidget;
}

class QqueryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QqueryWidget(QWidget *parent = nullptr);
    ~QqueryWidget();

private slots:
    void on_QqueryBt_clicked();
    void on_QexportBt_clicked();
    void on_QtableCbb_currentIndexChanged(int index);

private:
    void setupModernLayout();
    AttendanceReportFilter attendanceFilter() const;
    void refreshAttendanceRecords();
    void updateTableMode();
    Ui::QqueryWidget *ui;
    QSqlTableModel *employeeModel;
    QSqlQueryModel *attendanceModel;
};

#endif // QQUERYWIDGET_H
