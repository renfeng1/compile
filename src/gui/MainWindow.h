#ifndef MINIPAS_MAINWINDOW_H
#define MINIPAS_MAINWINDOW_H

#include "core/Compiler.h"

#include <QMainWindow>

class QPlainTextEdit;
class QTableWidget;
class QTabWidget;

namespace minipas_gui {

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void buildUi();
    void openFile();
    void saveFile();
    void loadSample();
    void compileSource();
    void showResult(const minipas::CompilationResult &result);
    void setTable(QTableWidget *table,
                  const QStringList &headers,
                  const QList<QStringList> &rows);
    QTableWidget *createTable();
    QString qstr(const std::string &text) const;

    QPlainTextEdit *sourceEdit_ = nullptr;
    QPlainTextEdit *diagnosticsEdit_ = nullptr;
    QPlainTextEdit *astEdit_ = nullptr;
    QPlainTextEdit *traceEdit_ = nullptr;
    QPlainTextEdit *outputEdit_ = nullptr;
    QTableWidget *tokenTable_ = nullptr;
    QTableWidget *identifierTable_ = nullptr;
    QTableWidget *constantTable_ = nullptr;
    QTableWidget *symbolTable_ = nullptr;
    QTableWidget *quadTable_ = nullptr;
    QTableWidget *optimizedQuadTable_ = nullptr;
    QTableWidget *targetTable_ = nullptr;
    QTableWidget *memoryTable_ = nullptr;
    QTabWidget *tabs_ = nullptr;
};

} // namespace minipas_gui

#endif // MINIPAS_MAINWINDOW_H
