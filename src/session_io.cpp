#include "session_io.h"
#include "flat_cap.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>
#include <limits>
#include <stdexcept>

namespace mw {
    namespace {
        void ensure(bool condition, const char *why) {
            if (!condition)
                throw std::runtime_error(why);
        }

        QJsonArray json(Vec3 v) {
            return {v.x, v.y, v.z};
        }

        double number(QJsonValue v) {
            ensure(v.isDouble() && std::isfinite(v.toDouble()), "Expected finite number");
            return v.toDouble();
        }

        int integer(QJsonValue v, int lo, int hi) {
            double n = number(v);
            ensure(n == std::floor(n) && n >= lo && n <= hi, "Integer out of range");
            return int(n);
        }

        Vec3 vector(QJsonValue v) {
            ensure(v.isArray() && v.toArray().size() == 3, "Expected XYZ vector");
            auto a = v.toArray();
            Vec3 p{number(a[0]), number(a[1]), number(a[2])};
            ensure(std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) <= 100000,
                "Coordinate outside supported range");
            return p;
        }

        QString string(QJsonValue v) {
            ensure(v.isString() && !v.toString().isEmpty() && v.toString().size() <= 256, "Invalid string");
            return v.toString();
        }

        bool boolean(QJsonValue v) {
            ensure(v.isBool(), "Expected boolean");
            return v.toBool();
        }

        std::int64_t timestamp(QJsonValue v) {
            bool ok = false;
            auto n = v.toString().toLongLong(&ok);
            ensure(v.isString() && ok && n >= 0 && n <= maxTimestampUs, "Invalid timestamp");
            return n;
        }

        QJsonObject encode(const Session &s) {
            ensure(!s.id.isEmpty() && s.id.size() <= 256, "Missing session identity");
            ensure(s.captures.points.size() <= 100000, "Too many captures");

            if (s.volumeTestBowl) {
                auto fixture = makeCapTestBowl();
                ensure(
                    fixture.vertices.size() == s.captures.points.size(), "Incomplete synthetic bowl fixture");

                for (size_t i = 0; i < fixture.vertices.size(); ++i)
                    ensure(s.captures.points[i].source == "SIMULATED VOLUME TEST BOWL" &&
                            norm(s.captures.points[i].position - fixture.vertices[i]) < 1e-9,
                        "Modified synthetic bowl fixture");
            }

            ensure(std::isfinite(s.settings.mergeDistance) && s.settings.mergeDistance >= 0.01 &&
                    s.settings.mergeDistance <= 50 && std::isfinite(s.settings.maxEdge) &&
                    s.settings.maxEdge >= 2 * s.settings.mergeDistance && s.settings.maxEdge <= 100,
                "Invalid reconstruction settings");
            QJsonArray captures, joints;
            ensure(std::isfinite(s.settings.capTolerance) && s.settings.capTolerance >= 0 &&
                    s.settings.capTolerance <= 10 && s.settings.capOpening >= 0 &&
                    s.settings.capOpening < 10000,
                "Invalid cap settings");
            auto validVector = [](Vec3 p) {
                return finite(p) && std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) <= 100000;
            };

            ensure(!s.geometry.name.empty() && QString::fromStdString(s.geometry.name).size() <= 256 &&
                    validVector(s.geometry.base) && validVector(s.geometry.probe),
                "Invalid geometry profile");

            for (const auto &j : s.geometry.joints) {
                ensure(validVector(j.axis) && std::abs(norm(j.axis) - 1) < 1e-6 && validVector(j.offset) &&
                        j.zero >= 0 && j.zero < 16384 && (j.sign == 1 || j.sign == -1),
                    "Invalid joint geometry");
                joints.append(QJsonObject{
                    {"axis", json(j.axis)}, {"offset", json(j.offset)}, {"zero", j.zero}, {"sign", j.sign}});
            }

            for (const auto &p : s.captures.points) {
                CaptureStore validate;
                ensure(validate.capture(p, p.timestampUs), "Invalid saved capture");
                ensure(std::max({std::abs(p.position.x), std::abs(p.position.y), std::abs(p.position.z)}) <=
                        100000,
                    "Coordinate outside supported range");
                QJsonArray counts;

                if (p.hasAngles)

                    for (int c : p.counts)
                        counts.append(c);
                captures.append(QJsonObject{{"xyz", json(p.position)},
                    {"timestamp_us", QString::number(p.timestampUs)},
                    {"interval_us", QString::number(p.intervalUs)}, {"sequence", QString::number(p.sequence)},
                    {"source", QString::fromStdString(p.source)}, {"counts", counts}});
            }

