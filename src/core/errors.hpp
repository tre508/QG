#pragma once

// QG domain error and result vocabulary (DESIGN.md §15.4).
//
// Services return typed `Result<T>` values for expected failures (access
// denied, collisions, unsupported media, ...) rather than throwing. The small
// exception hierarchy exists only for genuinely exceptional flows and must
// never cross a Qt signal or the QML boundary — controllers map errors to
// overlays/toasts instead.

#include <QString>

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace qg {

enum class ErrorCode {
    None = 0,
    AccessDenied,
    VolumeUnavailable,
    NameCollision,
    UnsupportedMedia,
    NotFound,
    InvalidArgument,
    IoError,
    OperationFailed,
    Cancelled,
    Unsupported,
};

// A single, actionable error value. `systemError` carries errno when relevant.
struct Error {
    ErrorCode code = ErrorCode::None;
    QString message;
    QString path;
    int systemError = 0;

    bool isError() const noexcept { return code != ErrorCode::None; }
    explicit operator bool() const noexcept { return isError(); }

    static Error ok() { return {}; }

    static Error make(ErrorCode code, QString message, QString path = {}, int systemError = 0)
    {
        return Error{code, std::move(message), std::move(path), systemError};
    }
};

// Result<T>: either a value or an Error. Kept small and value-based so it can
// be returned across service boundaries without exceptions.
template <typename T>
class Result {
public:
    Result(T value) : m_value(std::move(value)) {}
    Result(Error error) : m_error(std::move(error)) {}

    bool hasValue() const noexcept { return m_value.has_value(); }
    explicit operator bool() const noexcept { return hasValue(); }

    T &value() & { return *m_value; }
    const T &value() const & { return *m_value; }
    T &&value() && { return std::move(*m_value); }

    const Error &error() const noexcept { return m_error; }

private:
    std::optional<T> m_value;
    Error m_error;
};

// Result<void>: success or Error.
template <>
class Result<void> {
public:
    Result() = default;
    Result(Error error) : m_error(std::move(error)) {}

    static Result success() { return Result(); }

    bool hasValue() const noexcept { return !m_error.isError(); }
    explicit operator bool() const noexcept { return hasValue(); }

    const Error &error() const noexcept { return m_error; }

private:
    Error m_error;
};

// Exception hierarchy for truly exceptional flows only.
class DomainException : public std::runtime_error {
public:
    DomainException(ErrorCode code, const QString &message)
        : std::runtime_error(message.toStdString()), m_code(code)
    {
    }

    ErrorCode code() const noexcept { return m_code; }

private:
    ErrorCode m_code;
};

class AccessDenied : public DomainException {
public:
    explicit AccessDenied(const QString &message)
        : DomainException(ErrorCode::AccessDenied, message)
    {
    }
};

class VolumeUnavailable : public DomainException {
public:
    explicit VolumeUnavailable(const QString &message)
        : DomainException(ErrorCode::VolumeUnavailable, message)
    {
    }
};

class NameCollision : public DomainException {
public:
    explicit NameCollision(const QString &message)
        : DomainException(ErrorCode::NameCollision, message)
    {
    }
};

class UnsupportedMedia : public DomainException {
public:
    explicit UnsupportedMedia(const QString &message)
        : DomainException(ErrorCode::UnsupportedMedia, message)
    {
    }
};

class OperationFailed : public DomainException {
public:
    explicit OperationFailed(const QString &message)
        : DomainException(ErrorCode::OperationFailed, message)
    {
    }
};

} // namespace qg
