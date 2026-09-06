#include "resizelabel.h"

ResizeLabel::ResizeLabel(QWidget *parent, Qt::WindowFlags flags):
    QLabel(parent, flags),
    originalSize(19)
{
}

ResizeLabel::ResizeLabel(const QString &text, QWidget *parent, Qt::WindowFlags flags) :
    QLabel(text, parent, flags),
    originalSize(19)
{
}

void ResizeLabel::setMaxFontSize(int size)
{
    this->originalSize = size;
}

void ResizeLabel::fitLabelToContainer()
{
    if(this->originalSize == 0)
    {
        this->originalSize = this->font().pixelSize();
    }

    QRect rect = contentsRect();
    QFont currentFont = font();

    int pixelSize = 1;
    for (; pixelSize <= originalSize; pixelSize++)
    {
        currentFont.setPixelSize(pixelSize);
        const QRect fontRect = QFontMetrics(currentFont).boundingRect(text());
        if(fontRect.width() > rect.width() - 10)
        {
            currentFont.setPixelSize(pixelSize - 1);
            break;
        }
    }
    setFont(currentFont);
}
