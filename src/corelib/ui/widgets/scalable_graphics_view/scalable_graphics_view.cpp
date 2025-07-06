#include "scalable_graphics_view.h"

#include <QApplication>
#include <QEvent>
#include <QGestureEvent>
#include <QGraphicsItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QVariantAnimation>
#include <QWheelEvent>

namespace {

/**
 * @brief Было ли инициировано событие нажатием заданной кнопки мыши
 */
bool eventTriggeredBy(QMouseEvent* _event, Qt::MouseButton _button)
{
    return _event->buttons() & _button || _event->button() == _button;
}

} // namespace


ScalableGraphicsView::ScalableGraphicsView(QWidget* _parent)
    : QGraphicsView(_parent)
{
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    setTransformationAnchor(AnchorUnderMouse);

    //
    // Отслеживаем жесты
    //
    grabGesture(Qt::PinchGesture);
    grabGesture(Qt::SwipeGesture);
}

void ScalableGraphicsView::setScaleAvailable(bool _available)
{
    if (m_isScaleAvailable == _available) {
        return;
    }

    m_isScaleAvailable = _available;
}

void ScalableGraphicsView::zoomIn()
{
    scaleView(qreal(0.05));
}

void ScalableGraphicsView::zoomOut()
{
    scaleView(qreal(-0.05));
}

void ScalableGraphicsView::animateCenterOn(QGraphicsItem* _item)
{
    const qreal width = viewport()->width();
    const qreal height = viewport()->height();
    const QPointF viewPoint
        = transform().map(QRectF(_item->mapToScene(_item->shape().boundingRect().topLeft()),
                                 _item->boundingRect().size())
                              .center());
    QPoint targetPosition;
    if (isRightToLeft()) {
        qint64 horizontal = 0;
        horizontal += horizontalScrollBar()->minimum();
        horizontal += horizontalScrollBar()->maximum();
        horizontal -= int(viewPoint.x() - width / 2.0);
        targetPosition.setX(horizontal);
    } else {
        targetPosition.setX(int(viewPoint.x() - width / 2.0));
    }
    targetPosition.setY(int(viewPoint.y() - height / 2.0));

    auto scrollingAnimation = new QVariantAnimation(this);
    scrollingAnimation->setEasingCurve(QEasingCurve::OutQuad);
    scrollingAnimation->setDuration(240);
    scrollingAnimation->setStartValue(
        QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value()));
    scrollingAnimation->setEndValue(targetPosition);
    connect(scrollingAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& _value) {
                const auto position = _value.toPoint();
                horizontalScrollBar()->setValue(position.x());
                verticalScrollBar()->setValue(position.y());
            });
    scrollingAnimation->start(QVariantAnimation::DeleteWhenStopped);
}

void ScalableGraphicsView::animateEnsureVisible(QGraphicsItem* _item)
{
    const qreal width = viewport()->width();
    const qreal height = viewport()->height();
    const QRectF viewRect
        = QRectF(transform().map(_item->mapToScene(_item->shape().boundingRect().topLeft())),
                 _item->boundingRect().size());
    const qreal left = horizontalScrollBar()->value();
    const qreal right = left + width;
    const qreal top = verticalScrollBar()->value();
    const qreal bottom = top + height;
    const int xmargin = 50;
    const int ymargin = 50;
    QPoint targetPosition(left, top);
    if (viewRect.left() <= left + xmargin) {
        targetPosition.setX(viewRect.left() - xmargin - 0.5);
    }
    if (viewRect.right() >= right - xmargin) {
        targetPosition.setX(viewRect.right() - width + xmargin + 0.5);
    }
    if (viewRect.top() <= top + ymargin) {
        targetPosition.setY(viewRect.top() - ymargin - 0.5);
    }
    if (viewRect.bottom() >= bottom - ymargin) {
        targetPosition.setY(viewRect.bottom() - height + ymargin + 0.5);
    }

    auto scrollingAnimation = new QVariantAnimation(this);
    scrollingAnimation->setEasingCurve(QEasingCurve::OutQuad);
    scrollingAnimation->setDuration(240);
    scrollingAnimation->setStartValue(QPoint(left, top));
    scrollingAnimation->setEndValue(targetPosition);
    connect(scrollingAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& _value) {
                const auto position = _value.toPoint();
                horizontalScrollBar()->setValue(position.x());
                verticalScrollBar()->setValue(position.y());
            });
    scrollingAnimation->start(QVariantAnimation::DeleteWhenStopped);
}

