#include "rtspconfigurationdialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

RtspConfigurationDialog::RtspConfigurationDialog(const RtspConfiguration &configuration,
                                                   QWidget *parent)
    : QDialog(parent)
    , mConfiguration(configuration)
    , mUrlEdit(new QLineEdit(configuration.url(), this))
    , mReconnectSpin(new QSpinBox(this))
    , mValidationLabel(new QLabel(this))
    , mButtons(new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this))
{
    setWindowTitle(QStringLiteral("RTSP 配置（未连接）"));
    auto *layout = new QVBoxLayout(this);
    auto *formLayout = new QFormLayout;
    mUrlEdit->setObjectName(QStringLiteral("rtspUrlEdit"));
    mUrlEdit->setPlaceholderText(QStringLiteral("rtsp://<host>:8554/live"));
    mUrlEdit->setClearButtonEnabled(true);
    mUrlEdit->setToolTip(QStringLiteral("仅校验地址格式，不会建立网络连接"));
    mReconnectSpin->setObjectName(QStringLiteral("rtspReconnectSpin"));
    mReconnectSpin->setRange(500, 60000);
    mReconnectSpin->setSingleStep(500);
    mReconnectSpin->setSuffix(QStringLiteral(" ms"));
    mReconnectSpin->setValue(configuration.reconnectIntervalMilliseconds());
    mValidationLabel->setObjectName(QStringLiteral("rtspValidationLabel"));
    mValidationLabel->setWordWrap(true);
    mButtons->setObjectName(QStringLiteral("rtspDialogButtons"));
    formLayout->addRow(QStringLiteral("RTSP 地址"), mUrlEdit);
    formLayout->addRow(QStringLiteral("重连等待"), mReconnectSpin);
    layout->addLayout(formLayout);
    layout->addWidget(mValidationLabel);
    layout->addWidget(mButtons);

    connect(mUrlEdit, &QLineEdit::textChanged, this, [this]() {
        validateConfiguration();
    });
    connect(mButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(mButtons, &QDialogButtonBox::accepted, this, [this]() {
        saveConfiguration();
    });
    validateConfiguration();
}

RtspConfiguration RtspConfigurationDialog::configuration() const
{
    return mConfiguration;
}

void RtspConfigurationDialog::validateConfiguration()
{
    QString errorMessage;
    RtspConfiguration configuration;
    const bool valid = configuration.setUrl(mUrlEdit->text(), &errorMessage);
    mValidationLabel->setText(valid
                              ? QStringLiteral("地址格式有效；保存仅更新当前程序会话，不会连接或探测网络。")
                              : errorMessage);
    mButtons->button(QDialogButtonBox::Save)->setEnabled(valid);
}

void RtspConfigurationDialog::saveConfiguration()
{
    QString errorMessage;
    RtspConfiguration configuration;
    if (!configuration.setUrl(mUrlEdit->text(), &errorMessage)
            || !configuration.setReconnectIntervalMilliseconds(mReconnectSpin->value(), &errorMessage)) {
        validateConfiguration();
        return;
    }
    mConfiguration = configuration;
    accept();
}
