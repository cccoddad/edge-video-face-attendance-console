#ifndef QQUERYWIDGET_H
#define QQUERYWIDGET_H

#include <QWidget>
#include <QSqlTableModel>
#include <QSqlRecord>

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

private:
    Ui::QqueryWidget *ui;
    QSqlTableModel *model;
};

#endif // QQUERYWIDGET_H
