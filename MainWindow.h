#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class SaveFile;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:

    SaveFile *m_saveFile;
};
#endif // MAINWINDOW_H
