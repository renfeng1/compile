#include "gui/MainWindow.h"

#include <QAction>
#include <QByteArray>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QList>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStatusBar>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>

namespace minipas_gui {
namespace {

QString joinLines(const std::vector<std::string> &lines) {
    QStringList list;
    for (const auto &line : lines) {
        list << QString::fromUtf8(line.c_str());
    }
    return list.join("\n");
}

QString instructionText(const minipas::TargetInstruction &instruction) {
    QStringList parts;
    parts << QString::fromUtf8(instruction.op.c_str());
    if (!instruction.a.empty()) parts << QString::fromUtf8(instruction.a.c_str());
    if (!instruction.b.empty()) parts << QString::fromUtf8(instruction.b.c_str());
    if (!instruction.c.empty()) parts << QString::fromUtf8(instruction.c.c_str());
    return parts.join(" ");
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    buildUi();
    loadSample();
}

void MainWindow::buildUi() {
    setWindowTitle("MiniPascal Visual Compiler");
    resize(1280, 760);

    auto *toolbar = addToolBar("Main");
    QAction *openAction = toolbar->addAction("Open");
    QAction *saveAction = toolbar->addAction("Save");
    QAction *sampleAction = toolbar->addAction("Sample");
    QAction *compileAction = toolbar->addAction("Compile");

    connect(openAction, &QAction::triggered, this, [this]() { openFile(); });
    connect(saveAction, &QAction::triggered, this, [this]() { saveFile(); });
    connect(sampleAction, &QAction::triggered, this, [this]() { loadSample(); });
    connect(compileAction, &QAction::triggered, this, [this]() { compileSource(); });

    auto *splitter = new QSplitter(this);
    sourceEdit_ = new QPlainTextEdit(splitter);
    sourceEdit_->setLineWrapMode(QPlainTextEdit::NoWrap);
    sourceEdit_->setPlaceholderText("Write MiniPascal source here...");

    tabs_ = new QTabWidget(splitter);
    tokenTable_ = createTable();
    identifierTable_ = createTable();
    constantTable_ = createTable();
    symbolTable_ = createTable();
    quadTable_ = createTable();
    optimizedQuadTable_ = createTable();
    targetTable_ = createTable();
    memoryTable_ = createTable();
    diagnosticsEdit_ = new QPlainTextEdit();
    astEdit_ = new QPlainTextEdit();
    traceEdit_ = new QPlainTextEdit();
    outputEdit_ = new QPlainTextEdit();

    for (auto *edit : {diagnosticsEdit_, astEdit_, traceEdit_, outputEdit_}) {
        edit->setReadOnly(true);
        edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    }

    tabs_->addTab(tokenTable_, "Tokens");
    tabs_->addTab(identifierTable_, "Identifiers");
    tabs_->addTab(constantTable_, "Constants");
    tabs_->addTab(symbolTable_, "Symbols");
    tabs_->addTab(astEdit_, "AST");
    tabs_->addTab(quadTable_, "Quadruples");
    tabs_->addTab(optimizedQuadTable_, "Optimized");
    tabs_->addTab(targetTable_, "Target");
    tabs_->addTab(traceEdit_, "VM Trace");
    tabs_->addTab(outputEdit_, "Output");
    tabs_->addTab(memoryTable_, "Memory");
    tabs_->addTab(diagnosticsEdit_, "Diagnostics");

    splitter->addWidget(sourceEdit_);
    splitter->addWidget(tabs_);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 5);
    setCentralWidget(splitter);
    statusBar()->showMessage("Ready");
}

void MainWindow::openFile() {
    const QString path = QFileDialog::getOpenFileName(this, "Open MiniPascal Source",
                                                      QString(), "MiniPascal (*.minipas *.txt);;All Files (*)");
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Open failed", file.errorString());
        return;
    }
    sourceEdit_->setPlainText(QString::fromUtf8(file.readAll()));
    statusBar()->showMessage("Opened " + path);
}

void MainWindow::saveFile() {
    const QString path = QFileDialog::getSaveFileName(this, "Save MiniPascal Source",
                                                      QString(), "MiniPascal (*.minipas);;All Files (*)");
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save failed", file.errorString());
        return;
    }
    file.write(sourceEdit_->toPlainText().toUtf8());
    statusBar()->showMessage("Saved " + path);
}

void MainWindow::loadSample() {
    QFile file("examples/sample.minipas");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        sourceEdit_->setPlainText(QString::fromUtf8(file.readAll()));
        return;
    }
    sourceEdit_->setPlainText(
        "program example\n"
        "var a,b: integer;\n"
        "begin\n"
        "  a := 2;\n"
        "  b := 2 * 5 + a;\n"
        "  write(b)\n"
        "end.");
}

void MainWindow::compileSource() {
    const QByteArray bytes = sourceEdit_->toPlainText().toUtf8();
    const std::string source(bytes.constData(), static_cast<size_t>(bytes.size()));
    minipas::Compiler compiler;
    const minipas::CompilationResult result = compiler.compile(source);
    showResult(result);
    statusBar()->showMessage(result.success ? "Compile succeeded" : "Compile failed");
    QWidget *current = result.success ? static_cast<QWidget *>(optimizedQuadTable_)
                                      : static_cast<QWidget *>(diagnosticsEdit_);
    tabs_->setCurrentWidget(current);
}

