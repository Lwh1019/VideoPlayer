#include "mainwid.h"
#include <QApplication>
#include <QFontDatabase>
#include <QDebug>


#undef main

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MainWid w;
    if (w.Init() == false)
    {
        return -1;
    }
    w.show();
    return a.exec();
}
