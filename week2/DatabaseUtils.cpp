#include "DatabaseUtils.h"
#include <stdio.h>
#include <cstdlib>
#include "pch.h"

void InitDB(sqlite3*& db) {
    int rc = sqlite3_open("C:/Users/OS/source/repos/week2/db.db", &db);

    if (rc) {
        CString errorMsg;
        errorMsg.Format(_T("Can't open database: %s"), CString(sqlite3_errmsg(db)));
        AfxMessageBox(errorMsg, MB_ICONERROR);
        exit(0);
    }
}