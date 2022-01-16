#pragma once

#include <ui/widgets/floating_tool_bar/floating_tool_bar.h>


namespace Ui {

/**
 * @brief Панель с информацией о соединении с сервером
 */
class CORE_LIBRARY_EXPORT ConnectionStatusToolBar : public FloatingToolBar
{
    Q_OBJECT

public:
    explicit ConnectionStatusToolBar(QWidget* _parent = nullptr);
    ~ConnectionStatusToolBar() override;

    /**
     * @brief Визуализировать состояние подключения
     */
    void setConnectionAvailable(bool _available);

protected:
    /**
     * @brief Переопределяем для корректировки позиции и положения при изменениях родителя
     */
    bool eventFilter(QObject* _watched, QEvent* _event) override;

    /**
     * @brief Рисуем крутящийся лоадер
     */
    void paintEvent(QPaintEvent* _event) override;

    /**
     * @brief Обновляем UI при изменении дизайн системы
     */
    void designSystemChangeEvent(DesignSystemChangeEvent* _event) override;

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace Ui
