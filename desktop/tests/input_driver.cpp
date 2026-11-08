#include <QWindow>
#include <QtTest/QTest>
// Kept in a separate CI-only module. Production has no Qt Test dependency.
extern "C" Q_DECL_EXPORT void FramoryxTestClick(QWindow* window,int x,int y){
    QTest::mouseMove(window,QPoint(x,y),0);
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,QPoint(x,y),0);
}