QByteArray ScalableGraphicsView::saveState() const
{
    QByteArray state;
    QDataStream stream(&state, QIODevice::WriteOnly);
    stream << transform();
    stream << horizontalScrollBar()->value();
    stream << verticalScrollBar()->value();
    return state;
}

void ScalableGraphicsView::restoreState(const QByteArray& _state)
{
    if (_state.isEmpty()) {
        return;
    }

    auto state = _state;
    QDataStream stream(&state, QIODevice::ReadOnly);
    QTransform transform;
    stream >> transform;
    setTransform(transform);
    int scrollValue = 0;
    stream >> scrollValue;
    horizontalScrollBar()->setValue(scrollValue);
    stream >> scrollValue;
    verticalScrollBar()->setValue(scrollValue);
}

QImage ScalableGraphicsView::saveSceneToPng(qreal _scaleFactor)
{
    scene()->clearSelection();
    const auto margin = 48;
    const auto contentsRect
        = scene()->itemsBoundingRect().marginsAdded({ margin, margin, margin, margin });
    QImage image(contentsRect.size().toSize() * _scaleFactor, QImage::Format_ARGB32_Premultiplied);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(image.rect(), backgroundBrush());
    scene()->render(&painter, image.rect(), contentsRect);
    return image;
}

bool ScalableGraphicsView::event(QEvent* _event)
{
    //
    // Определяем особый обработчик для жестов
    //
    if (_event->type() == QEvent::Gesture) {
        gestureEvent(static_cast<QGestureEvent*>(_event));
        return true;
    }

    //
    // Прочие стандартные обработчики событий
    //
    return QGraphicsView::event(_event);
}

void ScalableGraphicsView::gestureEvent(QGestureEvent* _event)
{
    //
    // Жест масштабирования
    //
    if (QGesture* gesture = _event->gesture(Qt::PinchGesture)) {
        if (QPinchGesture* pinch = qobject_cast<QPinchGesture*>(gesture)) {
            //
            // При масштабировании за счёт жестов приходится немного притормаживать
            // т.к. события приходят слишком часто и при обработке каждого события
            // пользователю просто невозможно корректно настроить масштаб
            //

            const int INERTION_BREAK_STOP = 4;
            qreal zoomDelta = 0;
            if (pinch->scaleFactor() > 1) {
                if (m_gestureZoomInertionBreak < 0) {
                    m_gestureZoomInertionBreak = 0;
                } else if (m_gestureZoomInertionBreak >= INERTION_BREAK_STOP) {
                    m_gestureZoomInertionBreak = 0;
                    zoomDelta = 0.1;
                } else {
                    ++m_gestureZoomInertionBreak;
                }
            } else if (pinch->scaleFactor() < 1) {
                if (m_gestureZoomInertionBreak > 0) {
                    m_gestureZoomInertionBreak = 0;
                } else if (m_gestureZoomInertionBreak <= -INERTION_BREAK_STOP) {
                    m_gestureZoomInertionBreak = 0;
                    zoomDelta = -0.1;
                } else {
                    --m_gestureZoomInertionBreak;
                }
            } else {
                //
                // При обычной прокрутке часто приходит событие с scaledFactor == 1,
                // так что просто игнорируем их
                //
            }

            //
            // Если необходимо масштабируем и перерисовываем представление
            //
            const bool needZoomIn = pinch->scaleFactor() > 1;
            const bool needZoomOut = pinch->scaleFactor() < 1;
            if (!qFuzzyCompare(zoomDelta, 0.0) && needZoomIn) {
                zoomIn();
            } else if (!qFuzzyCompare(zoomDelta, 0.0) && needZoomOut) {
                zoomOut();
            }

            _event->accept();
        }
    }
}

void ScalableGraphicsView::showEvent(QShowEvent* _event)
{
    //
    // QGraphicsView всегда центрирует сцену при отображении, а нам этого не нужно, поэтому на время
    // отображения отключаем якорь трансформаций, а после отображения возвращаем его в предыдущее
    // заданное состояние
    //

    const auto anchor = transformationAnchor();
    setTransformationAnchor(NoAnchor);

    QGraphicsView::showEvent(_event);

    setTransformationAnchor(anchor);
}

