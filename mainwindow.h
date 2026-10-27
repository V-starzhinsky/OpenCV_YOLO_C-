#pragma once

#include <QMainWindow>
#include <QThread>

class QLineEdit;
class QPushButton;
class QProgressBar;
class QPlainTextEdit;
class BatchWorker;

class MainWindow : public QMainWindow {
    Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow() override;

    signals:
        //Команда воркеру. Два параметра - два аргумента придут в слот
        void startRequested(const QString &inputDir, const QString &outputDir);

        private slots:
        //кнопки Обзор
        void onBrowseInput();
        void onBrowseOutput();

        //кнопка управления
        void onStartClicked();
        void onStopClicked();

        //прием данных от воркера
        void onProgress(int curent, int total);
        void onLog(const QString &message);
        void onFinished();

    private:
        //Двигатель и его поток
        BatchWorker *m_worker = nullptr;
        QThread *m_thread = nullptr;

        //выбор папок: поле показывает путь
        QLineEdit *m_inputEdit;
        QLineEdit *m_outputEdit;

        //Управление
        QPushButton *m_browseInputBtn;
        QPushButton *m_browseOutputBtn;
        QPushButton *m_startBtn;
        QPushButton *m_stopBtn;

        //Индикаторы
        QProgressBar *m_progress;
        QPlainTextEdit *m_log;

};