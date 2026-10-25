#include "type_to_select_buffer.hpp"
#include <QTimer>
TypeToSelectBuffer::TypeToSelectBuffer(QObject* p) : QObject(p) {
    timeoutTimer_.setSingleShot(true);
    timeoutTimer_.setInterval(700);
    connect(&timeoutTimer_, &QTimer::timeout, this, [this]() { buffer_.clear(); });
}
void TypeToSelectBuffer::addChar(const QString& ch) {
    buffer_ += ch;
    timeoutTimer_.start();
}
void TypeToSelectBuffer::reset() { buffer_.clear(); timeoutTimer_.stop(); }
QString TypeToSelectBuffer::buffer() const { return buffer_; }