void ScalableGraphicsView::wheelEvent(QWheelEvent* _event)
{
    //
    // Собственно масштабирование
    //
    if (_event->modifiers() & Qt::ControlModifier) {
        if (_event->angleDelta().y() != 0) {
#ifdef Q_OS_MAC
            const qreal angleDivider = 20.;
#else
            const qreal angleDivider = 120.;
#endif

            //
            // zoomRange > 0 - масштаб увеличивается
            // zoomRange < 0 - масштаб уменьшается
            //
            const qreal zoom = _event->angleDelta().y() / angleDivider;
            if (zoom > 0) {
                zoomIn();
            } else if (zoom < 0) {
                zoomOut();
            }

            _event->accept();
        }
    }
    //
    // В противном случае прокручиваем редактор
    //
    else {
        QGraphicsView::wheelEvent(_event);
    }
}

void ScalableGraphicsView::keyPressEvent(QKeyEvent* _event)
{
    if ((_event->key() == Qt::Key_Plus || _event->key() == Qt::Key_Equal)
        && _event->modifiers() & Qt::ControlModifier) {
        zoomIn();
        return;
    }

    if (_event->key() == Qt::Key_Minus && _event->modifiers() & Qt::ControlModifier) {
        zoomOut();
        return;
    }

    if (_event->key() == Qt::Key_Delete || _event->key() == Qt::Key_Backspace) {
        emit deletePressed();
        return;
    }

    if (_event->key() == Qt::Key_Space && !_event->isAutoRepeat()) {
        m_inScrolling = true;
        QApplication::setOverrideCursor(QCursor(Qt::OpenHandCursor));
        return;
    }

    QGraphicsView::keyPressEvent(_event);
}

void ScalableGraphicsView::keyReleaseEvent(QKeyEvent* _event)
{
    if (_event->key() == Qt::Key_Space && !_event->isAutoRepeat()) {
        m_inScrolling = false;
        QApplication::restoreOverrideCursor();
    }

    QGraphicsView::keyReleaseEvent(_event);
}

void ScalableGraphicsView::mousePressEvent(QMouseEvent* _event)
{
    if (eventTriggeredBy(_event, Qt::MiddleButton)) {
        m_inScrolling = true;
    }

    if (m_inScrolling
        && (eventTriggeredBy(_event, Qt::LeftButton)
            || eventTriggeredBy(_event, Qt::MiddleButton))) {
        m_scrollingLastPos = _event->globalPos();
        QApplication::setOverrideCursor(QCursor(Qt::ClosedHandCursor));
        return;
    }

    QGraphicsView::mousePressEvent(_event);
}

void ScalableGraphicsView::mouseMoveEvent(QMouseEvent* _event)
{
    if (m_inScrolling) {
        //
        // Если в данный момент происходит прокрутка полотна
        //
        if (eventTriggeredBy(_event, Qt::LeftButton)
            || eventTriggeredBy(_event, Qt::MiddleButton)) {
            const QPoint prevPos = m_scrollingLastPos;
            m_scrollingLastPos = _event->globalPos();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value()
                                            + (prevPos.x() - m_scrollingLastPos.x()));
            verticalScrollBar()->setValue(verticalScrollBar()->value()
                                          + (prevPos.y() - m_scrollingLastPos.y()));
            return;
        }
    }

    QGraphicsView::mouseMoveEvent(_event);
}

void ScalableGraphicsView::mouseReleaseEvent(QMouseEvent* _event)
{
    //
    // Восстанавливаем курсор
    // NOTE: если пробел был отпущен раньше чем кнопка мыши, то мы попадём сюда уже с m_inScrolling
    // равным false, поэтому в любом случае восстанавливаем исходный курсор виджета
    //
    while (QApplication::overrideCursor()) {
        QApplication::restoreOverrideCursor();
    }

    if (m_inScrolling && eventTriggeredBy(_event, Qt::MiddleButton)) {
        m_inScrolling = false;
    }

    QGraphicsView::mouseReleaseEvent(_event);
}

void ScalableGraphicsView::scaleView(qreal _factor)
{
    if (!m_isScaleAvailable) {
        return;
    }

    scale(1 + _factor, 1 + _factor);
    emit scaleChanged();
}
