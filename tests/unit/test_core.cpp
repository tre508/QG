// Core domain tests (Prompt 05): value types, errors/results, identity, events.
//
// Identity tests use temporary files only; fingerprint fallback is exercised
// directly (no reliance on filesystems that report ino == 0).

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <sys/stat.h>

#include <thread>

#include "core/errors.hpp"
#include "core/events.hpp"
#include "core/identity.hpp"
#include "core/types.hpp"

using namespace qg;

class CoreTest : public QObject
{
    Q_OBJECT

private slots:
    // --- types ---------------------------------------------------------------
    void entryDiscriminatesKind();
    void sortSpecEquality();
    void coreTypesAreRegistered();

    // --- errors --------------------------------------------------------------
    void errorCarriesCodeAndMessage();
    void resultCarriesValueOrError();
    void resultVoidCarriesSuccessOrError();
    void domainExceptionsCarryCode();

    // --- identity ------------------------------------------------------------
    void inodeEncodingIsDeterministic();
    void identityUsesInodeForLocalFile();
    void identitySurvivesRename();
    void fingerprintIsContentSensitive();
    void identityReportsMissingFile();

    // --- events --------------------------------------------------------------
    void eventBusDeliversOnOwnerThread();
    void eventBusDeliversFromWorkerThread();
};

void CoreTest::entryDiscriminatesKind()
{
    Entry entry;
    QCOMPARE(entry.kind, EntryKind::Video);
    QVERIFY(entry.isVideo());
    QVERIFY(!entry.isFolder());

    entry.kind = EntryKind::Folder;
    QVERIFY(entry.isFolder());
    QVERIFY(!entry.isVideo());
}

void CoreTest::sortSpecEquality()
{
    const SortSpec a{};
    const SortSpec b{};
    QVERIFY(a == b);

    SortSpec c;
    c.key = SortKey::Modified;
    QVERIFY(!(a == c));
}

void CoreTest::coreTypesAreRegistered()
{
    registerCoreTypes();
    QVERIFY(QMetaType::fromType<Entry>().id() != QMetaType::UnknownType);
    QVERIFY(QMetaType::fromType<FolderSnapshot>().id() != QMetaType::UnknownType);
    QVERIFY(QMetaType::fromType<SortSpec>().id() != QMetaType::UnknownType);
}

void CoreTest::errorCarriesCodeAndMessage()
{
    const Error error = Error::make(ErrorCode::NameCollision,
                                    QStringLiteral("A file with that name exists"),
                                    QStringLiteral("/tmp/x"));
    QVERIFY(error.isError());
    QCOMPARE(error.code, ErrorCode::NameCollision);
    QCOMPARE(error.message, QStringLiteral("A file with that name exists"));
    QCOMPARE(error.path, QStringLiteral("/tmp/x"));
}

void CoreTest::resultCarriesValueOrError()
{
    const Result<int> ok(7);
    QVERIFY(ok.hasValue());
    QCOMPARE(ok.value(), 7);

    const Result<int> failed(Error::make(ErrorCode::IoError, QStringLiteral("boom")));
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, ErrorCode::IoError);
}

void CoreTest::resultVoidCarriesSuccessOrError()
{
    const Result<void> ok = Result<void>::success();
    QVERIFY(ok);
    const Result<void> failed(Error::make(ErrorCode::OperationFailed, QStringLiteral("nope")));
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, ErrorCode::OperationFailed);
}

void CoreTest::domainExceptionsCarryCode()
{
    try {
        throw AccessDenied(QStringLiteral("denied"));
    } catch (const DomainException &ex) {
        QCOMPARE(ex.code(), ErrorCode::AccessDenied);
        QCOMPARE(QString::fromStdString(ex.what()), QStringLiteral("denied"));
    }

    try {
        throw NameCollision(QStringLiteral("collision"));
    } catch (const DomainException &ex) {
        QCOMPARE(ex.code(), ErrorCode::NameCollision);
    }
}

void CoreTest::inodeEncodingIsDeterministic()
{
    const QString a = encodeInodeIdentity(2049, 12345);
    const QString b = encodeInodeIdentity(2049, 12345);
    QCOMPARE(a, b);
    QCOMPARE(a, QStringLiteral("inode:801:3039"));
    QVERIFY(encodeInodeIdentity(2049, 12345) != encodeInodeIdentity(2049, 12346));
}

void CoreTest::identityUsesInodeForLocalFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("clip.mp4"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("hello");
    file.close();

    struct stat st {};
    QVERIFY(::stat(QFile::encodeName(path).constData(), &st) == 0);

    const Result<FileIdentity> identity = identityFromPath(path);
    QVERIFY(identity);
    QCOMPARE(identity.value().kind, IdentityKind::Inode);
    QCOMPARE(identity.value().key,
             encodeInodeIdentity(static_cast<std::uint64_t>(st.st_dev),
                                 static_cast<std::uint64_t>(st.st_ino)));
}

void CoreTest::identitySurvivesRename()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString original = QDir(dir.path()).filePath(QStringLiteral("a.mp4"));
    const QString renamed = QDir(dir.path()).filePath(QStringLiteral("b.mp4"));

    QFile file(original);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("data");
    file.close();

    const Result<FileIdentity> before = identityFromPath(original);
    QVERIFY(before);

    QVERIFY(QFile::rename(original, renamed));

    const Result<FileIdentity> after = identityFromPath(renamed);
    QVERIFY(after);
    QCOMPARE(before.value().key, after.value().key);
}

void CoreTest::fingerprintIsContentSensitive()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("clip.mp4"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("AAAAAAAA");
    file.close();

    const Result<FileIdentity> first = fingerprintIdentity(path, 8, 1000);
    QVERIFY(first);
    QCOMPARE(first.value().kind, IdentityKind::Fingerprint);
    QVERIFY(first.value().key.startsWith(QStringLiteral("fp:")));

    // Same size/mtime but different bytes => different identity.
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("BBBBBBBB");
    file.close();

    const Result<FileIdentity> second = fingerprintIdentity(path, 8, 1000);
    QVERIFY(second);
    QVERIFY(first.value().key != second.value().key);
}

void CoreTest::identityReportsMissingFile()
{
    const Result<FileIdentity> identity =
        identityFromPath(QStringLiteral("/definitely/not/here/qg.mp4"));
    QVERIFY(!identity);
    QCOMPARE(identity.error().code, ErrorCode::NotFound);
}

void CoreTest::eventBusDeliversOnOwnerThread()
{
    EventBus bus;
    QSignalSpy spy(&bus, &EventBus::announced);
    bus.post(AppEvent{AppEventType::Info, QStringLiteral("hello"), QString()});
    QCOMPARE(spy.count(), 1);
    const auto event = spy.takeFirst().at(0).value<AppEvent>();
    QCOMPARE(event.message, QStringLiteral("hello"));
}

void CoreTest::eventBusDeliversFromWorkerThread()
{
    EventBus bus;
    QSignalSpy spy(&bus, &EventBus::announced);

    std::thread worker([&bus]() {
        bus.post(AppEvent{AppEventType::Success, QStringLiteral("from worker"), QString()});
    });
    worker.join();

    // Delivery is queued onto the owner thread; spin the loop until it arrives.
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);
    const auto event = spy.takeFirst().at(0).value<AppEvent>();
    QCOMPARE(event.message, QStringLiteral("from worker"));
}

QTEST_GUILESS_MAIN(CoreTest)
#include "test_core.moc"
