#pragma once

// Structured logging for QG (DESIGN.md 15.1, 23.1).
//
// A single Qt message handler writes timestamped, category-tagged lines to a
// log file under the XDG state directory and mirrors them to stderr. It is
// thread-safe (Qt message handlers may be invoked from any thread).

#include <QString>

namespace qg::logging {

// Install the message handler and open `logFilePath` for appending. Creates the
// parent directory as needed. Returns false if the log file could not be opened
// (logging then falls back to stderr only).
bool initialize(const QString &logFilePath);

// Restore the previous message handler and close the log file.
void shutdown();

// The active log file path, or an empty string when logging is not initialized.
QString logFilePath();

} // namespace qg::logging
