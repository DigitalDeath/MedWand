#include "flat_cap.h"
#include "session_io.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
void require(bool ok, const char *why) {
    if (!ok)
        throw std::runtime_error(why);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    try {
        QTemporaryDir dir;
        require(dir.isValid(), "temporary dir");
        mw::Session original;
        original.id = "test-session";
        original.settings.flatCap = true;
        original.settings.capTolerance = 0.75;
        original.settings.capOpening = 2;
        original.settings.radialCavity = true;
        original.captures.capture(mw::simulate(12, 123), 123);
        original.captures.capture(mw::simulate(13, 124), 124);
        QString error, file = dir.filePath("session.json");
        require(mw::saveSession(file, original, error), "save session");
        mw::Session copy;
        require(mw::loadSession(file, copy, error), "load session");
        require(copy.id == original.id && copy.captures.points.size() == 2, "roundtrip metadata/count");
        require(mw::norm(copy.captures.points[1].position - original.captures.points[1].position) < 1e-12,
            "raw doubles/order preserved");
        require(copy.captures.points[0].timestampUs == 123, "timestamp preserved");
        require(copy.settings.flatCap && copy.settings.capTolerance == 0.75 && copy.settings.capOpening == 2,
            "cap settings survive session roundtrip");
        require(copy.settings.radialCavity, "radial reconstruction choice survives roundtrip");
        QFile current(file);
        require(current.open(QIODevice::ReadOnly), "read new session for legacy fixture");
        auto legacyRoot = QJsonDocument::fromJson(current.readAll()).object();
        current.close();
        auto legacySettings = legacyRoot["settings"].toObject();

        for (const char *key : {"radial_cavity", "flat_cap", "cap_tolerance_mm", "cap_opening"})
            legacySettings.remove(key);
        legacyRoot["settings"] = legacySettings;
        legacyRoot.remove("volume_test_bowl");
        QFile legacyFile(dir.filePath("legacy.json"));
        require(legacyFile.open(QIODevice::WriteOnly), "write legacy session");
        legacyFile.write(QJsonDocument(legacyRoot).toJson());
        legacyFile.close();
        mw::Session legacy;
        require(mw::loadSession(legacyFile.fileName(), legacy, error), "legacy session loads");
        require(!legacy.settings.radialCavity && !legacy.settings.flatCap &&
                legacy.settings.maxEdge == original.settings.maxEdge,
            "legacy sessions retain local preview and stored connection limit");
        QFile bad(file);
        require(bad.open(QIODevice::WriteOnly), "write corrupt file");
        bad.write("{\"version\":99}");
        bad.close();
        require(!mw::loadSession(file, copy, error), "unsupported version rejected");
        require(copy.captures.points.size() == 2, "failed load preserves session");
        original.captures.points[0].position.x = std::numeric_limits<double>::quiet_NaN();
        require(!mw::saveSession(file, original, error), "NaN rejected on save");
        original.captures.points[0] = mw::simulate(12, 123);
        original.geometry.joints[0].axis = {};
        require(!mw::saveSession(file, original, error), "invalid geometry rejected on save");
        original.geometry = mw::Geometry::fictional();
        require(mw::exportCsv(dir.filePath("points.csv"), original, error), "csv export");
        require(mw::exportPly(dir.filePath("points.ply"), original, nullptr, error), "point ply export");
        QFile ply(dir.filePath("points.ply"));
        ply.open(QIODevice::ReadOnly);
        require(ply.readAll().contains("element vertex 2"), "ply points count");
        mw::Session large;
        large.id = "large";
        auto sample = mw::simulate(0, 1234567890123456LL);
        sample.source = std::string(256, 's');
        sample.hasAngles = true;
        sample.counts = {16383, 16383, 16383, 16383, 16383};
        sample.position = {12345.678901234567, 23456.789012345678, 34567.890123456789};
        large.captures.points.assign(100000, sample);
        require(mw::saveSession(file, large, error), "boundary session save");
        require(QFileInfo(file).size() <= 64 * 1024 * 1024,
            "saved session remains within its own load size limit");
        mw::Session roundtrip;
        require(mw::loadSession(file, roundtrip, error), "boundary session can reopen");
        mw::Session bowl;
        bowl.id = "synthetic-bowl";
        bowl.volumeTestBowl = true;
        bowl.settings.flatCap = true;
        auto fixture = mw::makeCapTestBowl();

        for (auto p : fixture.vertices) {
            mw::Snapshot sample;
            sample.position = p;
            sample.source = "SIMULATED VOLUME TEST BOWL";
            bowl.captures.points.push_back(sample);
        }

        require(mw::saveSession(file, bowl, error), "save synthetic bowl provenance");
        require(mw::loadSession(file, roundtrip, error) && roundtrip.volumeTestBowl,
            "restore synthetic bowl model");
        auto closed = mw::closeFlatOpening(fixture, 0, 0.1, [] {
            return false;
        });

        require(closed.volumeValid, "fixture closure for PLY export");
        auto capFile = dir.filePath("capped.ply");
        require(mw::exportPly(capFile, bowl, &closed, error), "export cap faces");
        QFile capped(capFile);
        capped.open(QIODevice::ReadOnly);
        auto contents = capped.readAll();
        require(contents.contains(
                    "element face " + QByteArray::number(closed.faces.size() + closed.capFaces.size())),
            "PLY includes wall and cap face counts");
        bowl.captures.points[0].position.x += 1;
        require(!mw::saveSession(file, bowl, error), "edited data cannot retain known fixture topology");
        std::cout << "Session checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
