#include "MainWindow.h"
#include <QCommandLineParser>
#include <QSqlDatabase>
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("CSV DB editor");
    parser.addHelpOption();
    parser.addPositionalArgument("dbPath", "Database to open");

    // If a path is not provided, get a path from the user on startup
    parser.process(app);
    QString dbPath;
    if(parser.positionalArguments().size() < 1)
    {
        dbPath = QFileDialog::getExistingDirectory(nullptr, "Open Database Directory", "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    }
    else
    {
        dbPath = QFileInfo(parser.positionalArguments()[0]).absoluteFilePath();
    }

    if (dbPath.size() == 0)
    {
        QMessageBox::critical(nullptr, "Database path required", "Select a valid DB path or a provide one as a parameter.");
        return 1;
    }

    // Setup global database
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    if (!db.open())
    {
        QMessageBox::critical(nullptr, "Cannot open database", "Unable to open SQLite DB.");
        return 1;
    }
/*
    std::vector<TableData> tableData;
    QString readLog;
    if(!DBReadCSV(dbPath, db, tableData, readLog))
    {
        QMessageBox::critical(0, "DB read failure", readLog + "\nClick Cancel to exit.", QMessageBox::Cancel);
        return 1;
    }
    else if(readLog.size() > 0)
    {
        QMessageBox::information(0, "DB read errors", readLog, QMessageBox::Ok);
    }

    // Turn database foreign key checking on
    db.exec("PRAGMA foreign_keys = ON");
    QSqlQuery fkOn = db.exec("PRAGMA foreign_key_check");
    QString errorStr;
    while(fkOn.next())
    {
        int i = 0;
        QVariant value = fkOn.value(i);
        while(value.isValid())
        {
            errorStr += value.toString();
            errorStr += ",";
            i++;
            value = fkOn.value(i);
        }
        errorStr += "\n";
    }
    if(errorStr.length() > 0)
    {
        o_log += "Foreign key constrains failed:\n" + errorStr;
        return 1;
    }
*/

    MainWindow w;
    w.show();
    return app.exec();
}
