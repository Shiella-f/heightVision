#include "MainWindow.h"
#include <QGroupBox>
#include <QToolButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QToolBar>
#include <QComboBox>
#include <QVariant>
#include <QSizePolicy>
#include <opencv2/imgproc.hpp>


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    init();

}

MainWindow::~MainWindow() = default;

void MainWindow::init() 
{
}