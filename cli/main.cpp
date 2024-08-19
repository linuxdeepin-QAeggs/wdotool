#include <QApplication>
#include "input.h"

int main(int argc,char*argv[])
{
    QApplication a(argc, argv);
    Input input;
    return a.exec();
}

