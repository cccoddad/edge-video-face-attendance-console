#ifndef THEME_H
#define THEME_H

#include <QPixmap>

class QApplication;

class Theme
{
public:
    static void apply(QApplication *application);
    static QPixmap circularAvatar(const QPixmap &source, int diameter);
};

#endif // THEME_H
