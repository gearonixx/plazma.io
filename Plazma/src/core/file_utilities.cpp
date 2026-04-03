#include <QString>

namespace FileUtils {

QString AllFilesFilter() {
#ifdef Q_OS_WIN
    return QStringLiteral("All files (*.*)");
#else   // Q_OS_WIN
    return QStringLiteral("All files (*)");
#endif  // Q_OS_WIN
}

}  // namespace FileUtils
