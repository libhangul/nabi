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

    QLineEdit *lineedit = new QLineEdit(&widget, 0);
    layout->addWidget(lineedit);

    QTextEdit *textedit = new QTextEdit(&widget, 0);
    layout->addWidget(textedit);

    app.setMainWidget(&widget);
    widget.show();

    return app.exec();
}
