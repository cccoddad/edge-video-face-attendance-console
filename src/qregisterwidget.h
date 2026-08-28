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
    void requestPhotoCapture(const QString &photoPath);
    void requestFaceRegistration(const cv::Mat &faceImage, quint64 requestId);
    void requestFaceDeletion(int faceid);
public slots:
    void handlePhotoCaptureResult(bool success, const QString &message);
    void handleRegistrationResult(int faceid, quint64 requestId, const QString &errorMessage);
private slots:
    void on_GcameraBt_clicked();

    void on_GregisterBt_clicked();

private:
    void invalidateCapturedPhoto();
    Ui::QRegisterWidget *ui;
    QFaceObject *mFaceObject;
    QString m_photoPath;
    quint64 m_registrationRequestId;
    bool m_photoCaptured;
    bool m_registrationPending;
};

#endif // QREGISTERWIDGET_H
