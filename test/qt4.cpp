#include <qlineedit.h>
#include <qtextedit.h>
#include <qlayout.h>
#include <qapplication.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget widget;
    widget.resize(300, 200);

    QBoxLayout *layout = new QVBoxLayout(&widget);

    QLineEdit *lineedit = new QLineEdit(&widget);
    layout->addWidget(lineedit);

    QTextEdit *textedit = new QTextEdit(&widget);
    layout->addWidget(textedit);

    widget.show();

    return app.exec();
}
