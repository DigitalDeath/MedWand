#pragma once
#include "core.h"
#include "reconstruction.h"
#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <memory>

namespace mw {
    void configureGraphics();

    class Viewer : public QOpenGLWidget, protected QOpenGLFunctions {
        Q_OBJECT

        public:
        explicit Viewer(QWidget *parent = nullptr);
        ~Viewer() override;
        void setScene(const std::vector<Snapshot> &points, const Mesh &mesh, Vec3 tip);
        void setLayers(bool points, bool surface);
        void fit();
        void standardView(int index);
        QString diagnostics() const {
            return diagnostic_;
        }

        bool graphicsOk() const {
            return ready_;
        }

        signals:
        void graphicsStatus(QString);

        protected:
        void initializeGL() override;
        void paintGL() override;
        void resizeGL(int, int) override;
        void mousePressEvent(QMouseEvent *) override;
        void mouseMoveEvent(QMouseEvent *) override;
        void wheelEvent(QWheelEvent *) override;

        private:

        struct Vertex {
            float x, y, z, nx, ny, nz;
        };

        void release();
        void draw(
            const std::vector<Vertex> &, unsigned mode, const QVector4D &, bool lit, float pointSize = 3);
        std::vector<Vertex> points_, triangles_, capTriangles_, grid_, tipVertices_;
        Vec3 tip_{}, center_{}, origin_{};
        double yaw_ = 38, pitch_ = 25, distance_ = 150, span_ = 80;
        QPoint lastMouse_;
        bool ready_ = false, showPoints_ = true, showSurface_ = true;
        QString diagnostic_ = "Graphics not initialized";
        std::unique_ptr<QOpenGLShaderProgram> shader_;
        QOpenGLBuffer buffer_{QOpenGLBuffer::VertexBuffer};
        QMatrix4x4 mvp_;
    };
} // namespace mw
