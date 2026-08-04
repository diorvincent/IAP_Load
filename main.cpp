#if defined(_WIN32)
#pragma execution_character_set("utf-8");
#endif

#include "widget.h"
#include <QApplication>
#include <QTextCodec>
#include <iostream>


int main(int argc, char *argv[])
{
//    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);
    qDebug() << "程序启动：初始化QApplication成功";

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("utf-8"));
    //SetConsoleOutputCP(65001);
    //SetConsoleCP(65001);

    Widget w;
    qDebug() << "Widget构造完成：准备显示UI";
    w.show();  // 显示UI
    qDebug() << "UI显示完成：进入事件循环";

    return a.exec();
}
