// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Generic flow layout that places widgets left-to-right and wraps to a new row
 * when the available width is exceeded.
 */

#ifndef FLOW_LAYOUT_H
#define FLOW_LAYOUT_H

#include <QLayout>
#include <QList>
#include <QStyle>

class FlowLayout : public QLayout {
    Q_OBJECT

public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = -1, int hSpacing = -1, int vSpacing = -1);
    explicit FlowLayout(int hSpacing = -1, int vSpacing = -1);
    ~FlowLayout() override;

    void addItem(QLayoutItem* item) override;
    void insertItem(int index, QLayoutItem* item);
    void insertWidget(int index, QWidget* widget);

    void addSpacing(int size);
    void insertSpacing(int index, int size);
    void addStretch(int stretch = 1);
    void insertStretch(int index, int stretch = 1);

    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize sizeForWidth(int width) const;
    int count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    QSize minimumSize() const override;
    void setGeometry(const QRect& rect) override;
    QSize sizeHint() const override;

    int horizontalSpacing() const;
    int verticalSpacing() const;

    Qt::Alignment alignment() const { return _alignment; }
    void setAlignment(Qt::Alignment alignment) { _alignment = alignment; }

    bool centering() const { return (_alignment & Qt::AlignHCenter) != 0; }
    void setCentering(bool centering) { _alignment = (centering ? Qt::AlignHCenter : Qt::AlignLeft) | (_alignment & Qt::AlignVertical_Mask); }

private:
    QSize doLayout(const QRect& rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem*> _items;
    int _hSpace = -1;
    int _vSpace = -1;
    Qt::Alignment _alignment = Qt::AlignLeft | Qt::AlignTop;
};

#endif // FLOW_LAYOUT_H
