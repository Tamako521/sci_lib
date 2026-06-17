#include <QApplication>
#include <QMessageBox>
#include <fstream>
#include "mainwindow.h"

// 诊断：写日志到 exe 所在目录，方便定位启动阶段的崩溃
static void startupLog(const std::string& msg) {
    std::ofstream log("startup.log", std::ios::app);
    log << msg << std::endl;
}

int main(int argc, char *argv[])
{
    startupLog("Step 1: QApplication 构造中...");
    QApplication a(argc, argv);
    startupLog("Step 2: QApplication 完成");

    try {
        startupLog("Step 3: MainWindow 构造中...");
        MainWindow w;
        startupLog("Step 4: MainWindow 完成");

        startupLog("Step 5: show()...");
        w.show();
        startupLog("Step 6: show() 完成, 进入事件循环...");

        return a.exec();
    } catch (const std::exception& e) {
        startupLog(std::string("异常: ") + e.what());
        QMessageBox::critical(nullptr, "启动失败", QString("程序崩溃: %1").arg(e.what()));
        return 1;
    }
}