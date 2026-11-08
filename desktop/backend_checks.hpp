#pragma once
#include <QString>
class Controller;
int runBackendChecks(Controller* controller,const QString& destination,bool* qmlFailure);
