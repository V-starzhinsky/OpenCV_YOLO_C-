#include "mainwindow.h"

#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QTime>
#include <QDir>

#include "batchworker.h"


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {

    setWindowTitle("PCB defects detector");
    resize(800, 600);

    //Виджеты
    m_inputEdit = new QLineEdit;
    m_inputEdit->setPlaceholderText("Выберите папку с изображениями...");
    m_outputEdit = new QLineEdit;
    m_outputEdit->setPlaceholderText("Выберите папку для результатов...");

    m_browseInputBtn = new QPushButton("Обзор", this);
    m_browseOutputBtn = new QPushButton("Обзор", this);
    m_startBtn = new QPushButton("Старт", this);
    m_stopBtn = new QPushButton("Стоп", this);
    m_stopBtn->setEnabled(false);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setPlaceholderText("Журнал обработки...");

    //Раскладка
    auto *central = new QWidget(this);
    auto mainLayout = new QVBoxLayout(central);

    auto *inputLay = new QHBoxLayout();
    inputLay->addWidget(m_inputEdit, 1);
    inputLay->addWidget(m_browseInputBtn);

    auto *outputLay = new QHBoxLayout();
    outputLay->addWidget(m_outputEdit, 1);
    outputLay->addWidget(m_browseOutputBtn);

    auto *controlLay = new QHBoxLayout();
    controlLay->addWidget(m_startBtn);
    controlLay->addWidget(m_stopBtn);
    controlLay->addStretch();
    controlLay->addWidget(m_progress, 1);


    mainLayout->addLayout(inputLay);
    mainLayout->addLayout(outputLay);
    mainLayout->addLayout(controlLay);
    mainLayout->addWidget(m_log, 1);

    setCentralWidget(central);

    //подключение кнопок
    connect(m_browseInputBtn, &QPushButton::clicked, this, &MainWindow::onBrowseInput);
    connect(m_browseOutputBtn, &QPushButton::clicked, this, &MainWindow::onBrowseOutput);
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);

    //поток и воркер
    m_thread = new QThread ;
    m_worker = new BatchWorker;
    m_worker->moveToThread(m_thread);

    //Команда: окно -> воркер. Слот выполнится в РАБОЧЕМ потоке
    connect(this, &MainWindow::startRequested, m_worker, &BatchWorker::start);

    //Отчеты: воркер -> окно. Слоты выполнятся в GUI потоке
    connect(m_worker, &BatchWorker::progressChanged,
        this, &MainWindow::onProgress);
    connect(m_worker, &BatchWorker::logMessage,
        this, &MainWindow::onLog);
    connect(m_worker, &BatchWorker::finished,
        this, &MainWindow::onFinished);


    //запускаем поток: в нем крутится цикл событий
    m_thread->start();
}

MainWindow::~MainWindow() {
    //если обработка шла, останавливаем
    if (m_worker) {
        m_worker->stop();
    }

    //Останавливаем цикл событий потока и ждем фактического конца
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
    }

    //Удаляем после остановки( worker ьез parent -  удаляем вручную. thread - Тоже)
    delete m_worker;
    delete m_thread;
}

//слоты: кнопки "Обзор"
void MainWindow::onBrowseInput() {
    const QString dir = QFileDialog::getExistingDirectory(this,
        "Выберите папку с изображениями");
    if (!dir.isEmpty())
        m_inputEdit->setText(dir);
}

void MainWindow::onBrowseOutput() {
    const QString dir = QFileDialog::getExistingDirectory(this,
        "Выберите папку для результатов");
    if (!dir.isEmpty())
        m_outputEdit->setText(dir);
}

//Управление
void MainWindow::onStartClicked() {
    const QString input = m_inputEdit->text().trimmed();
    const QString output = m_outputEdit->text().trimmed();

    if (input.isEmpty() || output.isEmpty()) {
        onLog("Укажите обе папки перед запуском");
        return;
    }

    if (QDir(input).absolutePath() == QDir(output).absolutePath()) {
        onLog("Ошибка: входная и выходная папки совпадают!");
        return;
    }

    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);

    emit startRequested(input, output);
}

void MainWindow::onStopClicked() {
    onLog("СТОП: жду завершения текущего файла.");
    m_worker->stop();
    m_progress->setValue(0);
}

//Прием данных от воркера
void MainWindow::onProgress(int curent, int total) {
    m_progress->setMaximum(total);
    m_progress->setValue(curent);
}

void MainWindow::onLog(const QString &message) {
    m_log->appendPlainText(QTime::currentTime().toString("[hh:mm:ss]") + message);
}

void MainWindow::onFinished() {
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_progress->setValue(0);
    onLog("Обработка завершена");
}