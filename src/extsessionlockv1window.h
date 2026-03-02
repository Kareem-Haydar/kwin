#pragma once

#include "wayland/extsessionlock_v1.h"
#include "waylandwindow.h"

#include <QList>
#include <QObject>

namespace KWin
{

class LogicalOutput;

struct ExtSessionLockV1ConfigureEvent
{
    quint32 serial;
    QSize size;
};

class ExtSessionLockV1Window : public WaylandWindow
{
    Q_OBJECT
public:
    explicit ExtSessionLockV1Window(ExtSessionLockSurfaceV1Interface *surface, LogicalOutput *output);

    WindowType windowType() const override;

    bool wantsInput() const override;
    void destroyWindow() override;
    void closeWindow() override {}  // lock surfaces cannot be closed

protected:
    Layer belongsToLayer() const override;
    bool acceptsFocus() const override;
    void moveResizeInternal(const RectF &rect, MoveResizeMode mode) override;
    void doSetNextTargetScale() override;
    void doSetPreferredBufferTransform() override;
    void doSetPreferredColorDescription() override;
    bool isLockScreen() const override;
    bool isPlaceable() const override;
    bool isCloseable() const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool isResizable() const override;

private:
    void handleConfigure(quint32 serial);
    void handleDestroy();
    void handleCommit();
    void handleSizeChanged();

    ExtSessionLockSurfaceV1Interface *m_surface;
    QList<ExtSessionLockV1ConfigureEvent> m_configureEvents;
};
}
