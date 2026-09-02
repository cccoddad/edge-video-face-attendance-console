#ifndef RTSPSCONFIGURATIONDIALOG_H
#define RTSPSCONFIGURATIONDIALOG_H

#include "rtspconfiguration.h"

#include <QDialog>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QSpinBox;

class RtspConfigurationDialog : public QDialog
{
public:
    explicit RtspConfigurationDialog(const RtspConfiguration &configuration,
                                     QWidget *parent = nullptr);

    RtspConfiguration configuration() const;

private:
    void validateConfiguration();
    void saveConfiguration();

    RtspConfiguration mConfiguration;
    QLineEdit *mUrlEdit;
    QSpinBox *mReconnectSpin;
    QLabel *mValidationLabel;
    QDialogButtonBox *mButtons;
};

#endif // RTSPSCONFIGURATIONDIALOG_H
