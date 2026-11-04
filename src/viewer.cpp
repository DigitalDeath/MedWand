#include "viewer.h"
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPainter>
#include <QSurfaceFormat>
#include <QWheelEvent>
#include <algorithm>

namespace mw {
    void configureGraphics() {
        QSurfaceFormat f;
#ifdef Q_OS_WIN
        f.setRenderableType(QSurfaceFormat::OpenGL);
        f.setVersion(2, 1);
#else
        f.setRenderableType(QSurfaceFormat::OpenGLES);
        f.setVersion(2, 0);
#endif
        f.setDepthBufferSize(24);
        f.setSamples(0);
        f.setSwapInterval(1);
        QSurfaceFormat::setDefaultFormat(f);
    }

    Viewer::Viewer(QWidget *p) : QOpenGLWidget(p) {
        setMinimumSize(480, 360);
        setFocusPolicy(Qt::StrongFocus);
    }

    Viewer::~Viewer() {
        release();
    }

    void Viewer::release() {
        if (context()) {
            makeCurrent();
            buffer_.destroy();
            shader_.reset();
            doneCurrent();
        }

        ready_ = false;
    }

    void Viewer::setScene(const std::vector<Snapshot> &points, const Mesh &mesh, Vec3 tip) {
        tip_ = tip;
        points_.clear();
        triangles_.clear();
        capTriangles_.clear();
        tipVertices_.clear();
        grid_.clear();

        if (!points.empty()) {
            Vec3 lo = points.front().position, hi = lo;

            for (const auto &s : points) {
                auto p = s.position;
                lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
                hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
            }

            origin_ = (lo + hi) * 0.5;
            span_ = std::max(20.0, norm(hi - lo));
        } else {
            origin_ = {};
            span_ = 80;
        }

        auto vertex = [&](Vec3 p, Vec3 n = Vec3{}) {
            p = p - origin_;
            return Vertex{float(p.x), float(p.y), float(p.z), float(n.x), float(n.y), float(n.z)};
        };

        size_t stride = std::max(size_t(1), (points.size() + 19999) / 20000);

        for (size_t i = 0; i < points.size(); i += stride)
            points_.push_back(vertex(points[i].position));

        for (auto f : mesh.faces) {
            Vec3 a = mesh.vertices[f[0]], b = mesh.vertices[f[1]], c = mesh.vertices[f[2]];
            auto n = unit(cross(b - a, c - a));
            triangles_.push_back(vertex(a, n));
            triangles_.push_back(vertex(b, n));
            triangles_.push_back(vertex(c, n));
        }

        for (auto f : mesh.capFaces) {
            Vec3 a = mesh.vertices[f[0]], b = mesh.vertices[f[1]], c = mesh.vertices[f[2]];
            Vec3 normal = unit(cross(b - a, c - a));
            capTriangles_.push_back(vertex(a, normal));
            capTriangles_.push_back(vertex(b, normal));
            capTriangles_.push_back(vertex(c, normal));
        }

        for (int i = -5; i <= 5; ++i) {
            double d = i * 10.0;
            grid_.push_back(vertex({-50, d, -16}));
            grid_.push_back(vertex({50, d, -16}));
            grid_.push_back(vertex({d, -50, -16}));
            grid_.push_back(vertex({d, 50, -16}));
        }

        tipVertices_.push_back(vertex(tip));
        update();
    }

    void Viewer::setLayers(bool points, bool surface) {
        showPoints_ = points;
        showSurface_ = surface;
        update();
    }

    void Viewer::fit() {
        center_ = origin_;
        distance_ = std::clamp(span_ * 1.6, 25.0, 1000000.0);
        update();
    }

    void Viewer::standardView(int index) {
        if (index == 0) {
            yaw_ = 38;
            pitch_ = 25;
        } else if (index == 1) {
            yaw_ = 0;
            pitch_ = 89.9;
        } else if (index == 2) {
            yaw_ = 0;
            pitch_ = 0;
        } else {
            yaw_ = 90;
            pitch_ = 0;
        }

        update();
    }

