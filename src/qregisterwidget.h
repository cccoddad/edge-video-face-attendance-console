#ifndef QREGISTERWIDGET_H
#define QREGISTERWIDGET_H

#include "qfaceobject.h"

#include <QWidget>

namespace Ui {
class QRegisterWidget;
}

class QRegisterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QRegisterWidget(QWidget *parent = nullptr);
    ~QRegisterWidget();
    void setFaceObject(QFaceObject *mfaceObject);
signals:
    void sendName(const QString &name);
private slots:
    void on_GcameraBt_clicked();

    void on_GregisterBt_clicked();

private:
    Ui::QRegisterWidget *ui;
    QFaceObject *mFaceObject;
};

#endif // QREGISTERWIDGET_H
