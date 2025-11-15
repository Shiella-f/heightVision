#include "MainWindow.h"
#include <QWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
{
    init();
    this->setWindowTitle(QStringLiteral("Height Measurement Tool"));
    this->setFixedSize(800, 600);

}

MainWindow::~MainWindow() = default;

void MainWindow::init() 
{
    m_matchWidget = new matchWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_matchWidget);
}