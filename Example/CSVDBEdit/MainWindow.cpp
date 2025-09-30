#include "MainWindow.h"
#include "./ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ui = std::make_unique<Ui::MainWindow>();
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
}