void MainWindow::showResult(const minipas::CompilationResult &result) {
    QList<QStringList> tokenRows;
    for (const auto &token : result.tokens) {
        tokenRows << (QStringList()
                      << QString::number(tokenRows.size() + 1)
                      << qstr(minipas::tokenKindName(token.kind))
                      << qstr(token.lexeme)
                      << qstr(token.table)
                      << QString::number(token.index)
                      << QString::number(token.line)
                      << QString::number(token.column)
                      << qstr(token.valueType));
    }
    setTable(tokenTable_, {"#", "Kind", "Lexeme", "Table", "Index", "Line", "Column", "Type"}, tokenRows);

    QList<QStringList> identifierRows;
    for (const auto &entry : result.identifiers) {
        identifierRows << (QStringList() << QString::number(entry.index) << qstr(entry.name) << qstr(entry.category));
    }
    setTable(identifierTable_, {"Index", "Name", "Category"}, identifierRows);

    QList<QStringList> constantRows;
    for (const auto &constant : result.constants) {
        constantRows << (QStringList()
                         << QString::number(constant.index)
                         << qstr(constant.literal)
                         << qstr(constant.type)
                         << qstr(constant.value));
    }
    setTable(constantTable_, {"Index", "Literal", "Type", "Value"}, constantRows);

    QList<QStringList> symbolRows;
    for (const auto &symbol : result.symbols) {
        symbolRows << (QStringList()
                       << QString::number(symbol.index)
                       << qstr(symbol.name)
                       << qstr(symbol.type)
                       << qstr(symbol.category)
                       << QString::number(symbol.address)
                       << QString::number(symbol.length)
                       << QString::number(symbol.arrayLength)
                       << QString::number(symbol.level));
    }
    setTable(symbolTable_, {"Index", "Name", "Type", "Category", "Address", "Length", "Array", "Level"}, symbolRows);

    auto quadRows = [](const std::vector<minipas::Quad> &quads) {
        QList<QStringList> rows;
        for (const auto &quad : quads) {
            rows << (QStringList()
                     << QString::number(quad.index)
                     << QString::number(quad.block)
                     << QString::fromUtf8(quad.op.c_str())
                     << QString::fromUtf8(quad.arg1.c_str())
                     << QString::fromUtf8(quad.arg2.c_str())
                     << QString::fromUtf8(quad.result.c_str()));
        }
        return rows;
    };
    setTable(quadTable_, {"#", "Block", "Op", "Arg1", "Arg2", "Result"}, quadRows(result.quads));
    setTable(optimizedQuadTable_, {"#", "Block", "Op", "Arg1", "Arg2", "Result"}, quadRows(result.optimizedQuads));

    QList<QStringList> targetRows;
    for (const auto &instruction : result.targetCode) {
        targetRows << (QStringList()
                       << QString::number(instruction.index)
                       << qstr(instruction.op)
                       << qstr(instruction.a)
                       << qstr(instruction.b)
                       << qstr(instruction.c)
                       << instructionText(instruction));
    }
    setTable(targetTable_, {"#", "Op", "A", "B", "C", "Text"}, targetRows);

    QList<QStringList> memoryRows;
    for (const auto &cell : result.finalMemory) {
        memoryRows << (QStringList() << qstr(cell.name) << qstr(cell.type) << qstr(cell.value));
    }
    setTable(memoryTable_, {"Name", "Type", "Value"}, memoryRows);

    astEdit_->setPlainText(qstr(result.astText));
    traceEdit_->setPlainText(joinLines(result.vmTrace));
    outputEdit_->setPlainText(joinLines(result.vmOutput));

    QStringList diagnostics;
    for (const auto &diagnostic : result.diagnostics) {
        diagnostics << QString("%1 %2 %3:%4 %5")
                           .arg(qstr(minipas::severityName(diagnostic.severity)))
                           .arg(qstr(diagnostic.stage))
                           .arg(diagnostic.line)
                           .arg(diagnostic.column)
                           .arg(qstr(diagnostic.message));
    }
    diagnosticsEdit_->setPlainText(diagnostics.join("\n"));
}

void MainWindow::setTable(QTableWidget *table,
                          const QStringList &headers,
                          const QList<QStringList> &rows) {
    table->clear();
    table->setColumnCount(headers.size());
    table->setRowCount(rows.size());
    table->setHorizontalHeaderLabels(headers);
    for (int row = 0; row < rows.size(); ++row) {
        const QStringList values = rows[row];
        for (int column = 0; column < headers.size(); ++column) {
            auto *item = new QTableWidgetItem(column < values.size() ? values[column] : QString());
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            table->setItem(row, column, item);
        }
    }
    table->resizeColumnsToContents();
}

QTableWidget *MainWindow::createTable() {
    auto *table = new QTableWidget();
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    return table;
}

QString MainWindow::qstr(const std::string &text) const {
    return QString::fromUtf8(text.c_str());
}

} // namespace minipas_gui