            return {{"version", 1}, {"volume_test_bowl", s.volumeTestBowl}, {"id", s.id}, {"units", "mm"},
                {"frame", "right-handed"},
                {"geometry",
                    QJsonObject{{"name", QString::fromStdString(s.geometry.name)},
                        {"validated", s.geometry.validated}, {"base", json(s.geometry.base)},
                        {"probe", json(s.geometry.probe)}, {"joints", joints}}},
                {"settings",
                    QJsonObject{{"merge_mm", s.settings.mergeDistance}, {"max_edge_mm", s.settings.maxEdge},
                        {"flat_cap", s.settings.flatCap}, {"curved_cap", s.settings.curvedCap},
                        {"cap_tolerance_mm", s.settings.capTolerance}, {"cap_opening", s.settings.capOpening},
                        {"radial_cavity", s.settings.radialCavity}}},
                {"captures", captures}};
        }

        bool write(const QString &path, const QByteArray &data, QString &error) {
            QSaveFile file(path);

            if (!file.open(QIODevice::WriteOnly)) {
                error = file.errorString();
                return false;
            }

            if (file.write(data) != data.size()) {
                error = file.errorString();
                return false;
            }

            if (!file.commit()) {
                error = file.errorString();
                return false;
            }

            error.clear();
            return true;
        }

        QString csvField(const std::string &text) {
            QString s = QString::fromStdString(text);
            s.replace('"', "\"\"");
            return '"' + s + '"';
        }
    } // namespace

    bool saveSession(const QString &path, const Session &s, QString &error) {
        try {
            const auto data = QJsonDocument(encode(s)).toJson(QJsonDocument::Compact);
            ensure(data.size() <= 64 * 1024 * 1024, "Session exceeds 64 MB limit; existing file preserved");
            return write(path, data, error);
        } catch (const std::exception &e) {
            error = e.what();
            return false;
        }
    }

    bool loadSession(const QString &path, Session &destination, QString &error) {
        try {
            QFile file(path);
            ensure(file.open(QIODevice::ReadOnly), "Cannot open session file");
            ensure(file.size() <= 64 * 1024 * 1024, "Session exceeds 64 MB limit");
            QJsonParseError parse;
            auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
            ensure(parse.error == QJsonParseError::NoError && doc.isObject(), "Invalid session JSON");

            auto root = doc.object();
            ensure(integer(root["version"], 1, 1) == 1, "Unsupported version");
            ensure(root["units"] == "mm" && root["frame"] == "right-handed", "Unsupported units/frame");
            Session s;

            if (root.contains("volume_test_bowl"))
                s.volumeTestBowl = boolean(root["volume_test_bowl"]);
            s.id = string(root["id"]);
            auto settings = root["settings"].toObject();
            s.settings = {number(settings["merge_mm"]), number(settings["max_edge_mm"])};

            if (settings.contains("radial_cavity"))
                s.settings.radialCavity = boolean(settings["radial_cavity"]);

            if (settings.contains("curved_cap"))
                s.settings.curvedCap = boolean(settings["curved_cap"]);

            if (settings.contains("flat_cap"))
                s.settings.flatCap = boolean(settings["flat_cap"]);

            if (settings.contains("cap_tolerance_mm"))
                s.settings.capTolerance = number(settings["cap_tolerance_mm"]);

            if (settings.contains("cap_opening"))
                s.settings.capOpening = integer(settings["cap_opening"], 0, 9999);
            auto g = root["geometry"].toObject();
            s.geometry.name = string(g["name"]).toStdString();
            s.geometry.validated = boolean(g["validated"]);
            s.geometry.base = vector(g["base"]);
            s.geometry.probe = vector(g["probe"]);
            ensure(g["joints"].isArray() && g["joints"].toArray().size() == 5, "Expected five joints");
            auto joints = g["joints"].toArray();

            for (int i = 0; i < 5; ++i) {
                auto j = joints[i].toObject();
                auto &out = s.geometry.joints[i];
                out.axis = vector(j["axis"]);
                out.offset = vector(j["offset"]);
                ensure(std::abs(norm(out.axis) - 1) < 1e-6, "Joint axis must be unit length");
                out.zero = integer(j["zero"], 0, 16383);
                out.sign = integer(j["sign"], -1, 1);
                ensure(out.sign != 0, "Zero joint direction");
            }

            ensure(root["captures"].isArray(), "Missing captures");
            auto captures = root["captures"].toArray();
            ensure(captures.size() <= 100000, "Too many captures");

            for (auto value : captures) {
                auto p = value.toObject();
                Snapshot sample;
                sample.position = vector(p["xyz"]);
                sample.timestampUs = timestamp(p["timestamp_us"]);
                sample.intervalUs = timestamp(p["interval_us"]);
                bool ok = false;
                sample.sequence = p["sequence"].toString().toULongLong(&ok);
                ensure(ok, "Invalid sequence");
                sample.source = string(p["source"]).toStdString();
                ensure(p["counts"].isArray(), "Missing counts");
                auto c = p["counts"].toArray();
                ensure(c.isEmpty() || c.size() == 5, "Expected zero or five counts");
                sample.hasAngles = !c.isEmpty();

                if (sample.hasAngles)

                    for (int i = 0; i < 5; ++i)
                        sample.counts[i] = integer(c[i], 0, 16383);
                ensure(s.captures.capture(sample, sample.timestampUs), "Invalid capture");
            }

            // Apply identical validation on the write boundary, then swap atomically at object level.
            encode(s);
            destination = std::move(s);
            error.clear();
            return true;
        } catch (const std::exception &e) {
            error = e.what();
            return false;
        }
    }

    bool exportCsv(const QString &path, const Session &s, QString &error) {
        try {
            encode(s);
            QString text;
            QTextStream out(&text);
            out.setRealNumberPrecision(17);
            out << "session_id,source,sequence,timestamp_us,interval_us,x_mm,y_mm,z_mm";

            for (int i = 1; i <= 5; ++i)
                out << ",q" << i << "_count,q" << i << "_rad";
            out << '\n';

            for (const auto &p : s.captures.points) {
                out << csvField(s.id.toStdString()) << ',' << csvField(p.source) << ',' << p.sequence << ','
                    << p.timestampUs << ',' << p.intervalUs << ',' << p.position.x << ',' << p.position.y
                    << ',' << p.position.z;

                for (int i = 0; i < 5; ++i) {
                    out << ',';

                    if (p.hasAngles)
                        out << p.counts[i];
                    out << ',';

                    if (p.hasAngles)
                        out << s.geometry.joints[i].sign *
                                countRadians(p.counts[i] - s.geometry.joints[i].zero);
                }

                out << '\n';
            }

            out.flush();
            return write(path, text.toUtf8(), error);
        } catch (const std::exception &e) {
            error = e.what();
            return false;
        }
    }

    bool exportPly(const QString &path, const Session &s, const Mesh *mesh, QString &error) {
        try {
            encode(s);
            QString text;
            QTextStream out(&text);
            out.setRealNumberPrecision(17);
            const bool faces = mesh && !mesh->faces.empty();
            const size_t count = faces ? mesh->vertices.size() : s.captures.points.size();
            out << "ply\nformat ascii 1.0\ncomment MedicalWand units mm; estimated geometry\n";

            if (faces)
                out << (s.settings.curvedCap ? "comment added_curved_cap_faces "
                                                : "comment added_flat_cap_faces ")
                    << mesh->capFaces.size() << '\n';

            if (faces && mesh->volumeValid)
                out << "comment estimated_volume_mm3 " << mesh->volumeMm3 << '\n';
            out << "element vertex " << count
                << "\nproperty double x\nproperty double y\nproperty double z\n";
            out << "element face " << (faces ? mesh->faces.size() + mesh->capFaces.size() : 0)
                << "\nproperty list uchar int vertex_indices\nend_header\n";

            if (faces) {
                for (auto p : mesh->vertices)
                    out << p.x << ' ' << p.y << ' ' << p.z << '\n';

                for (auto f : mesh->faces) {
                    for (int v : f)
                        ensure(v >= 0 && size_t(v) < count, "Invalid mesh index");
                    out << "3 " << f[0] << ' ' << f[1] << ' ' << f[2] << '\n';
                }

                for (auto f : mesh->capFaces) {
                    for (int v : f)
                        ensure(v >= 0 && size_t(v) < count, "Invalid cap index");
                    out << "3 " << f[0] << ' ' << f[1] << ' ' << f[2] << '\n';
                }
            } else

                for (const auto &p : s.captures.points)
                    out << p.position.x << ' ' << p.position.y << ' ' << p.position.z << '\n';
            out.flush();
            return write(path, text.toUtf8(), error);
        } catch (const std::exception &e) {
            error = e.what();
            return false;
        }
    }
} // namespace mw
