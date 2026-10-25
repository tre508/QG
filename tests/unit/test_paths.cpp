// Path helpers and filename validation tests (Prompt 08).

#include <QtTest/QtTest>

#include <QDir>
#include <QTemporaryDir>

#include "filesystem/paths.hpp"

using namespace qg;

class PathsTest : public QObject
{
    Q_OBJECT

private slots:
    void uriRoundTrip();
    void expandsLeadingTilde();
    void parentAndBaseName();
    void displayPathShortensHome();
    void validationAcceptsValidNames();
    void validationRejectsEmptyAndInvalidChars();
    void validationRejectsLeadingDashAndReserved();
    void validationDetectsCollisions();
};

void PathsTest::uriRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("clip with space.mp4"));

    const QString uri = toFileUri(path);
    QVERIFY(uri.startsWith(QStringLiteral("file://")));
    QCOMPARE(fromUriOrPath(uri), path);

    // A plain path is returned unchanged.
    QCOMPARE(fromUriOrPath(path), path);
}

void PathsTest::expandsLeadingTilde()
{
    QCOMPARE(expandUser(QStringLiteral("~/Videos"), QStringLiteral("/home/u")),
             QStringLiteral("/home/u/Videos"));
    QCOMPARE(expandUser(QStringLiteral("~"), QStringLiteral("/home/u")),
             QStringLiteral("/home/u"));
    QCOMPARE(expandUser(QStringLiteral("/abs/path"), QStringLiteral("/home/u")),
             QStringLiteral("/abs/path"));
    // ~user is not expanded.
    QCOMPARE(expandUser(QStringLiteral("~bob/x"), QStringLiteral("/home/u")),
             QStringLiteral("~bob/x"));
}

void PathsTest::parentAndBaseName()
{
    QCOMPARE(parentPath(QStringLiteral("/a/b/c.mp4")), QStringLiteral("/a/b"));
    QCOMPARE(parentPath(QStringLiteral("/a")), QStringLiteral("/"));
    QCOMPARE(parentPath(QStringLiteral("rel")), QString());
    QCOMPARE(baseName(QStringLiteral("/a/b/c.mp4")), QStringLiteral("c.mp4"));
}

void PathsTest::displayPathShortensHome()
{
    QCOMPARE(displayPath(QStringLiteral("/home/u/Videos/x.mp4"), QStringLiteral("/home/u")),
             QStringLiteral("~/Videos/x.mp4"));
    QCOMPARE(displayPath(QStringLiteral("/home/u"), QStringLiteral("/home/u")),
             QStringLiteral("~"));
    QCOMPARE(displayPath(QStringLiteral("/mnt/x"), QStringLiteral("/home/u")),
             QStringLiteral("/mnt/x"));
}

void PathsTest::validationAcceptsValidNames()
{
    QVERIFY(validateFilename(QStringLiteral("clip.mp4")).ok());
    QVERIFY(validateFilename(QStringLiteral("my video.mkv")).ok());
    QVERIFY(validateFilename(QStringLiteral("folder name")).ok());
}

void PathsTest::validationRejectsEmptyAndInvalidChars()
{
    QCOMPARE(validateFilename(QString()).code, FilenameErrorCode::Empty);
    QCOMPARE(validateFilename(QStringLiteral("   ")).code, FilenameErrorCode::Empty);
    QCOMPARE(validateFilename(QStringLiteral("a/b")).code, FilenameErrorCode::InvalidCharacter);
    QCOMPARE(validateFilename(QStringLiteral("a\0b")).code, FilenameErrorCode::InvalidCharacter);
}

void PathsTest::validationRejectsLeadingDashAndReserved()
{
    QCOMPARE(validateFilename(QStringLiteral("-x.mp4")).code, FilenameErrorCode::LeadingDash);
    QCOMPARE(validateFilename(QStringLiteral(".")).code, FilenameErrorCode::ReservedName);
    QCOMPARE(validateFilename(QStringLiteral("..")).code, FilenameErrorCode::ReservedName);
}

void PathsTest::validationDetectsCollisions()
{
    const QStringList existing = {QStringLiteral("Clip.MP4"), QStringLiteral("other.mkv")};

    QCOMPARE(validateFilename(QStringLiteral("clip.mp4"), existing).code,
             FilenameErrorCode::Collision);
    // Case-sensitive check does not treat case variants as collisions.
    QVERIFY(validateFilename(QStringLiteral("clip.mp4"), existing, false).ok());
    QVERIFY(validateFilename(QStringLiteral("new.mp4"), existing).ok());
}

QTEST_GUILESS_MAIN(PathsTest)
#include "test_paths.moc"
