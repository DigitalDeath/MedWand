#pragma once
#include "core.h"
#include "reconstruction.h"
#include <QString>

namespace mw {
    struct Session {
        bool volumeTestBowl = false;
        QString id;
        Geometry geometry = Geometry::fictional();
        MeshSettings settings = [] {
            MeshSettings value;
            value.radialCavity = true;
            value.maxEdge = 6;
            return value;
        }();
        CaptureStore captures;
    };

    bool saveSession(const QString &, const Session &, QString &error);
    bool loadSession(const QString &, Session &, QString &error);
    bool exportCsv(const QString &, const Session &, QString &error);
    bool exportStl(const QString &, const Session &, const Mesh *, QString &error);
    bool exportPly(const QString &, const Session &, const Mesh *, QString &error);
} // namespace mw
