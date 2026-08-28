#include "theme.h"

#include <QApplication>
#include <QFont>
#include <QPainter>
#include <QPainterPath>

void Theme::apply(QApplication *application)
{
    if (!application) {
        return;
    }

    application->setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    application->setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget#centralwidget { background: #f4f7fb; color: #1f2937; }
        QWidget#mediaCard, QWidget#recognitionPage, QWidget#QRegisterWidget, QWidget#QqueryWidget {
            background: #ffffff; border: 1px solid #dbe3ee; border-radius: 8px;
        }
        QLabel#pageTitle { color: #172b4d; font-size: 20px; font-weight: 600; }
        QLabel#sectionTitle { color: #172b4d; font-size: 16px; font-weight: 600; }
        QLabel#videoLb { background: #172033; color: #d7e5ff; border-radius: 6px; padding: 12px; }
        QLabel#avatar { background: #e9f1ff; color: #5b6b82; border: 1px solid #c9d8ef; border-radius: 64px; }
        QLabel#videoStatusLb, QLabel#attendanceStatusLb {
            background: #f7f9fc; color: #526177; border-radius: 5px; padding: 8px;
        }
        QLabel#attendanceStatusLb[failed="true"] { background: #fff0f0; color: #b42318; }
        QLabel#attendanceStatusLb[failed="false"] { color: #1f6f43; }
        QTabWidget::pane { border: 0; background: transparent; }
        QTabBar::tab { background: #e9eef5; color: #526177; border: 0; border-radius: 5px; padding: 9px 18px; margin-right: 6px; }
        QTabBar::tab:selected { background: #1668dc; color: #ffffff; }
        QPushButton { background: #ffffff; color: #28466f; border: 1px solid #b9cae2; border-radius: 5px; padding: 8px 12px; min-height: 20px; }
        QPushButton:hover { background: #edf5ff; border-color: #1668dc; }
        QPushButton:disabled { color: #9aa7b9; background: #f2f4f7; border-color: #e1e7ef; }
        QPushButton#primaryAction { background: #1668dc; color: #ffffff; border-color: #1668dc; font-weight: 600; }
        QPushButton#primaryAction:hover { background: #0f56ba; }
        QLineEdit, QComboBox, QDateEdit { background: #ffffff; border: 1px solid #c9d4e3; border-radius: 5px; padding: 7px; min-height: 20px; }
        QLineEdit:focus, QComboBox:focus, QDateEdit:focus { border: 1px solid #1668dc; }
        QTableView { background: #ffffff; alternate-background-color: #f7f9fc; border: 1px solid #dbe3ee; border-radius: 5px; gridline-color: #e8edf4; selection-background-color: #dceaff; selection-color: #172b4d; }
        QHeaderView::section { background: #f1f5fa; color: #42526e; border: 0; border-bottom: 1px solid #dbe3ee; padding: 8px; font-weight: 600; }
    )"));
}

QPixmap Theme::circularAvatar(const QPixmap &source, int diameter)
{
    if (source.isNull() || diameter <= 0) {
        return QPixmap();
    }

    const QPixmap scaled = source.scaled(diameter, diameter, Qt::KeepAspectRatioByExpanding,
                                         Qt::SmoothTransformation);
    QPixmap avatar(diameter, diameter);
    avatar.fill(Qt::transparent);
    QPainter painter(&avatar);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clipPath;
    clipPath.addEllipse(0, 0, diameter, diameter);
    painter.setClipPath(clipPath);
    painter.drawPixmap((diameter - scaled.width()) / 2, (diameter - scaled.height()) / 2, scaled);
    return avatar;
}