    void Viewer::initializeGL() {
        initializeOpenGLFunctions();
        connect(
            context(), &QOpenGLContext::aboutToBeDestroyed, this,
            [this] {
                release();
            },
            Qt::DirectConnection);
        const auto *version = glGetString(GL_VERSION);
        const auto *renderer = glGetString(GL_RENDERER);
        diagnostic_ = QString("%1 | %2").arg(version ? reinterpret_cast<const char *>(version) : "unknown",
            renderer ? reinterpret_cast<const char *>(renderer) : "unknown");
        shader_ = std::make_unique<QOpenGLShaderProgram>();
        const bool es = context()->isOpenGLES();
        const QByteArray vertexPrefix =
            es ? "#version 100\nprecision highp float;\n" : "#version 120\n#define mediump\n";
        const QByteArray fragmentPrefix =
            es ? "#version 100\nprecision mediump float;\n" : "#version 120\n#define mediump\n";
        const QByteArray vs = vertexPrefix +
            "attribute vec3 position; attribute vec3 normal; uniform mat4 mvp; uniform "
            "float pointSize; varying "
            "mediump vec3 N; void "
            "main(){gl_Position=mvp*vec4(position,1.0);gl_PointSize=pointSize;N=normal;}";
        const QByteArray fs = fragmentPrefix +
            "varying mediump vec3 N; uniform vec4 color; uniform float lit; void main(){float "
            "shade=1.0;if(lit>0.5){shade=0.35+0.65*abs(dot(normalize(N),normalize(vec3(0.3,-0.4,"
            "0.85))));}gl_FragColor=vec4(color.rgb*shade,color.a);}";
        ready_ = shader_->addShaderFromSourceCode(QOpenGLShader::Vertex, vs) &&
            shader_->addShaderFromSourceCode(QOpenGLShader::Fragment, fs) && shader_->link() &&
            buffer_.create();

        if (!ready_)
            diagnostic_ = "Graphics error: " + shader_->log() + " | " + diagnostic_;
        qInfo().noquote() << diagnostic_;
        emit graphicsStatus(diagnostic_);
    }

    void Viewer::draw(const std::vector<Vertex> &vertices, unsigned mode, const QVector4D &color, bool lit,
        float pointSize) {
        if (vertices.empty())
            return;
        buffer_.bind();
        buffer_.allocate(vertices.data(), int(vertices.size() * sizeof(Vertex)));
        shader_->setUniformValue("color", color);
        shader_->setUniformValue("lit", lit ? 1.0f : 0.0f);
        shader_->setUniformValue("pointSize", pointSize);
        int p = shader_->attributeLocation("position"), n = shader_->attributeLocation("normal");
        shader_->enableAttributeArray(p);
        shader_->enableAttributeArray(n);
        shader_->setAttributeBuffer(p, GL_FLOAT, 0, 3, sizeof(Vertex));
        shader_->setAttributeBuffer(n, GL_FLOAT, 3 * sizeof(float), 3, sizeof(Vertex));
        glDrawArrays(mode, 0, int(vertices.size()));
        shader_->disableAttributeArray(p);
        shader_->disableAttributeArray(n);
        buffer_.release();
    }

