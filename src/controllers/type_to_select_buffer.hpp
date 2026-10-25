#pragma once
#include <QObject>
#include <QString>
#include <QTimer>

class TypeToSelectBuffer : public QObject {
    Q_OBJECT
public:
    explicit TypeToSelectBuffer(QObject* parent = nullptr);
    Q_INVOKABLE void addChar(const QString& ch);
    Q_INVOKABLE void reset();
    Q_INVOKABLE QString buffer() const;
private:
    QString buffer_;
    QTimer timeoutTimer_;
};
