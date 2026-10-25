// Media format registry tests (Prompt 07).

#include <QtTest/QtTest>

#include "media/formats.hpp"

using namespace qg;

class FormatsTest : public QObject
{
    Q_OBJECT

private slots:
    void acceptsEveryListedExtensionCaseInsensitively();
    void rejectsUnknownAndExtensionless();
};

void FormatsTest::acceptsEveryListedExtensionCaseInsensitively()
{
    for (const QString &ext : supportedVideoExtensions()) {
        QVERIFY2(isSupportedVideoName(QStringLiteral("clip.") + ext),
                 qPrintable(ext));
        QVERIFY2(isSupportedVideoName(QStringLiteral("clip.") + ext.toUpper()), qPrintable(ext));
        QVERIFY2(isSupportedVideoName(QStringLiteral("/some/dir/clip.") + ext), qPrintable(ext));
    }
}

void FormatsTest::rejectsUnknownAndExtensionless()
{
    QVERIFY(!isSupportedVideoName(QStringLiteral("notes.txt")));
    QVERIFY(!isSupportedVideoName(QStringLiteral("photo.jpg")));
    QVERIFY(!isSupportedVideoName(QStringLiteral("archive.tar.gz")));
    QVERIFY(!isSupportedVideoName(QStringLiteral("README")));
    QVERIFY(!isSupportedVideoName(QStringLiteral("clip.")));
    QVERIFY(!isSupportedVideoName(QString()));
}

QTEST_GUILESS_MAIN(FormatsTest)
#include "test_formats.moc"
