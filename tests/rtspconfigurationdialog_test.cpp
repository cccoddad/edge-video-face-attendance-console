#include "../src/rtspconfigurationdialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>

#include <cstdio>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    RtspConfiguration initialConfiguration(QStringLiteral("rtsp://initial.invalid:8554/live"), 3000);
    RtspConfigurationDialog dialog(initialConfiguration);
    QLineEdit *urlEdit = dialog.findChild<QLineEdit *>(QStringLiteral("rtspUrlEdit"));
    QSpinBox *reconnectSpin = dialog.findChild<QSpinBox *>(QStringLiteral("rtspReconnectSpin"));
    QDialogButtonBox *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("rtspDialogButtons"));
    if (!urlEdit || !reconnectSpin || !buttons || !buttons->button(QDialogButtonBox::Save)) {
        std::fprintf(stderr, "RTSP configuration dialog controls were not created\n");
        return 2;
    }

    urlEdit->setText(QStringLiteral("https://example.invalid/live"));
    QApplication::processEvents();
    if (buttons->button(QDialogButtonBox::Save)->isEnabled()) {
        std::fprintf(stderr, "RTSP configuration dialog accepted an invalid URL\n");
        return 3;
    }

    urlEdit->setText(QStringLiteral("rtsp://example.invalid:8554/live"));
    reconnectSpin->setValue(1500);
    QApplication::processEvents();
    if (!buttons->button(QDialogButtonBox::Save)->isEnabled()) {
        std::fprintf(stderr, "RTSP configuration dialog rejected a valid URL\n");
        return 4;
    }
    QTest::mouseClick(buttons->button(QDialogButtonBox::Save), Qt::LeftButton);
    if (dialog.result() != QDialog::Accepted
            || dialog.configuration().url() != QStringLiteral("rtsp://example.invalid:8554/live")
            || dialog.configuration().reconnectIntervalMilliseconds() != 1500) {
        std::fprintf(stderr, "RTSP configuration dialog did not save the expected values\n");
        return 5;
    }

    std::fprintf(stdout, "RTSP configuration dialog test passed: validation and save verified\n");
    return 0;
}
