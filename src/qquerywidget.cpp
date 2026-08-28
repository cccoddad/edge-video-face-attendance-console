#include "qquerywidget.h"
#include "appconfig.h"
#include "attendancereport.h"
#include "ui_qquerywidget.h"

#include <QDate>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>


QqueryWidget::QqueryWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QqueryWidget),
    employeeModel(new QSqlTableModel(this)),
    attendanceModel(new QSqlQueryModel(this))
{
    ui->setupUi(this);
    //往下拉列表中添加数据
    ui->QtableCbb->addItem("员工数据");
    ui->QtableCbb->addItem("考勤数据");
    ui->QeventCbb->addItem("全部事件", "");
    ui->QeventCbb->addItem("签到", "签到");
    ui->QeventCbb->addItem("签退", "签退");
    ui->QstartDateDe->setDate(QDate::currentDate());
    ui->QendDateDe->setDate(QDate::currentDate());

    employeeModel->setTable("user");
    updateTableMode();
}

QqueryWidget::~QqueryWidget()
{
    delete ui;
}

void QqueryWidget::on_QqueryBt_clicked()
{
    if (ui->QtableCbb->currentIndex() == 0) {
        employeeModel->select();
    } else {
        refreshAttendanceRecords();
    }
}

void QqueryWidget::on_QexportBt_clicked()
{
    if (ui->QtableCbb->currentIndex() != 1) {
        QMessageBox::information(this, "导出提示", "请先选择考勤数据");
        return;
    }

    const QString defaultPath = QDir(AppConfig::dataDirectory()).filePath(
                QString("attendance-%1.csv").arg(QDate::currentDate().toString("yyyyMMdd")));
    const QString filePath = QFileDialog::getSaveFileName(this, "导出考勤 CSV", defaultPath,
                                                           "CSV 文件 (*.csv)");
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!AttendanceReport::exportCsv(QSqlDatabase::database(), attendanceFilter(), filePath, &errorMessage)) {
        QMessageBox::warning(this, "导出失败", errorMessage);
        return;
    }
    QMessageBox::information(this, "导出完成", QString("考勤数据已导出到：%1").arg(filePath));
}

void QqueryWidget::on_QtableCbb_currentIndexChanged(int)
{
    updateTableMode();
}

AttendanceReportFilter QqueryWidget::attendanceFilter() const
{
    AttendanceReportFilter filter;
    filter.number = ui->QnumberLe->text();
    filter.startDate = ui->QstartDateDe->date();
    filter.endDate = ui->QendDateDe->date();
    filter.eventType = ui->QeventCbb->currentData().toString();
    return filter;
}

void QqueryWidget::refreshAttendanceRecords()
{
    const AttendanceReportFilter filter = attendanceFilter();
    if (filter.startDate > filter.endDate) {
        QMessageBox::warning(this, "查询提示", "开始日期不能晚于结束日期");
        return;
    }

    QSqlQuery query;
    QString errorMessage;
    if (!AttendanceReport::query(QSqlDatabase::database(), filter, &query, &errorMessage)) {
        QMessageBox::warning(this, "查询失败", errorMessage);
        return;
    }
    attendanceModel->setQuery(query);
    attendanceModel->setHeaderData(0, Qt::Horizontal, "工号");
    attendanceModel->setHeaderData(1, Qt::Horizontal, "时间");
    attendanceModel->setHeaderData(2, Qt::Horizontal, "事件");
    attendanceModel->setHeaderData(3, Qt::Horizontal, "相似度");
    attendanceModel->setHeaderData(4, Qt::Horizontal, "来源");
}

void QqueryWidget::updateTableMode()
{
    const bool attendanceMode = ui->QtableCbb->currentIndex() == 1;
    ui->QnumberLe->setEnabled(attendanceMode);
    ui->QstartDateDe->setEnabled(attendanceMode);
    ui->QendDateDe->setEnabled(attendanceMode);
    ui->QeventCbb->setEnabled(attendanceMode);
    ui->QexportBt->setEnabled(attendanceMode);

    if (attendanceMode) {
        ui->QdataView->setModel(attendanceModel);
        refreshAttendanceRecords();
    } else {
        ui->QdataView->setModel(employeeModel);
        employeeModel->select();
    }
}
