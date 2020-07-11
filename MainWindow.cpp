#include "MainWindow.h"

#include "SaveFile.h"
#include <QFile>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_saveFile = new SaveFile(this);

    QFile file("/home/sandsmark/src/masseffectandromeda-save-editor/Careerfe87459e-0ManualSave");
//    QFile file("/home/sandsmark/src/masseffectandromeda-save-editor/test.sav");
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "failed open";
        return;
    }
    m_saveFile->load(&file);
}

MainWindow::~MainWindow()
{
}

