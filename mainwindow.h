#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_actionOpen_ROM_triggered();

    void on_actionPlugin_Paths_triggered();

    void on_actionStop_Game_triggered();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
