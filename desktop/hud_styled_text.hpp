#pragma once
#include "presentation.hpp"
#include <QFontMetricsF>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QTextLayout>

class HudStyledText : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString caption MEMBER m_caption NOTIFY appearanceChanged)
    Q_PROPERTY(QFont font MEMBER m_font NOTIFY appearanceChanged)
    Q_PROPERTY(bool elide MEMBER m_elide NOTIFY appearanceChanged)
    Q_PROPERTY(bool rightAligned MEMBER m_rightAligned NOTIFY appearanceChanged)
    Q_PROPERTY(QColor ink MEMBER m_ink NOTIFY appearanceChanged)
    Q_PROPERTY(bool rainbow MEMBER m_rainbow NOTIFY appearanceChanged)
    Q_PROPERTY(float phase MEMBER m_phase NOTIFY appearanceChanged)
    Q_PROPERTY(bool glow MEMBER m_glow NOTIFY appearanceChanged)

public:
    explicit HudStyledText(QQuickItem* parent = nullptr) : QQuickPaintedItem(parent) {
        connect(this, &HudStyledText::appearanceChanged, this, [this] { update(); });
    }

    void paint(QPainter* painter) override {
        if (m_caption.isEmpty() || width() <= 0)
            return;
        const qreal elideWidth = m_elide ? width() : 0;
        if (m_caption != m_layoutCaption || m_font != m_layout.font() || elideWidth != m_elideWidth) {
            m_layoutCaption = m_caption;
            m_elideWidth = elideWidth;
            m_layout.setFont(m_font);
            m_layout.setText(m_elide ? QFontMetricsF(m_font).elidedText(m_caption, Qt::ElideRight, width())
                                     : m_caption);
            m_layout.setCacheEnabled(true);
            m_layout.beginLayout();
            auto line = m_layout.createLine();
            if (line.isValid()) line.setNumColumns(int(m_layout.text().size()));
            m_layout.endLayout();
        }
        const auto line = m_layout.lineAt(0);
        if (!line.isValid())
            return;
        const qreal advance = line.naturalTextWidth();
        const qreal left = m_rightAligned ? width() - advance : 0;
        QBrush brush(m_ink);
        if (m_rainbow) {
            QLinearGradient spectrum(left, 0, left + qMax(1., advance), 0);
            for (size_t index = 0; index < fg::hudRainbowStops.size(); ++index) {
                const float position = float(index) / (fg::hudRainbowStops.size() - 1);
                spectrum.setColorAt(position, QColor::fromRgb(fg::hudRainbowColor(position + m_phase)));
            }
            brush = QBrush(spectrum);
        }
        painter->setRenderHint(QPainter::TextAntialiasing);
        painter->setPen(QPen(brush, 1));
        if (m_glow) {
            painter->setOpacity(.25);
            line.draw(painter, QPointF(left - 1, 0));
            line.draw(painter, QPointF(left + 1, 0));
            painter->setOpacity(1);
        }
        line.draw(painter, QPointF(left, 0));
    }

signals:
    void appearanceChanged();

private:
    QString m_caption;
    QFont m_font;
    bool m_elide = false;
    bool m_rightAligned = false;
    QColor m_ink = Qt::white;
    bool m_rainbow = false;
    float m_phase = 0.f;
    bool m_glow = false;
    QTextLayout m_layout;
    QString m_layoutCaption;
    qreal m_elideWidth = 0;
};
