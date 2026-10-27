#pragma once

#include <QObject>
#include <QString>
#include <atomic>      //std::atomic

class BatchWorker : public QObject {
    Q_OBJECT

public:
    explicit BatchWorker(QObject *parent = nullptr);

    ~BatchWorker() override;
    void stop();

public slots:
    //точка входа
    void start(const QString &inputDir, const QString &outputDir);

    //команда останова. Вызывается из GUI потока , но безопасно
    //внутри - только атомарная запитсь. Цикл увидит флг на след итерации


signals:
    //Доклады воркера (уходят в GUI через очередь)
    void progressChanged(int current, int total); //прогресс бар
    void logMessage(const QString &text); //строка в лог
    void finished(); //завершение либо нажатие "стоп"

private:
    std::atomic<bool> m_stop{false}; //флаг отмены; {false} - инициализация
};