    void Viewer::paintGL() {
        glClearColor(0.035f, 0.055f, 0.08f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (ready_) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);

            if (!context()->isOpenGLES())
                glEnable(0x8642); // GL_VERTEX_PROGRAM_POINT_SIZE, core since desktop GL 2.0.
            QMatrix4x4 projection, view;
            projection.perspective(40, float(width()) / std::max(1, height()),
                float(std::max(0.01, distance_ / 1000)), float(distance_ + span_ * 5 + 100));
            double yaw = yaw_ * pi / 180, pitch = pitch_ * pi / 180;
            Vec3 relative = center_ - origin_,
                    eye = relative +
                Vec3{std::cos(pitch) * std::sin(yaw), -std::cos(pitch) * std::cos(yaw), std::sin(pitch)} *
                    distance_;
            view.lookAt(QVector3D(float(eye.x), float(eye.y), float(eye.z)),
                QVector3D(float(relative.x), float(relative.y), float(relative.z)), {0, 0, 1});
            mvp_ = projection * view;
            shader_->bind();
            shader_->setUniformValue("mvp", mvp_);
            draw(grid_, GL_LINES, {0.12f, 0.19f, 0.25f, 1}, false);
            auto vertex = [&](Vec3 p) {
                p = p - origin_;
                return Vertex{float(p.x), float(p.y), float(p.z), 0, 0, 0};
            };

            draw({vertex({}), vertex({50, 0, 0})}, GL_LINES, {0.95f, 0.34f, 0.34f, 1}, false);
            draw({vertex({}), vertex({0, 50, 0})}, GL_LINES, {0.35f, 0.88f, 0.5f, 1}, false);
            draw({vertex({}), vertex({0, 0, 30})}, GL_LINES, {0.38f, 0.64f, 1, 1}, false);

            if (showSurface_) {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(1, 1);
                draw(triangles_, GL_TRIANGLES, {0.24f, 0.75f, 0.71f, 1}, true);
                draw(capTriangles_, GL_TRIANGLES, {1.0f, 0.56f, 0.22f, 1}, true);
                glDisable(GL_POLYGON_OFFSET_FILL);
            }

            if (showPoints_)
                draw(points_, GL_POINTS, {1, 0.78f, 0.37f, 1}, false, 4);
            draw(tipVertices_, GL_POINTS, {1, 1, 1, 1}, false, 8);
            shader_->release();
        }

        QPainter painter(this);
        painter.setPen(QColor("#dce8f0"));
        painter.drawText(18, 28, "CAVITY SPACE  /  mm");
        painter.setPen(QColor("#91a7ba"));
        painter.drawText(18, height() - 18, "Drag: orbit    Right drag: pan    Wheel: zoom");

        if (!ready_) {
            painter.setPen(QColor("#ff9b8a"));
            painter.drawText(rect().adjusted(24, 50, -24, -40), Qt::TextWordWrap, diagnostic_);
        } else {
            for (auto label : std::array<std::pair<Vec3, QString>, 3>{
                        {{{50, 0, 0}, "X"}, {{0, 50, 0}, "Y"}, {{0, 0, 30}, "Z"}}}) {
                Vec3 p = label.first - origin_;
                auto clip = mvp_ * QVector4D(float(p.x), float(p.y), float(p.z), 1);

                if (clip.w() > 0) {
                    auto q = clip.toVector3DAffine();
                    painter.drawText(
                        QPointF((q.x() + 1) * width() / 2 + 5, (1 - q.y()) * height() / 2 - 4), label.second);
                }
            }
        }
    }

    void Viewer::resizeGL(int, int) {
    }

    void Viewer::mousePressEvent(QMouseEvent *e) {
        lastMouse_ = e->position().toPoint();
    }

    void Viewer::mouseMoveEvent(QMouseEvent *e) {
        QPoint delta = e->position().toPoint() - lastMouse_;
        lastMouse_ = e->position().toPoint();

        if (e->buttons() & Qt::LeftButton) {
            yaw_ += delta.x() * 0.4;
            pitch_ = std::clamp(pitch_ + delta.y() * 0.4, -89.0, 89.0);
        } else if (e->buttons() & (Qt::RightButton | Qt::MiddleButton)) {
            double a = yaw_ * pi / 180, p = pitch_ * pi / 180,
                    scale = distance_ * 0.75 / std::max(1, height());
            Vec3 right{std::cos(a), std::sin(a), 0},
                up{-std::sin(p) * std::sin(a), std::sin(p) * std::cos(a), std::cos(p)};
            center_ = center_ - right * (delta.x() * scale) + up * (delta.y() * scale);
        }

        update();
    }

    void Viewer::wheelEvent(QWheelEvent *e) {
        distance_ = std::clamp(distance_ * std::exp(-e->angleDelta().y() / 800.0), 1.0, 1000000.0);
        update();
        e->accept();
    }
} // namespace mw
