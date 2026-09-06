#ifndef RESIZELABEL_H
#define RESIZELABEL_H

#include <QLabel>
#include <QTimer>
#include <QDebug>
class ResizeLabel : public QLabel{
    Q_OBJECT
    Q_DISABLE_COPY(ResizeLabel)
public:
    explicit ResizeLabel(QWidget* parent = nullptr,
                         Qt::WindowFlags flags = Qt::WindowFlags());


    explicit ResizeLabel(const QString &text,
                         QWidget *parent = nullptr,
                         Qt::WindowFlags flags = Qt::WindowFlags());

    /**
     * @brief setMaxFontSize Sets the maximum allowed font size, it will
     * not try to automatically resize above this size
     * @param size The pixel size of the font.
     */
    void setMaxFontSize(int size);

public slots:
    /**
     * @brief fitLabelToContainer Performs a check if the current text with
     * the current font size fits in its container, if not, reduces the font
     * size until it fits.
     */
    void fitLabelToContainer();

private:
    int originalSize;
};


#endif // RESIZELABEL_H
